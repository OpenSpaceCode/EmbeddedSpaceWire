/*
 * CCSDS Packet Transfer Protocol over SpaceWire (ECSS-E-ST-50-53C).
 *
 * Encapsulates a CCSDS Space Packet into a SpaceWire packet on the send path
 * and extracts it on the receive path. No checksum is added: ECSS-E-ST-50-53C
 * relies on SpaceWire character parity and the EOP/EEP markers for error
 * detection.
 */

#include "../include/spacewire_packet.h"

#include <string.h>

/* Global statistics */
static sw_statistics_t g_sw_stats = {0};

/* ============================================================================
 * PACKET INITIALISATION
 * ============================================================================ */

void sw_packet_init(sw_packet_frame_t *pf, const sw_packet_config_t *config)
{
    if (!pf || !config)
        return;

    memset(pf, 0, sizeof(*pf));

    pf->path = config->path;
    pf->path_len = config->path_len;
    pf->logical_addr = config->logical_addr;
    pf->user_app = config->user_app;

    sp_packet_init(&pf->packet);
    pf->packet.ph.version = 0;
    pf->packet.ph.type = SP_PACKET_TYPE_TC; /* default Packet Type (CCSDS: TM=0, TC=1) */
    pf->packet.ph.sec_hdr_flag = 0;
    pf->packet.ph.apid = 0;
}

/* ============================================================================
 * PACKET ENCODING (clause 5.4)
 * ============================================================================ */

size_t sw_packet_encode(const sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len)
{
    if (!pf || !buf)
        return 0;

    if (pf->packet.data_len > 0 && pf->packet.data == NULL)
        return 0;

    /* Path octets must be valid SpaceWire path addresses (0..31). */
    if (pf->path_len > 0 && pf->path == NULL)
        return 0;
    for (uint8_t i = 0; i < pf->path_len; i++)
    {
        if (pf->path[i] > SW_PTP_PATH_OCTET_MAX)
            return 0;
    }

    /* CCSDS packet length must be within bounds (clause 5.1.2). */
    size_t ccsds_len = sp_packet_serialize_size(&pf->packet);
    if (ccsds_len < SW_PTP_CCSDS_MIN_LEN || ccsds_len > SW_PTP_CCSDS_MAX_LEN)
        return 0;

    size_t total = (size_t)pf->path_len + SW_PTP_HEADER_LEN + ccsds_len;
    if (buf_len < total)
        return 0;

    size_t offset = 0;

    /* Target SpaceWire (path) Address (clause 5.3.1). */
    if (pf->path_len > 0)
    {
        memcpy(&buf[offset], pf->path, pf->path_len);
        offset += pf->path_len;
    }

    /* Encapsulation header (clause 5.4.1). */
    buf[offset++] = pf->logical_addr;             /* Target Logical Address (5.3.2) */
    buf[offset++] = (uint8_t)SW_PTP_PROTOCOL_ID;  /* Protocol Identifier = 0x02     */
    buf[offset++] = (uint8_t)SW_PTP_RESERVED;     /* Reserved = 0x00                */
    buf[offset++] = pf->user_app;                 /* User Application (5.3.5)        */

    /* CCSDS Space Packet (clause 5.3.6). */
    size_t written = sp_packet_serialize(&pf->packet, &buf[offset], buf_len - offset);
    if (written == 0)
        return 0;
    offset += written;

    g_sw_stats.packets_sent++;
    g_sw_stats.bytes_sent += (uint32_t)offset;

    return offset;
}

/* ============================================================================
 * PACKET DECODING (clause 5.5.4)
 * ============================================================================ */

/* Clear the delivered packet/user-application fields (clause 5.2.3.2 b). */
static void sw_packet_clear_payload(sw_packet_frame_t *pf)
{
    pf->path = NULL;
    pf->path_len = 0;
    pf->logical_addr = 0;
    pf->user_app = 0;
    sp_packet_init(&pf->packet);
}

/* Discard a received packet: clear the delivered fields, count it, and report
 * the status code (clause 5.5.4). Always returns 0; @p pf must be non-NULL. */
static int sw_packet_discard(sw_packet_frame_t *pf, sw_ptp_status_t *status, sw_ptp_status_t code)
{
    sw_packet_clear_payload(pf);
    g_sw_stats.packets_discarded++;
    if (status)
        *status = code;
    return 0;
}

int sw_packet_decode(sw_packet_frame_t *pf,
                     const uint8_t *buf,
                     size_t buf_len,
                     sw_end_marker_t end,
                     sw_ptp_status_t *status)
{
    if (!pf || !buf)
    {
        if (status)
            *status = SW_PTP_STATUS_INVALID;
        return 0;
    }

    /* clause 5.5.4.4: a packet terminated by EEP shall be discarded. */
    if (end == SW_END_EEP)
        return sw_packet_discard(pf, status, SW_PTP_STATUS_EEP);

    /* Need the encapsulation header plus a minimal CCSDS packet. */
    if (buf_len < (size_t)SW_PTP_HEADER_LEN + SW_PTP_CCSDS_MIN_LEN)
        return sw_packet_discard(pf, status, SW_PTP_STATUS_INVALID);

    uint8_t logical = buf[0];
    uint8_t proto = buf[1];
    uint8_t reserved = buf[2];
    uint8_t user_app = buf[3];

    /* clause 5.5.4.1: only Protocol Identifier 0x02 is a CCSDS PTP packet. */
    if (proto != SW_PTP_PROTOCOL_ID)
        return sw_packet_discard(pf, status, SW_PTP_STATUS_INVALID);

    /* clause 5.5.4.3: Reserved field non-zero -> discard, packet/user app null. */
    if (reserved != SW_PTP_RESERVED)
        return sw_packet_discard(pf, status, SW_PTP_STATUS_RESERVED_NONZERO);

    /* clause 5.5.4.2: extract the CCSDS packet and User Application value. */
    if (!sp_packet_parse(&pf->packet, &buf[SW_PTP_HEADER_LEN], buf_len - SW_PTP_HEADER_LEN))
        return sw_packet_discard(pf, status, SW_PTP_STATUS_INVALID);

    pf->path = NULL;
    pf->path_len = 0;
    pf->logical_addr = logical;
    pf->user_app = user_app;

    g_sw_stats.packets_received++;
    g_sw_stats.bytes_received += (uint32_t)buf_len;

    if (status)
        *status = SW_PTP_STATUS_OK;
    return 1;
}

/* ============================================================================
 * CONVENIENCE FUNCTION
 * ============================================================================ */

size_t sw_packet_create(uint8_t logical_addr,
                        uint8_t user_app,
                        uint16_t apid,
                        const uint8_t *payload,
                        uint16_t payload_len,
                        uint8_t *buf,
                        size_t buf_len)
{
    if (!buf)
        return 0;

    sw_packet_config_t config = {
        .path = NULL, .path_len = 0, .logical_addr = logical_addr, .user_app = user_app};

    sw_packet_frame_t pf;
    sw_packet_init(&pf, &config);

    pf.packet.ph.apid = (unsigned)(apid & 0x7FFu);
    pf.packet.data = payload;
    pf.packet.data_len = payload_len;

    return sw_packet_encode(&pf, buf, buf_len);
}

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

void sw_get_statistics(sw_statistics_t *stats)
{
    if (!stats)
        return;
    *stats = g_sw_stats;
}

void sw_reset_statistics(void)
{
    memset(&g_sw_stats, 0, sizeof(g_sw_stats));
}
