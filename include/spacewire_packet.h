/*
 * CCSDS Space Wire + CCSDS Space Packet Integration
 * High-level API for Space Wire packet transmission
 */

#ifndef SPACEWIRE_PACKET_H
#define SPACEWIRE_PACKET_H

#include "spacewire.h"
#include "../external/EmbeddedSpacePacket/include/space_packet.h"

/* ============================================================================
 * SPACEWIRE PACKET TRANSMISSION
 * ============================================================================ */

/* Packet frame with CCSDS Space Packet payload */
typedef struct {
  sw_frame_t frame;        /* Space Wire frame */
  sp_packet_t packet;      /* CCSDS Space Packet */
} sw_packet_frame_t;

/* Configuration for packet transmission */
typedef struct {
  uint8_t device_addr;
  uint8_t target_addr;
  uint8_t protocol_id;     /* Should be 1 for CCSDS packets */
  uint8_t enable_crc;
} sw_packet_config_t;

/* Initialize packet frame */
void sw_packet_init(sw_packet_frame_t *pf, const sw_packet_config_t *config);

/* Build and serialize Space Wire frame containing CCSDS packet */
size_t sw_packet_encode(const sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len);

/* Parse incoming Space Wire frame and extract CCSDS packet */
int sw_packet_decode(sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len);

/* Utility: Create complete packet frame in one call */
size_t sw_packet_create(uint8_t device_addr,
                        uint8_t target_addr,
                        uint16_t apid,
                        const uint8_t *payload,
                        uint16_t payload_len,
                        uint8_t *buf,
                        size_t buf_len);

/* ============================================================================
 * STATISTICS AND DEBUG
 * ============================================================================ */

typedef struct {
  uint32_t packets_sent;
  uint32_t packets_received;
  uint32_t crc_errors;
  uint32_t frame_errors;
  uint32_t link_errors;
  uint32_t bytes_sent;
  uint32_t bytes_received;
} sw_statistics_t;

/* Get current statistics */
void sw_get_statistics(sw_statistics_t *stats);

/* Reset statistics */
void sw_reset_statistics(void);

#endif /* SPACEWIRE_PACKET_H */
