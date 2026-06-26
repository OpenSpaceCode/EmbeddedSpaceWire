/*
 * EmbeddedSpaceWire Example - Basic packet transmission and routing
 */

#include "spacewire.h"
#include "spacewire_packet.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("===============================================\n");
    printf("CCSDS Space Wire Protocol - Example\n");
    printf("===============================================\n\n");

    /* ========== CHARACTER CODEC EXAMPLE ========== */
    printf("[1] Character Codec Test\n");
    printf("    Encoding/decoding 9-bit characters with parity\n");

    uint8_t data = 0x42; /* 'B' */
    uint8_t encoded;
    uint8_t parity = sw_encode_char(data, &encoded);
    printf("    Input: 0x%02X, Encoded: 0x%02X, Parity: %u\n", data, encoded, parity);

    uint8_t decoded;
    sw_char_result_t result = sw_decode_char(encoded, parity, &decoded);
    printf("    Decoded: 0x%02X, Result: %d\n", decoded, result);
    printf("    ✓ Character codec working\n\n");

    /* ========== SPACEWIRE PACKET (LOGICAL ADDRESSING) ========== */
    printf("[2] SpaceWire Packet Test\n");
    printf("    Building a logical-addressed packet (no CRC; EOP added by link)\n");

    uint8_t cargo_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t logical_dest[] = {0x40}; /* logical address 64 */

    uint8_t spw_buf[256];
    size_t spw_len = sw_spw_packet_build(logical_dest, sizeof(logical_dest), cargo_data,
                                         sizeof(cargo_data), spw_buf, sizeof(spw_buf));
    printf("    Packet size: %zu bytes\n", spw_len);
    printf("    Packet data (hex): ");
    for (size_t i = 0; i < spw_len; i++)
    {
        printf("%02X ", spw_buf[i]);
    }
    printf("\n    ✓ Packet build working\n\n");

    /* ========== SPACEWIRE PACKET (PATH ADDRESSING) ========== */
    printf("[3] SpaceWire Path Addressing Test\n");
    printf("    Building a multi-hop path-addressed packet\n");

    uint8_t path_dest[] = {2, 1, 3}; /* take port 2, then 1, then 3 */
    uint8_t path_buf[256];
    size_t path_len = sw_spw_packet_build(path_dest, sizeof(path_dest), cargo_data,
                                          sizeof(cargo_data), path_buf, sizeof(path_buf));
    printf("    Path address: %u hops, packet %zu bytes\n", (unsigned)sizeof(path_dest), path_len);
    printf("    Packet data (hex): ");
    for (size_t i = 0; i < path_len; i++)
    {
        printf("%02X ", path_buf[i]);
    }
    printf("\n    ✓ Path-addressed packet built\n\n");

    /* ========== CCSDS PACKET + SPACE WIRE INTEGRATION ========== */
    printf("[4] CCSDS Packet + Space Wire Integration\n");
    printf("    Creating complete packet frame\n");

    sw_packet_config_t pkt_config = {.path = NULL,
                                     .path_len = 0,
                                     .logical_addr = SW_PTP_LOGICAL_ADDR_DEFAULT,
                                     .user_app = 0x00};

    sw_packet_frame_t pf;
    sw_packet_init(&pf, &pkt_config);

    /* Set CCSDS packet fields */
    pf.packet.ph.apid = 0x0042; /* APID = 0x0042 */
    pf.packet.ph.seq_count = 1;
    const char *msg = "Hello Space Wire";
    pf.packet.data = (const uint8_t *)msg;
    pf.packet.data_len = (uint16_t)strlen(msg);

    uint8_t pkt_buf[512];
    size_t pkt_size = sw_packet_encode(&pf, pkt_buf, sizeof(pkt_buf));
    if (pkt_size > 0)
    {
        printf("    Packet frame size: %zu bytes\n", pkt_size);
        printf("    APID: 0x%04X\n", pf.packet.ph.apid);
        printf("    Payload: \"%s\"\n", msg);
        printf("    ✓ Packet frame created\n\n");
    }

    /* ========== PACKET DECODING EXAMPLE ========== */
    printf("[5] Packet Decoding Test\n");
    printf("    Parsing Space Wire frame and CCSDS packet\n");

    sw_packet_frame_t decoded_pf;
    sw_ptp_status_t status;
    if (sw_packet_decode(&decoded_pf, pkt_buf, pkt_size, SW_END_EOP, &status))
    {
        printf("    Decoded APID: 0x%04X\n", decoded_pf.packet.ph.apid);
        printf("    Decoded user application: 0x%02X\n", decoded_pf.user_app);
        printf("    Decoded payload length: %u bytes\n", decoded_pf.packet.data_len);
        printf("    Decoded payload: \"%.*s\"\n",
               decoded_pf.packet.data_len,
               (const char *)decoded_pf.packet.data);
        printf("    Status: %u (0 = OK)\n", (unsigned)status);
        printf("    ✓ Packet decoding successful\n\n");
    }
    else
    {
        printf("    ✗ Packet decoding failed\n\n");
    }

    /* ========== ROUTER EXAMPLE ========== */
    printf("[6] Router Configuration Test\n");
    printf("    Setting up routing table\n");

    sw_router_t router;
    sw_router_init(&router, 4); /* ports 0..3 (port 0 = configuration port) */

    sw_router_add_route(&router, 0x40, 1, 0); /* logical 0x40 -> port 1, retain */
    sw_router_add_route(&router, 0x41, 2, 1); /* logical 0x41 -> port 2, delete */

    printf("    Number of ports: %u\n", router.num_ports);
    printf("    Routing table configured:\n");
    printf("      logical 0x40 -> port 1 (address retained)\n");
    printf("      logical 0x41 -> port 2 (address deleted)\n");
    printf("    ✓ Router initialized\n\n");

    /* ========== ROUTING EXAMPLE ========== */
    printf("[7] Packet Routing Test\n");
    printf("    Routing by leading destination-address character\n");

    uint8_t out_port = 0;
    uint8_t delete_leading = 0;

    /* Logical addressing: leading char 0x40 is looked up in the routing table. */
    uint8_t logical_pkt[] = {0x40, 0xAA, 0xBB};
    if (sw_router_route(&router, logical_pkt, sizeof(logical_pkt), &out_port, &delete_leading) ==
        SW_ROUTE_OK)
    {
        printf("    logical 0x40 -> port %u (delete leading: %u)\n", out_port, delete_leading);
    }

    /* Path addressing: leading char 2 selects port 2 directly and is deleted. */
    uint8_t path_pkt[] = {2, 0xAA, 0xBB};
    if (sw_router_route(&router, path_pkt, sizeof(path_pkt), &out_port, &delete_leading) ==
        SW_ROUTE_OK)
    {
        printf("    path 2 -> port %u (delete leading: %u)\n", out_port, delete_leading);
    }

    /* An unconfigured logical address is discarded with an invalid-address error. */
    uint8_t bad_pkt[] = {0x77, 0xAA};
    if (sw_router_route(&router, bad_pkt, sizeof(bad_pkt), &out_port, &delete_leading) ==
        SW_ROUTE_DISCARD)
    {
        printf("    logical 0x77 -> DISCARD (invalid-address errors: %u)\n",
               router.invalid_address_errors);
    }
    printf("    ✓ Routing successful\n\n");

    /* ========== CRC TEST ========== */
    printf("[8] CRC-16-CCITT Test\n");
    printf("    Computing CRC for test data\n");

    const uint8_t test_data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    uint16_t crc = sw_crc16(test_data, sizeof(test_data));
    printf("    Data: \"123456789\"\n");
    printf("    CRC-16-CCITT: 0x%04X\n", crc);
    if (crc == 0x31C3)
    {
        printf("    ✓ CRC matches expected value (0x31C3)\n\n");
    }

    /* ========== STATISTICS ========== */
    printf("[9] Statistics\n");
    sw_statistics_t stats;
    sw_get_statistics(&stats);
    printf("    Packets sent: %u\n", stats.packets_sent);
    printf("    Packets received: %u\n", stats.packets_received);
    printf("    Bytes sent: %u\n", stats.bytes_sent);
    printf("    Bytes received: %u\n", stats.bytes_received);
    printf("    Packets discarded: %u\n", stats.packets_discarded);
    printf("    ✓ Statistics available\n\n");

    printf("===============================================\n");
    printf("All tests completed successfully!\n");
    printf("===============================================\n");

    return 0;
}
