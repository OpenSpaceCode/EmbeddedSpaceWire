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

size_t sw_spw_packet_build(const uint8_t *dest,
                           size_t dest_len,
                           const uint8_t *cargo,
                           size_t cargo_len,
                           uint8_t *buf,
                           size_t buf_len)
{
    if (!buf)
    {
        return 0;
    }

    if ((dest_len > 0) && (!dest))
    {
        return 0;
    }

    if ((cargo_len > 0) && (!cargo))
    {
        return 0;
    }

    /* Compared part by part: dest_len + cargo_len can wrap and pass a size check. */
    if ((dest_len > buf_len) || (cargo_len > (buf_len - dest_len)))
    {
        return 0;
    }

    /* An empty packet needs no check of its own: nothing is copied and the result,
     * 0, is the error value. */
    const size_t total = dest_len + cargo_len;

    if (dest_len > 0)
    {
        memcpy(buf, dest, dest_len);
    }

    if (cargo_len > 0)
    {
        memcpy(&buf[dest_len], cargo, cargo_len);
    }

    return total;
}
