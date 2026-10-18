/**
 * @file    spacewire_ptp.c
 * @brief   CCSDS Packet Transfer Protocol over SpaceWire
 *
 * Implements the encapsulation and extraction of ECSS-E-ST-50-53C clause 5.
 * The carried packet is reached only through the payload adapter hooks
 * (spacewire_payload.h), so this file knows nothing about its layout. No
 * checksum is added: the standard relies on SpaceWire character parity and the
 * EOP/EEP markers for error detection.
 * See also: docs/space_packet_integration.md
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "spacewire_ptp.h"

#include "spacewire_payload.h"

#include <string.h>

/** @brief Offset of the Target Logical Address in the encapsulation header (clause 5.3.2). */
#define SW_PTP_OFFSET_LOGICAL_ADDR 0u

/** @brief Offset of the Protocol Identifier in the encapsulation header (clause 5.3.3). */
#define SW_PTP_OFFSET_PROTOCOL_ID 1u

/** @brief Offset of the Reserved field in the encapsulation header (clause 5.3.4). */
#define SW_PTP_OFFSET_RESERVED 2u

/** @brief Offset of the User Application field in the encapsulation header (clause 5.3.5). */
#define SW_PTP_OFFSET_USER_APP 3u

static sw_ptp_statistics_t g_sw_stats = {0};

/**
 * @brief Check a frame's addressing fields and the carried packet's length.
 *
 * The Target Logical Address is a logical address, 32..254: 255 shall not be used
 * (ECSS-E-ST-50-12C clause 5.6.8.4 a, d), and a value in 0..31 is a path address
 * that the first router would take as a port number and delete (clause 5.6.8.3 a,
 * c, e). The Target SpaceWire Address may mix path (0..31) and regional logical
 * (32..254) octets (ECSS-E-ST-50-53C clause 5.3.1 b); only 255 is refused. The
 * carried packet is 7..65542 octets (clause 5.1.2 a).
 *
 * @param[in] pf          Packet frame; must be non-NULL.
 * @param[in] payload_len Serialised size of the carried packet.
 * @return ::SW_OK, ::SW_WRONG_ADDRESS, ::SW_INVALID_PARAM (address octets
 *         announced but @p pf->path is NULL) or ::SW_WRONG_LENGTH.
 */
static sw_status_t sw_ptp_validate(const sw_ptp_frame_t *pf, uint32_t payload_len)
{
    if ((pf->logical_addr < SW_LOGICAL_ADDR_MIN) || (pf->logical_addr == SW_LOGICAL_ADDR_RESERVED))
    {
        return SW_WRONG_ADDRESS;
    }

    if ((pf->path_len > 0u) && (!pf->path))
    {
        return SW_INVALID_PARAM;
    }

    for (uint8_t i = 0; i < pf->path_len; i++)
    {
        if (pf->path[i] == SW_LOGICAL_ADDR_RESERVED)
        {
            return SW_WRONG_ADDRESS;
        }
    }

    if ((payload_len < SW_PTP_CCSDS_MIN_LEN) || (payload_len > SW_PTP_CCSDS_MAX_LEN))
    {
        return SW_WRONG_LENGTH;
    }

    return SW_OK;
}

/**
 * @brief Write the optional Target SpaceWire Address and the encapsulation header
 *        (clauses 5.3.1 to 5.3.5).
 *
 * @param[in]  pf  Packet frame with a valid Target SpaceWire Address.
 * @param[out] buf Output buffer with room for the address plus
 *                 ::SW_PTP_HEADER_LEN octets.
 */
static void sw_ptp_write_header(const sw_ptp_frame_t *pf, uint8_t *buf)
{
    uint8_t *header = &buf[pf->path_len];

    if (pf->path_len > 0u)
    {
        memcpy(buf, pf->path, pf->path_len);
    }

    header[SW_PTP_OFFSET_LOGICAL_ADDR] = pf->logical_addr;
    header[SW_PTP_OFFSET_PROTOCOL_ID] = (uint8_t)SW_PTP_PROTOCOL_ID;
    header[SW_PTP_OFFSET_RESERVED] = (uint8_t)SW_PTP_RESERVED;
    header[SW_PTP_OFFSET_USER_APP] = pf->user_app;
}

sw_status_t sw_ptp_encode(const sw_ptp_frame_t *pf,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len,
                          uint32_t *out_len)
{
    if ((!pf) || (!payload) || (!buf) || (!out_len))
    {
        return SW_INVALID_PARAM;
    }

    const uint32_t payload_len = sw_payload_size(payload);
    const sw_status_t valid = sw_ptp_validate(pf, payload_len);
    if (valid != SW_OK)
    {
        return valid;
    }

    const uint32_t header_len = (uint32_t)pf->path_len + SW_PTP_HEADER_LEN;
    const uint32_t total = header_len + payload_len;
    if (buf_len < total)
    {
        return SW_BUFFER_TOO_SMALL;
    }

    /* The carried packet is serialised before the header is written, so a
     * failing hook leaves the caller's buffer untouched. */
    if (sw_payload_serialize(payload, &buf[header_len], buf_len - header_len) != SW_OK)
    {
        return SW_PAYLOAD_ERROR;
    }

    sw_ptp_write_header(pf, buf);

    g_sw_stats.packets_sent++;
    g_sw_stats.bytes_sent += total;
    *out_len = total;

    return SW_OK;
}

