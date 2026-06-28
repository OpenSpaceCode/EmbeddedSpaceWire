/**
 * @file spacewire_router.c
 * @brief SpaceWire routing switch (ECSS-E-ST-50-12C clause 5.6.8) and link-state helpers.
 */

#include "../include/spacewire.h"

#include <string.h>

/* ============================================================================
 * ROUTER INITIALISATION
 * ============================================================================ */

void sw_router_init(sw_router_t *router, uint8_t num_ports)
{
    if (!router)
        return;

    memset(router, 0, sizeof(*router));

    if (num_ports == 0)
        num_ports = 1;
    if (num_ports > SW_NUM_PORTS)
        num_ports = SW_NUM_PORTS;
    router->num_ports = num_ports;

    for (uint8_t i = 0; i < router->num_ports; i++)
    {
        router->links[i].port_id = i;
        router->links[i].state = SW_LINK_UNINITIALIZED;
    }
}

/* ============================================================================
 * ROUTING CONFIGURATION
 * ============================================================================ */

int sw_router_add_route(sw_router_t *router,
                        uint8_t logical_addr,
                        uint8_t output_port,
                        int delete_addr)
{
    if (!router)
        return 0;

    /* Only logical addresses 32..254 are configurable (clause 5.6.8.4). */
    if (logical_addr < SW_LOGICAL_ADDR_MIN || logical_addr == SW_LOGICAL_ADDR_RESERVED)
        return 0;
    if (output_port >= router->num_ports)
        return 0;

    router->routes[logical_addr].output_port = output_port;
    router->routes[logical_addr].configured = 1;
    router->routes[logical_addr].delete_addr = delete_addr ? 1u : 0u;
    return 1;
}

/* ============================================================================
 * ROUTING DECISION
 * ============================================================================ */

/**
 * @brief Discard a packet whose leading address references a non-existent port
 *        or an unconfigured routing-table entry (clause 5.6.8.5).
 * @param[in,out] router Router whose counters are updated.
 * @return Always ::SW_ROUTE_DISCARD.
 */
static sw_route_result_t sw_router_invalid_address(sw_router_t *router)
{
    router->invalid_address_errors++;
    router->packets_discarded++;
    return SW_ROUTE_DISCARD;
}

sw_route_result_t sw_router_route(sw_router_t *router,
                                  const uint8_t *packet,
                                  size_t len,
                                  uint8_t *output_port,
                                  uint8_t *delete_leading)
{
    if (!router || !packet || !output_port || !delete_leading)
        return SW_ROUTE_DISCARD;

    *delete_leading = 0;

    /* An empty packet is discarded by the first routing switch (clause 5.6.2.1). */
    if (len == 0)
    {
        router->packets_discarded++;
        return SW_ROUTE_DISCARD;
    }

    const uint8_t lead = packet[0];

    if (lead <= SW_PATH_ADDR_MAX)
    {
        /* Path addressing (clause 5.6.8.3): the character names the output port. */
        if (lead >= router->num_ports)
            return sw_router_invalid_address(router);
        *output_port = lead;
        *delete_leading = 1; /* a path address is always deleted (Table 5-11) */
        router->packets_routed++;
        return SW_ROUTE_OK;
    }

    /* Logical addressing (clause 5.6.8.4): look up the routing table. A logical
     * address that is unconfigured (including the reserved address 255) or that
     * maps to a non-existent port is discarded with an invalid-address error
     * (clause 5.6.8.5). */
    const sw_route_entry_t *entry = &router->routes[lead];
    if (!entry->configured || entry->output_port >= router->num_ports)
        return sw_router_invalid_address(router);

    *output_port = entry->output_port;
    *delete_leading = entry->delete_addr ? 1u : 0u; /* clause 5.6.8.6 */
    router->packets_routed++;
    return SW_ROUTE_OK;
}

/* ============================================================================
 * LINK STATE MANAGEMENT (STUB)
 * ============================================================================ */

void sw_link_init(sw_link_layer_t *link, const sw_link_config_t *config)
{
    if (!link || !config)
        return;

    link->config = *config;
    link->state = SW_LINK_UNINITIALIZED;
    link->rx_credits = config->rx_credit_max;
    link->tx_credits = 0;
}

sw_link_state_t sw_link_get_state(const sw_link_layer_t *link)
{
    if (!link)
        return SW_LINK_ERROR;
    return link->state;
}

void sw_link_set_state(sw_link_layer_t *link, sw_link_state_t state)
{
    if (!link)
        return;
    link->state = state;
}
