/**
 * @file    test_payload_override.c
 * @brief   Unit tests for the payload adapter with application-supplied hooks
 *
 * Built into its own binary. The strong sw_payload_*() definitions in this
 * file replace the library's weak defaults at link time, which proves the
 * override mechanism and reaches the hook-failure paths of the packet transfer
 * layer that the default raw-octet hooks cannot trigger.
 *
 * The hooks implement a toy length-prefixed protocol: [total length][body].
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "cunit.h"
#include "spacewire_packet.h"
#include "spacewire_payload.h"

#include <stdlib.h>
#include <string.h>

/** @brief Length of the toy protocol's length prefix in octets. */
#define TOY_PREFIX_LEN 1u

/** @brief Body length used by the tests; with the prefix it exceeds the 7-octet minimum. */
#define TOY_BODY_LEN 8u

/** @brief Serialised length of the test packet: prefix plus body. */
#define TOY_PACKET_LEN (TOY_PREFIX_LEN + TOY_BODY_LEN)

/** @brief Length of the encoded test packet: encapsulation header plus toy packet. */
#define TOY_WIRE_LEN (SW_PTP_HEADER_LEN + TOY_PACKET_LEN)

/** @brief Fill value used to detect writes into memory that must stay untouched. */
#define FILL 0x5Au

/**
 * @brief Payload object of the toy protocol.
 *
 * @note @p body points into caller-owned memory, or into the parsed buffer.
 */
typedef struct
{
    const uint8_t *body; /**< Body octets. */
    uint8_t body_len;    /**< Number of body octets. */
} toy_packet_t;

static int g_fail_serialize = 0;
static int g_fail_parse = 0;

static const uint8_t toy_body[TOY_BODY_LEN] = {1, 2, 3, 4, 5, 6, 7, 8};
static const toy_packet_t toy_packet = {.body = toy_body, .body_len = TOY_BODY_LEN};
static const sw_packet_frame_t toy_frame = {.path = NULL,
                                            .path_len = 0,
                                            .logical_addr = 0x20,
                                            .user_app = 0x05};

uint32_t sw_payload_size(const void *payload)
{
    const toy_packet_t *pkt = payload;

    return (uint32_t)pkt->body_len + TOY_PREFIX_LEN;
}

uint32_t sw_payload_serialize(const void *payload, uint8_t *buf, uint32_t buf_len)
{
    const toy_packet_t *pkt = payload;
    const uint32_t need = (uint32_t)pkt->body_len + TOY_PREFIX_LEN;

    if ((g_fail_serialize) || (buf_len < need))
    {
        return 0;
    }

    buf[0] = (uint8_t)need;
    memcpy(&buf[TOY_PREFIX_LEN], pkt->body, pkt->body_len);

    return need;
}

uint32_t sw_payload_length(const uint8_t *buf, uint32_t buf_len)
{
    if (buf_len < TOY_PREFIX_LEN)
    {
        return 0;
    }

    return buf[0];
}

int sw_payload_parse(void *payload, const uint8_t *buf, uint32_t buf_len)
{
    toy_packet_t *pkt = payload;

    if ((g_fail_parse) || (buf_len < TOY_PREFIX_LEN))
    {
        return 0;
    }

    pkt->body = &buf[TOY_PREFIX_LEN];
    pkt->body_len = (uint8_t)(buf[0] - TOY_PREFIX_LEN);

    return 1;
}

/**
 * @brief Decode a buffer that must be discarded as invalid and check nothing was delivered.
 *
 * @param[in] buf     Received buffer.
 * @param[in] buf_len Buffer length in octets.
 * @return 0 if the packet was discarded with ::SW_PTP_STATUS_INVALID and both
 *         the frame and the payload object were left unchanged, 1 otherwise.
 */
