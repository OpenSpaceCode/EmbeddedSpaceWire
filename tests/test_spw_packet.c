/**
 * @file    test_spw_packet.c
 * @brief   Unit tests for the SpaceWire packet builder
 *
 * Checks sw_spw_packet_build() against the packet layout of
 * ECSS-E-ST-50-12C Rev.1, clause 5.6.2.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "cunit.h"
#include "spacewire.h"
#include "test_runners.h"

#include <stdlib.h>
#include <string.h>

/** @brief Fill value used to detect writes into a buffer that must stay untouched. */
#define FILL 0x5Au

/**
 * @brief Build a packet into an exact-size heap buffer and compare it with the expected octets.
 *
 * The buffer is exactly @p expected_len octets, so ASan flags any overrun.
 *
 * @param[in] dest         Destination-address octets, or NULL.
 * @param[in] dest_len     Number of destination-address octets.
 * @param[in] cargo        Cargo octets, or NULL.
 * @param[in] cargo_len    Number of cargo octets.
 * @param[in] expected     Expected packet octets.
 * @param[in] expected_len Expected packet length.
 * @return 0 if the packet was built exactly as expected, 1 otherwise.
 */
static int build_exact(const uint8_t *dest,
                       size_t dest_len,
                       const uint8_t *cargo,
                       size_t cargo_len,
                       const uint8_t *expected,
                       size_t expected_len)
{
    uint8_t *buf = (uint8_t *)malloc(expected_len);
    if (!buf)
    {
        return 1;
    }

    const size_t n = sw_spw_packet_build(dest, dest_len, cargo, cargo_len, buf, expected_len);
    const int matches = (n == expected_len) && (memcmp(buf, expected, expected_len) == 0);
    free(buf);

    ASSERT_EQ_UINT(expected_len, n);
    ASSERT_TRUE(matches);
    return 0;
}

/* A logical-addressed packet: one address octet followed by the cargo. */
static int test_spw_packet_build_logical(void)
{
    const uint8_t dest[1] = {0x40};
    const uint8_t cargo[3] = {0xAA, 0xBB, 0xCC};
    const uint8_t expected[4] = {0x40, 0xAA, 0xBB, 0xCC};

    ASSERT_EQ_INT(
        0,
        build_exact(dest, sizeof(dest), cargo, sizeof(cargo), expected, sizeof(expected)));
    return 0;
}

/* A path-addressed packet: a multi-hop path address followed by the cargo. */
static int test_spw_packet_build_path(void)
{
    const uint8_t dest[3] = {2, 1, 3};
    const uint8_t cargo[2] = {0xDE, 0xAD};
    const uint8_t expected[5] = {2, 1, 3, 0xDE, 0xAD};

    ASSERT_EQ_INT(
        0,
        build_exact(dest, sizeof(dest), cargo, sizeof(cargo), expected, sizeof(expected)));
    return 0;
}

/* A point-to-point link needs no destination address (clause 4.2.3.3). */
static int test_spw_packet_build_cargo_only(void)
{
    const uint8_t cargo[2] = {0x11, 0x22};

    ASSERT_EQ_INT(0, build_exact(NULL, 0, cargo, sizeof(cargo), cargo, sizeof(cargo)));
    return 0;
}

/* A destination address with no cargo (e.g. an addressed empty-payload frame). */
static int test_spw_packet_build_dest_only(void)
{
    const uint8_t dest[2] = {0x40, 0x05};

    ASSERT_EQ_INT(0, build_exact(dest, sizeof(dest), NULL, 0, dest, sizeof(dest)));
    return 0;
}

/* The smallest packet (one octet) fits an exact one-octet buffer. */
static int test_spw_packet_build_single_octet(void)
{
    const uint8_t dest[1] = {0x07};

    ASSERT_EQ_INT(0, build_exact(dest, sizeof(dest), NULL, 0, dest, sizeof(dest)));
    return 0;
}

/* One octet short of the packet: rejected, and the heap buffer is left untouched. */
static int test_spw_packet_build_one_short(void)
{
    const uint8_t dest[1] = {0x40};
    const uint8_t cargo[3] = {0xAA, 0xBB, 0xCC};
    const size_t one_short = sizeof(dest) + sizeof(cargo) - 1u;
    uint8_t *buf = (uint8_t *)malloc(one_short);
    if (!buf)
    {
        return 1;
    }
    memset(buf, FILL, one_short);

    const size_t n = sw_spw_packet_build(dest, sizeof(dest), cargo, sizeof(cargo), buf, one_short);
    int untouched = 1;
    for (size_t i = 0; i < one_short; i++)
    {
        untouched = (untouched) && (buf[i] == FILL);
    }
    free(buf);

    ASSERT_EQ_UINT(0, n);
    ASSERT_TRUE(untouched);
    return 0;
}

