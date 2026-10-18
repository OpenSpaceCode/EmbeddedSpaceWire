/**
 * @file    spacewire.h
 * @brief   SpaceWire packet builder and routing switch
 *
 * Minimal, embedded-optimized implementation of the SpaceWire packet
 * structure and routing as per ECSS-E-ST-50-12C Rev.1 (SpaceWire — links,
 * nodes, routers and networks), clauses 5.6.2 and 5.6.8. The CCSDS packet
 * transfer protocol (ECSS-E-ST-50-53C) is declared in spacewire_ptp.h.
 *
 * Scope: this library implements the packet and network layers only. The
 * character/signal and data-link levels — character encoding and parity,
 * data-strobe (DS) signalling, link initialisation and flow control, and the
 * generation/detection of the EOP/EEP end-of-packet markers — are provided by
 * the SpaceWire hardware CODEC. EOP/EEP are therefore exchanged with this
 * library as out-of-band metadata (::sw_end_marker_t), not as bytes within
 * packet buffers.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef SPACEWIRE_H
#define SPACEWIRE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief Configuration port; reached by path addressing only (clauses 5.6.8.2 a, 5.6.8.3 f). */
#define SW_PORT_CONFIG 0u

/** @brief Most ports a router can have, configuration port 0 included (clause 5.6.8.2). */
#define SW_NUM_PORTS 32u

/** @brief Largest path-address character; 0..31 select an output port (clause 5.6.8.3). */
#define SW_PATH_ADDR_MAX 31u

/** @brief Smallest logical address; 32..255 index the routing table (clause 5.6.8.4). */
#define SW_LOGICAL_ADDR_MIN 32u

/** @brief Reserved logical address that must not be used (clause 5.6.8.4 d). */
#define SW_LOGICAL_ADDR_RESERVED 255u

/** @brief Routing-table size; indexed directly by the leading data character. */
#define SW_ROUTE_TABLE_SIZE 256u

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Status returned by every library function that can fail, and by the
 *        payload adapter hooks.
 *
 * ::SW_OK is zero and every error is negative, so `status < 0` tests for failure.
 * Lengths are returned through an `out_len` parameter, never through the status.
 * On any error, every output of the call is left unchanged.
 */
typedef enum
{
    SW_OK = 0,                   /**< Operation succeeded, or a packet was delivered. */
    SW_INVALID_PARAM = -0x01,    /**< NULL pointer, pointer/length mismatch or bad argument. */
    SW_BUFFER_TOO_SMALL = -0x02, /**< Output buffer cannot hold the result. */
    SW_WRONG_ADDRESS = -0x03,    /**< Address outside its range, or unroutable. */
    SW_WRONG_PORT = -0x04,       /**< Port missing, or not usable for this route. */
    SW_WRONG_LENGTH = -0x05,     /**< Packet or payload length out of range. */
    SW_WRONG_PROTOCOL = -0x06,   /**< Protocol Identifier of another protocol. */
    SW_DISCARDED = -0x07,        /**< Packet discarded by protocol rule; see its status. */
    SW_PAYLOAD_ERROR = -0x08,    /**< A payload adapter hook failed. */
} sw_status_t;

/**
 * @brief End-of-packet marker reported by the SpaceWire link layer (clause 5.4.3.2 d, e).
 *
 * EOP and EEP are control characters, not data octets, so they are exchanged
 * with this library as out-of-band metadata rather than stored in packet buffers.
 */
typedef enum
{
    SW_END_EOP = 0, /**< Normal End Of Packet. */
    SW_END_EEP = 1  /**< Error End of Packet. */
} sw_end_marker_t;

/**
 * @brief One routing-table entry mapping a logical address to an output port.
 */
typedef struct
{
    uint8_t output_port; /**< Port the packet is forwarded through; never ::SW_PORT_CONFIG. */
    uint8_t configured;  /**< 1 if this logical address has a valid route. */
    uint8_t delete_addr; /**< 1 to delete the logical address before forwarding (clause 5.6.8.6). */
} sw_route_entry_t;

/**
 * @brief A SpaceWire routing switch: a port count, a routing table and counters.
 */
