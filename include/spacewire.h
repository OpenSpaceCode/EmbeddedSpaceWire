/*
 * SpaceWire — packet and network layers.
 *
 * Minimal, embedded-optimized implementation following:
 * - ECSS-E-ST-50-12C Rev.1 (SpaceWire — links, nodes, routers and networks):
 *   the SpaceWire packet structure and routing (this file).
 * - ECSS-E-ST-50-53C        (SpaceWire — CCSDS packet transfer protocol):
 *   see spacewire_packet.h.
 *
 * Scope: this library implements the packet and network layers only. The
 * character/signal and data-link levels — character encoding and parity,
 * data-strobe (DS) signalling, link initialisation and flow control, and the
 * generation/detection of the EOP/EEP end-of-packet markers — are provided by
 * the SpaceWire hardware CODEC. EOP/EEP are therefore exchanged with this
 * library as out-of-band metadata (see sw_end_marker_t in spacewire_packet.h),
 * not as bytes within packet buffers.
 */

#ifndef SPACEWIRE_H
#define SPACEWIRE_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * SPACEWIRE PACKET (ECSS-E-ST-50-12C clause 5.6.2)
 *
 * A SpaceWire packet is a sequence of data characters terminated by an end of
 * packet marker (EOP or EEP). The leading data characters form the destination
 * address; the remaining characters form the cargo:
 *
 *     [ Destination Address ][ Cargo ][ EOP | EEP ]
 *
 * The end marker is a control character emitted by the link layer, not a data
 * octet, so it is not stored in the packet buffer. No checksum is added.
 * ============================================================================ */

/* Build a SpaceWire packet by prefixing a destination address to a cargo.
 * Copies [dest][cargo] into buf; either part may be empty (NULL with length 0).
 * Returns the total length written, or 0 on error (a NULL pointer paired with a
 * non-zero length, an empty result, or a buffer that is too small). */
size_t sw_spw_packet_build(const uint8_t *dest,
                           size_t dest_len,
                           const uint8_t *cargo,
                           size_t cargo_len,
                           uint8_t *buf,
                           size_t buf_len);

/* ============================================================================
 * SPACEWIRE ROUTING (ECSS-E-ST-50-12C clause 5.6.8)
 *
 * A routing switch forwards a packet to an output port selected by the leading
 * data character of the packet's destination address (Table 5-11):
 *   - 0        configuration port (path addressing only)          (clause 5.6.8.2)
 *   - 1..31    external output port with that number (path addr)  (clause 5.6.8.3)
 *   - 32..254  logical address, mapped via the routing table      (clause 5.6.8.4)
 *   - 255      reserved logical address, must not be used
 * A path address character is always deleted after use; a logical address is
 * retained unless its routing-table entry requests deletion (clause 5.6.8.6).
 * ============================================================================ */

/* Port 0 is the configuration port; external ports are 1..31 (clause 5.6.8.2). */
#define SW_PORT_CONFIG 0u
#define SW_PORT_EXTERNAL_MAX 31u

/* Number of ports a router can have (0..31). Override with -DSW_NUM_PORTS=n to
 * shrink the per-router footprint for small nodes. */
#ifndef SW_NUM_PORTS
#define SW_NUM_PORTS 32u
#endif

/* Address ranges (clause 5.6.8, Table 5-11). */
#define SW_PATH_ADDR_MAX 31u        /* 0..31    path address    */
#define SW_LOGICAL_ADDR_MIN 32u     /* 32..255  logical address */
#define SW_LOGICAL_ADDR_RESERVED 255u
#define SW_LOGICAL_ADDR_DEFAULT 254u

/* The routing table is indexed directly by the leading data character. */
#define SW_ROUTE_TABLE_SIZE 256u

/* SpaceWire link state (clause 5.5.7 link initialisation). */
typedef enum
{
    SW_LINK_UNINITIALIZED = 0,
    SW_LINK_READY = 1,
    SW_LINK_STARTED = 2,
    SW_LINK_CONNECTED = 3,
    SW_LINK_ERROR = 4
} sw_link_state_t;

typedef struct
{
    uint8_t port_id;
    sw_link_state_t state;
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t errors;
} sw_link_t;

/* One routing-table entry mapping a logical address to an output port. */
typedef struct
{
    uint8_t output_port; /* port the packet is forwarded through */
    uint8_t configured;  /* 1 if this logical address has a valid route */
    uint8_t delete_addr; /* 1 to delete the logical address before forwarding (clause 5.6.8.6) */
} sw_route_entry_t;

typedef struct
{
    sw_link_t links[SW_NUM_PORTS];
    sw_route_entry_t routes[SW_ROUTE_TABLE_SIZE];
    uint8_t num_ports;               /* ports present: 1..SW_NUM_PORTS (port 0 is config) */
    uint32_t invalid_address_errors; /* invalid / unconfigured address discards (clause 5.6.8.5) */
    uint32_t packets_routed;
    uint32_t packets_discarded;
} sw_router_t;

/* Result of a routing decision. */
typedef enum
{
    SW_ROUTE_OK = 0,     /* forward via the returned output port */
    SW_ROUTE_DISCARD = 1 /* discard: empty packet, non-existent port, or unconfigured address */
} sw_route_result_t;

/* Initialize a router. num_ports is clamped to [1, SW_NUM_PORTS] and counts the
 * configuration port 0. */
void sw_router_init(sw_router_t *router, uint8_t num_ports);

/* Configure a logical-address route. logical_addr must be 32..254; output_port
 * must be an existing port; delete_addr requests logical-address deletion
 * (clause 5.6.8.6). Returns 1 on success, 0 on invalid arguments. */
int sw_router_add_route(sw_router_t *router,
                        uint8_t logical_addr,
                        uint8_t output_port,
                        int delete_addr);

/* Decide the output port for a packet from its leading destination-address
 * character. On SW_ROUTE_OK, *output_port is set and *delete_leading is 1 when
 * the leading character must be removed before forwarding (always for path
 * addressing; for logical addressing only when configured for deletion), else 0.
 * On SW_ROUTE_DISCARD the packet must be dropped; the router error/discard
 * counters are updated accordingly. */
sw_route_result_t sw_router_route(sw_router_t *router,
                                  const uint8_t *packet,
                                  size_t len,
                                  uint8_t *output_port,
                                  uint8_t *delete_leading);

/* ============================================================================
 * SPACEWIRE LINK LAYER
 *
 * The link layer proper (encoding, flow control, link initialisation) is in the
 * SpaceWire hardware CODEC. These types model the small amount of host-visible
 * link configuration and state used to drive and observe that hardware.
 * ============================================================================ */

/* Link configuration */
typedef struct
{
    uint32_t bit_rate;           /* bits per second */
    uint32_t disconnect_timeout; /* microseconds */
    uint8_t rx_credit_max;       /* max rx buffer credits */
    uint8_t enable_crc;          /* optional HW link-level CRC: 1 = enable, 0 = disable */
} sw_link_config_t;

typedef struct
{
    sw_link_config_t config;
    sw_link_state_t state;
    uint8_t rx_credits;
    uint8_t tx_credits;
} sw_link_layer_t;

/* Initialize link layer */
void sw_link_init(sw_link_layer_t *link, const sw_link_config_t *config);

/* Check link state */
sw_link_state_t sw_link_get_state(const sw_link_layer_t *link);

/* Update link state */
void sw_link_set_state(sw_link_layer_t *link, sw_link_state_t state);

#endif /* SPACEWIRE_H */