/**
 * @brief Run the receive checks of clause 5.5.4.
 *
 * Only the encapsulation header is checked. The carried packet is passed on
 * whatever its length or format (clauses 4.3 and 5.5.4.2 a); the payload adapter
 * alone decides whether it can be extracted.
 *
 * @param[in]  buf     Received buffer; must be non-NULL.
 * @param[in]  buf_len Buffer length in octets.
 * @param[in]  end     End-of-packet marker reported by the link layer.
 * @param[out] code    Receive status of a ::SW_DISCARDED packet; written only then.
 * @return ::SW_OK if the packet may be extracted; ::SW_DISCARDED for a discard
 *         defined by the standard (EEP, non-zero Reserved field); otherwise
 *         ::SW_INVALID_PARAM, ::SW_WRONG_LENGTH or ::SW_WRONG_PROTOCOL.
 */
static sw_status_t sw_ptp_check(const uint8_t *buf,
                                uint32_t buf_len,
                                sw_end_marker_t end,
                                sw_ptp_status_t *code)
{
    /* clause 5.5.4.4: a packet terminated by EEP shall be discarded. */
    if (end == SW_END_EEP)
    {
        *code = SW_PTP_STATUS_EEP;
        return SW_DISCARDED;
    }

    /* Fail closed: a marker the link layer cannot have meant as EOP delivers nothing. */
    if (end != SW_END_EOP)
    {
        return SW_INVALID_PARAM;
    }

    /* Without a complete header the packet cannot be identified at all. */
    if (buf_len < SW_PTP_HEADER_LEN)
    {
        return SW_WRONG_LENGTH;
    }

    /* clause 5.5.4.1: only Protocol Identifier 0x02 is a CCSDS PTP packet. */
    if (buf[SW_PTP_OFFSET_PROTOCOL_ID] != SW_PTP_PROTOCOL_ID)
    {
        return SW_WRONG_PROTOCOL;
    }

    /* clause 5.5.4.3: a non-zero Reserved field discards the packet. */
    if (buf[SW_PTP_OFFSET_RESERVED] != SW_PTP_RESERVED)
    {
        *code = SW_PTP_STATUS_RESERVED_NONZERO;
        return SW_DISCARDED;
    }

    return SW_OK;
}

sw_status_t sw_ptp_decode(sw_ptp_frame_t *pf,
                          void *payload,
                          const uint8_t *buf,
                          uint32_t buf_len,
                          sw_end_marker_t end,
                          sw_ptp_status_t *status)
{
    if ((!pf) || (!payload) || (!buf))
    {
        return SW_INVALID_PARAM;
    }

    sw_ptp_status_t code = SW_PTP_STATUS_OK;
    sw_status_t result = sw_ptp_check(buf, buf_len, end, &code);

    /* clause 5.5.4.2: extract the carried packet. A hook that cannot represent it
     * fails and leaves the payload object unchanged; nothing is delivered then. */
    if ((result == SW_OK) &&
        (sw_payload_parse(payload, &buf[SW_PTP_HEADER_LEN], buf_len - SW_PTP_HEADER_LEN) != SW_OK))
    {
        result = SW_PAYLOAD_ERROR;
    }

    if (result != SW_OK)
    {
        g_sw_stats.packets_discarded++;
    }
    else
    {
        pf->path = NULL;
        pf->path_len = 0;
        pf->logical_addr = buf[SW_PTP_OFFSET_LOGICAL_ADDR];
        pf->user_app = buf[SW_PTP_OFFSET_USER_APP];
        g_sw_stats.packets_received++;
        g_sw_stats.bytes_received += buf_len;
    }

    /* The status carries the standard's codes only (clauses 5.1.3, 5.2.3.2 c). */
    if ((status) && ((result == SW_OK) || (result == SW_DISCARDED)))
    {
        *status = code;
    }

    return result;
}

sw_status_t sw_ptp_create(uint8_t logical_addr,
                          uint8_t user_app,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len,
                          uint32_t *out_len)
{
    const sw_ptp_frame_t pf = {.path = NULL,
                               .path_len = 0,
                               .logical_addr = logical_addr,
                               .user_app = user_app};

    return sw_ptp_encode(&pf, payload, buf, buf_len, out_len);
}

void sw_ptp_get_statistics(sw_ptp_statistics_t *stats)
{
    if (!stats)
    {
        return;
    }

    *stats = g_sw_stats;
}

void sw_ptp_reset_statistics(void)
{
    memset(&g_sw_stats, 0, sizeof(g_sw_stats));
}
