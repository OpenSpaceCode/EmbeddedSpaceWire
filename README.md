# EmbeddedSpaceWire

Minimal, embedded-optimized implementation of **CCSDS Space Wire Protocol** combined with **CCSDS Space Packet Transfer** protocol, following international standards.

## Standards Compliance

- **CCSDS 131.0-B-2**: Space Wire - Communication Protocol
- **ECSS-E-ST-50-53C**: Space Wire Protocol Specification
- **CCSDS 133.0-B-2**: CCSDS Space Packet

## Features

### Core Protocol Implementation

- ✅ **Character Codec**: 9-bit character encoding with parity support
- ✅ **Frame Layer**: Space Wire frame structure with CRC-16-CCITT
- ✅ **Router**: Packet routing with virtual channel support
- ✅ **Link Layer**: Link state management and flow control
- ✅ **CCSDS Integration**: Combined Space Wire + Space Packet transmission

### Design Principles

- **Minimal footprint**: ~5KB library size (stripped)
- **Zero allocation**: Stack-based, no dynamic memory
- **Embedded-optimized**: No external dependencies except EmbeddedSpacePacket
- **Portable**: Pure C99, big-endian network byte order
- **Fast**: Lookup-table CRC computation

## Project Structure

```
EmbeddedSpaceWire/
├── include/
│   ├── spacewire.h          # Core Space Wire protocol
│   └── spacewire_packet.h   # CCSDS integration
├── src/
│   ├── spacewire_codec.c    # Character codec + CRC
│   ├── spacewire_frame.c    # Frame layer
│   ├── spacewire_router.c   # Router + link layer
│   └── spacewire_packet.c   # CCSDS integration
├── external/
│   └── EmbeddedSpacePacket/ # CCSDS Space Packet library
├── examples/
│   └── main.c               # Example usage
├── tests/
│   └── unit_tests.c         # Unit tests
├── Makefile
└── README.md
```

## Building

### Build Everything

```bash
make all
```

### Build Library Only

```bash
make lib
# Produces: libspacewire.a (static) and libspacewire.so (shared)
```

### Build Example

```bash
make example
./spacewire_example
```

### Run Tests

```bash
make test
```

### Clean

```bash
make clean      # Remove build artifacts
make distclean  # Deep clean including object files
```

## Quick Start

### Create and Send a Space Wire Packet

```c
#include "spacewire.h"
#include "spacewire_packet.h"

// Create a CCSDS packet wrapped in Space Wire frame
uint8_t buffer[256];
const char *data = "Hello Space Wire";

size_t frame_size = sw_packet_create(
    0x01,                      // Local device address
    0x02,                      // Target device address
    0x0042,                    // APID (Application ID)
    (uint8_t *)data,           // Payload
    strlen(data),              // Payload length
    buffer,                    // Output buffer
    sizeof(buffer)             // Buffer size
);

if (frame_size > 0) {
    printf("Packet created: %zu bytes\n", frame_size);
}
```

### Parse Incoming Packet

```c
sw_packet_frame_t pf;
sw_packet_config_t config = {
    .device_addr = 0x01,
    .target_addr = 0x02,
    .protocol_id = 1,
    .enable_crc = 1
};

sw_packet_init(&pf, &config);

if (sw_packet_decode(&pf, buffer, frame_size)) {
    printf("APID: 0x%04X, Payload: %d bytes\n",
           pf.packet.ph.apid,
           pf.packet.payload_len);
}
```

### Frame Routing

```c
sw_router_t router;
sw_router_init(&router, 0x01, 2);  // Device 0x01, 2 ports

// Add routing rule: packets to 0x03 go out port 1
sw_router_add_route(&router, 0x03, 1);

// Route a frame
uint8_t out_port;
if (sw_router_route_frame(&router, &frame, &out_port)) {
    printf("Route to port: %u\n", out_port);
}
```

## API Reference

### Character Codec

```c
// Decode 9-bit character
sw_char_result_t sw_decode_char(uint8_t byte, uint8_t parity_bit, uint8_t *data);

// Encode character with parity
uint8_t sw_encode_char(uint8_t data, uint8_t *byte);

// Compute CRC
uint16_t sw_crc16(const uint8_t *data, size_t len);
```

### Frame Layer

```c
void sw_frame_init(sw_frame_t *frame);
size_t sw_frame_size(const sw_frame_t *frame);
size_t sw_frame_encode(const sw_frame_t *frame, uint8_t *buf, size_t buf_len);
int sw_frame_decode(sw_frame_t *frame, uint8_t *buf, size_t buf_len);
```

### Router

```c
void sw_router_init(sw_router_t *router, uint8_t device_addr, uint8_t num_ports);
void sw_router_add_route(sw_router_t *router, uint8_t dest_addr, uint8_t output_port);
int sw_router_open_channel(sw_router_t *router, uint8_t channel_id);
int sw_router_route_frame(sw_router_t *router, const sw_frame_t *frame, uint8_t *output_port);
```

### Packet Integration

```c
void sw_packet_init(sw_packet_frame_t *pf, const sw_packet_config_t *config);
size_t sw_packet_encode(const sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len);
int sw_packet_decode(sw_packet_frame_t *pf, uint8_t *buf, size_t buf_len);
size_t sw_packet_create(uint8_t device_addr, uint8_t target_addr, uint16_t apid,
                        const uint8_t *payload, uint16_t payload_len,
                        uint8_t *buf, size_t buf_len);
```

## Protocol Stack

```
┌─────────────────────────────────────┐
│   CCSDS Space Packet (6+ bytes)     │  Primary/Secondary headers + payload
├─────────────────────────────────────┤
│   Space Wire Frame (4+ bytes)       │  Address, Protocol ID, CRC
├─────────────────────────────────────┤
│   Character Codec (9-bit)           │  Parity encoding
├─────────────────────────────────────┤
│   Physical Link (serial, fiber)     │  Bit transmission
└─────────────────────────────────────┘
```

## Memory Usage (Estimated)

- **Library (stripped)**: ~4-5 KB
- **Per-frame buffer**: Frame size + 4 bytes overhead
- **Router state**: ~200 bytes base + 16 bytes per port
- **Packet frame**: ~100 bytes (including CCSDS header)

## Limitations and Extensions

Current implementation focuses on core protocol features:

- ✅ Single-destination routing (no path routing)
- ✅ Flow control (basic credit-based)
- ✅ CRC validation
- ⚠️ No automatic retransmission handling
- ⚠️ No bandwidth management
- ⚠️ No advanced QoS features

These can be extended as needed for specific mission requirements.

## References

- CCSDS 131.0-B-2: Space Wire - Communication Protocol
- ECSS-E-ST-50-53C: European Cooperation for Space Standardization
- CCSDS 133.0-B-2: CCSDS Space Packet Protocol

## License

See LICENSE file.
