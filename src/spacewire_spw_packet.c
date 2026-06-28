/**
 * @file spacewire_spw_packet.c
 * @brief SpaceWire packet builder (ECSS-E-ST-50-12C clause 5.6.2).
 *
 * A SpaceWire packet is [destination address][cargo] terminated by an EOP/EEP
 * marker. The marker is a link-layer control character and is not part of the
 * packet buffer, and no checksum is added.
 */

#include "../include/spacewire.h"

#include <string.h>

size_t sw_spw_packet_build(const uint8_t *dest,
                           size_t dest_len,
                           const uint8_t *cargo,
                           size_t cargo_len,
                           uint8_t *buf,
                           size_t buf_len)
{
    if (!buf)
        return 0;

    if (dest_len > 0 && !dest)
        return 0;

    if (cargo_len > 0 && !cargo)
        return 0;

    const size_t total = dest_len + cargo_len;
    if (total == 0 || total > buf_len)
        return 0;

    if (dest_len > 0)
        memcpy(buf, dest, dest_len);

    if (cargo_len > 0)
        memcpy(&buf[dest_len], cargo, cargo_len);

    return total;
}
