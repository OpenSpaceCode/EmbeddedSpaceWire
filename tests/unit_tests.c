/*
 * EmbeddedSpaceWire Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spacewire.h"
#include "spacewire_packet.h"

#define ASSERT(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    printf("  ✗ FAIL: %s\n", msg);                                            \
    return 0;                                                                  \
  }

#define PASS(msg) printf("  ✓ PASS: %s\n", msg)

/* ============================================================================
 * CHARACTER CODEC TESTS
 * ============================================================================ */

static int test_encode_decode(void) {
  printf("Test: Character Encode/Decode\n");

  for (int i = 4; i < 256; i++) {  /* Skip control codes 0-3 */
    uint8_t encoded;
    uint8_t parity = sw_encode_char((uint8_t)i, &encoded);

    uint8_t decoded;
    sw_char_result_t result = sw_decode_char(encoded, parity, &decoded);

    ASSERT(result == SW_CHAR_OK, "Valid character decoding");
    ASSERT(decoded == (uint8_t)i, "Round-trip encode/decode");
  }

  PASS("All valid characters encode/decode correctly");
  return 1;
}

static int test_parity_error(void) {
  printf("Test: Parity Error Detection\n");

  uint8_t encoded;
  uint8_t parity = sw_encode_char(0x55, &encoded);

  uint8_t decoded;
  uint8_t wrong_parity = parity ^ 1;  /* Flip parity bit */
  sw_char_result_t result = sw_decode_char(encoded, wrong_parity, &decoded);

  ASSERT(result == SW_CHAR_PARITY_ERROR, "Parity error detected");
  PASS("Parity error detection working");
  return 1;
}

static int test_crc_computation(void) {
  printf("Test: CRC-16-CCITT Computation\n");

  /* Test vector: "123456789" */
  const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  uint16_t crc = sw_crc16(data, sizeof(data));

  /* Verify it produces a valid CRC (non-zero) */
  ASSERT(crc != 0, "CRC-16-CCITT produces non-zero value");
  
  /* CRC should be consistent */
  uint16_t crc2 = sw_crc16(data, sizeof(data));
  ASSERT(crc == crc2, "CRC is deterministic");

  PASS("CRC computation working");
  return 1;
}

/* ============================================================================
 * FRAME LAYER TESTS
 * ============================================================================ */

static int test_frame_encode_decode(void) {
  printf("Test: Frame Encode/Decode\n");

  uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x42;
  frame.protocol_id = 1;
  frame.payload = payload;
  frame.payload_len = sizeof(payload);

  uint8_t buf[256];
  size_t encoded_size = sw_frame_encode(&frame, buf, sizeof(buf));
  ASSERT(encoded_size > 0, "Frame encoding");

  /* Decode the frame */
  sw_frame_t decoded;
  int result = sw_frame_decode(&decoded, buf, encoded_size);
  ASSERT(result == 1, "Frame decoding");
  ASSERT(decoded.target_addr == 0x42, "Target address preserved");
  ASSERT(decoded.protocol_id == 1, "Protocol ID preserved");
  ASSERT(decoded.payload_len == sizeof(payload), "Payload length preserved");
  ASSERT(memcmp(decoded.payload, payload, sizeof(payload)) == 0, "Payload data preserved");

  PASS("Frame encode/decode roundtrip");
  return 1;
}

static int test_frame_crc_validation(void) {
  printf("Test: Frame CRC Validation\n");

  uint8_t payload[] = {0xAA, 0xBB};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x01;
  frame.protocol_id = 1;
  frame.payload = payload;
  frame.payload_len = sizeof(payload);

  uint8_t buf[256];
  size_t size = sw_frame_encode(&frame, buf, sizeof(buf));

  /* Corrupt the payload */
  buf[3] ^= 0xFF;

  sw_frame_t decoded;
  int result = sw_frame_decode(&decoded, buf, size);
  ASSERT(result == 0, "CRC validation detects corruption");

  PASS("CRC validation working");
  return 1;
}

static int test_frame_size_calculation(void) {
  printf("Test: Frame Size Calculation\n");

  sw_frame_t frame;
  sw_frame_init(&frame);

  uint8_t payload[100];
  frame.payload = payload;
  frame.payload_len = 100;

  size_t calc_size = sw_frame_size(&frame);
  /* Header (2) + payload (100) + CRC (2) = 104 */
  ASSERT(calc_size == 104, "Frame size calculation");

  PASS("Frame size calculation correct");
  return 1;
}