static int expect_invalid(const uint8_t *buf, uint32_t buf_len)
{
    sw_packet_frame_t pf;
    sw_packet_frame_t pf_before;
    toy_packet_t pkt;
    toy_packet_t pkt_before;
    memset(&pf, FILL, sizeof(pf));
    memset(&pkt, FILL, sizeof(pkt));
    memcpy(&pf_before, &pf, sizeof(pf));
    memcpy(&pkt_before, &pkt, sizeof(pkt));

    sw_ptp_status_t status = SW_PTP_STATUS_OK;
    ASSERT_EQ_INT(SW_ERR, sw_packet_decode(&pf, &pkt, buf, buf_len, SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_PTP_STATUS_INVALID, status);
    ASSERT_EQ_MEM(&pf, &pf_before, sizeof(pf));
    ASSERT_EQ_MEM(&pkt, &pkt_before, sizeof(pkt));
    return 0;
}

/**
 * @brief Encode the test packet into a heap buffer sized for a given received length.
 *
 * @param[in] wire_len Buffer length to allocate; octets beyond the encoded
 *                     packet are zero, a shorter buffer truncates it.
 * @return Buffer of @p wire_len octets owned by the caller, or NULL if allocation fails.
 */
static uint8_t *alloc_toy_wire(uint32_t wire_len)
{
    uint8_t encoded[TOY_WIRE_LEN];
    uint8_t *wire = (uint8_t *)calloc(wire_len, 1);

    if ((wire) && (sw_packet_encode(&toy_frame, &toy_packet, encoded, sizeof(encoded)) != 0u))
    {
        memcpy(wire, encoded, (wire_len < TOY_WIRE_LEN) ? wire_len : TOY_WIRE_LEN);
    }

    return wire;
}

/* The length prefix on the wire can only come from the hooks in this file. */
static int test_override_replaces_defaults(void)
{
    uint8_t *wire = alloc_toy_wire(TOY_WIRE_LEN);
    if (!wire)
    {
        return 1;
    }

    sw_packet_frame_t pf;
    toy_packet_t pkt = {.body = NULL, .body_len = 0};
    const sw_result_t result = sw_packet_decode(&pf, &pkt, wire, TOY_WIRE_LEN, SW_END_EOP, NULL);
    const uint8_t prefix = wire[SW_PTP_HEADER_LEN];
    const int zero_copy = (pkt.body == &wire[SW_PTP_HEADER_LEN + TOY_PREFIX_LEN]);
    const int same_body =
        (pkt.body_len == TOY_BODY_LEN) && (memcmp(pkt.body, toy_body, TOY_BODY_LEN) == 0);
    free(wire);

    ASSERT_EQ_INT(TOY_PACKET_LEN, prefix);
    ASSERT_EQ_INT(SW_OK, result);
    ASSERT_EQ_INT(0x20, pf.logical_addr);
    ASSERT_TRUE(zero_copy);
    ASSERT_TRUE(same_body);
    return 0;
}

/* The carried packet declares one octet fewer than arrived: a trailing octet. */
static int test_override_trailing_octet_rejected(void)
{
    uint8_t *wire = alloc_toy_wire(TOY_WIRE_LEN + 1u);
    if (!wire)
    {
        return 1;
    }

    const int failed = expect_invalid(wire, TOY_WIRE_LEN + 1u);
    free(wire);

    ASSERT_EQ_INT(0, failed);
    return 0;
}

/* The carried packet declares one octet more than arrived: it is truncated. */
static int test_override_truncated_packet_rejected(void)
{
    uint8_t *wire = alloc_toy_wire(TOY_WIRE_LEN - 1u);
    if (!wire)
    {
        return 1;
    }

    const int failed = expect_invalid(wire, TOY_WIRE_LEN - 1u);
    free(wire);

    ASSERT_EQ_INT(0, failed);
    return 0;
}

static int test_override_parse_failure_discards(void)
{
    uint8_t *wire = alloc_toy_wire(TOY_WIRE_LEN);
    if (!wire)
    {
        return 1;
    }

    sw_statistics_t stats;
    sw_reset_statistics();
    g_fail_parse = 1;
    const int failed = expect_invalid(wire, TOY_WIRE_LEN);
    g_fail_parse = 0;
    sw_get_statistics(&stats);
    free(wire);

    ASSERT_EQ_INT(0, failed);
    ASSERT_EQ_INT(1, stats.packets_discarded);
    ASSERT_EQ_INT(0, stats.packets_received);
    return 0;
}

/* The header is written only after the hook succeeded, so the buffer stays untouched. */
static int test_override_serialize_failure_leaves_buffer_unchanged(void)
{
    uint8_t untouched[TOY_WIRE_LEN];
    uint8_t wire[TOY_WIRE_LEN];
    sw_statistics_t stats;
    memset(untouched, FILL, sizeof(untouched));
    memset(wire, FILL, sizeof(wire));

    sw_reset_statistics();
    g_fail_serialize = 1;
    const uint32_t n = sw_packet_encode(&toy_frame, &toy_packet, wire, sizeof(wire));
    g_fail_serialize = 0;
    sw_get_statistics(&stats);

    ASSERT_EQ_INT(0, n);
    ASSERT_EQ_MEM(wire, untouched, sizeof(wire));
    ASSERT_EQ_INT(0, stats.packets_sent);
    return 0;
}

int main(void)
{
    RUN_TEST(test_override_replaces_defaults);
    RUN_TEST(test_override_trailing_octet_rejected);
    RUN_TEST(test_override_truncated_packet_rejected);
    RUN_TEST(test_override_parse_failure_discards);
    RUN_TEST(test_override_serialize_failure_leaves_buffer_unchanged);

    printf("  %-14s Passed %d/%d\n",
           "override:",
           cunit_total_tests - cunit_overall_failures,
           cunit_total_tests);

    return (cunit_overall_failures == 0) ? 0 : 1;
}