/**
 * @brief Expect a rejected build and an untouched buffer.
 *
 * Only the lengths matter: the pointers are valid but small, so the call must
 * be rejected before anything is copied.
 */
static int expect_length_rejected(size_t dest_len, size_t cargo_len, size_t buf_len)
{
    const uint8_t dest[8] = {0x40};
    const uint8_t cargo[8] = {0x11};
    const uint8_t untouched[8] = {FILL, FILL, FILL, FILL, FILL, FILL, FILL, FILL};
    uint8_t buf[8];
    memset(buf, FILL, sizeof(buf));

    ASSERT_EQ_UINT(0, sw_spw_packet_build(dest, dest_len, cargo, cargo_len, buf, buf_len));
    ASSERT_EQ_MEM(buf, untouched, sizeof(untouched));
    return 0;
}

/* Lengths whose sum wraps around SIZE_MAX are rejected before any copy. */
static int test_spw_packet_build_length_overflow_rejected(void)
{
    ASSERT_EQ_INT(0, expect_length_rejected(SIZE_MAX, 5, 8)); /* sum wraps to 4 */
    ASSERT_EQ_INT(0, expect_length_rejected(5, SIZE_MAX, 8)); /* sum wraps to 4 */
    ASSERT_EQ_INT(0, expect_length_rejected(SIZE_MAX, SIZE_MAX, 8));
    ASSERT_EQ_INT(0, expect_length_rejected(SIZE_MAX, 1, 8)); /* sum wraps to 0 */
    return 0;
}

/* A destination address alone longer than the buffer: 4 octets fit, 5 do not. */
static int test_spw_packet_build_dest_longer_than_buffer(void)
{
    const uint8_t dest[5] = {1, 2, 3, 4, 5};

    ASSERT_EQ_INT(0, build_exact(dest, 4, NULL, 0, dest, 4));
    ASSERT_EQ_INT(0, expect_length_rejected(5, 0, 4));
    return 0;
}

static int test_spw_packet_build_errors(void)
{
    const uint8_t dest[1] = {0x40};
    const uint8_t cargo[2] = {0x11, 0x22};
    uint8_t buf[2];

    /* NULL output buffer. */
    ASSERT_EQ_INT(0, sw_spw_packet_build(dest, 1, cargo, 2, NULL, 8));
    /* dest_len > 0 but dest NULL. */
    ASSERT_EQ_INT(0, sw_spw_packet_build(NULL, 1, cargo, 2, buf, sizeof(buf)));
    /* cargo_len > 0 but cargo NULL. */
    ASSERT_EQ_INT(0, sw_spw_packet_build(dest, 1, NULL, 2, buf, sizeof(buf)));
    /* Total length exceeds the buffer (1 + 2 > 2). */
    ASSERT_EQ_INT(0, sw_spw_packet_build(dest, 1, cargo, 2, buf, sizeof(buf)));
    /* Empty packet (no data characters). */
    ASSERT_EQ_INT(0, sw_spw_packet_build(NULL, 0, NULL, 0, buf, sizeof(buf)));
    return 0;
}

/* Every rejected call leaves the output buffer exactly as the caller passed it. */
static int test_spw_packet_build_failure_leaves_buffer_unchanged(void)
{
    const uint8_t dest[1] = {0x40};
    const uint8_t cargo[2] = {0x11, 0x22};
    const uint8_t untouched[2] = {0x5A, 0x5A};
    uint8_t buf[2] = {0x5A, 0x5A};

    ASSERT_EQ_INT(0, sw_spw_packet_build(NULL, 1, cargo, 2, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, sw_spw_packet_build(dest, 1, NULL, 2, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, sw_spw_packet_build(dest, 1, cargo, 2, buf, sizeof(buf))); /* one short */
    ASSERT_EQ_INT(0, sw_spw_packet_build(NULL, 0, NULL, 0, buf, sizeof(buf)));
    ASSERT_EQ_MEM(buf, untouched, sizeof(untouched));
    return 0;
}

test_result_t test_spacewire_spw_packet_run_all(void)
{
    RUN_TEST(test_spw_packet_build_logical);
    RUN_TEST(test_spw_packet_build_path);
    RUN_TEST(test_spw_packet_build_cargo_only);
    RUN_TEST(test_spw_packet_build_dest_only);
    RUN_TEST(test_spw_packet_build_single_octet);
    RUN_TEST(test_spw_packet_build_one_short);
    RUN_TEST(test_spw_packet_build_length_overflow_rejected);
    RUN_TEST(test_spw_packet_build_dest_longer_than_buffer);
    RUN_TEST(test_spw_packet_build_errors);
    RUN_TEST(test_spw_packet_build_failure_leaves_buffer_unchanged);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
