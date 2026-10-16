/**
 * @file    test_router.c
 * @brief   Unit tests for the SpaceWire routing switch
 *
 * Exercises path and logical addressing, header deletion and invalid-address
 * handling of ECSS-E-ST-50-12C Rev.1, clause 5.6.8.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "cunit.h"
#include "spacewire.h"
#include "test_runners.h"

static int test_router_init(void)
{
    sw_router_t router;
    sw_router_init(&router, 4);
    ASSERT_EQ_INT(4, router.num_ports);
    ASSERT_EQ_INT(0, (int)router.invalid_address_errors);
    ASSERT_EQ_INT(0, (int)router.packets_routed);
    ASSERT_EQ_INT(0, (int)router.packets_discarded);

    sw_router_init(NULL, 4); /* must not crash */
    return 0;
}

/* num_ports is clamped to [1, SW_NUM_PORTS]: each limit and its neighbour. */
static int test_router_init_port_count_bounds(void)
{
    sw_router_t router;

    sw_router_init(&router, 0);
    ASSERT_EQ_INT(1, router.num_ports);
    sw_router_init(&router, 1);
    ASSERT_EQ_INT(1, router.num_ports);
    sw_router_init(&router, SW_NUM_PORTS);
    ASSERT_EQ_INT(SW_NUM_PORTS, router.num_ports);
    sw_router_init(&router, SW_NUM_PORTS + 1u);
    ASSERT_EQ_INT(SW_NUM_PORTS, router.num_ports);
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
    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, pkt, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(3, port);
    ASSERT_EQ_INT(1, del);

    /* Leading char 0 -> configuration port. */
    const uint8_t cfg[2] = {0, 0x11};
    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, cfg, sizeof(cfg), &port, &del));
    ASSERT_EQ_INT(SW_PORT_CONFIG, port);
    ASSERT_EQ_INT(1, del);

    /* A path char referencing a non-existent port is discarded (clause 5.6.8.5). */
    const uint8_t bad[2] = {20, 0x11};
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_route(&router, bad, sizeof(bad), &port, &del));
    ASSERT_EQ_INT(1, (int)router.invalid_address_errors);
    ASSERT_EQ_INT(2, (int)router.packets_routed);
    ASSERT_EQ_INT(1, (int)router.packets_discarded);
    return 0;
}

/* Path addressing: the last existing port is reachable, the next one is not. */
static int test_router_path_port_bounds(void)
{
    sw_router_t router;
    sw_router_init(&router, 8); /* ports 0..7 */

    uint8_t port = 0xEE;
    uint8_t del = 0xEE;
    const uint8_t last_port[1] = {7};
    const uint8_t first_missing[1] = {8};

    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, last_port, sizeof(last_port), &port, &del));
    ASSERT_EQ_INT(7, port);
    ASSERT_EQ_INT(1, del);
    ASSERT_EQ_INT(SW_WRONG_ADDRESS,
                  sw_router_route(&router, first_missing, sizeof(first_missing), &port, &del));
    ASSERT_EQ_INT(1, (int)router.packets_routed);
    ASSERT_EQ_INT(1, (int)router.invalid_address_errors);
    return 0;
}

/* 31 is the highest path address; 32 is the lowest logical address (clauses 5.6.8.3-4). */
static int test_router_path_logical_boundary(void)
{
    sw_router_t router;
    sw_router_init(&router, SW_NUM_PORTS); /* ports 0..31 */

    uint8_t port = 0xEE;
    uint8_t del = 0xEE;
    const uint8_t highest_path[1] = {31};
    const uint8_t lowest_logical[1] = {32};

    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, highest_path, sizeof(highest_path), &port, &del));
    ASSERT_EQ_INT(31, port);
    ASSERT_EQ_INT(1, del);

    /* Unconfigured, 32 is an invalid logical address, not a path to port 32. */
    ASSERT_EQ_INT(SW_WRONG_ADDRESS,
                  sw_router_route(&router, lowest_logical, sizeof(lowest_logical), &port, &del));
    ASSERT_EQ_INT(1, (int)router.invalid_address_errors);

    /* Configured, it follows the routing table and keeps its address. */
    ASSERT_EQ_INT(SW_OK, sw_router_add_route(&router, 32, 5, 0));
    ASSERT_EQ_INT(SW_OK,
                  sw_router_route(&router, lowest_logical, sizeof(lowest_logical), &port, &del));
    ASSERT_EQ_INT(5, port);
    ASSERT_EQ_INT(0, del);
    ASSERT_EQ_INT(2, (int)router.packets_routed);
    return 0;
}

