# EmbeddedSpaceWire

Minimal, embedded-optimized implementation of **CCSDS Space Wire Protocol** combined with **CCSDS Space Packet Transfer** protocol, following standards.

## Standards Compliance

- **ECSS-E-ST-50-12C**: Space Wire Protocol Specification
- **ECSS-E-ST-50-53C**: SpaceWire – CCSDS packet transfer protocol
- **CCSDS 133.0-B-2**: CCSDS Space Packet

## Features

### Core Protocol Implementation

- **Network layer**: SpaceWire packets and routing — path (0–31) and logical
  (32–254) addressing with header deletion (ECSS-E-ST-50-12C §5.6)
- **CCSDS Packet Transfer Protocol**: encapsulation/extraction of CCSDS Space
  Packets with EOP/EEP receive status (ECSS-E-ST-50-53C)
- **CCSDS Integration**: built on the EmbeddedSpacePacket library

### Scope (hardware boundary)

This is a **packet- and network-layer** library. The character/signal and
data-link levels — character encoding and parity, data-strobe signalling, link
initialisation and flow control, and EOP/EEP generation and detection — are
provided by the **SpaceWire hardware CODEC**. EOP/EEP are exchanged with this
library as out-of-band metadata, not as bytes within packet buffers.

### Design Principles

- **Minimal footprint**: small static library, no dynamic allocation
- **Zero allocation**: stack- and caller-owned buffers only
- **Embedded-optimized**: no external dependencies except EmbeddedSpacePacket
- **Portable**: pure C99, big-endian network byte order
- **Standards-driven**: no non-standard framing or checksums on the wire

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
│   ├── cunit.h              # Tiny C test helpers
│   └── unit_tests.c         # Unit tests
├── tools/
│   └── coverage_html.sh     # HTML coverage generator
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
# Produces: build/lib/libspacewire.a (static) and build/lib/libspacewire.so (shared)
```

### Build Example

```bash
make example
./build/bin/spacewire_example
```

### Run Tests

```bash
make test
```

Test binary path: `build/bin/spacewire_tests`.

### Coverage (HTML)

```bash
make coverage-html
```

Coverage report is generated at `build/coverage/index.html`.

### Clean

```bash
make clean      # Remove build/ artifacts
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

> Note: the API reference below predates the Phase 1–3 refactor and is being
> updated; see the headers in `include/` for the current signatures.

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

## Memory Usage (Estimated)

- **Library (stripped)**: ~4-5 KB
- **Per-frame buffer**: Frame size + 4 bytes overhead
- **Router state**: ~200 bytes base + 16 bytes per port
- **Packet frame**: ~100 bytes (including CCSDS header)

## Limitations and Extensions

Current implementation focuses on core protocol features:

- Single-destination routing (no path routing)
- Flow control (basic credit-based)
- CRC validation
- No automatic retransmission handling
- No bandwidth management
- No advanced QoS features

These can be extended as needed for specific mission requirements.

## References

- CCSDS 131.0-B-2: Space Wire - Communication Protocol
- ECSS-E-ST-50-53C: European Cooperation for Space Standardization
- CCSDS 133.0-B-2: CCSDS Space Packet Protocol

## License

See LICENSE file.
