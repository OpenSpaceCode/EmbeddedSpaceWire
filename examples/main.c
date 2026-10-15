/**
 * @file    main.c
 * @brief   EmbeddedSpaceWire example: SpaceWire packets, packet transfer and routing
 *
 * Exercises the packet and network layers in software, using the default
 * payload hooks, so the carried packet is raw octets. The character/signal and
 * link levels are handled by the SpaceWire hardware CODEC.
 * See also: docs/space_packet_integration.md
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "spacewire.h"
#include "spacewire_packet.h"
#include "spacewire_payload.h"

#include <inttypes.h>
#include <stdio.h>

/**
 * @brief Print a labelled buffer as hex octets on one line.
 *
 * @param[in] label Text printed before the octets.
 * @param[in] buf   Octets to print.
 * @param[in] len   Number of octets.
 */
static void example_print_hex(const char *label, const uint8_t *buf, size_t len)
{
    printf("    %-18s", label);
    for (size_t i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

/**
 * @brief Build a logical-addressed and a path-addressed SpaceWire packet
 *        (ECSS-E-ST-50-12C clause 5.6.2).
 *
 * @return 0 on success, 1 on failure.
 */
static int example_spw_packets(void)
{
    static const uint8_t cargo[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    static const uint8_t logical_dest[] = {0x40};
    static const uint8_t path_dest[] = {2, 1, 3}; /* leave by port 2, then 1, then 3 */
    uint8_t buf[16];

    printf("[1] SpaceWire packets (no CRC; the link appends EOP)\n");

    size_t len = sw_spw_packet_build(logical_dest,
                                     sizeof(logical_dest),
                                     cargo,
                                     sizeof(cargo),
                                     buf,
                                     sizeof(buf));
    if (len == 0)
    {
        return 1;
    }
    example_print_hex("logical-addressed:", buf, len);

    len = sw_spw_packet_build(path_dest, sizeof(path_dest), cargo, sizeof(cargo), buf, sizeof(buf));
    if (len == 0)
    {
        return 1;
    }
    example_print_hex("path-addressed:", buf, len);

    return 0;
}

/**
 * @brief Encapsulate a carried packet and extract it again (ECSS-E-ST-50-53C).
 *
 * @return 0 on success, 1 on failure.
 */
static int example_packet_transfer(void)
{
    /* With the default payload hooks the carried packet is raw octets: here a
     * ready-made CCSDS Space Packet (TM, APID 0x042, 4 data octets). */
    static const uint8_t space_packet[] =
        {0x00, 0x42, 0xC0, 0x01, 0x00, 0x03, 0xDE, 0xAD, 0xBE, 0xEF};
    const sw_payload_raw_t payload = {.data = space_packet, .len = sizeof(space_packet)};
    const sw_packet_frame_t frame = {.path = NULL,
                                     .path_len = 0,
                                     .logical_addr = SW_PTP_LOGICAL_ADDR_DEFAULT,
                                     .user_app = 0x00};
    uint8_t buf[32];

    printf("[2] CCSDS Packet Transfer Protocol\n");

    const uint32_t len = sw_packet_encode(&frame, &payload, buf, sizeof(buf));
    if (len == 0u)
    {
        return 1;
    }
    example_print_hex("encoded:", buf, len);

    sw_packet_frame_t received;
    sw_payload_raw_t carried;
    sw_ptp_status_t status;
    if (sw_packet_decode(&received, &carried, buf, len, SW_END_EOP, &status) != SW_OK)
    {
        return 1;
    }
    printf("    decoded: logical address 0x%02X, user application 0x%02X, status %u\n",
           received.logical_addr,
           received.user_app,
           (unsigned)status);
    example_print_hex("carried packet:", carried.data, carried.len);

    return 0;
}

/**
 * @brief Route one packet and print the decision.
 *
 * @param[in,out] router   Router to ask.
 * @param[in]     packet   Packet octets; the destination address leads.
 * @param[in]     len      Packet length in octets.
 * @param[in]     expected Decision the example expects.
 * @return 0 if the router decided as expected, 1 otherwise.
 */
static int example_route_one(sw_router_t *router,
                             const uint8_t *packet,
                             size_t len,
                             sw_route_result_t expected)
{
    uint8_t port = 0;
    uint8_t delete_leading = 0;
    const sw_route_result_t result = sw_router_route(router, packet, len, &port, &delete_leading);

    if (result == SW_ROUTE_OK)
    {
        printf("    leading 0x%02X -> port %u (delete leading: %u)\n",
               packet[0],
               port,
               delete_leading);
    }
    else
    {
        printf("    leading 0x%02X -> discarded\n", packet[0]);
    }

    return (result == expected) ? 0 : 1;
}

/**
 * @brief Configure a routing switch and route three packets
 *        (ECSS-E-ST-50-12C clause 5.6.8).
 *
 * @return 0 on success, 1 on failure.
 */
static int example_routing(void)
{
    static const uint8_t logical_pkt[] = {0x40, 0xAA, 0xBB};
    static const uint8_t path_pkt[] = {2, 0xAA, 0xBB};
    static const uint8_t unknown_pkt[] = {0x77, 0xAA};
    sw_router_t router;
    int failures = 0;

    printf("[3] Routing (port 0 is the configuration port)\n");

    sw_router_init(&router, 4);
    if ((sw_router_add_route(&router, 0x40, 1, 0) != SW_OK) ||
        (sw_router_add_route(&router, 0x41, 2, 1) != SW_OK))
    {
        return 1;
    }

    failures += example_route_one(&router, logical_pkt, sizeof(logical_pkt), SW_ROUTE_OK);
    failures += example_route_one(&router, path_pkt, sizeof(path_pkt), SW_ROUTE_OK);
    failures += example_route_one(&router, unknown_pkt, sizeof(unknown_pkt), SW_ROUTE_DISCARD);
    printf("    invalid-address errors: %" PRIu32 "\n", router.invalid_address_errors);

    return (failures == 0) ? 0 : 1;
}

/**
 * @brief Print the packet transfer counters gathered by the steps above.
 */
static void example_statistics(void)
{
    sw_packet_statistics_t stats;

    sw_packet_get_statistics(&stats);
    printf("[4] Statistics\n");
    printf("    packets sent/received/discarded: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n",
           stats.packets_sent,
           stats.packets_received,
           stats.packets_discarded);
    printf("    octets sent/received: %" PRIu32 "/%" PRIu32 "\n",
           stats.bytes_sent,
           stats.bytes_received);
}

int main(void)
{
    int failures = 0;

    failures += example_spw_packets();
    failures += example_packet_transfer();
    failures += example_routing();
    example_statistics();

    printf("%s\n", (failures == 0) ? "Example completed." : "Example FAILED.");

    return (failures == 0) ? 0 : 1;
}
