# EmbeddedSpaceWire

Minimal, embedded-optimized implementation of the **SpaceWire** packet and network layers (ECSS-E-ST-50-12C) carrying the **CCSDS Packet Transfer Protocol** (ECSS-E-ST-50-53C).

## Standards Compliance

- **ECSS-E-ST-50-12C** Rev.1: SpaceWire — links, nodes, routers and networks
- **ECSS-E-ST-50-53C**: SpaceWire — CCSDS packet transfer protocol
- **ECSS-E-ST-50-51C**: SpaceWire — protocol identification (Protocol ID `0x02`)
- **CCSDS 133.0-B-2**: Space Packet Protocol

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
│   ├── spacewire.h          # SpaceWire packet + network (routing) layer
│   └── spacewire_packet.h   # CCSDS packet transfer protocol (ECSS-E-ST-50-53C)
├── src/
│   ├── spacewire_frame.c    # SpaceWire packet builder
│   ├── spacewire_router.c   # Routing switch + link state
│   └── spacewire_packet.c   # CCSDS packet transfer protocol
├── external/
│   └── EmbeddedSpacePacket/ # CCSDS Space Packet library (submodule)
├── examples/
│   └── main.c               # End-to-end usage
├── tests/
│   ├── cunit.h              # Tiny C test helpers
│   ├── test_frame.c         # Packet-builder tests
│   ├── test_router.c        # Routing + link tests
│   ├── test_packet.c        # CCSDS PTP tests (+ golden wire vector)
│   └── unit_tests.c         # Test runner
├── tools/
│   └── coverage_html.sh     # HTML coverage generator
├── Makefile
└── README.md
```

## Building

### Prerequisites

This repository vendors the CCSDS Space Packet library as a git submodule.
After cloning, initialise it:

```bash
git submodule update --init --recursive
```

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

See [examples/main.c](examples/main.c).


## Memory Usage

- **Library code**: ~3 KB (`.text`); no dynamic allocation, all buffers caller-owned
- **`sw_packet_frame_t`**: 40 bytes (CCSDS PTP packet state)
- **`sw_router_t`**: ~1.4 KB — a 256-entry routing table (3 B/entry) plus per-port
  link state; set `-DSW_NUM_PORTS=n` to shrink the per-router footprint

## Thread Safety

Packet, routing and link operations work entirely on caller-owned state passed
by pointer and take no internal locks, so independent objects can be used from
independent tasks. The one shared item is the library's statistics block:
`sw_packet_encode` and `sw_packet_decode` update a process-global counter (read
via `sw_get_statistics`, cleared via `sw_reset_statistics`), which is **not**
thread-safe. In a multi-task system, serialise the encode/decode/statistics
calls or confine them to a single task.

## Limitations and Extensions

The library implements the packet and network layers; the following are out of
scope or not yet implemented:

- Character/signal and data-link levels — provided by the SpaceWire hardware CODEC
- Time-codes, broadcast codes and distributed interrupts (ECSS-E-ST-50-12C §5.6)
- Group adaptive routing (one output port per logical address)
- Guaranteed delivery — per ECSS-E-ST-50-53C the service is unconfirmed and
  incomplete (no acknowledgement, retransmission or QoS)

These can be added as needed for specific mission requirements.

## References

- ECSS-E-ST-50-12C Rev.1 — SpaceWire: links, nodes, routers and networks
- ECSS-E-ST-50-53C — SpaceWire: CCSDS packet transfer protocol
- ECSS-E-ST-50-51C — SpaceWire: protocol identification
- CCSDS 133.0-B-2 — Space Packet Protocol
- https://www.spacewire.esa.int — SpaceWire website

## License

See LICENSE file.
