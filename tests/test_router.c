#include "cunit.h"
#include "spacewire.h"
#include "test_runners.h"

#include <string.h>

static int test_router_init(void)
{
    sw_router_t router;
    sw_router_init(&router, 4);
    ASSERT_EQ_INT(4, router.num_ports);
    ASSERT_EQ_INT(0, router.links[0].port_id);
    ASSERT_EQ_INT(SW_LINK_UNINITIALIZED, router.links[0].state);
    ASSERT_EQ_INT(0, (int)router.invalid_address_errors);

    /* num_ports is clamped to [1, SW_NUM_PORTS]. */
    sw_router_init(&router, 0);
    ASSERT_EQ_INT(1, router.num_ports);
    sw_router_init(&router, 200);
    ASSERT_EQ_INT(SW_NUM_PORTS, router.num_ports);

    sw_router_init(NULL, 4); /* must not crash */
    return 0;
}

static int test_router_path_addressing(void)
{
    sw_router_t router;
    sw_router_init(&router, 8); /* ports 0..7 */

    uint8_t port = 0xFF;
    uint8_t del = 0xFF;

    /* Leading char 3 -> output port 3, always deleted (clause 5.6.8.3). */
    const uint8_t pkt[3] = {3, 0xAA, 0xBB};
    ASSERT_EQ_INT(SW_ROUTE_OK, sw_router_route(&router, pkt, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(3, port);
    ASSERT_EQ_INT(1, del);

    /* Leading char 0 -> configuration port. */
    const uint8_t cfg[2] = {0, 0x11};
    ASSERT_EQ_INT(SW_ROUTE_OK, sw_router_route(&router, cfg, sizeof(cfg), &port, &del));
    ASSERT_EQ_INT(SW_PORT_CONFIG, port);
    ASSERT_EQ_INT(1, del);

    /* A path char referencing a non-existent port is discarded (clause 5.6.8.5). */
    const uint8_t bad[2] = {20, 0x11};
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, bad, sizeof(bad), &port, &del));
    ASSERT_EQ_INT(1, (int)router.invalid_address_errors);
    return 0;
}

static int test_router_logical_addressing(void)
{
    sw_router_t router;
    sw_router_init(&router, 8);

    ASSERT_EQ_INT(1, sw_router_add_route(&router, 0x40, 5, 0)); /* retain address */
    ASSERT_EQ_INT(1, sw_router_add_route(&router, 0x41, 6, 1)); /* delete address */

    uint8_t port = 0xFF;
    uint8_t del = 0xFF;

    /* Logical address retained by default (clause 5.6.8.4 f). */
    const uint8_t p1[2] = {0x40, 0x99};
    ASSERT_EQ_INT(SW_ROUTE_OK, sw_router_route(&router, p1, sizeof(p1), &port, &del));
    ASSERT_EQ_INT(5, port);
    ASSERT_EQ_INT(0, del);

    /* Logical address deleted when configured to (clause 5.6.8.6). */
    const uint8_t p2[2] = {0x41, 0x99};
    ASSERT_EQ_INT(SW_ROUTE_OK, sw_router_route(&router, p2, sizeof(p2), &port, &del));
    ASSERT_EQ_INT(6, port);
    ASSERT_EQ_INT(1, del);

    /* Unconfigured logical address -> discard + error. */
    const uint8_t p3[2] = {0x77, 0x99};
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, p3, sizeof(p3), &port, &del));

    /* Reserved logical address 255 -> discard + error (clause 5.6.8.5 NOTE). */
    const uint8_t p4[2] = {0xFF, 0x99};
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, p4, sizeof(p4), &port, &del));
    ASSERT_EQ_INT(2, (int)router.invalid_address_errors);
    return 0;
}

static int test_router_add_route_validation(void)
{
    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 */

    ASSERT_EQ_INT(0, sw_router_add_route(NULL, 0x40, 1, 0));
    /* A path-range address may not be used as a logical route. */
    ASSERT_EQ_INT(0, sw_router_add_route(&router, 0x10, 1, 0));
    /* The reserved address 255 may not be configured. */
    ASSERT_EQ_INT(0, sw_router_add_route(&router, 0xFF, 1, 0));
    /* Output port must exist. */
    ASSERT_EQ_INT(0, sw_router_add_route(&router, 0x40, 4, 0));
    /* Boundaries: lowest logical address and highest valid port. */
    ASSERT_EQ_INT(1, sw_router_add_route(&router, 0x20, 3, 0));
    ASSERT_EQ_INT(1, sw_router_add_route(&router, 0xFE, 0, 0));
    return 0;
}

static int test_router_route_invalid_args_and_empty(void)
{
    sw_router_t router;
    sw_router_init(&router, 4);

    const uint8_t pkt[2] = {0x40, 0x00};
    uint8_t port = 0;
    uint8_t del = 0;

    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(NULL, pkt, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, NULL, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, pkt, sizeof(pkt), NULL, &del));
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, pkt, sizeof(pkt), &port, NULL));
    /* Invalid arguments are rejected before any counter is touched. */
    ASSERT_EQ_INT(0, (int)router.packets_discarded);

    /* An empty packet is discarded by the first routing switch (clause 5.6.2.1). */
    ASSERT_EQ_INT(SW_ROUTE_DISCARD, sw_router_route(&router, pkt, 0, &port, &del));
    ASSERT_EQ_INT(1, (int)router.packets_discarded);
    return 0;
}

static int test_link_layer_state_helpers(void)
{
    const sw_link_config_t config = {
        .bit_rate = 1000000,
        .disconnect_timeout = 2500,
        .rx_credit_max = 12,
        .enable_crc = 1,
    };

    sw_link_layer_t link;
    memset(&link, 0xA5, sizeof(link));

    sw_link_init(NULL, &config);
    sw_link_init(&link, NULL);

    sw_link_init(&link, &config);
    ASSERT_EQ_INT(config.bit_rate, link.config.bit_rate);
    ASSERT_EQ_INT(config.disconnect_timeout, link.config.disconnect_timeout);
    ASSERT_EQ_INT(config.rx_credit_max, link.rx_credits);
    ASSERT_EQ_INT(0, link.tx_credits);
    ASSERT_EQ_INT(SW_LINK_UNINITIALIZED, link.state);

    ASSERT_EQ_INT(SW_LINK_ERROR, sw_link_get_state(NULL));
    ASSERT_EQ_INT(SW_LINK_UNINITIALIZED, sw_link_get_state(&link));

    sw_link_set_state(NULL, SW_LINK_CONNECTED);
    sw_link_set_state(&link, SW_LINK_CONNECTED);
    ASSERT_EQ_INT(SW_LINK_CONNECTED, sw_link_get_state(&link));

    return 0;
}

test_result_t test_spacewire_router_run_all(void)
{
    RUN_TEST(test_router_init);
    RUN_TEST(test_router_path_addressing);
    RUN_TEST(test_router_logical_addressing);
    RUN_TEST(test_router_add_route_validation);
    RUN_TEST(test_router_route_invalid_args_and_empty);
    RUN_TEST(test_link_layer_state_helpers);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
