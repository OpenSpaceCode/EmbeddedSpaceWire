# Carrying CCSDS Space Packets

EmbeddedSpaceWire contains no Space Packet code. Its packet transfer layer
(ECSS-E-ST-50-53C) reaches the packet it carries through three hooks declared in
[`include/spacewire_payload.h`](../include/spacewire_payload.h). The library's
default hooks treat that packet as raw octets. This guide replaces them so that
`sw_ptp_encode()` and `sw_ptp_decode()` work directly on `sp_packet_t`
objects from [EmbeddedSpacePacket](https://github.com/OpenSpaceCode/EmbeddedSpacePacket).

The code below was built and run against EmbeddedSpacePacket v2.0.0 with gcc
and clang.

## 1. Add the overrides to your application

Create one source file in the application, for example
`sw_payload_space_packet.c`:

```c
#include "space_packet.h"
#include "spacewire_payload.h"

uint32_t sw_payload_size(const void *payload)
{
    return sp_packet_serialize_size(payload);
}

uint32_t sw_payload_serialize(const void *payload, uint8_t *buf, uint32_t buf_len)
{
    return sp_packet_serialize(payload, buf, buf_len);
}

int sw_payload_parse(void *payload, const uint8_t *buf, uint32_t buf_len)
{
    return sp_packet_parse(payload, buf, buf_len);
}
```

Define all three. Together they decide what type sits behind the `void *`
payload object; a mix of overridden and default hooks would disagree about it.

## 2. Link the overrides as an object file

```sh
cc -Iinclude -Ipath/to/EmbeddedSpacePacket/include \
   app.c sw_payload_space_packet.c path/to/EmbeddedSpacePacket/src/space_packet.c \
   build/lib/libspacewire.a -o app
```

The overrides must reach the linker as an object file (or a source file on the
compiler command line). If they exist only inside a static library, the linker
keeps the weak defaults without any warning, and the defaults then misread
your `sp_packet_t` as raw octets.

To check which definitions were linked:

```sh
nm app | grep sw_payload_
```

`T` marks your overrides; `W` means the weak defaults are still in use.

If you compile the library sources into the application instead of linking
`libspacewire.a`, you can leave `src/weak/` out of the build altogether: it
holds nothing but the default hooks.

## 3. Pass `sp_packet_t` as the payload object

```c
#include "space_packet.h"
#include "spacewire_ptp.h"

static const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};

sp_packet_t pkt;
sp_packet_init(&pkt);
sp_set_primary_header(&pkt, SP_PACKET_TYPE_TC, 0, 0x042, SP_SEQ_FLAG_UNSEGMENTED, 1);
sp_set_data(&pkt, data, sizeof(data));

const sw_ptp_frame_t frame = {.path = NULL,
                                 .path_len = 0,
                                 .logical_addr = SW_PTP_LOGICAL_ADDR_DEFAULT,
                                 .user_app = 0x00};
uint8_t wire[64];
const uint32_t n = sw_ptp_encode(&frame, &pkt, wire, sizeof(wire));
/* wire: FE 02 00 00 | 10 42 C0 01 00 03 | DE AD BE EF */

sw_ptp_frame_t rx_frame;
sp_packet_t rx_pkt;
sw_ptp_status_t status;
if (sw_ptp_decode(&rx_frame, &rx_pkt, wire, n, SW_END_EOP, &status) == SW_OK)
{
    /* rx_pkt.ph.apid == 0x042; rx_pkt.data points into wire (zero-copy). */
}
```

`rx_pkt.data` points into `wire`, so keep the receive buffer alive for as long
as the decoded packet is in use.

## What the receive path rejects

The packet transfer layer checks only the encapsulation header: end marker,
Protocol Identifier and Reserved field (clause 5.5.4). It does not check the
carried packet's length or format (clauses 4.3 and 5.5.4.2 a); every octet after
the header goes to `sw_payload_parse()`.

With these overrides, `sw_ptp_decode()` therefore returns `SW_ERR` with status
`SW_PTP_STATUS_INVALID`, and leaves `rx_frame` and `rx_pkt` unchanged, when:

| Input | Detected by |
|---|---|
| Fewer than 4 octets (no complete header) | the packet transfer layer |
| Protocol Identifier other than 0x02 | the packet transfer layer (clause 5.5.4.1) |
| Fewer than 6 octets after the header | `sp_packet_parse()` |
| Packet Version Number other than 0 | `sp_packet_parse()` |
| Space Packet cut short | `sp_packet_parse()` |

Octets left over after the Space Packet are not rejected: `sp_packet_parse()`
reads the length the packet declares and ignores the rest.

## Contract for any override

The same rules apply if you carry a protocol other than Space Packets:

- Report errors through the return value: 0 from every hook means failure.
- Leave outputs unchanged on failure. `sw_ptp_encode()` and
  `sw_ptp_decode()` rely on this to keep their own buffers and objects
  untouched when they fail.
- `sw_payload_serialize()` must return exactly what `sw_payload_size()` returns
  for the same object.
- `sw_payload_parse()` receives every octet after the header, unchecked. It must
  not read past `buf_len`, and returns 0 if it cannot extract a packet.