static int test_router_logical_addressing(void)
{
    sw_router_t router;
    sw_router_init(&router, 8);

    ASSERT_EQ_INT(SW_OK, sw_router_add_route(&router, 0x40, 5, 0)); /* retain address */
    ASSERT_EQ_INT(SW_OK, sw_router_add_route(&router, 0x41, 6, 1)); /* delete address */

    uint8_t port = 0xFF;
    uint8_t del = 0xFF;

    /* Logical address retained by default (clause 5.6.8.4 f). */
    const uint8_t p1[2] = {0x40, 0x99};
    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, p1, sizeof(p1), &port, &del));
    ASSERT_EQ_INT(5, port);
    ASSERT_EQ_INT(0, del);

    /* Logical address deleted when configured to (clause 5.6.8.6). */
    const uint8_t p2[2] = {0x41, 0x99};
    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, p2, sizeof(p2), &port, &del));
    ASSERT_EQ_INT(6, port);
    ASSERT_EQ_INT(1, del);

    /* Unconfigured logical address -> discard + error. */
    const uint8_t p3[2] = {0x77, 0x99};
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_route(&router, p3, sizeof(p3), &port, &del));

    /* Reserved logical address 255 -> discard + error (clause 5.6.8.5 NOTE). */
    const uint8_t p4[2] = {0xFF, 0x99};
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_route(&router, p4, sizeof(p4), &port, &del));
    ASSERT_EQ_INT(2, (int)router.invalid_address_errors);
    ASSERT_EQ_INT(2, (int)router.packets_routed);
    ASSERT_EQ_INT(2, (int)router.packets_discarded);
    return 0;
}

/* A configured routing-table entry whose output port no longer exists is
 * discarded with an invalid-address error (clause 5.6.8.5). sw_router_add_route()
 * rejects such ports, so the stale entry is injected directly. */
static int test_router_logical_route_stale_port(void)
{
    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 */

    router.routes[0x40].configured = 1;
    router.routes[0x40].output_port = 9; /* >= num_ports */
    router.routes[0x40].delete_addr = 0;

    uint8_t port = 0xFF;
    uint8_t del = 0xFF;
    const uint8_t pkt[2] = {0x40, 0x99};
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_route(&router, pkt, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(1, (int)router.invalid_address_errors);
    return 0;
}

/* A logical address never reaches the configuration port (clause 5.6.8.3 f), even
 * through an entry written into the table directly; path address 0 still does. */
static int test_router_logical_route_to_config_port(void)
{
    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 */

    router.routes[0x40].configured = 1;
    router.routes[0x40].output_port = SW_PORT_CONFIG;
    router.routes[0x40].delete_addr = 0;

    uint8_t port = 0xEE;
    uint8_t del = 0xEE;
    const uint8_t logical[2] = {0x40, 0x99};
    ASSERT_EQ_INT(SW_WRONG_ADDRESS,
                  sw_router_route(&router, logical, sizeof(logical), &port, &del));
    ASSERT_EQ_INT(1, (int)router.invalid_address_errors);
    ASSERT_EQ_INT(0xEE, port);
    ASSERT_EQ_INT(0xEE, del);

    const uint8_t path[2] = {SW_PORT_CONFIG, 0x99};
    ASSERT_EQ_INT(SW_OK, sw_router_route(&router, path, sizeof(path), &port, &del));
    ASSERT_EQ_INT(SW_PORT_CONFIG, port);
    ASSERT_EQ_INT(1, del);
    return 0;
}

static int test_router_add_route_validation(void)
{
    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 */

    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_add_route(NULL, 0x40, 1, 0));
    /* A path-range address may not be used as a logical route; 31 is the highest. */
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_add_route(&router, 0x10, 1, 0));
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_add_route(&router, 31, 1, 0));
    /* The reserved address 255 may not be configured. */
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_add_route(&router, 0xFF, 1, 0));
    /* Output port must exist. */
    ASSERT_EQ_INT(SW_WRONG_PORT, sw_router_add_route(&router, 0x40, 4, 0));
    /* The configuration port is reached by path addressing only (clause 5.6.8.3 f). */
    ASSERT_EQ_INT(SW_WRONG_PORT, sw_router_add_route(&router, 0x40, SW_PORT_CONFIG, 0));
    /* Boundaries: lowest and highest logical address, lowest and highest external port. */
    ASSERT_EQ_INT(SW_OK, sw_router_add_route(&router, 0x20, 3, 0));
    ASSERT_EQ_INT(SW_OK, sw_router_add_route(&router, 0xFE, 1, 0));
    return 0;
}

