/**
 * @file    spacewire_packet.h
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
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef SPACEWIRE_PACKET_H
#define SPACEWIRE_PACKET_H

#include "spacewire.h"

#ifdef __cplusplus
extern "C"
{
#endif

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

/**
 * @brief Largest valid path-address octet.
 *
 * A SpaceWire path address character selects an output port and is in the range
 * 0..31 (ECSS-E-ST-50-12C clause 5.6.8.3).
 */
#define SW_PTP_PATH_OCTET_MAX 31u

/**
 * @brief End-of-packet marker supplied by the SpaceWire link layer
 *        (ECSS-E-ST-50-12C clause 5.4.3.2).
 *
 * In a SpaceWire network the terminator is a control character, not a data
 * octet, so it is carried here as out-of-band metadata rather than appended to
 * the encoded buffer.
 */
typedef enum
{
    SW_END_EOP = 0, /**< Normal End Of Packet. */
    SW_END_EEP = 1  /**< Error End of Packet. */
} sw_end_marker_t;

/**
 * @brief Receive status reported to the target user application (clauses 5.1.3, 5.2.3.2).
 *
 * The first three values are the status codes defined by the standard.
 * ::SW_PTP_STATUS_INVALID is an implementation sentinel for input that is not a
 * recognisable CCSDS PTP packet.
 */
typedef enum
{
    SW_PTP_STATUS_OK = 0x00,               /**< Packet arrived with no known error. */
    SW_PTP_STATUS_EEP = 0x01,              /**< Packet terminated by EEP. */
    SW_PTP_STATUS_RESERVED_NONZERO = 0x02, /**< Reserved field was non-zero. */
    SW_PTP_STATUS_INVALID = 0xFF           /**< Not a CCSDS PTP packet / malformed. */
} sw_ptp_status_t;

/**
 * @brief Addressing and encapsulation fields of a CCSDS Packet Transfer Protocol packet.
 *
 * The wire layout produced by sw_packet_encode() follows ECSS-E-ST-50-53C
 * Figure 5-1:
 *
 *     [ Target SpW Address: path, 0..N octets, each 0-31 ]   (optional)
 *     [ Target Logical Address | 0x02 | 0x00 | User Application ]
 *     [ Carried packet: 7 to 65542 octets ]
 *
 * followed by an EOP that the link layer appends (not stored in the buffer).
 * The carried packet is not part of this structure; it is passed to
 * sw_packet_encode() and sw_packet_decode() as a separate payload object.
 *
 * @note @p path points into caller-owned memory; the library neither copies nor
 *       frees it.
 * @note The Target SpaceWire (path) Address is stripped by the network before
 *       the packet reaches the target, so it is absent on the receive path.
 */
typedef struct
{
    const uint8_t *path;  /**< Target SpaceWire (path) Address; NULL if unused (clause 5.3.1). */
    uint8_t path_len;     /**< Number of path octets (each must be 0..31). */
    uint8_t logical_addr; /**< Target Logical Address (clause 5.3.2). */
    uint8_t user_app; /**< User Application field; may carry a virtual channel (clause 5.3.5). */
} sw_packet_frame_t;

/**
 * @brief Configuration for a CCSDS PTP packet endpoint.
 *
 * @note @p path points into caller-owned memory; the library neither copies nor
 *       frees it.
 */
typedef struct
{
    const uint8_t *path;  /**< Optional path address; NULL if unused. */
    uint8_t path_len;     /**< Number of path octets. */
    uint8_t logical_addr; /**< Target Logical Address. */
    uint8_t user_app;     /**< User Application value. */
} sw_packet_config_t;

/**
 * @brief Virtual channel number.
 *
 * ECSS-E-ST-50-53C clause 5.3.5 (NOTE 2) allows the User Application field to
 * carry a virtual channel number, so a virtual channel is represented as that
 * 8-bit value rather than as separate router state.
 */
typedef uint8_t sw_virtual_channel_t;

/**
 * @brief Select a virtual channel by setting the User Application field.
 *
 * @param[out] pf Packet frame. No-op if NULL.
 * @param[in]  vc Virtual channel number.
 */
static inline void sw_packet_set_virtual_channel(sw_packet_frame_t *pf, sw_virtual_channel_t vc)
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
static inline sw_virtual_channel_t sw_packet_virtual_channel(const sw_packet_frame_t *pf)
{
    return pf ? pf->user_app : (sw_virtual_channel_t)0;
}

/**
 * @brief Initialise a packet frame from configuration.
 *
 * @param[out] pf     Packet frame to initialise. No-op if NULL.
 * @param[in]  config Endpoint configuration. No-op if NULL.
 */