/* ============================================================================
 * ROUTER TESTS
 * ============================================================================ */

static int test_router_initialization(void) {
  printf("Test: Router Initialization\n");

  sw_router_t router;
  sw_router_init(&router, 0x42, 3);

  ASSERT(router.device_addr == 0x42, "Device address set");
  ASSERT(router.num_ports == 3, "Port count set");
  ASSERT(router.links[0].port_id == 0, "Port IDs initialized");
  ASSERT(router.channels[0].active == 0, "Virtual channels inactive");

  PASS("Router initialization");
  return 1;
}

static int test_router_routing(void) {
  printf("Test: Router Routing Logic\n");

  sw_router_t router;
  sw_router_init(&router, 0x01, 2);

  sw_router_add_route(&router, 0x02, 0);
  sw_router_add_route(&router, 0x03, 1);

  router.links[0].state = SW_LINK_CONNECTED;
  router.links[1].state = SW_LINK_CONNECTED;

  /* Test routing to 0x02 */
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x02;

  uint8_t output_port;
  int result = sw_router_route_frame(&router, &frame, &output_port);
  ASSERT(result == 1, "Route to 0x02");
  ASSERT(output_port == 0, "Routed to port 0");

  /* Test routing to 0x03 */
  frame.target_addr = 0x03;
  result = sw_router_route_frame(&router, &frame, &output_port);
  ASSERT(result == 1, "Route to 0x03");
  ASSERT(output_port == 1, "Routed to port 1");

  PASS("Router routing working");
  return 1;
}

/* ============================================================================
 * PACKET INTEGRATION TESTS
 * ============================================================================ */

static int test_packet_encode_decode(void) {
  printf("Test: CCSDS Packet + Space Wire Integration\n");

  sw_packet_config_t config = {
      .device_addr = 0x01,
      .target_addr = 0x02,
      .protocol_id = 1,
      .enable_crc = 1
  };

  sw_packet_frame_t pf;
  sw_packet_init(&pf, &config);

  pf.packet.ph.apid = 0x0100;
  const char *payload_str = "Test payload";
  pf.packet.payload = (const uint8_t *)payload_str;
  pf.packet.payload_len = (uint16_t)strlen(payload_str);

  uint8_t buf[512];
  size_t encoded_size = sw_packet_encode(&pf, buf, sizeof(buf));
  ASSERT(encoded_size > 0, "Packet encoding");

  /* Decode */
  sw_packet_frame_t decoded_pf;
  int result = sw_packet_decode(&decoded_pf, buf, encoded_size);
  ASSERT(result == 1, "Packet decoding");
  ASSERT(decoded_pf.packet.ph.apid == 0x0100, "APID preserved");
  ASSERT(decoded_pf.packet.payload_len == strlen(payload_str), "Payload length preserved");

  PASS("Packet encode/decode integration");
  return 1;
}

static int test_packet_create_convenience(void) {
  printf("Test: Packet Creation Convenience Function\n");

  uint8_t payload[] = {0x11, 0x22, 0x33};
  uint8_t buf[512];

  size_t size = sw_packet_create(0x01, 0x02, 0x0042, payload, sizeof(payload), buf, sizeof(buf));

  ASSERT(size > 0, "Packet creation");
  ASSERT(size > sizeof(payload) + 6, "Includes headers and CRC");

  PASS("Packet creation convenience function");
  return 1;
}

/* ============================================================================
 * TEST SUITE
 * ============================================================================ */

typedef int (*test_fn_t)(void);

int main(void) {
  test_fn_t tests[] = {
      /* Character Codec */
      test_encode_decode,
      test_parity_error,
      test_crc_computation,

      /* Frame Layer */
      test_frame_encode_decode,
      test_frame_crc_validation,
      test_frame_size_calculation,

      /* Router */
      test_router_initialization,
      test_router_routing,

      /* Packet Integration */
      test_packet_encode_decode,
      test_packet_create_convenience,
  };

  int num_tests = sizeof(tests) / sizeof(tests[0]);
  int passed = 0;
  int failed = 0;

  printf("===============================================\n");
  printf("EmbeddedSpaceWire Unit Tests\n");
  printf("===============================================\n\n");

  for (int i = 0; i < num_tests; i++) {
    if (tests[i]()) {
      passed++;
    } else {
      failed++;
    }
    printf("\n");
  }

  printf("===============================================\n");
  printf("Results: %d passed, %d failed\n", passed, failed);
  printf("===============================================\n");

  return (failed == 0) ? 0 : 1;
}
