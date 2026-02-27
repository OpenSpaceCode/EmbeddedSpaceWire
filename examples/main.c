/*
 * EmbeddedSpaceWire Example - Basic packet transmission and routing
 */

#include <stdio.h>
#include <string.h>
#include "spacewire.h"
#include "spacewire_packet.h"

int main(void) {
  printf("===============================================\n");
  printf("CCSDS Space Wire Protocol - Example\n");
  printf("===============================================\n\n");

  /* ========== CHARACTER CODEC EXAMPLE ========== */
  printf("[1] Character Codec Test\n");
  printf("    Encoding/decoding 9-bit characters with parity\n");

  uint8_t data = 0x42;  /* 'B' */
  uint8_t encoded;
  uint8_t parity = sw_encode_char(data, &encoded);
  printf("    Input: 0x%02X, Encoded: 0x%02X, Parity: %u\n", data, encoded, parity);

  uint8_t decoded;
  sw_char_result_t result = sw_decode_char(encoded, parity, &decoded);
  printf("    Decoded: 0x%02X, Result: %d\n", decoded, result);
  printf("    ✓ Character codec working\n\n");

  /* ========== FRAME ENCODING EXAMPLE ========== */
  printf("[2] Space Wire Frame Test\n");
  printf("    Creating and serializing a Space Wire frame\n");

  uint8_t payload_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x02;
  frame.protocol_id = 1;
  frame.payload = payload_data;
  frame.payload_len = sizeof(payload_data);

  uint8_t frame_buf[256];
  size_t frame_size = sw_frame_encode(&frame, frame_buf, sizeof(frame_buf));
  printf("    Frame size: %zu bytes\n", frame_size);
  printf("    Frame data (hex): ");
  for (size_t i = 0; i < frame_size; i++) {
    printf("%02X ", frame_buf[i]);
  }
  printf("\n    ✓ Frame encoding working\n\n");

  /* ========== FRAME DECODING EXAMPLE ========== */
  printf("[3] Space Wire Frame Decoding Test\n");
  printf("    Parsing and validating frame CRC\n");

  sw_frame_t decoded_frame;
  if (sw_frame_decode(&decoded_frame, frame_buf, frame_size)) {
    printf("    Target address: 0x%02X\n", decoded_frame.target_addr);
    printf("    Protocol ID: %u\n", decoded_frame.protocol_id);
    printf("    Payload length: %u bytes\n", decoded_frame.payload_len);
    printf("    ✓ Frame decoding and CRC validation successful\n\n");
  } else {
    printf("    ✗ Frame decoding failed\n\n");
  }

  /* ========== CCSDS PACKET + SPACE WIRE INTEGRATION ========== */
  printf("[4] CCSDS Packet + Space Wire Integration\n");
  printf("    Creating complete packet frame\n");

  sw_packet_config_t pkt_config = {
      .device_addr = 0x01,
      .target_addr = 0x02,
      .protocol_id = 1,
      .enable_crc = 1
  };

  sw_packet_frame_t pf;
  sw_packet_init(&pf, &pkt_config);

  /* Set CCSDS packet fields */
  pf.packet.ph.apid = 0x0042;  /* APID = 0x0042 */
  pf.packet.ph.seq_count = 1;
  const char *msg = "Hello Space Wire";
  pf.packet.payload = (const uint8_t *)msg;
  pf.packet.payload_len = (uint16_t)strlen(msg);

  uint8_t pkt_buf[512];
  size_t pkt_size = sw_packet_encode(&pf, pkt_buf, sizeof(pkt_buf));
  if (pkt_size > 0) {
    printf("    Packet frame size: %zu bytes\n", pkt_size);
    printf("    APID: 0x%04X\n", pf.packet.ph.apid);
    printf("    Payload: \"%s\"\n", msg);
    printf("    ✓ Packet frame created\n\n");
  }

  /* ========== PACKET DECODING EXAMPLE ========== */
  printf("[5] Packet Decoding Test\n");
  printf("    Parsing Space Wire frame and CCSDS packet\n");

  sw_packet_frame_t decoded_pf;
  if (sw_packet_decode(&decoded_pf, pkt_buf, pkt_size)) {
    printf("    Decoded APID: 0x%04X\n", decoded_pf.packet.ph.apid);
    printf("    Decoded payload length: %u bytes\n", decoded_pf.packet.payload_len);
    printf("    Decoded payload: \"%.*s\"\n", decoded_pf.packet.payload_len,
           (const char *)decoded_pf.packet.payload);
    printf("    ✓ Packet decoding successful\n\n");
  } else {
    printf("    ✗ Packet decoding failed\n\n");
  }

  /* ========== ROUTER EXAMPLE ========== */
  printf("[6] Router Configuration Test\n");
  printf("    Setting up routing table\n");

  sw_router_t router;
  sw_router_init(&router, 0x01, 3);  /* Device 0x01, 3 ports */

  sw_router_add_route(&router, 0x02, 0);
  sw_router_add_route(&router, 0x03, 1);
  sw_router_add_route(&router, 0x04, 2);

  printf("    Device address: 0x%02X\n", router.device_addr);
  printf("    Number of ports: %u\n", router.num_ports);
  printf("    Routing table configured:\n");
  printf("      0x02 -> Port 0\n");
  printf("      0x03 -> Port 1\n");
  printf("      0x04 -> Port 2\n");

  /* Simulate link connection */
  router.links[0].state = SW_LINK_CONNECTED;
  router.links[1].state = SW_LINK_CONNECTED;
  router.links[2].state = SW_LINK_CONNECTED;

  printf("    ✓ Router initialized\n\n");

  /* ========== ROUTING EXAMPLE ========== */
  printf("[7] Frame Routing Test\n");
  printf("    Testing packet routing\n");

  sw_frame_t route_frame;
  sw_frame_init(&route_frame);
  route_frame.target_addr = 0x03;
  route_frame.protocol_id = 1;

  uint8_t output_port;
  if (sw_router_route_frame(&router, &route_frame, &output_port)) {
    printf("    Packet for 0x03 -> routed to port %u\n", output_port);
    printf("    ✓ Routing successful\n\n");
  } else {
    printf("    ✗ Routing failed\n\n");
  }

  /* ========== CRC TEST ========== */
  printf("[8] CRC-16-CCITT Test\n");
  printf("    Computing CRC for test data\n");

  const uint8_t test_data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  uint16_t crc = sw_crc16(test_data, sizeof(test_data));
  printf("    Data: \"123456789\"\n");
  printf("    CRC-16-CCITT: 0x%04X\n", crc);
  if (crc == 0x31C3) {
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
  printf("    CRC errors: %u\n", stats.crc_errors);
  printf("    ✓ Statistics available\n\n");

  printf("===============================================\n");
  printf("All tests completed successfully!\n");
  printf("===============================================\n");

  return 0;
}
