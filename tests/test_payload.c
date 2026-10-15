/**
 * @file    test_payload.c
 * @brief   Unit tests for the default (raw-octet) payload adapter hooks
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "cunit.h"
#include "spacewire_payload.h"
#include "test_runners.h"

#include <stdlib.h>
#include <string.h>

/** @brief Fill value used to detect writes into a buffer that must stay untouched. */
#define FILL 0x5Au

static const uint8_t sample[5] = {0x11, 0x22, 0x33, 0x44, 0x55};

static int test_payload_size(void)
{
    const sw_payload_raw_t raw = {.data = sample, .len = sizeof(sample)};
    const sw_payload_raw_t one_octet = {.data = sample, .len = 1};
    const sw_payload_raw_t no_data = {.data = NULL, .len = sizeof(sample)};
    const sw_payload_raw_t empty = {.data = sample, .len = 0};

    ASSERT_EQ_INT(5, sw_payload_size(&raw));
    ASSERT_EQ_INT(1, sw_payload_size(&one_octet));
    ASSERT_EQ_INT(0, sw_payload_size(NULL));
    ASSERT_EQ_INT(0, sw_payload_size(&no_data));
    ASSERT_EQ_INT(0, sw_payload_size(&empty));
    return 0;
}

/* The buffer is exactly as large as the payload, so ASan would flag any overrun. */
static int test_payload_serialize_exact_buffer(void)
{
    const sw_payload_raw_t raw = {.data = sample, .len = sizeof(sample)};
    uint8_t *buf = (uint8_t *)malloc(sizeof(sample));
    if (!buf)
    {
        return 1;
    }
    memset(buf, FILL, sizeof(sample));

    const uint32_t n = sw_payload_serialize(&raw, buf, sizeof(sample));
    const int copied = (memcmp(buf, sample, sizeof(sample)) == 0);
    free(buf);

    ASSERT_EQ_INT(5, n);
    ASSERT_TRUE(copied);
    return 0;
}

static int test_payload_serialize_short_buffer_unchanged(void)
{
    const sw_payload_raw_t raw = {.data = sample, .len = sizeof(sample)};
    const uint8_t untouched[4] = {FILL, FILL, FILL, FILL};
    uint8_t *one_short = (uint8_t *)malloc(sizeof(untouched));
    if (!one_short)
    {
        return 1;
    }
    memset(one_short, FILL, sizeof(untouched));

    const uint32_t n = sw_payload_serialize(&raw, one_short, sizeof(untouched));
    const int unchanged = (memcmp(one_short, untouched, sizeof(untouched)) == 0);
    free(one_short);

    ASSERT_EQ_INT(0, n);
    ASSERT_TRUE(unchanged);
    return 0;
}

static int test_payload_serialize_invalid_args_unchanged(void)
{
    const sw_payload_raw_t raw = {.data = sample, .len = sizeof(sample)};
    const sw_payload_raw_t no_data = {.data = NULL, .len = sizeof(sample)};
    const sw_payload_raw_t empty = {.data = sample, .len = 0};
    const uint8_t untouched[5] = {FILL, FILL, FILL, FILL, FILL};
    uint8_t buf[5] = {FILL, FILL, FILL, FILL, FILL};

    ASSERT_EQ_INT(0, sw_payload_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, sw_payload_serialize(&no_data, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, sw_payload_serialize(&empty, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, sw_payload_serialize(&raw, NULL, sizeof(buf)));
    ASSERT_EQ_MEM(buf, untouched, sizeof(untouched));
    return 0;
}

/* Raw octets declare no length of their own: all available octets are payload. */
static int test_payload_length(void)
{
    ASSERT_EQ_INT(5, sw_payload_length(sample, sizeof(sample)));
    ASSERT_EQ_INT(0, sw_payload_length(sample, 0));
    ASSERT_EQ_INT(0, sw_payload_length(NULL, sizeof(sample)));
    return 0;
}

static int test_payload_parse_zero_copy(void)
{
    sw_payload_raw_t raw = {.data = NULL, .len = 0};

    ASSERT_EQ_INT(1, sw_payload_parse(&raw, sample, sizeof(sample)));
    ASSERT_TRUE(raw.data == sample);
    ASSERT_EQ_INT(5, raw.len);

    ASSERT_EQ_INT(1, sw_payload_parse(&raw, sample, 1));
    ASSERT_EQ_INT(1, raw.len);
    return 0;
}

static int test_payload_parse_failure_unchanged(void)
{
    const uint8_t other[1] = {0};
    sw_payload_raw_t raw = {.data = other, .len = sizeof(other)};

    ASSERT_EQ_INT(0, sw_payload_parse(NULL, sample, sizeof(sample)));
    ASSERT_EQ_INT(0, sw_payload_parse(&raw, NULL, sizeof(sample)));
    ASSERT_EQ_INT(0, sw_payload_parse(&raw, sample, 0));
    ASSERT_TRUE(raw.data == other);
    ASSERT_EQ_INT(1, raw.len);
    return 0;
}

test_result_t test_spacewire_payload_run_all(void)
{
    RUN_TEST(test_payload_size);
    RUN_TEST(test_payload_serialize_exact_buffer);
    RUN_TEST(test_payload_serialize_short_buffer_unchanged);
    RUN_TEST(test_payload_serialize_invalid_args_unchanged);
    RUN_TEST(test_payload_length);
    RUN_TEST(test_payload_parse_zero_copy);
    RUN_TEST(test_payload_parse_failure_unchanged);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
