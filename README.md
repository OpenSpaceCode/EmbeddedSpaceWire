# EmbeddedSpaceWire

Minimal, embedded-optimized implementation of the **SpaceWire** packet and network layers (ECSS-E-ST-50-12C) carrying the **CCSDS Packet Transfer Protocol** (ECSS-E-ST-50-53C).

## Standards Compliance

- **ECSS-E-ST-50-12C** Rev.1: SpaceWire — links, nodes, routers and networks
- **ECSS-E-ST-50-53C**: SpaceWire — CCSDS packet transfer protocol
- **ECSS-E-ST-50-51C**: SpaceWire — protocol identification (Protocol ID `0x02`)
- **CCSDS 133.0-B-2**: Space Packet Protocol — the packet being carried; supplied by
  the application through the payload adapter, not implemented here

## Features

### Core Protocol Implementation

- **Network layer**: SpaceWire packets and routing — path (0–31) and logical
  (32–254) addressing with header deletion (ECSS-E-ST-50-12C §5.6.8)
- **CCSDS Packet Transfer Protocol**: encapsulation/extraction of the carried
  packet with EOP/EEP receive status (ECSS-E-ST-50-53C)
- **Payload adapter**: the carried packet is reached through four overridable
  hooks, so the application plugs in its own Space Packet implementation

### Scope (hardware boundary)

This is a **packet- and network-layer** library. The character/signal and
data-link levels — character encoding and parity, data-strobe signalling, link
initialisation and flow control, and EOP/EEP generation and detection — are
provided by the **SpaceWire hardware CODEC**. EOP/EEP are exchanged with this
library as out-of-band metadata, not as bytes within packet buffers.

### Design Principles

- **Minimal footprint**: small static library, no dynamic allocation
- **Zero allocation**: stack- and caller-owned buffers only
- **Standalone**: no external dependencies
- **Portable**: pure C99, big-endian network byte order
- **Standards-driven**: no non-standard framing or checksums on the wire

## Project Structure

```
EmbeddedSpaceWire/
├── include/
│   ├── spacewire.h              # SpaceWire packet + network (routing) layer
│   ├── spacewire_packet.h       # CCSDS packet transfer protocol (ECSS-E-ST-50-53C)
│   └── spacewire_payload.h      # Payload adapter hooks
├── src/
│   ├── spacewire_spw_packet.c   # SpaceWire packet builder
│   ├── spacewire_router.c       # Routing switch
│   ├── spacewire_packet.c       # CCSDS packet transfer protocol
│   └── weak/
│       └── spacewire_payload.c  # Default (raw-octet) payload hooks, replaceable
├── docs/
│   └── space_packet_integration.md  # Carrying CCSDS Space Packets
├── examples/
│   └── main.c                   # End-to-end usage
├── tests/
│   ├── cunit.h                  # Tiny C test helpers
│   ├── test_runners.h           # Suite runner declarations
│   ├── test_spw_packet.c        # Packet-builder tests
│   ├── test_router.c            # Routing tests
│   ├── test_packet.c            # CCSDS PTP tests (+ golden wire vector)
│   ├── test_payload.c           # Default payload hook tests
│   ├── test_payload_override.c  # Overridden payload hooks (own binary)
│   └── unit_tests.c             # Test runner
├── tools/
│   └── coverage_html.sh         # HTML coverage report + 100% gate
├── .github/workflows/
│   └── ci.yml                   # CI: tests, coverage, sanitizers, format
├── .clang-format                # Code style
├── Makefile
└── README.md
```

## Building

### Requirements

- A C99 compiler. The build must be warning-free under both **gcc** and
  **clang**; CI checks both.
- `make` and `ar`
- Optional: `gcovr` for coverage, `clang-format` for the format check

Pick the compiler with `CC`:

```bash
make CC=gcc
make CC=clang
```

The Makefile builds with `-Werror` and a strict warning set (`-Wall -Wextra
-Wpedantic -Wconversion -Wshadow` and others), so any warning fails the build.
`OPT` sets the optimisation flags (default `-O2`), e.g. `make OPT="-O0 -g"`.

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

