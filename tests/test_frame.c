#include "cunit.h"
#include "spacewire.h"
#include "test_runners.h"

#include <string.h>

/* A logical-addressed packet: one address octet followed by the cargo. */
static int test_packet_build_logical(void)
{
    const uint8_t dest[1] = {0x40};
    const uint8_t cargo[3] = {0xAA, 0xBB, 0xCC};
    uint8_t buf[16];

    size_t n = sw_spw_packet_build(dest, sizeof(dest), cargo, sizeof(cargo), buf, sizeof(buf));
    ASSERT_EQ_INT(4, (int)n);

    const uint8_t expected[4] = {0x40, 0xAA, 0xBB, 0xCC};
    ASSERT_EQ_MEM(buf, expected, sizeof(expected));
    return 0;
}

/* A path-addressed packet: a multi-hop path address followed by the cargo. */
static int test_packet_build_path(void)
{
    const uint8_t dest[3] = {2, 1, 3};
    const uint8_t cargo[2] = {0xDE, 0xAD};
    uint8_t buf[16];

    size_t n = sw_spw_packet_build(dest, sizeof(dest), cargo, sizeof(cargo), buf, sizeof(buf));
    ASSERT_EQ_INT(5, (int)n);

    const uint8_t expected[5] = {2, 1, 3, 0xDE, 0xAD};
    ASSERT_EQ_MEM(buf, expected, sizeof(expected));
    return 0;
}

/* A point-to-point link needs no destination address (clause 4.2.3.3). */
static int test_packet_build_cargo_only(void)
{
    const uint8_t cargo[2] = {0x11, 0x22};
    uint8_t buf[8];

    size_t n = sw_spw_packet_build(NULL, 0, cargo, sizeof(cargo), buf, sizeof(buf));
    ASSERT_EQ_INT(2, (int)n);
    ASSERT_EQ_MEM(buf, cargo, sizeof(cargo));
    return 0;
}

static int test_packet_build_errors(void)
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

test_result_t test_spacewire_frame_run_all(void)
{
    RUN_TEST(test_packet_build_logical);
    RUN_TEST(test_packet_build_path);
    RUN_TEST(test_packet_build_cargo_only);
    RUN_TEST(test_packet_build_errors);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
