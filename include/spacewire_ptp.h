/**
 * @file    spacewire_ptp.h
 * @brief   CCSDS Packet Transfer Protocol over SpaceWire (ECSS-E-ST-50-53C)
 *
 * Encapsulates a packet into a SpaceWire packet on the send path and extracts
 * it on the receive path. The carried packet is opaque to this layer: it is
 * sized, serialised and parsed through the payload adapter hooks declared in
 * spacewire_payload.h, which an application overrides to carry CCSDS Space
 * Packets (CCSDS 133.0-B-2). The protocol adds no checksum of its own —
 * ECSS-E-ST-50-53C defines none; error detection is provided by the underlying
 * SpaceWire link (character parity) and by the EOP/EEP end-of-packet markers.
 * See also: docs/space_packet_integration.md
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef SPACEWIRE_PTP_H
#define SPACEWIRE_PTP_H

#include "spacewire.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** @brief Protocol Identifier for the CCSDS Packet Transfer Protocol (clause 5.3.3). */
#define SW_PTP_PROTOCOL_ID 0x02u

/** @brief Reserved field value (clause 5.3.4). */
#define SW_PTP_RESERVED 0x00u

/**
 * @brief Default Target Logical Address when the target has no specific one
 *        (clause 5.3.2, NOTE 2).
 *
 * @note Logical address 255 (0xFF) is reserved and must not be used.
 */
#define SW_PTP_LOGICAL_ADDR_DEFAULT 0xFEu

/**
 * @brief Encapsulation header length in octets.
 *
 * Target Logical Address + Protocol Identifier + Reserved + User Application.
 */
#define SW_PTP_HEADER_LEN 4u

/** @brief Minimum CCSDS packet length: 6-octet primary header + 1 data octet (clause 5.1.2). */
#define SW_PTP_CCSDS_MIN_LEN 7u

/** @brief Maximum CCSDS packet length: 6-octet primary header + 65536 data octets (clause 5.1.2).
 */
#define SW_PTP_CCSDS_MAX_LEN 65542u

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Receive status reported to the target user application (clauses 5.1.3, 5.2.3.2).
 *
 * Exactly the three status codes the standard defines (clauses 5.1.3 b, 5.2.3.2 c).
 * Input that is not delivered for any other reason is reported through the
 * ::sw_status_t returned by sw_ptp_decode(), not through this value.
 */
typedef enum
{
    SW_PTP_STATUS_OK = 0x00,              /**< Packet arrived with no known error. */
    SW_PTP_STATUS_EEP = 0x01,             /**< Packet terminated by EEP. */
    SW_PTP_STATUS_RESERVED_NONZERO = 0x02 /**< Reserved field was non-zero. */
} sw_ptp_status_t;

/**
 * @brief Addressing and encapsulation fields of a CCSDS Packet Transfer Protocol packet.
 *
 * The wire layout produced by sw_ptp_encode() follows ECSS-E-ST-50-53C
 * Figure 5-1:
 *
 *     [ Target SpW Address: 0..N octets, each 0-254 ]       (optional)
 *     [ Target Logical Address | 0x02 | 0x00 | User Application ]
 *     [ Carried packet: 7 to 65542 octets ]
 *
 * followed by an EOP that the link layer appends (not stored in the buffer).
 * The carried packet is not part of this structure; it is passed to
 * sw_ptp_encode() and sw_ptp_decode() as a separate payload object.
 *
 * @note @p path points into caller-owned memory; the library neither copies nor
 *       frees it.
 * @note The Target SpaceWire Address is stripped by the network before the
 *       packet reaches the target, so it is absent on the receive path.
 */
typedef struct
{
    const uint8_t *path;  /**< Target SpaceWire Address; NULL if unused (clause 5.3.1). Path (0..31)
                           * and regional logical (32..254) octets may be mixed; 255 is reserved. */
    uint8_t path_len;     /**< Number of Target SpaceWire Address octets. */
    uint8_t logical_addr; /**< Target Logical Address (clause 5.3.2); 32..254 per ECSS-E-ST-50-12C
                             clause 5.6.8.4. */
    uint8_t user_app; /**< User Application field; may carry a virtual channel (clause 5.3.5). */
} sw_ptp_frame_t;

/**
 * @brief Virtual channel number.
 *
 * ECSS-E-ST-50-53C clause 5.3.5 (NOTE 2) allows the User Application field to
 * carry a virtual channel number, so a virtual channel is represented as that
 * 8-bit value rather than as separate router state.
 */
typedef uint8_t sw_ptp_virtual_channel_t;

/**
 * @brief Cumulative CCSDS PTP traffic counters.
 *
 * @note The statistics are a single process-global instance updated by
 *       sw_ptp_encode() and sw_ptp_decode(); access is not thread-safe.
 */
typedef struct
{
    uint32_t packets_sent;      /**< Successfully encoded packets. */
    uint32_t packets_received;  /**< Successfully decoded packets. */
    uint32_t packets_discarded; /**< Received but not delivered (any decode error but NULL). */
    uint32_t bytes_sent;        /**< Total octets emitted by sw_ptp_encode(). */
    uint32_t bytes_received;    /**< Total octets consumed by successful decode. */
} sw_ptp_statistics_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Select a virtual channel by setting the User Application field.
 *
 * @param[in,out] pf Packet frame. No-op if NULL.
 * @param[in]  vc Virtual channel number.
 */
static inline void sw_ptp_set_virtual_channel(sw_ptp_frame_t *pf, sw_ptp_virtual_channel_t vc)
{
    if (pf)
    {
        pf->user_app = vc;
    }
}

/**
 * @brief Read the virtual channel carried in the User Application field.
 *
 * @param[in] pf Packet frame.
 * @return Virtual channel number, or 0 if @p pf is NULL.
 */
