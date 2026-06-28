/**
 * @file spacewire.h
 * @brief SpaceWire packet and network (routing) layers.
 *
 * Minimal, embedded-optimized implementation following:
 * - ECSS-E-ST-50-12C Rev.1 (SpaceWire — links, nodes, routers and networks):
 *   the SpaceWire packet structure and routing (this file).
 * - ECSS-E-ST-50-53C (SpaceWire — CCSDS packet transfer protocol): see
 *   spacewire_packet.h.
 *
 * Scope: this library implements the packet and network layers only. The
 * character/signal and data-link levels — character encoding and parity,
 * data-strobe (DS) signalling, link initialisation and flow control, and the
 * generation/detection of the EOP/EEP end-of-packet markers — are provided by
 * the SpaceWire hardware CODEC. EOP/EEP are therefore exchanged with this
 * library as out-of-band metadata (see ::sw_end_marker_t in spacewire_packet.h),
 * not as bytes within packet buffers.
 */

#ifndef SPACEWIRE_H
#define SPACEWIRE_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * RETURN CODES
 * ============================================================================ */

/**
 * @brief Generic success/failure result for functions that report status.
 *
 * Values are chosen so that ::SW_OK is truthy and ::SW_ERR is falsy.
 */
typedef enum
{
    SW_OK = 0,                /**< Operation succeeded, or a packet was delivered. */
    SW_ERR = -0x01,           /**< Operation failed, or no packet was delivered. */
    SW_INVALID_PARAM = -0x02, /**< Invalid parameter */
    SW_WRONG_ADDRESS = -0x03, /**< Invalid address */
    SW_WRONG_PORT = -0x04,    /**< Invalid port */
} sw_result_t;

/* ============================================================================
 * SPACEWIRE PACKET (ECSS-E-ST-50-12C clause 5.6.2)
 * ============================================================================ */

/**
 * @brief Build a SpaceWire packet by prefixing a destination address to a cargo.
 *
 * A SpaceWire packet is a destination address followed by cargo and terminated
 * by an EOP/EEP marker (clause 5.6.2). The marker is a link-layer control
 * character emitted by the hardware CODEC, so it is not stored in the buffer,
 * and no checksum is added. Copies `[dest][cargo]` into @p buf; either part may
 * be empty (a NULL pointer with length 0).
 *
 * @param[in]  dest      Destination-address octets, or NULL if @p dest_len is 0.
 * @param[in]  dest_len  Number of destination-address octets.
 * @param[in]  cargo     Cargo octets, or NULL if @p cargo_len is 0.
 * @param[in]  cargo_len Number of cargo octets.
 * @param[out] buf       Output buffer.
 * @param[in]  buf_len   Buffer capacity in octets.
 * @return Total octets written, or 0 on error (a NULL pointer paired with a
 *         non-zero length, an empty result, or a buffer that is too small).
 */
size_t sw_spw_packet_build(const uint8_t *dest,
                           size_t dest_len,
                           const uint8_t *cargo,
                           size_t cargo_len,
                           uint8_t *buf,
                           size_t buf_len);

/* ============================================================================
 * SPACEWIRE ROUTING (ECSS-E-ST-50-12C clause 5.6.8)
 * ============================================================================ */

/** @brief Configuration port number; reachable by path addressing only (clause 5.6.8.2). */
#define SW_PORT_CONFIG 0u

/** @brief Highest external port number (clause 5.6.8.2). */
#define SW_PORT_EXTERNAL_MAX 31u

/**
 * @brief Number of ports a router can have (0..31).
 *
 * Override with `-DSW_NUM_PORTS=n` to shrink the per-router footprint for small
 * nodes.
 */
#ifndef SW_NUM_PORTS
#    define SW_NUM_PORTS 32u
#endif

/** @brief Largest path-address character; 0..31 select an output port (clause 5.6.8.3). */
#define SW_PATH_ADDR_MAX 31u

/** @brief Smallest logical address; 32..255 index the routing table (clause 5.6.8.4). */
#define SW_LOGICAL_ADDR_MIN 32u

/** @brief Reserved logical address that must not be used (clause 5.6.8.4 d). */
#define SW_LOGICAL_ADDR_RESERVED 255u

/** @brief Default Target Logical Address. */
#define SW_LOGICAL_ADDR_DEFAULT 254u

/** @brief Routing-table size; indexed directly by the leading data character. */
#define SW_ROUTE_TABLE_SIZE 256u

/**
 * @brief SpaceWire link state (clause 5.5.7 link initialisation).
 */
typedef enum
{
    SW_LINK_UNINITIALIZED = 0, /**< Link not yet started. */
    SW_LINK_READY = 1,         /**< Ready to start. */
    SW_LINK_STARTED = 2,       /**< Start requested. */
    SW_LINK_CONNECTED = 3,     /**< Connected and running. */
    SW_LINK_ERROR = 4          /**< Link error. */
} sw_link_state_t;

/**
 * @brief Per-port link state and counters.
 */
typedef struct
{
    uint8_t port_id;       /**< Port number. */
    sw_link_state_t state; /**< Current link state. */
    uint32_t tx_packets;   /**< Packets transmitted on this port. */
    uint32_t rx_packets;   /**< Packets received on this port. */
    uint32_t errors;       /**< Errors observed on this port. */
} sw_link_t;

