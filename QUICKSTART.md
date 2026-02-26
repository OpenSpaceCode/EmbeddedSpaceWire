# EmbeddedSpaceWire - Quick Start Guide

## Quick start

```bash
make all

# run an example
./spacewire_example

# run tests (installs libgtest-dev automatically if missing)
./run_tests.sh

# or, if dependencies are already installed
make test

# clean
make distclean
```

## Installation

```bash
git clone https://github.com/OpenSpaceCode/EmbeddedSpaceWire.git
cd EmbeddedSpaceWire
make lib
```

Output:
- `libspacewire.a` - Static library (~15 KB)
- `libspacewire.so` - Shared library (~17 KB)

## Basic Usage

### 1. Simple Packet Creation

```c
#include "spacewire_packet.h"

int main() {
    uint8_t buffer[256];
    const char *data = "Hello Space Wire";
    
    // Create packet: local address 0x01 → target 0x02, APID 0x42
    size_t size = sw_packet_create(
        0x01,                      // Source (this device)
        0x02,                      // Destination
        0x0042,                    // Application ID
        (uint8_t *)data,           // Payload
        strlen(data),              // Length
        buffer,                    // Output buffer
        sizeof(buffer)             // Buffer size
    );
    
    if (size > 0) {
        printf("Packet created: %zu bytes\n", size);
        // Send buffer via UART/SPI/Fiber
        send_packet(buffer, size);
    }
    
    return 0;
}
```

### 2. Packet Reception

```c
uint8_t rx_buffer[256];
size_t rx_size = receive_packet(rx_buffer, sizeof(rx_buffer));

if (rx_size > 0) {
    sw_packet_frame_t pf;
    
    if (sw_packet_decode(&pf, rx_buffer, rx_size)) {
        printf("Received APID: 0x%04X\n", pf.packet.ph.apid);
        printf("Payload: %.*s\n", pf.packet.payload_len, 
               (char *)pf.packet.payload);
    }
}
```

### 3. Manual Frame Construction

```c
sw_packet_config_t config = {
    .device_addr = 0x01,
    .target_addr = 0x02,
    .protocol_id = 1,  // 1 = CCSDS
    .enable_crc = 1
};

sw_packet_frame_t pf;
sw_packet_init(&pf, &config);

// Set CCSDS packet fields
pf.packet.ph.version = 0;
pf.packet.ph.type = 1;        // Telemetry
pf.packet.ph.apid = 0x0100;
pf.packet.ph.seq_count = 1;
pf.packet.payload = my_data;
pf.packet.payload_len = my_data_len;

// Encode
uint8_t buffer[512];
size_t size = sw_packet_encode(&pf, buffer, sizeof(buffer));
```

### 4. Routing

```c
// Initialize router
sw_router_t router;
sw_router_init(&router, 0x01, 3);  // This device is 0x01, has 3 ports

// Configure routing table
sw_router_add_route(&router, 0x02, 0);  // 0x02 → Port 0
sw_router_add_route(&router, 0x03, 1);  // 0x03 → Port 1
sw_router_add_route(&router, 0x04, 2);  // 0x04 → Port 2

// Simulate link connection
router.links[0].state = SW_LINK_CONNECTED;
router.links[1].state = SW_LINK_CONNECTED;
router.links[2].state = SW_LINK_CONNECTED;

// On reception, route packet
uint8_t out_port;
if (sw_router_route_frame(&router, &received_frame, &out_port)) {
    printf("Route to port: %u\n", out_port);
    uart_send(port_handles[out_port], buffer, size);
}
```

### 5. Low-Level Frame Operations

```c
// Create frame directly
sw_frame_t frame;
sw_frame_init(&frame);
frame.target_addr = 0x02;
frame.protocol_id = 1;
frame.payload = my_data;
frame.payload_len = data_len;

// Encode frame
uint8_t buffer[256];
size_t size = sw_frame_encode(&frame, buffer, sizeof(buffer));

// Decode and verify
sw_frame_t decoded;
if (sw_frame_decode(&decoded, buffer, size)) {
    printf("Frame valid, target: 0x%02X\n", decoded.target_addr);
}
```

## Compilation

### Link with Your Project

