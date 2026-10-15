/**
 * @file    spacewire_payload.h
 * @brief   Payload adapter between SpaceWire and the protocol it carries
 *
 * The SpaceWire packet transfer layer (ECSS-E-ST-50-53C, see spacewire_ptp.h)
 * never interprets the packet it carries. It reaches it only through the three
 * sw_payload_*() hooks declared here, which keeps this library independent of
 * any particular upper-layer protocol implementation.
 *
 * The library ships weak default hooks (src/weak/spacewire_payload.c) that treat the
 * payload as raw octets described by ::sw_payload_raw_t. An application that
 * carries a structured protocol, for example CCSDS Space Packets
 * (CCSDS 133.0-B-2), defines the same three functions itself; its definitions
 * replace the defaults at link time.
 *
 * Rules for an override:
 * - Define all three hooks, so they agree on the type behind the payload object.
 * - Do not mark the definitions weak; this header deliberately declares the
 *   hooks without ::SW_WEAK.
 * - Link the overrides as an object file. A linker does not pull an archive
 *   member to replace a symbol that is already defined, so overrides that exist
 *   only inside a static library are silently ignored.
 * - Honour the contract documented on each hook: report errors through the
 *   return value and leave outputs unchanged on failure.
 *
 * See also: docs/space_packet_integration.md
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef SPACEWIRE_PAYLOAD_H
#define SPACEWIRE_PAYLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#ifndef SW_WEAK
#    if (defined(__GNUC__)) || (defined(__clang__))
/**
 * @brief Marks a function definition as weak, so another definition can replace it at link time.
 *
 * Weak symbols are not part of ISO C99. Toolchains that are not GNU-compatible
 * must supply their own spelling at build time, e.g. `-DSW_WEAK=__weak` for IAR
 * and Keil.
 */
#        define SW_WEAK __attribute__((weak))
#    else
#        error "SW_WEAK: unknown compiler - define SW_WEAK to its weak-symbol attribute"
#    endif
#endif

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

/**
 * @brief Raw-octet payload object understood by the default hooks.
 *
 * @note @p data points into caller-owned memory; the library neither copies nor
 *       frees it. After the default sw_payload_parse() it points into the
 *       parsed buffer (zero-copy), which must outlive this object.
 */
typedef struct
{
    const uint8_t *data; /**< Payload octets. */
    uint32_t len;        /**< Number of payload octets. */
} sw_payload_raw_t;

/* -------------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */

/**
 * @brief Return the serialised size of a payload object.
 *
 * Default: @p payload is a ::sw_payload_raw_t and the result is its length.
 *
 * @param[in] payload Payload object; its type is defined by the hook implementation.
 * @return Size in octets, or 0 if @p payload cannot be serialised (default:
 *         NULL object, NULL data or zero length).
 */
uint32_t sw_payload_size(const void *payload);

/**
 * @brief Serialise a payload object into a caller-supplied buffer.
 *
 * Default: copies the octets of a ::sw_payload_raw_t. On failure, @p buf is
 * left unchanged.
 *
 * @param[in]  payload Payload object; its type is defined by the hook implementation.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in octets.
 * @return Octets written, equal to sw_payload_size() for the same object, or 0
 *         on error (default: NULL args, zero length or buffer too small).
 */
uint32_t sw_payload_serialize(const void *payload, uint8_t *buf, uint32_t buf_len);

/**
 * @brief Parse a serialised payload into a payload object.
 *
 * @p buf_len is every octet that followed the encapsulation header, unchecked:
 * the packet transfer layer does not check the carried packet's length or
 * format (ECSS-E-ST-50-53C clause 4.3). An implementation must not read past
 * @p buf_len, and decides itself what to do with octets its packet does not use.
 * Returning 0 discards the packet.
 *
 * Default: points a ::sw_payload_raw_t at @p buf (zero-copy), zero length
 * included. On failure, @p payload is left unchanged.
 *
 * @param[out] payload Payload object; its type is defined by the hook implementation.
 * @param[in]  buf     Serialised payload.
 * @param[in]  buf_len Octets available at @p buf; may be 0.
 * @return 1 on success, 0 on failure (default: NULL args).
 */
int sw_payload_parse(void *payload, const uint8_t *buf, uint32_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* SPACEWIRE_PAYLOAD_H */
