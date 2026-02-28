#include "cunit.h"
#include "spacewire.h"
#include "spacewire_packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_encode_decode(void) {
  for (int i = 4; i < 256; i++) {
    uint8_t encoded;
    uint8_t parity = sw_encode_char((uint8_t)i, &encoded);

    uint8_t decoded;
    sw_char_result_t result = sw_decode_char(encoded, parity, &decoded);

    ASSERT_EQ_INT(SW_CHAR_OK, result);
    ASSERT_EQ_INT((uint8_t)i, decoded);
  }

  return 0;
}

static int test_parity_error(void) {
  uint8_t encoded;
  uint8_t parity = sw_encode_char(0x55, &encoded);

  uint8_t decoded;
  uint8_t wrong_parity = parity ^ 1;
  sw_char_result_t result = sw_decode_char(encoded, wrong_parity, &decoded);

  ASSERT_EQ_INT(SW_CHAR_PARITY_ERROR, result);
  return 0;
}

static int test_crc_computation(void) {
  const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35,
                          0x36, 0x37, 0x38, 0x39};
  uint16_t crc = sw_crc16(data, sizeof(data));

  ASSERT_TRUE(crc != 0);

  uint16_t crc2 = sw_crc16(data, sizeof(data));
  ASSERT_EQ_INT(crc, crc2);

  return 0;
}

static int test_codec_special_chars_and_invalid_args(void) {
  uint8_t decoded = 0xAA;

  ASSERT_EQ_INT(SW_CHAR_INVALID, sw_decode_char(0x10, 0, NULL));

  uint8_t encoded;
  uint8_t parity;

  parity = sw_encode_char(SPACEWIRE_ESC, &encoded);
  ASSERT_EQ_INT(SW_CHAR_ESCAPE, sw_decode_char(encoded, parity, &decoded));

  parity = sw_encode_char(SPACEWIRE_FCT, &encoded);
  ASSERT_EQ_INT(SW_CHAR_FCT, sw_decode_char(encoded, parity, &decoded));

  parity = sw_encode_char(SPACEWIRE_EOP, &encoded);
  ASSERT_EQ_INT(SW_CHAR_EOP, sw_decode_char(encoded, parity, &decoded));

  parity = sw_encode_char(SPACEWIRE_EEP, &encoded);
  ASSERT_EQ_INT(SW_CHAR_EEP, sw_decode_char(encoded, parity, &decoded));

  ASSERT_EQ_INT(0, sw_encode_char(0xAB, NULL));
  ASSERT_EQ_INT(0xFFFF, sw_crc16(NULL, 12));

  return 0;
}

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

static int test_router_initialization(void) {
  sw_router_t router;
  sw_router_init(&router, 0x42, 3);

  ASSERT_EQ_INT(0x42, router.device_addr);
  ASSERT_EQ_INT(3, router.num_ports);
  ASSERT_EQ_INT(0, router.links[0].port_id);
  ASSERT_EQ_INT(0, router.channels[0].active);

  return 0;
}

static int test_router_routing(void) {
  sw_router_t router;
  sw_router_init(&router, 0x01, 2);

  sw_router_add_route(&router, 0x02, 0);
  sw_router_add_route(&router, 0x03, 1);

  router.links[0].state = SW_LINK_CONNECTED;
  router.links[1].state = SW_LINK_CONNECTED;

  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x02;

  uint8_t output_port;
  int result = sw_router_route_frame(&router, &frame, &output_port);
  ASSERT_EQ_INT(1, result);
  ASSERT_EQ_INT(0, output_port);

  frame.target_addr = 0x03;
  result = sw_router_route_frame(&router, &frame, &output_port);
  ASSERT_EQ_INT(1, result);
  ASSERT_EQ_INT(1, output_port);

  return 0;
}

static int test_router_error_paths_and_channels(void) {
  sw_router_init(NULL, 0x01, 2);

  sw_router_t router;
  sw_router_init(&router, 0x44, SW_MAX_PORTS + 2);
  ASSERT_EQ_INT(SW_MAX_PORTS, router.num_ports);

  sw_router_add_route(NULL, 0x01, 0);
  sw_router_add_route(&router, SW_MAX_PORTS, 0);

  sw_router_add_route(&router, 0x01, 1);
  ASSERT_EQ_INT(1, router.routes[0x01].output_port);
  sw_router_add_route(&router, 0x01, SW_MAX_PORTS);
  ASSERT_EQ_INT(1, router.routes[0x01].output_port);

  ASSERT_EQ_INT(0, sw_router_open_channel(NULL, 0));
  ASSERT_EQ_INT(0, sw_router_open_channel(&router, SW_MAX_VIRTUAL_CHANNELS));
  ASSERT_EQ_INT(1, sw_router_open_channel(&router, 3));
  ASSERT_EQ_INT(1, router.channels[3].active);

  sw_frame_t frame;
  sw_frame_init(&frame);
  frame.target_addr = 0x01;

  uint8_t output_port = 0xFF;
  ASSERT_EQ_INT(0, sw_router_route_frame(NULL, &frame, &output_port));
  ASSERT_EQ_INT(0, sw_router_route_frame(&router, NULL, &output_port));
  ASSERT_EQ_INT(0, sw_router_route_frame(&router, &frame, NULL));

  frame.target_addr = router.device_addr;
  ASSERT_EQ_INT(0, sw_router_route_frame(&router, &frame, &output_port));

  frame.target_addr = SW_MAX_PORTS;
  ASSERT_EQ_INT(0, sw_router_route_frame(&router, &frame, &output_port));

  sw_router_t one_port_router;
  sw_router_init(&one_port_router, 0x10, 1);
  one_port_router.routes[0x01].output_port = 1;
  frame.target_addr = 0x01;
  ASSERT_EQ_INT(0, sw_router_route_frame(&one_port_router, &frame, &output_port));

  sw_router_add_route(&one_port_router, 0x02, 0);
  frame.target_addr = 0x02;
  ASSERT_EQ_INT(0, sw_router_route_frame(&one_port_router, &frame, &output_port));

  return 0;
}

static int test_link_layer_state_helpers(void) {
  sw_link_config_t config = {
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

int main(void) {
  RUN_TEST(test_encode_decode);
  RUN_TEST(test_parity_error);
  RUN_TEST(test_crc_computation);
  RUN_TEST(test_codec_special_chars_and_invalid_args);
  RUN_TEST(test_frame_encode_decode);
  RUN_TEST(test_frame_crc_validation);
  RUN_TEST(test_frame_size_calculation);
  RUN_TEST(test_frame_invalid_args_and_minimal_frame);
  RUN_TEST(test_router_initialization);
  RUN_TEST(test_router_routing);
  RUN_TEST(test_router_error_paths_and_channels);
  RUN_TEST(test_link_layer_state_helpers);
  RUN_TEST(test_packet_encode_decode);
  RUN_TEST(test_packet_create_convenience);
  RUN_TEST(test_packet_init_invalid_args_and_defaults);
  RUN_TEST(test_packet_encode_error_paths);
  RUN_TEST(test_packet_decode_error_paths_and_stats);
  RUN_TEST(test_packet_create_null_buffer);

  if (cunit_overall_failures == 0) {
    printf("ALL TESTS PASSED\n");
    return 0;
  } else {
    printf("%d TEST(S) FAILED\n", cunit_overall_failures);
    return 1;
  }
}
