/*
 * EmbeddedSpaceWire Unit Tests
 * Google Test Framework
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "spacewire.h"
#include "spacewire_packet.h"
}

/* ============================================================================
 * CHARACTER CODEC TESTS
 * ============================================================================ */

TEST(CharacterCodec, EncodeDecode) {
  for (int i = 4; i < 256; i++) {  /* Skip control codes 0-3 */
    uint8_t encoded;
    uint8_t parity = sw_encode_char((uint8_t)i, &encoded);

    uint8_t decoded;
    sw_char_result_t result = sw_decode_char(encoded, parity, &decoded);

    EXPECT_EQ(result, SW_CHAR_OK) << "Valid character decoding failed for i=" << i;
    EXPECT_EQ(decoded, (uint8_t)i) << "Round-trip encode/decode failed for i=" << i;
  }
}

TEST(CharacterCodec, ParityError) {
  uint8_t encoded;
  uint8_t parity = sw_encode_char(0x55, &encoded);

  uint8_t decoded;
  uint8_t wrong_parity = parity ^ 1;  /* Flip parity bit */
  sw_char_result_t result = sw_decode_char(encoded, wrong_parity, &decoded);

  EXPECT_EQ(result, SW_CHAR_PARITY_ERROR);
}

/* ============================================================================
 * CRC TESTS
 * ============================================================================ */

TEST(CRCComputation, CRC16CCITTNonZero) {
  /* Test vector: "123456789" */
  const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  uint16_t crc = sw_crc16(data, sizeof(data));

  EXPECT_NE(crc, 0);
}

TEST(CRCComputation, CRC16CCITTDeterministic) {
  const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  uint16_t crc1 = sw_crc16(data, sizeof(data));
  uint16_t crc2 = sw_crc16(data, sizeof(data));

  EXPECT_EQ(crc1, crc2);
}

/* ============================================================================
 * FRAME LAYER TESTS
 * ============================================================================ */

TEST(FrameLayer, EncodeDecodeRoundtrip) {
  uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x42;
  frame.protocol_id = 1;
  frame.payload = payload;
  frame.payload_len = sizeof(payload);

  uint8_t buf[256];
  size_t encoded_size = sw_frame_encode(&frame, buf, sizeof(buf));
  ASSERT_GT(encoded_size, (size_t)0);

  sw_frame_t decoded;
  int result = sw_frame_decode(&decoded, buf, encoded_size);
  ASSERT_EQ(result, 1);
  EXPECT_EQ(decoded.target_addr, 0x42);
  EXPECT_EQ(decoded.protocol_id, 1);
  EXPECT_EQ(decoded.payload_len, (uint16_t)sizeof(payload));
  EXPECT_EQ(memcmp(decoded.payload, payload, sizeof(payload)), 0);
}

TEST(FrameLayer, CRCValidationDetectsCorruption) {
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
  EXPECT_EQ(result, 0);
}

TEST(FrameLayer, SizeCalculation) {
  sw_frame_t frame;
  sw_frame_init(&frame);

  uint8_t payload[100];
  frame.payload = payload;
  frame.payload_len = 100;

  size_t calc_size = sw_frame_size(&frame);
  /* Header (2) + payload (100) + CRC (2) = 104 */
  EXPECT_EQ(calc_size, (size_t)104);
}

/* ============================================================================
 * ROUTER TESTS
 * ============================================================================ */

TEST(Router, Initialization) {
  sw_router_t router;
  sw_router_init(&router, 0x42, 3);

  EXPECT_EQ(router.device_addr, 0x42);
  EXPECT_EQ(router.num_ports, 3);
  EXPECT_EQ(router.links[0].port_id, 0);
  EXPECT_EQ(router.channels[0].active, 0);
}

TEST(Router, RoutingLogic) {
  sw_router_t router;
  sw_router_init(&router, 0x01, 2);

  sw_router_add_route(&router, 0x02, 0);
  sw_router_add_route(&router, 0x03, 1);

  router.links[0].state = SW_LINK_CONNECTED;
  router.links[1].state = SW_LINK_CONNECTED;

  sw_frame_t frame;
  sw_frame_init(&frame);

  /* Test routing to 0x02 */
  frame.target_addr = 0x02;
  uint8_t output_port;
  int result = sw_router_route_frame(&router, &frame, &output_port);
  ASSERT_EQ(result, 1);
  EXPECT_EQ(output_port, 0);

  /* Test routing to 0x03 */
  frame.target_addr = 0x03;
  result = sw_router_route_frame(&router, &frame, &output_port);
  ASSERT_EQ(result, 1);
  EXPECT_EQ(output_port, 1);
}

/* ============================================================================
 * PACKET INTEGRATION TESTS
 * ============================================================================ */

TEST(PacketIntegration, EncodeDecode) {
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
  pf.packet.payload_len = strlen(payload_str);

  uint8_t buf[512];
  size_t encoded_size = sw_packet_encode(&pf, buf, sizeof(buf));
  ASSERT_GT(encoded_size, (size_t)0);

  sw_packet_frame_t decoded_pf;
  int result = sw_packet_decode(&decoded_pf, buf, encoded_size);
  ASSERT_EQ(result, 1);
  EXPECT_EQ(decoded_pf.packet.ph.apid, (uint16_t)0x0100);
  EXPECT_EQ(decoded_pf.packet.payload_len, (uint16_t)strlen(payload_str));
}

TEST(PacketIntegration, CreateConvenience) {
  uint8_t payload[] = {0x11, 0x22, 0x33};
  uint8_t buf[512];

  size_t size = sw_packet_create(0x01, 0x02, 0x0042, payload, sizeof(payload), buf, sizeof(buf));

  EXPECT_GT(size, (size_t)0);
  EXPECT_GT(size, sizeof(payload) + 6);
}