**Static Library**:
```bash
gcc -I/path/to/EmbeddedSpaceWire/include \
    -c your_app.c -o your_app.o
gcc your_app.o -L/path/to/EmbeddedSpaceWire \
    -lspacewire -o your_app
```

**Shared Library**:
```bash
gcc -I/path/to/EmbeddedSpaceWire/include \
    your_app.c -L/path/to/EmbeddedSpaceWire \
    -lspacewire -o your_app

# Run with LD_LIBRARY_PATH
LD_LIBRARY_PATH=/path/to/EmbeddedSpaceWire ./your_app
```

## Examples

### Run Built-in Example
```bash
make example
./spacewire_example
```

Output shows:
- Character codec test
- Frame encoding/decoding
- CRC validation
- CCSDS integration
- Router configuration
- Statistics

### Run Tests
```bash
./run_tests.sh
```

The script installs `libgtest-dev` and `g++` via `apt` if they are not already present, initialises the `EmbeddedSpacePacket` submodule, and runs `make test`.

All 11 tests should pass:
- Character codec (encode/decode, parity, error detection)
- Frame layer (encode/decode, CRC validation, size calculation)
- Router (initialization, routing logic)
- Packet integration (encode/decode, convenience functions)

## API Overview

### Packet-Level API (Recommended)

```c
// High-level - handles everything
size_t sw_packet_create(uint8_t device_addr, uint8_t target_addr,
                        uint16_t apid, const uint8_t *payload,
                        uint16_t payload_len, uint8_t *buf,
                        size_t buf_len);

// Structured approach
void sw_packet_init(sw_packet_frame_t *pf, const sw_packet_config_t *cfg);
size_t sw_packet_encode(const sw_packet_frame_t *pf, uint8_t *buf, size_t len);
int sw_packet_decode(sw_packet_frame_t *pf, uint8_t *buf, size_t len);
```

### Frame-Level API

```c
void sw_frame_init(sw_frame_t *frame);
size_t sw_frame_encode(const sw_frame_t *frame, uint8_t *buf, size_t len);
int sw_frame_decode(sw_frame_t *frame, uint8_t *buf, size_t len);
```

### Router API

```c
void sw_router_init(sw_router_t *router, uint8_t addr, uint8_t num_ports);
void sw_router_add_route(sw_router_t *router, uint8_t dest, uint8_t port);
int sw_router_route_frame(sw_router_t *router, const sw_frame_t *frame,
                          uint8_t *out_port);
```

### Low-Level API

```c
// Character codec
sw_char_result_t sw_decode_char(uint8_t byte, uint8_t parity, uint8_t *data);
uint8_t sw_encode_char(uint8_t data, uint8_t *byte);

// CRC
uint16_t sw_crc16(const uint8_t *data, size_t len);

// Statistics
void sw_get_statistics(sw_statistics_t *stats);
void sw_reset_statistics(void);
```

## Common Issues

### Compilation Errors
- Missing include path: Use `-I./include`
- Linking errors: Use `-lspacewire` with correct `-L` path
- C99 requirement: Use `-std=c99` or newer

### Runtime Issues
- Buffer overflow: Ensure buffer ≥ payload + 14 bytes
- CRC mismatch: Check data corruption in transmission
- Routing failure: Verify link states are `SW_LINK_CONNECTED`

## Performance Notes

- **Packet creation**: ~5 microseconds (mostly CRC)
- **Memory footprint**: ~9 KB library + ~500 bytes runtime state
- **Maximum payload**: 65535 bytes per frame
- **Maximum devices**: 256 addresses (0x00-0xFF)
- **Maximum ports**: 8 (per router)
- **Virtual channels**: 16 (per router)

## Next Steps

1. Read [ARCHITECTURE.md](ARCHITECTURE.md) for detailed protocol info
2. Check [README.md](README.md) for standards references
3. Review examples in `examples/main.c`
4. Look at tests in `tests/unit_tests.cpp`
5. Integrate with your physical link layer

## Support

For issues, questions, or contributions:
- GitHub: https://github.com/OpenSpaceCode/EmbeddedSpaceWire
- Standards: See README.md for CCSDS/ECSS references