Two binaries are built and run: `build/bin/spacewire_tests` uses the default
payload hooks, and `build/bin/spacewire_override_tests` replaces them with its
own.

### Sanitizers

```bash
make sanitize
make sanitize CC=clang
```

Rebuilds the library, both test binaries and the example with AddressSanitizer
and UndefinedBehaviorSanitizer (leak detection on, every report fatal), runs
them, and prints one result line per binary. Program output is shown only on
failure. The instrumented build goes to `build/sanitize/` and is removed
afterwards.

### Coverage (HTML)

```bash
make coverage-html
```

Requires `gcovr`. Runs `make clean` first, so any existing `build/` output is
removed. It then builds instrumented tests in `build/coverage-build/`, runs
both test binaries, and writes the report to `build/coverage/index.html`.
Coverage of `src/` must be **100% line and 100% branch**; below that the
command still writes the report, then exits non-zero.

### Formatting

Code style is defined in [.clang-format](.clang-format). Check every C file
with:

```bash
git ls-files '*.c' '*.h' | xargs clang-format --dry-run --Werror
```

Drop `--dry-run --Werror` and add `-i` to reformat in place.

### Continuous Integration

[.github/workflows/ci.yml](.github/workflows/ci.yml) runs on every push to
`main` and on every pull request:

| Job | What it checks |
|---|---|
| Unit tests | `make` with gcc and with clang: libraries, both test binaries, example |
| Coverage | `make coverage-html`; fails below 100% line or branch; report uploaded as an artifact |
| Sanitizers | `make sanitize` with gcc and with clang |
| clang-format | Every tracked `.c` and `.h` file matches `.clang-format` |

Run the same checks locally before pushing.

### Clean

```bash
make clean      # Remove build/ artifacts
make distclean  # Deep clean including object files
```

## Quick Start

See [examples/main.c](examples/main.c).

## Payload Adapter

The packet transfer layer never looks inside the packet it carries. It sizes,
serialises, measures and parses it through four hooks declared in
[include/spacewire_payload.h](include/spacewire_payload.h):

| Hook | Purpose |
|---|---|
| `sw_payload_size()` | Serialised size of a payload object |
| `sw_payload_serialize()` | Write a payload object into the output buffer |
| `sw_payload_length()` | Length a received payload declares for itself |
| `sw_payload_parse()` | Fill a payload object from a received buffer |

The library ships weak default hooks in `src/weak/` that treat the payload as
raw octets (`sw_payload_raw_t`), so it works on its own. To carry a structured
protocol, define the same four functions in the application; they replace the
defaults at link time. An application that compiles the library sources itself
and overrides all four hooks can leave `src/weak/` out of its build.

The overrides must be linked as an object file. Overrides that exist only
inside a static library are silently ignored by the linker.

[docs/space_packet_integration.md](docs/space_packet_integration.md) shows the
overrides for CCSDS Space Packets using EmbeddedSpacePacket.

## Memory Usage

Figures for x86-64:

- **Library code**: ~2.2 KB (`.text`); no dynamic allocation, all buffers caller-owned
- **`sw_packet_frame_t`**: 16 bytes (CCSDS PTP addressing fields)
- **`sw_router_t`**: 784 bytes — a 256-entry routing table (3 B/entry) plus
  counters

## Thread Safety

Packet and routing operations work entirely on caller-owned state passed
by pointer and take no internal locks, so independent objects can be used from
independent tasks. The one shared item is the library's statistics block:
`sw_packet_encode` and `sw_packet_decode` update a process-global counter (read
via `sw_packet_get_statistics`, cleared via `sw_packet_reset_statistics`), which is **not**
thread-safe. In a multi-task system, serialise the encode/decode/statistics
calls or confine them to a single task.

## Limitations and Extensions

The library implements the packet and network layers; the following are out of
scope or not yet implemented:

- Character/signal and data-link levels — provided by the SpaceWire hardware CODEC
- Building and validating the carried CCSDS Space Packet — provided by the
  application through the payload adapter
- Time-codes, broadcast codes and distributed interrupts (ECSS-E-ST-50-12C §5.6.3–5.6.5)
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