void sw_packet_init(sw_packet_frame_t *pf, const sw_packet_config_t *config);

/**
 * @brief Encode a complete CCSDS PTP packet into a buffer.
 *
 * Writes the optional path address, the 4-octet encapsulation header and the
 * carried packet, which is sized by sw_payload_size() and serialised by
 * sw_payload_serialize(). No checksum and no in-band EOP are added.
 *
 * On failure, @p buf is left unchanged, provided sw_payload_serialize() honours
 * its own contract.
 *
 * @param[in]  pf      Packet frame to encode.
 * @param[in]  payload Payload object handed to the payload adapter hooks; with
 *                     the default hooks, a ::sw_payload_raw_t.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Octets written, or 0 on error (NULL args, invalid path octet, carried
 *         packet outside ::SW_PTP_CCSDS_MIN_LEN to ::SW_PTP_CCSDS_MAX_LEN octets,
 *         buffer too small, or a failing payload hook).
 */
uint32_t sw_packet_encode(const sw_packet_frame_t *pf,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len);

/**
 * @brief Decode a received CCSDS PTP packet.
 *
 * The input is the packet as delivered to the target, i.e. with the path address
 * already stripped by the network, beginning at the Target Logical Address:
 *
 *     [ Target Logical Address | 0x02 | 0x00 | User Application | carried packet ]
 *
 * The carried packet must fill the rest of the buffer: its length, as reported
 * by sw_payload_length(), has to equal the octets that follow the header. It is
 * then extracted by sw_payload_parse().
 *
 * On failure, @p pf and @p payload are left unchanged; only @p status is
 * written. A discarded packet is thereby "null" in the sense of clause
 * 5.2.3.2 b: nothing is delivered.
 *
 * @param[out] pf      Decoded Target Logical Address and User Application value.
 * @param[out] payload Payload object filled by sw_payload_parse(); with the
 *                     default hooks, a ::sw_payload_raw_t that points into @p buf.
 * @param[in]  buf     Received buffer to parse.
 * @param[in]  buf_len Buffer length in octets.
 * @param[in]  end     End-of-packet marker reported by the link layer.
 * @param[out] status  Receive status (clause 5.1.3); may be NULL. Set to
 *                      ::SW_PTP_STATUS_OK on delivery, ::SW_PTP_STATUS_EEP or
 *                      ::SW_PTP_STATUS_RESERVED_NONZERO on a recognised discard,
 *                      or ::SW_PTP_STATUS_INVALID for NULL args, a bad Protocol
 *                      Identifier or malformed input.
 * @return ::SW_OK if a valid packet is delivered, ::SW_ERR if it is discarded,
 *         or ::SW_INVALID_PARAM if @p pf, @p payload or @p buf is NULL.
 */
sw_result_t sw_packet_decode(sw_packet_frame_t *pf,
                             void *payload,
                             const uint8_t *buf,
                             uint32_t buf_len,
                             sw_end_marker_t end,
                             sw_ptp_status_t *status);

/**
 * @brief Build a complete CCSDS PTP packet using logical addressing only.
 *
 * Convenience wrapper around sw_packet_encode() for a frame without a path
 * address. On failure, @p buf is left unchanged.
 *
 * @param[in]  logical_addr Target Logical Address.
 * @param[in]  user_app     User Application value.
 * @param[in]  payload      Payload object handed to the payload adapter hooks.
 * @param[out] buf          Output buffer.
 * @param[in]  buf_len      Buffer capacity in octets.
 * @return Octets written, or 0 on error (see sw_packet_encode()).
 */
uint32_t sw_packet_create(uint8_t logical_addr,
                          uint8_t user_app,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len);

/**
 * @brief Cumulative CCSDS PTP traffic counters.
 *
 * @note The statistics are a single process-global instance updated by
 *       sw_packet_encode() and sw_packet_decode(); access is not thread-safe.
 */
typedef struct
{
    uint32_t packets_sent;      /**< Successfully encoded packets. */
    uint32_t packets_received;  /**< Successfully decoded packets. */
    uint32_t packets_discarded; /**< Discarded (EEP / Reserved!=0 / bad ID / malformed). */
    uint32_t bytes_sent;        /**< Total octets emitted by sw_packet_encode(). */
    uint32_t bytes_received;    /**< Total octets consumed by successful decode. */
} sw_statistics_t;

/**
 * @brief Copy the current statistics.
 *
 * @param[out] stats Destination. No-op if NULL.
 */
void sw_get_statistics(sw_statistics_t *stats);

/**
 * @brief Reset all statistics counters to zero.
 */
void sw_reset_statistics(void);

#ifdef __cplusplus
}
#endif

#endif /* SPACEWIRE_PACKET_H */