typedef struct
{
    sw_route_entry_t routes[SW_ROUTE_TABLE_SIZE]; /**< Logical-address routing table. */
    uint8_t num_ports;                            /**< Ports present (port 0 = config). */
    uint32_t invalid_address_errors;              /**< Invalid-address discards (clause 5.6.8.5). */
    uint32_t packets_routed;                      /**< Packets successfully routed. */
    uint32_t packets_discarded;                   /**< Packets discarded. */
} sw_router_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Build a SpaceWire packet by prefixing a destination address to a cargo.
 *
 * A SpaceWire packet is a destination address followed by cargo and terminated
 * by an EOP/EEP marker (clause 5.6.2). The marker is a link-layer control
 * character emitted by the hardware CODEC, so it is not stored in the buffer,
 * and no checksum is added. Copies `[dest][cargo]` into @p buf; either part may
 * be empty (a NULL pointer with length 0). On failure, @p buf is left unchanged.
 *
 * @param[in]  dest      Destination-address octets, or NULL if @p dest_len is 0.
 * @param[in]  dest_len  Number of destination-address octets.
 * @param[in]  cargo     Cargo octets, or NULL if @p cargo_len is 0.
 * @param[in]  cargo_len Number of cargo octets.
 * @param[out] buf       Output buffer.
 * @param[in]  buf_len   Buffer capacity in octets.
 * @param[out] out_len   Total octets written; set only on ::SW_OK.
 * @return ::SW_OK on success; ::SW_INVALID_PARAM if @p buf or @p out_len is NULL
 *         or a NULL part has a non-zero length; ::SW_WRONG_LENGTH if both parts
 *         are empty (a packet has at least one data character, clause 5.6.2.1);
 *         ::SW_BUFFER_TOO_SMALL if the packet does not fit @p buf_len.
 */
sw_status_t sw_spw_packet_build(const uint8_t *dest,
                                size_t dest_len,
                                const uint8_t *cargo,
                                size_t cargo_len,
                                uint8_t *buf,
                                size_t buf_len,
                                size_t *out_len);

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
 * On failure, the routing table is left unchanged.
 *
 * @param[in,out] router       Target router.
 * @param[in]     logical_addr Logical address to route; must be 32..254.
 * @param[in]     output_port  Existing external output port to forward through
 *                             (1..num_ports - 1).
 * @param[in]     delete_addr  Non-zero to delete the logical address before
 *                             forwarding (logical-address deletion, clause 5.6.8.6).
 * @return ::SW_OK on success, ::SW_INVALID_PARAM if @p router is NULL,
 *         ::SW_WRONG_ADDRESS if @p logical_addr is outside 32..254, or
 *         ::SW_WRONG_PORT if @p output_port is the configuration port, which is
 *         reached by path addressing only (clause 5.6.8.3 f), or a port the
 *         router does not have.
 */
sw_status_t sw_router_add_route(sw_router_t *router,
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
 * On any error the packet is discarded, and @p output_port and
 * @p delete_leading are left unchanged.
 *
 * @param[in,out] router         Router (its counters are updated).
 * @param[in]     packet         Packet octets (the destination address leads).
 * @param[in]     len            Packet length in octets.
 * @param[out]    output_port    Selected output port.
 * @param[out]    delete_leading 1 if the leading character must be removed before
 *                               forwarding, else 0.
 * @return ::SW_OK to forward through @p output_port; ::SW_INVALID_PARAM for a NULL
 *         argument (no counter is touched); ::SW_WRONG_LENGTH for an empty packet
 *         (clause 5.6.2.1); ::SW_WRONG_ADDRESS for a leading address that names a
 *         non-existent port, is unconfigured, or is a logical address routed to the
 *         configuration port, which also bumps the router's invalid-address counter
 *         (clauses 5.6.8.3 f, 5.6.8.5).
 */
sw_status_t sw_router_route(sw_router_t *router,
                            const uint8_t *packet,
                            size_t len,
                            uint8_t *output_port,
                            uint8_t *delete_leading);

#ifdef __cplusplus
}
#endif

#endif /* SPACEWIRE_H */
