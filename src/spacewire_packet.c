/**
 * @file    spacewire_packet.c
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

#include "spacewire_packet.h"

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

static sw_packet_statistics_t g_sw_stats = {0};

/**
 * @brief Check that a frame's path address is usable (clause 5.3.1).
 *
 * @param[in] pf Packet frame; must be non-NULL.
 * @return 1 if the path is empty, or present with every octet in
 *         0..::SW_PATH_ADDR_MAX; otherwise 0.
 */
static int sw_packet_path_is_valid(const sw_packet_frame_t *pf)
{
    if ((pf->path_len > 0u) && (!pf->path))
    {
        return 0;
    }

    for (uint8_t i = 0; i < pf->path_len; i++)
    {
        if (pf->path[i] > SW_PATH_ADDR_MAX)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Write the optional path address and the encapsulation header
 *        (clauses 5.3.1 to 5.3.5).
 *
 * @param[in]  pf  Packet frame with a valid path.
 * @param[out] buf Output buffer with room for the path plus
 *                 ::SW_PTP_HEADER_LEN octets.
 */
static void sw_packet_write_header(const sw_packet_frame_t *pf, uint8_t *buf)
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

uint32_t sw_packet_encode(const sw_packet_frame_t *pf,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len)
{
    if ((!pf) || (!payload) || (!buf))
    {
        return 0;
    }

    /* A logical address is 32..255 and 255 shall not be used (ECSS-E-ST-50-12C
     * clause 5.6.8.4 a, d). A value in 0..31 is a path address instead: the first
     * router would forward the packet to that port number and delete the octet
     * (clause 5.6.8.3 a, c, e). */
    if ((pf->logical_addr < SW_LOGICAL_ADDR_MIN) || (pf->logical_addr == SW_LOGICAL_ADDR_RESERVED))
    {
        return 0;
    }

    if (!sw_packet_path_is_valid(pf))
    {
        return 0;
    }

    const uint32_t payload_len = sw_payload_size(payload);
    if ((payload_len < SW_PTP_CCSDS_MIN_LEN) || (payload_len > SW_PTP_CCSDS_MAX_LEN))
    {
        return 0;
    }

    const uint32_t header_len = (uint32_t)pf->path_len + SW_PTP_HEADER_LEN;
    const uint32_t total = header_len + payload_len;
    if (buf_len < total)
    {
        return 0;
    }

    /* The carried packet is serialised before the header is written, so a
     * failing hook leaves the caller's buffer untouched. */
    if (sw_payload_serialize(payload, &buf[header_len], buf_len - header_len) != payload_len)
    {
        return 0;
    }

    sw_packet_write_header(pf, buf);

    g_sw_stats.packets_sent++;
    g_sw_stats.bytes_sent += total;

    return total;
}

/**
 * @brief Write the receive status if the caller asked for it.
 *
 * @param[out] status Receive-status output; may be NULL.
 * @param[in]  code   Status code to report.
 */
static void sw_packet_report(sw_ptp_status_t *status, sw_ptp_status_t code)
{
    if (status)
    {
        *status = code;
    }
}

/**
 * @brief Count a discarded packet and report why (clause 5.5.4).
 *
 * @param[out] status Receive-status output; may be NULL.
 * @param[in]  code   Status code to report.
 * @return Always ::SW_ERR.
 */
static sw_result_t sw_packet_discard(sw_ptp_status_t *status, sw_ptp_status_t code)
{
    g_sw_stats.packets_discarded++;
    sw_packet_report(status, code);

    return SW_ERR;
}

/**
 * @brief Run every receive check that does not need the payload object
 *        (clause 5.5.4).
 *
 * @param[in] buf     Received buffer; must be non-NULL.
 * @param[in] buf_len Buffer length in octets.
 * @param[in] end     End-of-packet marker reported by the link layer.
 * @return ::SW_PTP_STATUS_OK if the packet may be extracted, otherwise the
 *         status to report for the discard.
 */
static sw_ptp_status_t sw_packet_check(const uint8_t *buf, uint32_t buf_len, sw_end_marker_t end)
{
    /* clause 5.5.4.4: a packet terminated by EEP shall be discarded. */
    if (end == SW_END_EEP)
    {
        return SW_PTP_STATUS_EEP;
    }

    /* clause 5.1.2 bounds the carried packet, and so the whole buffer. */
    if ((buf_len < (SW_PTP_HEADER_LEN + SW_PTP_CCSDS_MIN_LEN)) ||
        (buf_len > (SW_PTP_HEADER_LEN + SW_PTP_CCSDS_MAX_LEN)))
    {
        return SW_PTP_STATUS_INVALID;
    }

    /* clause 5.5.4.1: only Protocol Identifier 0x02 is a CCSDS PTP packet. */
    if (buf[SW_PTP_OFFSET_PROTOCOL_ID] != SW_PTP_PROTOCOL_ID)
    {
        return SW_PTP_STATUS_INVALID;
    }

    /* clause 5.5.4.3: a non-zero Reserved field discards the packet. */
    if (buf[SW_PTP_OFFSET_RESERVED] != SW_PTP_RESERVED)
    {
        return SW_PTP_STATUS_RESERVED_NONZERO;
    }

    /* The carried packet must end exactly where the SpaceWire packet does;
     * octets missing or left over mean the two layers disagree on its length. */
    const uint32_t payload_len = buf_len - SW_PTP_HEADER_LEN;
    if (sw_payload_length(&buf[SW_PTP_HEADER_LEN], payload_len) != payload_len)
    {
        return SW_PTP_STATUS_INVALID;
    }

    return SW_PTP_STATUS_OK;
}

sw_result_t sw_packet_decode(sw_packet_frame_t *pf,
                             void *payload,
                             const uint8_t *buf,
                             uint32_t buf_len,
                             sw_end_marker_t end,
                             sw_ptp_status_t *status)
{
    if ((!pf) || (!payload) || (!buf))
    {
        sw_packet_report(status, SW_PTP_STATUS_INVALID);

        return SW_INVALID_PARAM;
    }

    const sw_ptp_status_t code = sw_packet_check(buf, buf_len, end);
    if (code != SW_PTP_STATUS_OK)
    {
        return sw_packet_discard(status, code);
    }

    /* clause 5.5.4.2: extract the carried packet. This is the last check; the
     * hook leaves the payload object unchanged when it fails. */
    if (!sw_payload_parse(payload, &buf[SW_PTP_HEADER_LEN], buf_len - SW_PTP_HEADER_LEN))
    {
        return sw_packet_discard(status, SW_PTP_STATUS_INVALID);
    }

    pf->path = NULL;
    pf->path_len = 0;
    pf->logical_addr = buf[SW_PTP_OFFSET_LOGICAL_ADDR];
    pf->user_app = buf[SW_PTP_OFFSET_USER_APP];

    g_sw_stats.packets_received++;
    g_sw_stats.bytes_received += buf_len;
    sw_packet_report(status, SW_PTP_STATUS_OK);

    return SW_OK;
}

uint32_t sw_packet_create(uint8_t logical_addr,
                          uint8_t user_app,
                          const void *payload,
                          uint8_t *buf,
                          uint32_t buf_len)
{
    const sw_packet_frame_t pf = {.path = NULL,
                                  .path_len = 0,
                                  .logical_addr = logical_addr,
                                  .user_app = user_app};

    return sw_packet_encode(&pf, payload, buf, buf_len);
}

void sw_packet_get_statistics(sw_packet_statistics_t *stats)
{
    if (!stats)
    {
        return;
    }

    *stats = g_sw_stats;
}

void sw_packet_reset_statistics(void)
{
    memset(&g_sw_stats, 0, sizeof(g_sw_stats));
}