/**
 * @brief One routing-table entry mapping a logical address to an output port.
 */
typedef struct
{
    uint8_t output_port; /**< Port the packet is forwarded through. */
    uint8_t configured;  /**< 1 if this logical address has a valid route. */
    uint8_t delete_addr; /**< 1 to delete the logical address before forwarding (clause 5.6.8.6). */
} sw_route_entry_t;

/**
 * @brief A SpaceWire routing switch: ports, a routing table and counters.
 */
typedef struct
{
    sw_link_t links[SW_NUM_PORTS];                /**< Per-port link state. */
    sw_route_entry_t routes[SW_ROUTE_TABLE_SIZE]; /**< Logical-address routing table. */
    uint8_t num_ports;                            /**< Ports present (port 0 = config). */
    uint32_t invalid_address_errors;              /**< Invalid-address discards (clause 5.6.8.5). */
    uint32_t packets_routed;                      /**< Packets successfully routed. */
    uint32_t packets_discarded;                   /**< Packets discarded. */
} sw_router_t;

/**
 * @brief Result of a routing decision.
 */
typedef enum
{
    SW_ROUTE_OK = 0,     /**< Forward via the returned output port. */
    SW_ROUTE_DISCARD = 1 /**< Discard: empty packet, non-existent port, or unconfigured address. */
} sw_route_result_t;

/**
 * @brief Initialize a router.
 *
 * @param[out] router    Router to initialise. No-op if NULL.
 * @param[in]  num_ports Number of ports, clamped to [1, ::SW_NUM_PORTS] and
 *                       counting the configuration port 0.
 */
void sw_router_init(sw_router_t *router, uint8_t num_ports);

/**
 * @brief Configure a logical-address route (clause 5.6.8.4).
 *
 * @param[in,out] router       Target router.
 * @param[in]     logical_addr Logical address to route; must be 32..254.
 * @param[in]     output_port  Existing output port to forward through.
 * @param[in]     delete_addr  Non-zero to delete the logical address before
 *                             forwarding (logical-address deletion, clause 5.6.8.6).
 * @return ::SW_OK on success, error code otherwise.
 */
sw_result_t sw_router_add_route(sw_router_t *router,
                                uint8_t logical_addr,
                                uint8_t output_port,
                                int delete_addr);

/**
 * @brief Decide the output port for a packet from its leading address character.
 *
 * Path addresses (0..31) name the output port directly and are always deleted;
 * logical addresses (32..254) are looked up in the routing table and retained
 * unless the entry requests deletion (Table 5-11, clauses 5.6.8.3–5.6.8.6).
 *
 * @param[in,out] router         Router (its counters are updated).
 * @param[in]     packet         Packet octets (the destination address leads).
 * @param[in]     len            Packet length in octets.
 * @param[out]    output_port    Selected output port (valid on ::SW_ROUTE_OK).
 * @param[out]    delete_leading 1 if the leading character must be removed before
 *                               forwarding, else 0 (valid on ::SW_ROUTE_OK).
 * @return ::SW_ROUTE_OK to forward, or ::SW_ROUTE_DISCARD to drop the packet. A
 *         non-existent port or unconfigured address also bumps the router's
 *         invalid-address counter (clause 5.6.8.5).
 */
sw_route_result_t sw_router_route(sw_router_t *router,
                                  const uint8_t *packet,
                                  size_t len,
                                  uint8_t *output_port,
                                  uint8_t *delete_leading);

/* ============================================================================
 * SPACEWIRE LINK LAYER
 * ============================================================================ */

/**
 * @brief Host-visible SpaceWire link configuration.
 *
 * The link layer proper (encoding, flow control, link initialisation) lives in
 * the SpaceWire hardware CODEC; these fields configure and observe it.
 */
typedef struct
{
    uint32_t bit_rate;           /**< Signalling rate in bits per second. */
    uint32_t disconnect_timeout; /**< Disconnect timeout in microseconds. */
    uint8_t rx_credit_max;       /**< Maximum receive buffer credits. */
} sw_link_config_t;

/**
 * @brief Link layer state.
 */
typedef struct
{
    sw_link_config_t config; /**< Link configuration. */
    sw_link_state_t state;   /**< Current link state. */
    uint8_t rx_credits;      /**< Available receive credits. */
    uint8_t tx_credits;      /**< Available transmit credits. */
} sw_link_layer_t;

/**
 * @brief Initialise the link layer from configuration.
 *
 * @param[out] link   Link layer to initialise. No-op if NULL.
 * @param[in]  config Link configuration. No-op if NULL.
 */
void sw_link_init(sw_link_layer_t *link, const sw_link_config_t *config);

/**
 * @brief Get the current link state.
 *
 * @param[in] link Link layer.
 * @return Current state, or ::SW_LINK_ERROR if @p link is NULL.
 */
sw_link_state_t sw_link_get_state(const sw_link_layer_t *link);

/**
 * @brief Set the link state.
 *
 * @param[out] link  Link layer. No-op if NULL.
 * @param[in]  state New state.
 */
void sw_link_set_state(sw_link_layer_t *link, sw_link_state_t state);

#endif /* SPACEWIRE_H */
