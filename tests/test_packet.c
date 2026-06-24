#include "cunit.h"
#include "test_runners.h"
#include "spacewire.h"
#include "spacewire_packet.h"
#include <string.h>

static int test_packet_encode_decode(void) {
  sw_packet_config_t config = {.device_addr = 0x01,
                               .target_addr = 0x02,
                               .protocol_id = 1,
                               .enable_crc = 1};

  sw_packet_frame_t pf;
  sw_packet_init(&pf, &config);

  pf.packet.ph.apid = 0x0100;
  const char *payload_str = "Test payload";
  uint16_t payload_len = (uint16_t)strlen(payload_str);
  pf.packet.payload = (const uint8_t *)payload_str;
  pf.packet.payload_len = payload_len;

  uint8_t buf[512];
  size_t encoded_size = sw_packet_encode(&pf, buf, sizeof(buf));
  ASSERT_TRUE(encoded_size > 0);

  sw_packet_frame_t decoded_pf;
  int result = sw_packet_decode(&decoded_pf, buf, encoded_size);
  ASSERT_EQ_INT(1, result);
  ASSERT_EQ_INT(0x0100, decoded_pf.packet.ph.apid);
  ASSERT_EQ_INT(payload_len, decoded_pf.packet.payload_len);

  return 0;
}

static int test_packet_create_convenience(void) {
  uint8_t payload[] = {0x11, 0x22, 0x33};
  uint8_t buf[512];

  size_t size = sw_packet_create(0x01, 0x02, 0x0042, payload, sizeof(payload),
                                 buf, sizeof(buf));

  ASSERT_TRUE(size > 0);
  ASSERT_TRUE(size > sizeof(payload) + 6);

  return 0;
}

static int test_packet_init_invalid_args_and_defaults(void) {
  sw_packet_config_t config = {
      .device_addr = 0xAA,
      .target_addr = 0x55,
      .protocol_id = 7,
      .enable_crc = 0,
  };

  sw_packet_frame_t pf;
  memset(&pf, 0xA5, sizeof(pf));

  sw_packet_init(NULL, &config);
  sw_packet_init(&pf, NULL);

  sw_packet_init(&pf, &config);
  ASSERT_EQ_INT(0x55, pf.frame.target_addr);
  ASSERT_EQ_INT(7, pf.frame.protocol_id);
  ASSERT_EQ_INT(0, pf.packet.ph.version);
  ASSERT_EQ_INT(1, pf.packet.ph.type);
  ASSERT_EQ_INT(0, pf.packet.ph.sec_hdr_flag);
  ASSERT_EQ_INT(0, pf.packet.ph.apid);

  return 0;
}

static int test_packet_encode_error_paths(void) {
  sw_packet_config_t config = {.device_addr = 0x01,
                               .target_addr = 0x02,
                               .protocol_id = 1,
                               .enable_crc = 1};
  sw_packet_frame_t pf;
  sw_packet_init(&pf, &config);

  uint8_t buf[512];

  ASSERT_EQ_INT(0, sw_packet_encode(NULL, buf, sizeof(buf)));
  ASSERT_EQ_INT(0, sw_packet_encode(&pf, NULL, sizeof(buf)));

  static uint8_t dummy_payload = 0x11;
  pf.packet.payload = &dummy_payload;
  pf.packet.payload_len = UINT16_MAX;
  ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

  pf.packet.payload = NULL;
  pf.packet.payload_len = 8;
  ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

  pf.packet.payload = &dummy_payload;
  pf.packet.payload_len = 0;
  pf.packet.ph.sec_hdr_flag = 1;
  pf.packet.sec_hdr = NULL;
  pf.packet.sec_hdr_len = 2;
  ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

  pf.packet.sec_hdr = &dummy_payload;
  pf.packet.sec_hdr_len = 1;
  ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

  return 0;
}

static int test_packet_decode_error_paths_and_stats(void) {
  sw_packet_config_t config = {.device_addr = 0x01,
                               .target_addr = 0x02,
                               .protocol_id = 1,
                               .enable_crc = 1};
  sw_packet_frame_t pf;
  sw_packet_init(&pf, &config);

  uint8_t tiny[3] = {0};
  ASSERT_EQ_INT(0, sw_packet_decode(NULL, tiny, sizeof(tiny)));
  ASSERT_EQ_INT(0, sw_packet_decode(&pf, NULL, sizeof(tiny)));
  ASSERT_EQ_INT(0, sw_packet_decode(&pf, tiny, sizeof(tiny)));

  uint8_t small_payload[] = {0xAB};
  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x22;
  frame.protocol_id = 1;
  frame.payload = small_payload;
  frame.payload_len = sizeof(small_payload);

  uint8_t frame_buf[64];
  size_t frame_len = sw_frame_encode(&frame, frame_buf, sizeof(frame_buf));
  ASSERT_TRUE(frame_len > 0);
  ASSERT_EQ_INT(0, sw_packet_decode(&pf, frame_buf, frame_len));

  sw_reset_statistics();
  sw_statistics_t stats;
  sw_get_statistics(NULL);
  sw_get_statistics(&stats);
  ASSERT_EQ_INT(0, stats.packets_sent);
  ASSERT_EQ_INT(0, stats.packets_received);
  ASSERT_EQ_INT(0, stats.bytes_sent);
  ASSERT_EQ_INT(0, stats.bytes_received);

  sw_packet_frame_t tx_pf;
  sw_packet_init(&tx_pf, &config);
  tx_pf.packet.ph.apid = 0x22;
  static uint8_t payload[] = {1, 2, 3, 4};
  tx_pf.packet.payload = payload;
  tx_pf.packet.payload_len = sizeof(payload);

  uint8_t packet_buf[512];
  size_t packet_len = sw_packet_encode(&tx_pf, packet_buf, sizeof(packet_buf));
  ASSERT_TRUE(packet_len > 0);

  sw_packet_frame_t rx_pf;
  ASSERT_EQ_INT(1, sw_packet_decode(&rx_pf, packet_buf, packet_len));

  sw_get_statistics(&stats);
  ASSERT_EQ_INT(1, stats.packets_sent);
  ASSERT_EQ_INT(1, stats.packets_received);
  ASSERT_TRUE(stats.bytes_sent > 0);
  ASSERT_TRUE(stats.bytes_received > 0);

  sw_reset_statistics();
  sw_get_statistics(&stats);
  ASSERT_EQ_INT(0, stats.packets_sent);
  ASSERT_EQ_INT(0, stats.packets_received);
  ASSERT_EQ_INT(0, stats.bytes_sent);
  ASSERT_EQ_INT(0, stats.bytes_received);

  return 0;
}

static int test_packet_create_null_buffer(void) {
  uint8_t payload[] = {0x11, 0x22};
  ASSERT_EQ_INT(0, sw_packet_create(0x01, 0x02, 0x0042, payload, sizeof(payload),
                                    NULL, 128));
  return 0;
}

pus_test_result_t test_spacewire_packet_run_all(void) {
  RUN_TEST(test_packet_encode_decode);
  RUN_TEST(test_packet_create_convenience);
  RUN_TEST(test_packet_init_invalid_args_and_defaults);
  RUN_TEST(test_packet_encode_error_paths);
  RUN_TEST(test_packet_decode_error_paths_and_stats);
  RUN_TEST(test_packet_create_null_buffer);
  return (pus_test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
