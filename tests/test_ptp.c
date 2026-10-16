/**
 * @file    test_ptp.c
 * @brief   Unit tests for the CCSDS Packet Transfer Protocol layer
 *
 * Runs against the default payload hooks, so the carried packet is a
 * ::sw_payload_raw_t. The hook-failure paths that the defaults cannot trigger
 * are covered by test_payload_override.c.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "cunit.h"
#include "spacewire_payload.h"
#include "spacewire_ptp.h"
#include "test_runners.h"

#include <stdlib.h>
#include <string.h>

/** @brief Length of the carried sample packet in octets. */
#define SAMPLE_PACKET_LEN 9u

/** @brief Length of the encoded sample: encapsulation header plus carried packet. */
#define SAMPLE_WIRE_LEN (SW_PTP_HEADER_LEN + SAMPLE_PACKET_LEN)

/** @brief Fill value used to detect writes into memory that must stay untouched. */
#define FILL 0x5Au

/* A CCSDS Space Packet (TC, APID 0x042, unsegmented, 3 data octets), carried as raw octets. */
static const uint8_t sample_packet[SAMPLE_PACKET_LEN] =
    {0x10, 0x42, 0xC0, 0x00, 0x00, 0x02, 0xAA, 0xBB, 0xCC};

static const sw_payload_raw_t sample_payload = {.data = sample_packet, .len = SAMPLE_PACKET_LEN};

static const sw_ptp_frame_t sample_frame = {.path = NULL,
                                            .path_len = 0,
                                            .logical_addr = 0x20,
                                            .user_app = 0x05};

/**
 * @brief Check that every octet of a buffer still holds ::FILL.
 *
 * @param[in] buf Buffer to inspect.
 * @param[in] len Buffer length in octets.
 * @return 1 if the buffer is untouched, 0 otherwise.
 */
