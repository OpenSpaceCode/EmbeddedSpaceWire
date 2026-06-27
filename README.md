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

Look at the example/


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
