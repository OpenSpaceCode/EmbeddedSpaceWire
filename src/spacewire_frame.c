/*
 * Space Wire Frame Layer - Framing and CRC
 */

#include "../include/spacewire.h"
#include <string.h>

/* ============================================================================
 * FRAME INITIALIZATION
 * ============================================================================ */

void sw_frame_init(sw_frame_t *frame) {
  if (!frame)
    return;
  frame->target_addr = 0;
  frame->protocol_id = 1;  /* CCSDS by default */
  frame->payload = NULL;
  frame->payload_len = 0;
}

/* ============================================================================
 * FRAME SIZE CALCULATION
 * ============================================================================ */

size_t sw_frame_size(const sw_frame_t *frame) {
  if (!frame)
    return 0;

  /* Header: Target Address (1) + Protocol ID (1) */
  size_t size = 2;

  /* Payload */
  size += frame->payload_len;

  /* CRC (2 bytes) */
  size += 2;

  return size;
}

/* ============================================================================
 * FRAME ENCODING (SERIALIZATION)
 * ============================================================================ */

size_t sw_frame_encode(const sw_frame_t *frame, uint8_t *buf, size_t buf_len) {
  if (!frame || !buf)
    return 0;

  size_t needed = sw_frame_size(frame);
  if (buf_len < needed)
    return 0;

  size_t offset = 0;

  /* Write header */
  buf[offset++] = frame->target_addr;
  buf[offset++] = frame->protocol_id;

  /* Write payload */
  if (frame->payload && frame->payload_len > 0) {
    memcpy(&buf[offset], frame->payload, frame->payload_len);
    offset += frame->payload_len;
  }

  /* Calculate and write CRC over header + payload */
  uint16_t crc = sw_crc16(buf, offset);
  buf[offset++] = (uint8_t)((crc >> 8) & 0xFF);
  buf[offset++] = (uint8_t)(crc & 0xFF);

  return offset;
}

/* ============================================================================
 * FRAME DECODING (PARSING)
 * ============================================================================ */

int sw_frame_decode(sw_frame_t *frame, uint8_t *buf, size_t buf_len) {
  if (!frame || !buf)
    return 0;

  /* Minimum frame: header (2) + CRC (2) */
  if (buf_len < 4)
    return 0;

  size_t offset = 0;

  /* Parse header */
  frame->target_addr = buf[offset++];
  frame->protocol_id = buf[offset++];

  /* Payload is everything except the CRC */
  size_t payload_start = offset;
  size_t payload_len = buf_len - 4;  /* Exclude 2 header + 2 CRC bytes */

  /* Verify CRC */
  uint16_t received_crc = ((uint16_t)buf[buf_len - 2] << 8) | buf[buf_len - 1];
  uint16_t calculated_crc = sw_crc16(buf, buf_len - 2);

  if (received_crc != calculated_crc)
    return 0;  /* CRC mismatch */

  /* Set payload pointer */
  frame->payload = &buf[payload_start];
  frame->payload_len = (uint16_t)payload_len;

  return 1;  /* Success */
}
