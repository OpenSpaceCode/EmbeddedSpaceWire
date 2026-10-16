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
 * application that overrides all three hooks can leave the directory out of its
 * build; linked from the static library, the file is simply not pulled in.
 * See also: docs/space_packet_integration.md
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
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

SW_WEAK sw_status_t sw_payload_serialize(const void *payload, uint8_t *buf, uint32_t buf_len)
{
    const sw_payload_raw_t *raw = payload;

    if ((!raw) || (!raw->data) || (!buf))
    {
        return SW_INVALID_PARAM;
    }

    if (raw->len == 0u)
    {
        return SW_WRONG_LENGTH;
    }

    if (buf_len < raw->len)
    {
        return SW_BUFFER_TOO_SMALL;
    }

    memcpy(buf, raw->data, raw->len);

    return SW_OK;
}

SW_WEAK sw_status_t sw_payload_parse(void *payload, const uint8_t *buf, uint32_t buf_len)
{
    sw_payload_raw_t *raw = payload;

    if ((!raw) || (!buf))
    {
        return SW_INVALID_PARAM;
    }

    raw->data = buf;
    raw->len = buf_len;

    return SW_OK;
}