static int test_router_route_invalid_args_and_empty(void)
{
    sw_router_t router;
    sw_router_init(&router, 4);

    const uint8_t pkt[2] = {0x40, 0x00};
    uint8_t port = 0;
    uint8_t del = 0;

    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_route(NULL, pkt, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_route(&router, NULL, sizeof(pkt), &port, &del));
    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_route(&router, pkt, sizeof(pkt), NULL, &del));
    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_route(&router, pkt, sizeof(pkt), &port, NULL));
    /* Invalid arguments are rejected before any counter is touched. */
    ASSERT_EQ_INT(0, (int)router.packets_discarded);

    /* An empty packet is discarded by the first routing switch (clause 5.6.2.1). */
    ASSERT_EQ_INT(SW_WRONG_LENGTH, sw_router_route(&router, pkt, 0, &port, &del));
    ASSERT_EQ_INT(1, (int)router.packets_discarded);
    return 0;
}

/* Every kind of discard leaves both outputs exactly as the caller passed them. */
static int test_router_route_discard_leaves_outputs_unchanged(void)
{
    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 */
    router.routes[0x41].configured = 1;
    router.routes[0x41].output_port = 9; /* stale entry: the port does not exist */

    const uint8_t missing_port[2] = {4, 0x99};    /* path address beyond the last port */
    const uint8_t unconfigured[2] = {0x40, 0x99}; /* logical address without a route */
    const uint8_t stale[2] = {0x41, 0x99};
    uint8_t port = 0xEE;
    uint8_t del = 0xEE;

    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_route(NULL, stale, sizeof(stale), &port, &del));
    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_router_route(&router, NULL, sizeof(stale), &port, &del));
    ASSERT_EQ_INT(SW_WRONG_LENGTH, sw_router_route(&router, stale, 0, &port, &del));
    ASSERT_EQ_INT(SW_WRONG_ADDRESS,
                  sw_router_route(&router, missing_port, sizeof(missing_port), &port, &del));
    ASSERT_EQ_INT(SW_WRONG_ADDRESS,
                  sw_router_route(&router, unconfigured, sizeof(unconfigured), &port, &del));
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_route(&router, stale, sizeof(stale), &port, &del));

    ASSERT_EQ_INT(0xEE, port);
    ASSERT_EQ_INT(0xEE, del);
    return 0;
}

/* A rejected call does not disturb the routing table. */
static int test_router_add_route_failure_leaves_table_unchanged(void)
{
    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 */
    ASSERT_EQ_INT(SW_OK, sw_router_add_route(&router, 0x40, 2, 1));

    /* Overwriting an existing route with a port that does not exist, or with the
     * configuration port. */
    ASSERT_EQ_INT(SW_WRONG_PORT, sw_router_add_route(&router, 0x40, 4, 0));
    ASSERT_EQ_INT(SW_WRONG_PORT, sw_router_add_route(&router, 0x40, SW_PORT_CONFIG, 0));
    ASSERT_EQ_INT(2, router.routes[0x40].output_port);
    ASSERT_EQ_INT(1, router.routes[0x40].configured);
    ASSERT_EQ_INT(1, router.routes[0x40].delete_addr);

    /* Addresses outside 32..254 never reach the table. */
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_add_route(&router, 31, 1, 0));
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_router_add_route(&router, 255, 1, 0));
    ASSERT_EQ_INT(0, router.routes[31].configured);
    ASSERT_EQ_INT(0, router.routes[255].configured);
    return 0;
}

test_result_t test_spacewire_router_run_all(void)
{
    RUN_TEST(test_router_init);
    RUN_TEST(test_router_init_port_count_bounds);
    RUN_TEST(test_router_path_addressing);
    RUN_TEST(test_router_path_port_bounds);
    RUN_TEST(test_router_path_logical_boundary);
    RUN_TEST(test_router_logical_addressing);
    RUN_TEST(test_router_logical_route_stale_port);
    RUN_TEST(test_router_logical_route_to_config_port);
    RUN_TEST(test_router_add_route_validation);
    RUN_TEST(test_router_route_invalid_args_and_empty);
    RUN_TEST(test_router_route_discard_leaves_outputs_unchanged);
    RUN_TEST(test_router_add_route_failure_leaves_table_unchanged);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
