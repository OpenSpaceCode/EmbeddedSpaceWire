/*
 * Space Wire Router - Packet routing and virtual channels
 */

#include "../include/spacewire.h"
#include <string.h>

/* ============================================================================
 * ROUTER INITIALIZATION
 * ============================================================================ */

void sw_router_init(sw_router_t *router, uint8_t device_addr, uint8_t num_ports) {
  if (!router)
    return;

  memset(router, 0, sizeof(sw_router_t));
  router->device_addr = device_addr;
  router->num_ports = (num_ports <= SW_MAX_PORTS) ? num_ports : SW_MAX_PORTS;

  /* Initialize ports */
  for (uint8_t i = 0; i < router->num_ports; i++) {
    router->links[i].port_id = i;
    router->links[i].state = SW_LINK_UNINITIALIZED;
  }

  /* Initialize virtual channels */
  for (uint8_t i = 0; i < SW_MAX_VIRTUAL_CHANNELS; i++) {
    router->channels[i].channel_id = i;
    router->channels[i].active = 0;
    router->channels[i].fct_credits = 64;  /* Default credits */
  }
}

/* ============================================================================
 * ROUTING CONFIGURATION
 * ============================================================================ */

void sw_router_add_route(sw_router_t *router, uint8_t dest_addr, uint8_t output_port) {
  if (!router || dest_addr >= SW_MAX_PORTS || output_port >= router->num_ports)
    return;

  router->routes[dest_addr].dest_addr = dest_addr;
  router->routes[dest_addr].output_port = output_port;
}

/* ============================================================================
 * VIRTUAL CHANNEL MANAGEMENT
 * ============================================================================ */

int sw_router_open_channel(sw_router_t *router, uint8_t channel_id) {
  if (!router || channel_id >= SW_MAX_VIRTUAL_CHANNELS)
    return 0;

  router->channels[channel_id].active = 1;
  return 1;
}

/* ============================================================================
 * FRAME ROUTING
 * ============================================================================ */

int sw_router_route_frame(sw_router_t *router, const sw_frame_t *frame, uint8_t *output_port) {
  if (!router || !frame || !output_port)
    return 0;

  /* If destination is this device, don't route (local delivery) */
  if (frame->target_addr == router->device_addr)
    return 0;

  /* Look up destination in routing table */
  if (frame->target_addr >= SW_MAX_PORTS)
    return 0;  /* Invalid destination */

  uint8_t port = router->routes[frame->target_addr].output_port;

  /* Check port exists and is connected */
  if (port >= router->num_ports)
    return 0;

  if (router->links[port].state != SW_LINK_CONNECTED)
    return 0;

  *output_port = port;
  router->links[port].tx_packets++;

  return 1;  /* Success */
}

/* ============================================================================
 * LINK STATE MANAGEMENT (STUB)
 * ============================================================================ */

void sw_link_init(sw_link_layer_t *link, const sw_link_config_t *config) {
  if (!link || !config)
    return;

  link->config = *config;
  link->state = SW_LINK_UNINITIALIZED;
  link->rx_credits = config->rx_credit_max;
  link->tx_credits = 0;
}

sw_link_state_t sw_link_get_state(const sw_link_layer_t *link) {
  if (!link)
    return SW_LINK_ERROR;
  return link->state;
}

void sw_link_set_state(sw_link_layer_t *link, sw_link_state_t state) {
  if (!link)
    return;
  link->state = state;
}
