/**
 * @file    spacewire_spw_packet.c
 * @brief   SpaceWire packet builder
 *
 * Implements the SpaceWire packet as per ECSS-E-ST-50-12C Rev.1, clause 5.6.2:
 * [destination address][cargo] terminated by an EOP/EEP marker. The marker is a
 * link-layer control character and is not part of the packet buffer, and no
 * checksum is added.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "spacewire.h"

#include <string.h>

sw_status_t sw_spw_packet_build(const uint8_t *dest,
                                size_t dest_len,
                                const uint8_t *cargo,
                                size_t cargo_len,
                                uint8_t *buf,
                                size_t buf_len,
                                size_t *out_len)
{
    /* Either part may be empty, but a part with a length must have octets. */
    if ((!buf) || (!out_len) || ((dest_len > 0) && (!dest)) || ((cargo_len > 0) && (!cargo)))
    {
        return SW_INVALID_PARAM;
    }

    /* A packet carries at least one data character (clause 5.6.2.1). */
    if ((dest_len == 0) && (cargo_len == 0))
    {
        return SW_WRONG_LENGTH;
    }

    /* Compared part by part: dest_len + cargo_len can wrap and pass a size check. */
    if ((dest_len > buf_len) || (cargo_len > (buf_len - dest_len)))
    {
        return SW_BUFFER_TOO_SMALL;
    }

    const size_t total = dest_len + cargo_len;

    if (dest_len > 0)
    {
        memcpy(buf, dest, dest_len);
    }

    if (cargo_len > 0)
    {
        memcpy(&buf[dest_len], cargo, cargo_len);
    }

    *out_len = total;

    return SW_OK;
}
