/**
 * @file    spacewire_router.c
 * @brief   SpaceWire routing switch
 *
 * Implements path and logical addressing, header deletion and invalid-address
 * handling as per ECSS-E-ST-50-12C Rev.1, clause 5.6.8.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "spacewire.h"

#include <string.h>

void sw_router_init(sw_router_t *router, uint8_t num_ports)
{
    if (!router)
    {
        return;
    }

    memset(router, 0, sizeof(*router));

    if (num_ports == 0)
    {
        num_ports = 1;
    }

    if (num_ports > SW_NUM_PORTS)
    {
        num_ports = SW_NUM_PORTS;
    }

    router->num_ports = num_ports;
}

sw_result_t sw_router_add_route(sw_router_t *router,
                                uint8_t logical_addr,
                                uint8_t output_port,
                                int delete_addr)
{
    if (!router)
    {
        return SW_INVALID_PARAM;
    }

    /* Only logical addresses 32..254 are configurable (clause 5.6.8.4). */
    if ((logical_addr < SW_LOGICAL_ADDR_MIN) || (logical_addr == SW_LOGICAL_ADDR_RESERVED))
    {
        return SW_WRONG_ADDRESS;
    }

    /* The configuration port is reached by path addressing only (clause 5.6.8.3 f). */
    if ((output_port == SW_PORT_CONFIG) || (output_port >= router->num_ports))
    {
        return SW_WRONG_PORT;
    }

    router->routes[logical_addr].output_port = output_port;
    router->routes[logical_addr].configured = 1;
    router->routes[logical_addr].delete_addr = delete_addr ? 1u : 0u;

    return SW_OK;
}

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

/**
 * @brief Resolve a leading address character to a route.
 *
 * A path address (0..31) names the output port directly and is always deleted
 * (clause 5.6.8.3, Table 5-11). A logical address is looked up in the routing
 * table (clause 5.6.8.4). An address that is unconfigured, which includes the
 * reserved address 255, or that leads to a port the router does not have, is
 * invalid (clause 5.6.8.5). So is a logical address that leads to the
 * configuration port, which is reached by path addressing only (clause 5.6.8.3 f).
 *
 * @param[in]  router Router to consult.
 * @param[in]  lead   Leading address character of the packet.
 * @param[out] route  Resolved route. On failure, left unchanged.
 * @return 1 if @p lead leads to an existing output port, 0 if it is invalid.
 */
static int sw_router_resolve(const sw_router_t *router, uint8_t lead, sw_route_entry_t *route)
{
    sw_route_entry_t candidate;
    const int is_logical = (lead > SW_PATH_ADDR_MAX);

    if (!is_logical)
    {
        candidate.output_port = lead;
        candidate.configured = 1;
        candidate.delete_addr = 1;
    }
    else
    {
        candidate = router->routes[lead];
    }

    /* sw_router_add_route() refuses a logical route to the configuration port, but the
     * table is caller-visible, so an entry written into it directly is refused here. */
    if ((!candidate.configured) || (candidate.output_port >= router->num_ports) ||
        ((is_logical) && (candidate.output_port == SW_PORT_CONFIG)))
    {
        return 0;
    }

    *route = candidate;

    return 1;
}

sw_route_result_t sw_router_route(sw_router_t *router,
                                  const uint8_t *packet,
                                  size_t len,
                                  uint8_t *output_port,
                                  uint8_t *delete_leading)
{
    if ((!router) || (!packet) || (!output_port) || (!delete_leading))
    {
        return SW_ROUTE_DISCARD;
    }

    /* An empty packet is discarded by the first routing switch (clause 5.6.2.1). */
    if (len == 0)
    {
        router->packets_discarded++;
        return SW_ROUTE_DISCARD;
    }

    sw_route_entry_t route;
    if (!sw_router_resolve(router, packet[0], &route))
    {
        return sw_router_invalid_address(router);
    }

    /* The routing table is caller-visible state, so the flag is normalised to 0 or 1. */
    *output_port = route.output_port;
    *delete_leading = route.delete_addr ? 1u : 0u;
    router->packets_routed++;

    return SW_ROUTE_OK;
}
