#include "cunit.h"
#include "test_runners.h"
#include "spacewire.h"

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

pus_test_result_t test_spacewire_router_run_all(void) {
  RUN_TEST(test_router_initialization);
  RUN_TEST(test_router_routing);
  RUN_TEST(test_router_error_paths_and_channels);
  RUN_TEST(test_link_layer_state_helpers);
  return (pus_test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
