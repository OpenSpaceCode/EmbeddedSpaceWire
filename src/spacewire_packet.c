/*
 * Space Wire + CCSDS Space Packet Integration
 * High-level API combining both protocols
 */

#include "../include/spacewire_packet.h"
#include <string.h>

/* Global statistics */
static sw_statistics_t g_sw_stats = {0};

/* ============================================================================
 * PACKET INITIALIZATION
 * ============================================================================ */

void sw_packet_init(sw_packet_frame_t *pf, const sw_packet_config_t *config) {
  if (!pf || !config)
    return;

  memset(pf, 0, sizeof(sw_packet_frame_t));

  /* Initialize frame */
  pf->frame.target_addr = config->target_addr;
  pf->frame.protocol_id = config->protocol_id;

  /* Initialize packet */
  sp_packet_init(&pf->packet);
  pf->packet.ph.version = 0;
  pf->packet.ph.type = 1;  /* Telemetry */
  pf->packet.ph.sec_hdr_flag = 0;
  pf->packet.ph.apid = 0;
}

/* ============================================================================
 * PACKET ENCODING (SERIALIZATION)
 * ============================================================================ */

size_t sw_packet_encode(const sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len) {
  if (!pf || !buf)
    return 0;

  /* First, serialize the CCSDS packet */
  size_t pkt_size = sp_packet_serialize_size(&pf->packet);
  if (pkt_size == 0 || pkt_size > SW_FRAME_MAX_PAYLOAD)
    return 0;

  /* Create temporary buffer for serialized packet */
  uint8_t pkt_buf[SW_FRAME_MAX_PAYLOAD];
  size_t serialized = sp_packet_serialize(&pf->packet, pkt_buf, sizeof(pkt_buf));
  if (serialized == 0)
    return 0;

  /* Now create Space Wire frame with packet as payload */
  sw_frame_t frame = pf->frame;
  frame.payload = pkt_buf;
  frame.payload_len = (uint16_t)serialized;

  size_t frame_size = sw_frame_encode(&frame, buf, buf_len);
  if (frame_size > 0) {
    g_sw_stats.packets_sent++;
    g_sw_stats.bytes_sent += frame_size;
  }

  return frame_size;
}

/* ============================================================================
 * PACKET DECODING (PARSING)
 * ============================================================================ */

int sw_packet_decode(sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len) {
  if (!pf || !buf)
    return 0;

  /* First, decode the Space Wire frame */
  if (!sw_frame_decode(&pf->frame, buf, buf_len))
    return 0;

  /* Then, parse the CCSDS packet from frame payload */
  uint8_t *payload_buf = (uint8_t *)pf->frame.payload;
  size_t payload_len = pf->frame.payload_len;

  if (!sp_packet_parse(&pf->packet, payload_buf, payload_len))
    return 0;

  g_sw_stats.packets_received++;
  g_sw_stats.bytes_received += buf_len;

  return 1;  /* Success */
}

/* ============================================================================
 * CONVENIENCE FUNCTION
 * ============================================================================ */

size_t sw_packet_create(uint8_t device_addr,
                        uint8_t target_addr,
                        uint16_t apid,
                        const uint8_t *payload,
                        uint16_t payload_len,
                        uint8_t *buf,
                        size_t buf_len) {
  if (!buf)
    return 0;

  /* Create packet frame */
  sw_packet_config_t config = {
      .device_addr = device_addr,
      .target_addr = target_addr,
      .protocol_id = 1,  /* CCSDS */
      .enable_crc = 1
  };

  sw_packet_frame_t pf;
  sw_packet_init(&pf, &config);

  /* Set CCSDS packet fields */
  pf.packet.ph.apid = apid;
  pf.packet.payload = payload;
  pf.packet.payload_len = payload_len;

  /* Serialize and return */
  return sw_packet_encode(&pf, buf, buf_len);
}

/* ============================================================================
 * STATISTICS
 * ============================================================================ */

void sw_get_statistics(sw_statistics_t *stats) {
  if (!stats)
    return;
  *stats = g_sw_stats;
}

void sw_reset_statistics(void) {
  memset(&g_sw_stats, 0, sizeof(g_sw_stats));
}