static inline sw_ptp_virtual_channel_t sw_ptp_get_virtual_channel(const sw_ptp_frame_t *pf)
{
    return pf ? pf->user_app : (sw_ptp_virtual_channel_t)0;
}

/**
 * @brief Encode a complete CCSDS PTP packet into a buffer.
 *
 * Writes the optional Target SpaceWire Address, the 4-octet encapsulation header
 * and the carried packet, which is sized by sw_payload_size() and serialised by
 * sw_payload_serialize(). No checksum and no in-band EOP are added.
 *
 * On failure, @p buf and @p out_len are left unchanged, provided
 * sw_payload_serialize() honours its own contract.
 *
 * @param[in]  pf      Packet frame to encode.
 * @param[in]  payload Payload object handed to the payload adapter hooks; with
 *                     the default hooks, a ::sw_payload_raw_t.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @param[out] out_len Octets written; set only on ::SW_OK.
 * @return ::SW_OK on success; ::SW_INVALID_PARAM for a NULL argument or a NULL
 *         path with a non-zero length; ::SW_WRONG_ADDRESS for a Target Logical
 *         Address outside 32..254 or a Target SpaceWire Address octet of 255;
 *         ::SW_WRONG_LENGTH for a carried packet outside ::SW_PTP_CCSDS_MIN_LEN to
 *         ::SW_PTP_CCSDS_MAX_LEN octets (clauses 5.1.2 a, 5.5.2 c);
 *         ::SW_BUFFER_TOO_SMALL; or ::SW_PAYLOAD_ERROR if sw_payload_serialize() fails.
 */
sw_status_t sw_ptp_encode(const sw_ptp_frame_t *pf,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len,
                          uint32_t *out_len);

/**
 * @brief Decode a received CCSDS PTP packet.
 *
 * The input is the packet as delivered to the target, i.e. with the Target
 * SpaceWire Address already stripped by the network, beginning at the Target
 * Logical Address:
 *
 *     [ Target Logical Address | 0x02 | 0x00 | User Application | carried packet ]
 *
 * Only the header is checked: Protocol Identifier, Reserved field and end
 * marker (clause 5.5.4). Every octet after the header is handed to
 * sw_payload_parse() as the carried packet, whatever its length or format
 * (clauses 4.3, 5.5.4.2 a); the send-side bounds of clause 5.1.2 are not
 * applied here.
 *
 * On failure, @p pf and @p payload are left unchanged. A discarded packet is
 * thereby "null" in the sense of clause 5.2.3.2 b: nothing is delivered.
 *
 * @param[out] pf      Decoded Target Logical Address and User Application value.
 * @param[out] payload Payload object filled by sw_payload_parse(); with the
 *                     default hooks, a ::sw_payload_raw_t that points into @p buf.
 * @param[in]  buf     Received buffer to parse.
 * @param[in]  buf_len Buffer length in octets.
 * @param[in]  end     End-of-packet marker reported by the link layer. Only
 *                     ::SW_END_EOP can deliver a packet; any value other than
 *                     ::SW_END_EOP or ::SW_END_EEP is treated as invalid.
 * @param[out] status  Receive status for the target user application (clause
 *                     5.1.3); may be NULL. Written only when the return value is
 *                     ::SW_OK (::SW_PTP_STATUS_OK) or ::SW_DISCARDED
 *                     (::SW_PTP_STATUS_EEP or ::SW_PTP_STATUS_RESERVED_NONZERO);
 *                     left unchanged otherwise.
 * @return ::SW_OK if the packet is delivered; ::SW_DISCARDED for a discard the
 *         standard defines (EEP, clause 5.5.4.4; non-zero Reserved field, clause
 *         5.5.4.3); ::SW_INVALID_PARAM if @p pf, @p payload or @p buf is NULL or
 *         @p end is neither EOP nor EEP; ::SW_WRONG_LENGTH for fewer than
 *         ::SW_PTP_HEADER_LEN octets; ::SW_WRONG_PROTOCOL for a Protocol
 *         Identifier other than 0x02 (clause 5.5.4.1); or ::SW_PAYLOAD_ERROR if
 *         sw_payload_parse() fails. Every error except a NULL argument counts as a
 *         discarded packet in the statistics.
 */
sw_status_t sw_ptp_decode(sw_ptp_frame_t *pf,
                          void *payload,
                          const uint8_t *buf,
                          uint32_t buf_len,
                          sw_end_marker_t end,
                          sw_ptp_status_t *status);

/**
 * @brief Build a complete CCSDS PTP packet using logical addressing only.
 *
 * Convenience wrapper around sw_ptp_encode() for a frame without a Target
 * SpaceWire Address. On failure, @p buf and @p out_len are left unchanged.
 *
 * @param[in]  logical_addr Target Logical Address.
 * @param[in]  user_app     User Application value.
 * @param[in]  payload      Payload object handed to the payload adapter hooks.
 * @param[out] buf          Output buffer.
 * @param[in]  buf_len      Buffer capacity in octets.
 * @param[out] out_len      Octets written; set only on ::SW_OK.
 * @return As sw_ptp_encode().
 */
sw_status_t sw_ptp_create(uint8_t logical_addr,
                          uint8_t user_app,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len,
                          uint32_t *out_len);

/**
 * @brief Copy the current statistics.
 *
 * @param[out] stats Destination. No-op if NULL.
 */
void sw_ptp_get_statistics(sw_ptp_statistics_t *stats);

/**
 * @brief Reset all statistics counters to zero.
 */
void sw_ptp_reset_statistics(void);

#ifdef __cplusplus
}
#endif

#endif /* SPACEWIRE_PTP_H */