static int is_untouched(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (buf[i] != FILL)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Encode the sample packet into an exact-size heap buffer.
 *
 * Exact-size buffers let ASan flag any one-octet overrun or over-read.
 *
 * @return Buffer of ::SAMPLE_WIRE_LEN octets owned by the caller, or NULL if
 *         allocation or encoding fails.
 */
static uint8_t *alloc_sample_wire(void)
{
    uint8_t *wire = (uint8_t *)malloc(SAMPLE_WIRE_LEN);
    uint32_t n = 0;

    if ((wire) &&
        ((sw_ptp_encode(&sample_frame, &sample_payload, wire, SAMPLE_WIRE_LEN, &n) != SW_OK) ||
         (n != SAMPLE_WIRE_LEN)))
    {
        free(wire);
        wire = NULL;
    }

    return wire;
}

/**
 * @brief Decode a buffer that must not be delivered and check nothing was.
 *
 * @param[in] buf             Received buffer.
 * @param[in] buf_len         Buffer length in octets.
 * @param[in] end             End-of-packet marker.
 * @param[in] expected        Status sw_ptp_decode() must return.
 * @param[in] expected_status Receive status it must report, or ::FILL if the
 *                            receive status must be left unchanged.
 * @return 0 if sw_ptp_decode() returned @p expected, reported @p expected_status
 *         and left the frame and the payload object unchanged, 1 otherwise.
 */
static int expect_not_delivered(const uint8_t *buf,
                                uint32_t buf_len,
                                sw_end_marker_t end,
                                sw_status_t expected,
                                int expected_status)
{
    sw_ptp_frame_t pf;
    sw_ptp_frame_t pf_before;
    sw_payload_raw_t payload;
    sw_payload_raw_t payload_before;
    memset(&pf, FILL, sizeof(pf));
    memset(&payload, FILL, sizeof(payload));
    memcpy(&pf_before, &pf, sizeof(pf));
    memcpy(&payload_before, &payload, sizeof(payload));

    sw_ptp_status_t status = (sw_ptp_status_t)FILL;
    ASSERT_EQ_INT(expected, sw_ptp_decode(&pf, &payload, buf, buf_len, end, &status));
    ASSERT_EQ_INT(expected_status, status);
    ASSERT_EQ_MEM(&pf, &pf_before, sizeof(pf));
    ASSERT_EQ_MEM(&payload, &payload_before, sizeof(payload));
    return 0;
}

/**
 * @brief Overwrite one octet of the encoded sample and expect it not to be delivered.
 *
 * @param[in] offset          Offset of the octet to overwrite.
 * @param[in] value           Value to write there.
 * @param[in] end             End-of-packet marker.
 * @param[in] expected        Status sw_ptp_decode() must return.
 * @param[in] expected_status Receive status it must report, or ::FILL if unchanged.
 * @return 0 on the expected outcome, 1 otherwise.
 */
static int discard_corrupted(uint32_t offset,
                             uint8_t value,
                             sw_end_marker_t end,
                             sw_status_t expected,
                             int expected_status)
{
    uint8_t *wire = alloc_sample_wire();
    if (!wire)
    {
        return 1;
    }
    wire[offset] = value;

    const int failed = expect_not_delivered(wire, SAMPLE_WIRE_LEN, end, expected, expected_status);
    free(wire);

    return failed;
}

/**
 * @brief Encode a zero-filled carried packet of a given length into an exact-size buffer.
 *
 * @param[in]  payload_len Length of the carried packet in octets.
 * @param[out] encoded_len Octets written; left unchanged unless the encode succeeds.
 * @return Result of sw_ptp_encode(), or ::SW_INVALID_PARAM if allocation fails.
 */
static sw_status_t encode_payload_of(uint32_t payload_len, uint32_t *encoded_len)
{
    const uint32_t wire_len = SW_PTP_HEADER_LEN + payload_len;
    uint8_t *data = (uint8_t *)calloc(payload_len, 1);
    uint8_t *wire = (uint8_t *)malloc(wire_len);
    sw_status_t result = SW_INVALID_PARAM;

    if ((data) && (wire))
    {
        const sw_payload_raw_t payload = {.data = data, .len = payload_len};
        result = sw_ptp_encode(&sample_frame, &payload, wire, wire_len, encoded_len);
    }

    free(wire);
    free(data);

    return result;
}

/**
 * @brief Decode a zero-filled carried packet of a given length from an exact-size buffer.
 *
 * @param[in]  payload_len Length of the carried packet in octets.
 * @param[out] decoded_len Length of the delivered payload, or 0 if none was delivered.
 * @return Result of sw_ptp_decode(), or ::SW_INVALID_PARAM if allocation fails.
 */
static sw_status_t decode_payload_of(uint32_t payload_len, uint32_t *decoded_len)
{
    const uint32_t wire_len = SW_PTP_HEADER_LEN + payload_len;
    uint8_t *wire = (uint8_t *)calloc(wire_len, 1);
    if (!wire)
    {
        return SW_INVALID_PARAM;
    }
    wire[0] = sample_frame.logical_addr;
    wire[1] = SW_PTP_PROTOCOL_ID;

    sw_ptp_frame_t pf;
    sw_payload_raw_t payload = {.data = NULL, .len = 0};
    const sw_status_t result = sw_ptp_decode(&pf, &payload, wire, wire_len, SW_END_EOP, NULL);
    *decoded_len = payload.len;
    free(wire);

    return result;
}

/* The encoded packet must match the ECSS-E-ST-50-53C Figure 5-1 layout exactly:
 * [logical | 0x02 | 0x00 | user_app | carried packet]. */
static int test_ptp_wire_format(void)
{
    const uint8_t expected[SAMPLE_WIRE_LEN] = {
        0x20,
        0x02,
        0x00,
        0x05, /* logical, proto=0x02, reserved=0, user_app */
        0x10,
        0x42,
        0xC0,
        0x00,
        0x00,
        0x02, /* carried packet: CCSDS primary header      */
        0xAA,
        0xBB,
        0xCC /* carried packet: data field                */
    };

    uint8_t *wire = alloc_sample_wire();
    if (!wire)
    {
        return 1;
    }
    const int matches = (memcmp(wire, expected, sizeof(expected)) == 0);
    free(wire);

    ASSERT_TRUE(matches);
    return 0;
}

/* The lowest and highest valid header field values are emitted byte-for-byte. */
static int test_ptp_wire_format_field_extremes(void)
{
    const sw_ptp_frame_t lowest = {.path = NULL,
                                   .path_len = 0,
                                   .logical_addr = SW_LOGICAL_ADDR_MIN,
                                   .user_app = 0x00};
    const sw_ptp_frame_t highest = {.path = NULL,
                                    .path_len = 0,
                                    .logical_addr = SW_LOGICAL_ADDR_RESERVED - 1u,
                                    .user_app = 0xFF};
    uint8_t wire[SAMPLE_WIRE_LEN];
    uint32_t n = 0;

    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&lowest, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_UINT(SAMPLE_WIRE_LEN, n);
    ASSERT_EQ_INT(0x20, wire[0]);
    ASSERT_EQ_INT(0x02, wire[1]);
    ASSERT_EQ_INT(0x00, wire[2]);
    ASSERT_EQ_INT(0x00, wire[3]);

    n = 0;
    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&highest, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_UINT(SAMPLE_WIRE_LEN, n);
    ASSERT_EQ_INT(0xFE, wire[0]);
    ASSERT_EQ_INT(0x02, wire[1]);
    ASSERT_EQ_INT(0x00, wire[2]);
    ASSERT_EQ_INT(0xFF, wire[3]);
    return 0;
}

/* Only 32..254 is a Target Logical Address: 31 would route as a path, 255 is reserved. */
static int test_ptp_encode_logical_addr_bounds(void)
{
    uint8_t wire[SAMPLE_WIRE_LEN];
    sw_ptp_frame_t pf = sample_frame;
    uint32_t n = UINT32_MAX;
    memset(wire, FILL, sizeof(wire));

    pf.logical_addr = SW_LOGICAL_ADDR_MIN - 1u;
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    pf.logical_addr = 0;
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    pf.logical_addr = SW_LOGICAL_ADDR_RESERVED;
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_INT(
        SW_WRONG_ADDRESS,
        sw_ptp_create(SW_LOGICAL_ADDR_RESERVED, 0, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_TRUE(is_untouched(wire, sizeof(wire)));
    ASSERT_EQ_UINT(UINT32_MAX, n);

    pf.logical_addr = SW_LOGICAL_ADDR_MIN;
    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_UINT(SAMPLE_WIRE_LEN, n);
    pf.logical_addr = SW_LOGICAL_ADDR_RESERVED - 1u;
    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    return 0;
}

static int test_ptp_encode_decode_roundtrip(void)
{
    uint8_t *wire = alloc_sample_wire();
    if (!wire)
    {
        return 1;
    }

    sw_ptp_frame_t pf;
    sw_payload_raw_t payload = {.data = NULL, .len = 0};
    sw_ptp_status_t status = (sw_ptp_status_t)FILL;
    memset(&pf, FILL, sizeof(pf));
    const sw_status_t result =
        sw_ptp_decode(&pf, &payload, wire, SAMPLE_WIRE_LEN, SW_END_EOP, &status);
    const int zero_copy = (payload.data == &wire[SW_PTP_HEADER_LEN]);
    const int same_data = (payload.len == SAMPLE_PACKET_LEN) &&
                          (memcmp(payload.data, sample_packet, payload.len) == 0);
    free(wire);

    ASSERT_EQ_INT(SW_OK, result);
    ASSERT_EQ_INT(SW_PTP_STATUS_OK, status);
    ASSERT_EQ_INT(0x20, pf.logical_addr);
    ASSERT_EQ_INT(0x05, pf.user_app);
    ASSERT_TRUE(!pf.path);
    ASSERT_EQ_INT(0, pf.path_len);
    ASSERT_TRUE(zero_copy);
    ASSERT_TRUE(same_data);
    return 0;
}

/* A path address is emitted ahead of the header; 31 is the largest valid path octet. */
static int test_ptp_with_path(void)
{
    static const uint8_t path[3] = {0, 2, 31};
    sw_ptp_frame_t pf = sample_frame;
    pf.path = path;
    pf.path_len = sizeof(path);

    uint8_t wire[sizeof(path) + SAMPLE_WIRE_LEN];
    uint32_t n = 0;
    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_UINT(sizeof(wire), n);
    ASSERT_EQ_MEM(wire, path, sizeof(path));
    ASSERT_EQ_INT(0x20, wire[3]); /* logical address follows the path */
    ASSERT_EQ_INT(0x02, wire[4]); /* protocol identifier */
    ASSERT_EQ_MEM(&wire[sizeof(path) + SW_PTP_HEADER_LEN], sample_packet, SAMPLE_PACKET_LEN);

    /* The network strips the path; the target decodes from the logical address. */
    sw_ptp_frame_t out;
    sw_payload_raw_t payload = {.data = NULL, .len = 0};
    ASSERT_EQ_INT(
        SW_OK,
        sw_ptp_decode(&out, &payload, &wire[sizeof(path)], SAMPLE_WIRE_LEN, SW_END_EOP, NULL));
    ASSERT_EQ_INT(SAMPLE_PACKET_LEN, payload.len);
    return 0;
}

/* Regional addressing (clause 5.3.1 b): logical-address octets 32..254 may sit in the
 * Target SpaceWire Address, alone or mixed with path octets. */
static int test_ptp_with_regional_address(void)
{
    static const uint8_t address[4] = {3, SW_LOGICAL_ADDR_MIN, 7, SW_LOGICAL_ADDR_RESERVED - 1u};
    sw_ptp_frame_t pf = sample_frame;
    pf.path = address;
    pf.path_len = sizeof(address);

    uint8_t wire[sizeof(address) + SAMPLE_WIRE_LEN];
    uint32_t n = 0;
    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_UINT(sizeof(wire), n);
    ASSERT_EQ_MEM(wire, address, sizeof(address));
    ASSERT_EQ_INT(0x20, wire[sizeof(address)]);
    ASSERT_EQ_INT(0x02, wire[sizeof(address) + 1u]);
    return 0;
}

/* The longest path the frame can describe (255 octets) fits an exact-size buffer. */
static int test_ptp_encode_max_path(void)
{
    static const uint8_t path[UINT8_MAX] = {0};
    const uint32_t wire_len = UINT8_MAX + SAMPLE_WIRE_LEN;
    sw_ptp_frame_t pf = sample_frame;
    pf.path = path;
    pf.path_len = UINT8_MAX;

    uint8_t *wire = (uint8_t *)calloc(wire_len, 1);
    if (!wire)
    {
        return 1;
    }
    uint32_t n = 0;
    const sw_status_t result = sw_ptp_encode(&pf, &sample_payload, wire, wire_len, &n);
    const int header_follows_path = (wire[UINT8_MAX] == 0x20) && (wire[UINT8_MAX + 1] == 0x02);
    free(wire);

    ASSERT_EQ_INT(SW_OK, result);
    ASSERT_EQ_UINT(wire_len, n);
    ASSERT_TRUE(header_follows_path);
    return 0;
}

static int test_ptp_encode_invalid_path_unchanged(void)
{
    /* 254 is the largest usable octet; 255 is the reserved logical address. */
    static const uint8_t bad_path[2] = {SW_LOGICAL_ADDR_RESERVED - 1u, SW_LOGICAL_ADDR_RESERVED};
    uint8_t wire[2 + SAMPLE_WIRE_LEN];
    uint32_t n = UINT32_MAX;
    memset(wire, FILL, sizeof(wire));

    sw_ptp_frame_t pf = sample_frame;
    pf.path = bad_path;
    pf.path_len = sizeof(bad_path);
    ASSERT_EQ_INT(SW_WRONG_ADDRESS, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));

    pf.path = NULL; /* path_len set but no path */
    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_TRUE(is_untouched(wire, sizeof(wire)));
    ASSERT_EQ_UINT(UINT32_MAX, n);
    return 0;
}

static int test_ptp_encode_null_args_unchanged(void)
{
    const sw_payload_raw_t no_data = {.data = NULL, .len = SAMPLE_PACKET_LEN};
    uint8_t wire[SAMPLE_WIRE_LEN];
    uint32_t n = UINT32_MAX;
    memset(wire, FILL, sizeof(wire));

    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_ptp_encode(NULL, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_INT(SW_INVALID_PARAM, sw_ptp_encode(&sample_frame, NULL, wire, sizeof(wire), &n));
    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_encode(&sample_frame, &sample_payload, NULL, sizeof(wire), &n));
    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_encode(&sample_frame, &sample_payload, wire, sizeof(wire), NULL));
    /* The default size hook reports 0 octets for a payload without data. */
    ASSERT_EQ_INT(SW_WRONG_LENGTH, sw_ptp_encode(&sample_frame, &no_data, wire, sizeof(wire), &n));
    ASSERT_TRUE(is_untouched(wire, sizeof(wire)));
    ASSERT_EQ_UINT(UINT32_MAX, n);
    return 0;
}

/* clause 5.1.2: the carried packet is 7 to 65542 octets long. */
static int test_ptp_encode_payload_length_bounds(void)
{
    uint32_t n = UINT32_MAX;

    ASSERT_EQ_INT(SW_WRONG_LENGTH, encode_payload_of(SW_PTP_CCSDS_MIN_LEN - 1u, &n));
    ASSERT_EQ_INT(SW_WRONG_LENGTH, encode_payload_of(SW_PTP_CCSDS_MAX_LEN + 1u, &n));
    ASSERT_EQ_UINT(UINT32_MAX, n);
    ASSERT_EQ_INT(SW_OK, encode_payload_of(SW_PTP_CCSDS_MIN_LEN, &n));
    ASSERT_EQ_UINT(SW_PTP_HEADER_LEN + SW_PTP_CCSDS_MIN_LEN, n);
    ASSERT_EQ_INT(SW_OK, encode_payload_of(SW_PTP_CCSDS_MAX_LEN, &n));
    ASSERT_EQ_UINT(SW_PTP_HEADER_LEN + SW_PTP_CCSDS_MAX_LEN, n);
    return 0;
}

/* An exact-size buffer is accepted; one octet less is rejected and left untouched. */
static int test_ptp_encode_buffer_boundary(void)
{
    uint8_t *exact = alloc_sample_wire();
    uint8_t *one_short = (uint8_t *)malloc(SAMPLE_WIRE_LEN - 1u);
    if ((!exact) || (!one_short))
    {
        free(exact);
        free(one_short);
        return 1;
    }
    memset(one_short, FILL, SAMPLE_WIRE_LEN - 1u);

    uint32_t n = UINT32_MAX;
    const sw_status_t result =
        sw_ptp_encode(&sample_frame, &sample_payload, one_short, SAMPLE_WIRE_LEN - 1u, &n);
    const int unchanged = is_untouched(one_short, SAMPLE_WIRE_LEN - 1u);
    free(exact);
    free(one_short);

    ASSERT_EQ_INT(SW_BUFFER_TOO_SMALL, result);
    ASSERT_EQ_UINT(UINT32_MAX, n);
    ASSERT_TRUE(unchanged);
    return 0;
}

/* clause 5.5.4.4: an otherwise valid packet terminated by EEP is discarded. */
static int test_ptp_decode_eep_discarded(void)
{
    ASSERT_EQ_INT(0, discard_corrupted(0, 0x20, SW_END_EEP, SW_DISCARDED, SW_PTP_STATUS_EEP));
    return 0;
}

/* An end marker that is neither EOP nor EEP is never taken for EOP; the receive
 * status, which has no code for it, is left unchanged. */
static int test_ptp_decode_unknown_end_marker_discarded(void)
{
    ASSERT_EQ_INT(0, discard_corrupted(0, 0x20, (sw_end_marker_t)2, SW_INVALID_PARAM, FILL));
    ASSERT_EQ_INT(0, discard_corrupted(0, 0x20, (sw_end_marker_t)7, SW_INVALID_PARAM, FILL));
    return 0;
}

/* clause 5.5.4.3: any non-zero Reserved field discards the packet. */
static int test_ptp_decode_reserved_nonzero(void)
{
    ASSERT_EQ_INT(
        0,
        discard_corrupted(2, 0x01, SW_END_EOP, SW_DISCARDED, SW_PTP_STATUS_RESERVED_NONZERO));
    ASSERT_EQ_INT(
        0,
        discard_corrupted(2, 0xFF, SW_END_EOP, SW_DISCARDED, SW_PTP_STATUS_RESERVED_NONZERO));
    return 0;
}

/* clause 5.5.4.1: the neighbours of Protocol Identifier 0x02 are not CCSDS PTP. */
static int test_ptp_decode_bad_protocol_id(void)
{
    ASSERT_EQ_INT(0, discard_corrupted(1, 0x01, SW_END_EOP, SW_WRONG_PROTOCOL, FILL));
    ASSERT_EQ_INT(0, discard_corrupted(1, 0x03, SW_END_EOP, SW_WRONG_PROTOCOL, FILL));
    return 0;
}

/* Every input shorter than the encapsulation header is rejected. */
static int test_ptp_decode_short_lengths_rejected(void)
{
    for (uint32_t len = 0; len < SW_PTP_HEADER_LEN; len++)
    {
        uint8_t *wire = (uint8_t *)calloc(len ? len : 1u, 1);
        if (!wire)
        {
            return 1;
        }
        if (len > 1u)
        {
            wire[1] = SW_PTP_PROTOCOL_ID;
        }

        const int failed = expect_not_delivered(wire, len, SW_END_EOP, SW_WRONG_LENGTH, FILL);
        free(wire);
        ASSERT_EQ_INT(0, failed);
    }
    return 0;
}

/* clause 5.5.4.2 a: with a valid header the carried packet is delivered whatever its
 * length. The send-side bounds of clause 5.1.2 (7..65542) are not applied on receive. */
static int test_ptp_decode_any_payload_length_delivered(void)
{
    static const uint32_t lengths[] = {0u,
                                       1u,
                                       SW_PTP_CCSDS_MIN_LEN - 1u,
                                       SW_PTP_CCSDS_MIN_LEN,
                                       SW_PTP_CCSDS_MAX_LEN,
                                       SW_PTP_CCSDS_MAX_LEN + 1u};

    for (size_t i = 0; i < (sizeof(lengths) / sizeof(lengths[0])); i++)
    {
        uint32_t len = UINT32_MAX;
        ASSERT_EQ_INT(SW_OK, decode_payload_of(lengths[i], &len));
        ASSERT_EQ_UINT(lengths[i], len);
    }
    return 0;
}

static int test_ptp_decode_null_args_unchanged(void)
{
    uint8_t wire[SAMPLE_WIRE_LEN] = {0};
    sw_ptp_frame_t pf = sample_frame;
    sw_payload_raw_t payload = sample_payload;
    sw_ptp_status_t status = (sw_ptp_status_t)FILL;

    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_decode(NULL, &payload, wire, sizeof(wire), SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_decode(&pf, NULL, wire, sizeof(wire), SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_decode(&pf, &payload, NULL, sizeof(wire), SW_END_EOP, &status));
    ASSERT_EQ_INT(FILL, status);

    /* A NULL status pointer is tolerated on every path. */
    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_decode(&pf, &payload, NULL, sizeof(wire), SW_END_EOP, NULL));
    ASSERT_EQ_INT(SW_WRONG_PROTOCOL,
                  sw_ptp_decode(&pf, &payload, wire, sizeof(wire), SW_END_EOP, NULL));
    ASSERT_EQ_INT(SW_DISCARDED, sw_ptp_decode(&pf, &payload, wire, sizeof(wire), SW_END_EEP, NULL));

    ASSERT_EQ_INT(sample_frame.logical_addr, pf.logical_addr);
    ASSERT_TRUE(payload.data == sample_packet);
    return 0;
}

static int test_ptp_create(void)
{
    uint8_t wire[SAMPLE_WIRE_LEN];
    uint32_t n = UINT32_MAX;
    memset(wire, FILL, sizeof(wire));

    /* One octet short: rejected, buffer and length untouched. */
    ASSERT_EQ_INT(SW_BUFFER_TOO_SMALL,
                  sw_ptp_create(0xFE, 0x07, &sample_payload, wire, sizeof(wire) - 1u, &n));
    ASSERT_TRUE(is_untouched(wire, sizeof(wire)));
    ASSERT_EQ_UINT(UINT32_MAX, n);

    ASSERT_EQ_INT(SW_OK, sw_ptp_create(0xFE, 0x07, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_UINT(SAMPLE_WIRE_LEN, n);
    ASSERT_EQ_INT(0xFE, wire[0]);
    ASSERT_EQ_INT(0x02, wire[1]);
    ASSERT_EQ_INT(0x00, wire[2]);
    ASSERT_EQ_INT(0x07, wire[3]);
    ASSERT_EQ_MEM(&wire[SW_PTP_HEADER_LEN], sample_packet, SAMPLE_PACKET_LEN);
    return 0;
}

static int test_ptp_statistics(void)
{
    uint8_t wire[SAMPLE_WIRE_LEN];
    sw_ptp_frame_t pf;
    sw_payload_raw_t payload;
    sw_ptp_statistics_t stats;
    uint32_t n = 0;

    sw_ptp_reset_statistics();
    sw_ptp_get_statistics(NULL); /* tolerate NULL */

    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&sample_frame, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_INT(SW_BUFFER_TOO_SMALL,
                  sw_ptp_encode(&sample_frame, &sample_payload, wire, 1, &n)); /* not sent */
    ASSERT_EQ_INT(SW_OK, sw_ptp_decode(&pf, &payload, wire, sizeof(wire), SW_END_EOP, NULL));
    ASSERT_EQ_INT(SW_DISCARDED, sw_ptp_decode(&pf, &payload, wire, sizeof(wire), SW_END_EEP, NULL));
    ASSERT_EQ_INT(SW_WRONG_LENGTH, sw_ptp_decode(&pf, &payload, wire, 1, SW_END_EOP, NULL));
    /* Invalid arguments are not a received packet, so they are not counted. */
    ASSERT_EQ_INT(SW_INVALID_PARAM,
                  sw_ptp_decode(NULL, &payload, wire, sizeof(wire), SW_END_EOP, NULL));

    sw_ptp_get_statistics(&stats);
    ASSERT_EQ_INT(1, stats.packets_sent);
    ASSERT_EQ_INT(SAMPLE_WIRE_LEN, stats.bytes_sent);
    ASSERT_EQ_INT(1, stats.packets_received);
    ASSERT_EQ_INT(SAMPLE_WIRE_LEN, stats.bytes_received);
    ASSERT_EQ_INT(2, stats.packets_discarded);

    sw_ptp_reset_statistics();
    sw_ptp_get_statistics(&stats);
    ASSERT_EQ_INT(0, stats.packets_sent + stats.packets_received + stats.packets_discarded);
    ASSERT_EQ_INT(0, stats.bytes_sent + stats.bytes_received);
    return 0;
}

/* The virtual channel rides in the User Application field (clause 5.3.5 NOTE 2). */
static int test_ptp_virtual_channel(void)
{
    sw_ptp_frame_t pf = sample_frame;
    uint8_t wire[SAMPLE_WIRE_LEN];

    uint32_t n = 0;

    sw_ptp_set_virtual_channel(&pf, 0x07);
    ASSERT_EQ_INT(0x07, sw_ptp_get_virtual_channel(&pf));
    ASSERT_EQ_INT(0x07, pf.user_app);

    ASSERT_EQ_INT(SW_OK, sw_ptp_encode(&pf, &sample_payload, wire, sizeof(wire), &n));
    ASSERT_EQ_INT(0x07, wire[3]);

    sw_ptp_frame_t out;
    sw_payload_raw_t payload;
    ASSERT_EQ_INT(SW_OK, sw_ptp_decode(&out, &payload, wire, sizeof(wire), SW_END_EOP, NULL));
    ASSERT_EQ_INT(0x07, sw_ptp_get_virtual_channel(&out));

    sw_ptp_set_virtual_channel(NULL, 0x09); /* no-op, must not crash */
    ASSERT_EQ_INT(0, sw_ptp_get_virtual_channel(NULL));
    return 0;
}

test_result_t test_spacewire_ptp_run_all(void)
{
    RUN_TEST(test_ptp_wire_format);
    RUN_TEST(test_ptp_wire_format_field_extremes);
    RUN_TEST(test_ptp_encode_logical_addr_bounds);
    RUN_TEST(test_ptp_encode_decode_roundtrip);
    RUN_TEST(test_ptp_with_path);
    RUN_TEST(test_ptp_with_regional_address);
    RUN_TEST(test_ptp_encode_max_path);
    RUN_TEST(test_ptp_encode_invalid_path_unchanged);
    RUN_TEST(test_ptp_encode_null_args_unchanged);
    RUN_TEST(test_ptp_encode_payload_length_bounds);
    RUN_TEST(test_ptp_encode_buffer_boundary);
    RUN_TEST(test_ptp_decode_eep_discarded);
    RUN_TEST(test_ptp_decode_unknown_end_marker_discarded);
    RUN_TEST(test_ptp_decode_reserved_nonzero);
    RUN_TEST(test_ptp_decode_bad_protocol_id);
    RUN_TEST(test_ptp_decode_short_lengths_rejected);
    RUN_TEST(test_ptp_decode_any_payload_length_delivered);
    RUN_TEST(test_ptp_decode_null_args_unchanged);
    RUN_TEST(test_ptp_create);
    RUN_TEST(test_ptp_statistics);
    RUN_TEST(test_ptp_virtual_channel);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
