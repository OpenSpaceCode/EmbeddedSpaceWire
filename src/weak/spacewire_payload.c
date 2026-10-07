/**
 * @file    spacewire_payload.c
 * @brief   Default payload adapter hooks: the payload is raw octets
 *
 * Weak implementations of the hooks declared in spacewire_payload.h. They make
 * the library usable on its own by treating whatever a SpaceWire packet
 * carries as an opaque octet string (::sw_payload_raw_t). An application
 * replaces them by defining the same functions; see the header for the rules.
 *
 * This is the only file in src/weak/. Everything here is replaceable, so an
 * application that overrides all four hooks can leave the directory out of its
 * build; linked from the static library, the file is simply not pulled in.
 * See also: docs/space_packet_integration.md
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "spacewire_payload.h"

#include <string.h>

SW_WEAK uint32_t sw_payload_size(const void *payload)
{
    const sw_payload_raw_t *raw = payload;

    if ((!raw) || (!raw->data))
    {
        return 0;
    }

    return raw->len;
}

SW_WEAK uint32_t sw_payload_serialize(const void *payload, uint8_t *buf, uint32_t buf_len)
{
    const sw_payload_raw_t *raw = payload;

    if ((!raw) || (!raw->data) || (!buf) || (raw->len == 0u) || (buf_len < raw->len))
    {
        return 0;
    }

    memcpy(buf, raw->data, raw->len);

    return raw->len;
}

SW_WEAK uint32_t sw_payload_length(const uint8_t *buf, uint32_t buf_len)
{
    if (!buf)
    {
        return 0;
    }

    return buf_len;
}

SW_WEAK int sw_payload_parse(void *payload, const uint8_t *buf, uint32_t buf_len)
{
    sw_payload_raw_t *raw = payload;

    if ((!raw) || (!buf) || (buf_len == 0u))
    {
        return 0;
    }

    raw->data = buf;
    raw->len = buf_len;

    return 1;
}
