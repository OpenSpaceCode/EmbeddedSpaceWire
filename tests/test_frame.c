#include "cunit.h"
#include "test_runners.h"
#include "spacewire.h"

static int test_frame_encode_decode(void) {
  uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x42;
  frame.protocol_id = 1;
  frame.payload = payload;
  frame.payload_len = sizeof(payload);

  uint8_t buf[256];
  size_t encoded_size = sw_frame_encode(&frame, buf, sizeof(buf));
  ASSERT_TRUE(encoded_size > 0);

  sw_frame_t decoded;
  int result = sw_frame_decode(&decoded, buf, encoded_size);
  ASSERT_EQ_INT(1, result);
  ASSERT_EQ_INT(0x42, decoded.target_addr);
  ASSERT_EQ_INT(1, decoded.protocol_id);
  ASSERT_EQ_INT(sizeof(payload), decoded.payload_len);
  ASSERT_EQ_MEM(decoded.payload, payload, sizeof(payload));

  return 0;
}

static int test_frame_crc_validation(void) {
  uint8_t payload[] = {0xAA, 0xBB};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x01;
  frame.protocol_id = 1;
  frame.payload = payload;
  frame.payload_len = sizeof(payload);

  uint8_t buf[256];
  size_t size = sw_frame_encode(&frame, buf, sizeof(buf));
  ASSERT_TRUE(size > 0);

  buf[3] ^= 0xFF;

  sw_frame_t decoded;
  int result = sw_frame_decode(&decoded, buf, size);
  ASSERT_EQ_INT(0, result);

  return 0;
}

static int test_frame_size_calculation(void) {
  sw_frame_t frame;
  sw_frame_init(&frame);

  uint8_t payload[100];
  frame.payload = payload;
  frame.payload_len = 100;

  size_t calc_size = sw_frame_size(&frame);
  ASSERT_EQ_INT(104, calc_size);

  return 0;
}

static int test_frame_invalid_args_and_minimal_frame(void) {
  sw_frame_init(NULL);
  ASSERT_EQ_INT(0, sw_frame_size(NULL));

  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x05;
  frame.protocol_id = 0x01;

  ASSERT_EQ_INT(0, sw_frame_encode(NULL, (uint8_t *)&frame, 4));

  uint8_t buf[8] = {0};
  ASSERT_EQ_INT(0, sw_frame_encode(&frame, NULL, sizeof(buf)));
  ASSERT_EQ_INT(0, sw_frame_encode(&frame, buf, 3));

  size_t encoded = sw_frame_encode(&frame, buf, sizeof(buf));
  ASSERT_EQ_INT(4, encoded);

  sw_frame_t decoded;
  ASSERT_EQ_INT(0, sw_frame_decode(NULL, buf, encoded));
  ASSERT_EQ_INT(0, sw_frame_decode(&decoded, NULL, encoded));
  ASSERT_EQ_INT(0, sw_frame_decode(&decoded, buf, 3));

  ASSERT_EQ_INT(1, sw_frame_decode(&decoded, buf, encoded));
  ASSERT_EQ_INT(0x05, decoded.target_addr);
  ASSERT_EQ_INT(0x01, decoded.protocol_id);
  ASSERT_EQ_INT(0, decoded.payload_len);

  return 0;
}

pus_test_result_t test_spacewire_frame_run_all(void) {
  RUN_TEST(test_frame_encode_decode);
  RUN_TEST(test_frame_crc_validation);
  RUN_TEST(test_frame_size_calculation);
  RUN_TEST(test_frame_invalid_args_and_minimal_frame);
  return (pus_test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
