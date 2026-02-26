/*
 * CCSDS Space Wire Protocol Implementation
 * Minimal, embedded-optimized implementation following:
 * - CCSDS 131.0-B-2 (Space Wire - Communication Protocol)
 * - ECSS-E-ST-50-53C (SpaceWire Protocol)
 */

#ifndef SPACEWIRE_H
#define SPACEWIRE_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * SPACEWIRE CHARACTER CODEC
 * Space Wire encodes 9-bit data (8 data bits + parity) using special characters
 * ============================================================================ */

/* Special characters in Space Wire */
#define SPACEWIRE_ESC   0x00   /* Escape character (triggers character mapping) */
#define SPACEWIRE_FCT   0x01   /* Flow Control Token */
#define SPACEWIRE_EOP   0x02   /* End Of Packet */
#define SPACEWIRE_EEP   0x03   /* End Of Error Packet */

/* Escaped character mappings (ESC followed by code) */
#define SPACEWIRE_ESC_ESC   0x00  /* Escaped ESC */
#define SPACEWIRE_ESC_FCT   0x01  /* Escaped FCT */
#define SPACEWIRE_ESC_EOP   0x02  /* Escaped EOP */
#define SPACEWIRE_ESC_EEP   0x03  /* Escaped EEP */

/* Parity types */
typedef enum {
  SW_PARITY_EVEN = 0,
  SW_PARITY_ODD = 1
} sw_parity_t;

/* Character codec result */
typedef enum {
  SW_CHAR_OK = 0,
  SW_CHAR_ESCAPE = 1,      /* Special character */
  SW_CHAR_FCT = 2,         /* Flow control token */
  SW_CHAR_EOP = 3,         /* End of packet */
  SW_CHAR_EEP = 4,         /* End of error packet */
  SW_CHAR_PARITY_ERROR = 5,
  SW_CHAR_INVALID = 6
} sw_char_result_t;

/* Decode single character (9-bit) from two bytes (8-bit + parity bit) */
sw_char_result_t sw_decode_char(uint8_t byte, uint8_t parity_bit, uint8_t *data);

/* Encode single character to byte + parity (returns parity bit) */
uint8_t sw_encode_char(uint8_t data, uint8_t *byte);

/* ============================================================================
 * SPACEWIRE FRAMING
 * Implements frame structure and error detection
 * ============================================================================ */

/* Frame structure:
 * - Header (1-4 bytes): Address path, Protocol ID
 * - Payload (0-255 bytes): CCSDS packet or raw data
 * - CRC (2 bytes): CRC-16-CCITT
 */

#define SW_FRAME_MAX_PAYLOAD 65535
#define SW_FRAME_HEADER_MAX  4
#define SW_FRAME_CRC_LEN     2

typedef struct {
  uint8_t target_addr;     /* Target logical address (0-254) */
  uint8_t protocol_id;     /* Protocol identifier (1=CCSDS, 2=Raw) */
  uint8_t *payload;        /* Non-const: decode path must pass to sp_packet_parse(uint8_t *) */
  uint16_t payload_len;
} sw_frame_t;

/* Initialize frame structure */
void sw_frame_init(sw_frame_t *frame);

/* Calculate required buffer size for frame (header + payload + CRC) */
size_t sw_frame_size(const sw_frame_t *frame);

/* Serialize frame into buffer with CRC calculation */
size_t sw_frame_encode(const sw_frame_t *frame, uint8_t *buf, size_t buf_len);

/* Parse frame from buffer and verify CRC */
int sw_frame_decode(sw_frame_t *frame, uint8_t *buf, size_t buf_len);

/* ============================================================================
 * SPACEWIRE ROUTER
 * Packet routing with virtual channel support
 * ============================================================================ */

#define SW_MAX_VIRTUAL_CHANNELS  16
#define SW_MAX_PORTS             8

/* Routing entry for packet forwarding */
typedef struct {
  uint8_t dest_addr;
  uint8_t output_port;
} sw_route_t;

/* Virtual channel (circuit) */
typedef struct {
  uint8_t channel_id;
  uint8_t active;
  uint16_t fct_credits;    /* Flow Control Credits */
} sw_virtual_channel_t;

/* Space Wire link state */
typedef enum {
  SW_LINK_UNINITIALIZED = 0,
  SW_LINK_READY = 1,
  SW_LINK_STARTED = 2,
  SW_LINK_CONNECTED = 3,
  SW_LINK_ERROR = 4
} sw_link_state_t;

typedef struct {
  uint8_t port_id;
  sw_link_state_t state;
  uint32_t tx_packets;
  uint32_t rx_packets;
  uint32_t errors;
} sw_link_t;

typedef struct {
  sw_link_t links[SW_MAX_PORTS];
  sw_virtual_channel_t channels[SW_MAX_VIRTUAL_CHANNELS];
  sw_route_t routes[SW_MAX_PORTS];
  uint8_t device_addr;
  uint8_t num_ports;
} sw_router_t;

/* Initialize router */
void sw_router_init(sw_router_t *router, uint8_t device_addr, uint8_t num_ports);

/* Configure routing entry */
void sw_router_add_route(sw_router_t *router, uint8_t dest_addr, uint8_t output_port);

/* Open virtual channel */
int sw_router_open_channel(sw_router_t *router, uint8_t channel_id);

/* Route frame to appropriate port */
int sw_router_route_frame(sw_router_t *router, const sw_frame_t *frame, uint8_t *output_port);

/* ============================================================================
 * SPACEWIRE LINK LAYER
 * Low-level link operations
 * ============================================================================ */

/* Link configuration */
typedef struct {
  uint32_t bit_rate;          /* bits per second */
  uint32_t disconnect_timeout; /* microseconds */
  uint8_t rx_credit_max;      /* max rx buffer credits */
  uint8_t enable_crc;         /* 1 = enable CRC, 0 = disable */
} sw_link_config_t;

typedef struct {
  sw_link_config_t config;
  sw_link_state_t state;
  uint8_t rx_credits;
  uint8_t tx_credits;
} sw_link_layer_t;

/* Initialize link layer */
void sw_link_init(sw_link_layer_t *link, const sw_link_config_t *config);

/* Check link state */
sw_link_state_t sw_link_get_state(const sw_link_layer_t *link);

/* Update link state */
void sw_link_set_state(sw_link_layer_t *link, sw_link_state_t state);

/* ============================================================================
 * CRC UTILITIES (shared with Space Packet)
 * ============================================================================ */

/* Compute CRC-16-CCITT (polynomial 0x1021, init 0xFFFF) */
uint16_t sw_crc16(const uint8_t *data, size_t len);

/* CRC table for faster computation */
extern const uint16_t sw_crc16_table[256];

#endif /* SPACEWIRE_H */
