/*
 * SpaceWire protocol — network, link and codec layers.
 * Minimal, embedded-optimized implementation following:
 * - ECSS-E-ST-50-12C Rev.1 (SpaceWire — links, nodes, routers and networks)
 * - ECSS-E-ST-50-53C (SpaceWire — CCSDS packet transfer protocol)
 */

#ifndef SPACEWIRE_H
#define SPACEWIRE_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * SPACEWIRE CHARACTER CODEC
 * Space Wire encodes 9-bit data (8 data bits + parity) using special characters
 * ============================================================================ */

/* Special characters in Space Wire */
#define SPACEWIRE_ESC 0x00 /* Escape character (triggers character mapping) */
#define SPACEWIRE_FCT 0x01 /* Flow Control Token */
#define SPACEWIRE_EOP 0x02 /* End Of Packet */
#define SPACEWIRE_EEP 0x03 /* End Of Error Packet */

/* Escaped character mappings (ESC followed by code) */
#define SPACEWIRE_ESC_ESC 0x00 /* Escaped ESC */
#define SPACEWIRE_ESC_FCT 0x01 /* Escaped FCT */
#define SPACEWIRE_ESC_EOP 0x02 /* Escaped EOP */
#define SPACEWIRE_ESC_EEP 0x03 /* Escaped EEP */

/* Parity types */
typedef enum
{
    SW_PARITY_EVEN = 0,
    SW_PARITY_ODD = 1
} sw_parity_t;

/* Character codec result */
typedef enum
{
    SW_CHAR_OK = 0,
    SW_CHAR_ESCAPE = 1, /* Special character */
    SW_CHAR_FCT = 2,    /* Flow control token */
    SW_CHAR_EOP = 3,    /* End of packet */
    SW_CHAR_EEP = 4,    /* End of error packet */
    SW_CHAR_PARITY_ERROR = 5,
    SW_CHAR_INVALID = 6
} sw_char_result_t;

/* Decode single character (9-bit) from two bytes (8-bit + parity bit) */
sw_char_result_t sw_decode_char(uint8_t byte, uint8_t parity_bit, uint8_t *data);

/* Encode single character to byte + parity (returns parity bit) */
uint8_t sw_encode_char(uint8_t data, uint8_t *byte);

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
 * Low-level link operations
 * ============================================================================ */

/* Link configuration */
typedef struct
{
    uint32_t bit_rate;           /* bits per second */
    uint32_t disconnect_timeout; /* microseconds */
    uint8_t rx_credit_max;       /* max rx buffer credits */
    uint8_t enable_crc;          /* 1 = enable CRC, 0 = disable */
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

/* ============================================================================
 * CRC UTILITIES (shared with Space Packet)
 * ============================================================================ */

/* Compute CRC-16-CCITT (polynomial 0x1021, init 0xFFFF) */
uint16_t sw_crc16(const uint8_t *data, size_t len);

/* CRC table for faster computation */
extern const uint16_t sw_crc16_table[256];

#endif /* SPACEWIRE_H */
