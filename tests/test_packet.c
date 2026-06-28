/**
 * @file test_packet.c
 * @brief Unit tests for the CCSDS Packet Transfer Protocol layer.
 */
#include "cunit.h"
#include "spacewire.h"
#include "spacewire_packet.h"
#include "test_runners.h"

#include <string.h>

/* Build a representative packet: logical addressing, no path. */
static void make_sample(sw_packet_frame_t *pf, const uint8_t *data, uint16_t data_len)
{
    const sw_packet_config_t config = {
        .path = NULL, .path_len = 0, .logical_addr = 0x20, .user_app = 0x05};
    sw_packet_init(pf, &config);
    pf->packet.ph.apid = 0x0042;
    pf->packet.data = data;
    pf->packet.data_len = data_len;
}

/* The encoded packet must match the ECSS-E-ST-50-53C Figure 5-1 layout exactly:
 * [logical | 0x02 | 0x00 | user_app | CCSDS primary header | data]. */
static int test_packet_wire_format(void)
{
    static const uint8_t data[3] = {0xAA, 0xBB, 0xCC};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));

    /* 4-octet header + 6-octet CCSDS primary header + 3 data octets. */
    ASSERT_EQ_INT(4 + 6 + 3, (int)n);

    const uint8_t expected[13] = {
        0x20, 0x02, 0x00, 0x05,             /* logical, proto=0x02, reserved=0, user_app */
        0x10, 0x42, 0x00, 0x00, 0x00, 0x02, /* CCSDS hdr: TC, APID 0x042, seq 0, len=2  */
        0xAA, 0xBB, 0xCC                    /* data field                                */
    };
    ASSERT_EQ_MEM(buf, expected, sizeof(expected));
    return 0;
}

static int test_packet_encode_decode_roundtrip(void)
{
    static const uint8_t data[12] = "Test payload";
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_INVALID;
    int ok = sw_packet_decode(&out, buf, n, SW_END_EOP, &status);

    ASSERT_EQ_INT(1, ok);
    ASSERT_EQ_INT(SW_PTP_STATUS_OK, status);
    ASSERT_EQ_INT(0x20, out.logical_addr);
    ASSERT_EQ_INT(0x05, out.user_app);
    ASSERT_EQ_INT(0x0042, out.packet.ph.apid);
    ASSERT_EQ_INT((int)sizeof(data), out.packet.data_len);
    ASSERT_EQ_MEM(out.packet.data, data, sizeof(data));
    return 0;
}

/* A path address is emitted ahead of the header and stripped before decode. */
static int test_packet_with_path(void)
{
    static const uint8_t path[3] = {1, 2, 3};
    static const uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));
    pf.path = path;
    pf.path_len = sizeof(path);

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));
    ASSERT_EQ_INT(3 + 4 + 6 + 4, (int)n); /* path + header + CCSDS hdr + data */
    ASSERT_EQ_MEM(buf, path, sizeof(path));
    ASSERT_EQ_INT(0x20, buf[3]); /* logical address follows the path */
    ASSERT_EQ_INT(0x02, buf[4]); /* protocol identifier */

    /* The network strips the path; the target decodes from the logical address. */
    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_INVALID;
    int ok = sw_packet_decode(&out, buf + sizeof(path), n - sizeof(path), SW_END_EOP, &status);
    ASSERT_EQ_INT(1, ok);
    ASSERT_EQ_INT(SW_PTP_STATUS_OK, status);
    ASSERT_EQ_MEM(out.packet.data, data, sizeof(data));
    return 0;
}

static int test_packet_decode_eep_discarded(void)
{
    static const uint8_t data[4] = {1, 2, 3, 4};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_OK;
    int ok = sw_packet_decode(&out, buf, n, SW_END_EEP, &status);

    ASSERT_EQ_INT(0, ok); /* clause 5.5.4.4 */
    ASSERT_EQ_INT(SW_PTP_STATUS_EEP, status);
    ASSERT_TRUE(out.packet.data == NULL); /* clause 5.2.3.2 b */
    return 0;
}

static int test_packet_decode_reserved_nonzero(void)
{
    static const uint8_t data[4] = {1, 2, 3, 4};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));
    buf[2] = 0x01; /* corrupt the Reserved field */

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_OK;
    int ok = sw_packet_decode(&out, buf, n, SW_END_EOP, &status);

    ASSERT_EQ_INT(0, ok); /* clause 5.5.4.3 */
    ASSERT_EQ_INT(SW_PTP_STATUS_RESERVED_NONZERO, status);
    ASSERT_TRUE(out.packet.data == NULL);
    return 0;
}

static int test_packet_decode_bad_protocol_id(void)
{
    static const uint8_t data[4] = {1, 2, 3, 4};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));
    buf[1] = 0x01; /* not the CCSDS PTP protocol identifier */

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_OK;
    int ok = sw_packet_decode(&out, buf, n, SW_END_EOP, &status);

    ASSERT_EQ_INT(0, ok); /* clause 5.5.4.1 */
    ASSERT_EQ_INT(SW_PTP_STATUS_INVALID, status);
    return 0;
}

/* Valid PTP header, but the CCSDS length field declares more data than is
 * present, so the encapsulated CCSDS parse fails (clause 5.5.4.2). */
static int test_packet_decode_malformed_ccsds(void)
{
    const uint8_t buf[11] = {
        0x20, 0x02, 0x00, 0x05,             /* logical, proto=0x02, reserved=0, user_app */
        0x10, 0x42, 0x00, 0x00, 0x00, 0x05, /* CCSDS hdr: length field 5 -> data_len 6 */
        0xAA                                /* only 1 of the 6 declared data octets */
    };

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_OK;
    int ok = sw_packet_decode(&out, buf, sizeof(buf), SW_END_EOP, &status);

    ASSERT_EQ_INT(0, ok);
    ASSERT_EQ_INT(SW_PTP_STATUS_INVALID, status);
    ASSERT_TRUE(out.packet.data == NULL); /* cleared on discard */
    return 0;
}

static int test_packet_init_defaults(void)
{
    const sw_packet_config_t config = {
        .path = NULL, .path_len = 0, .logical_addr = 0x55, .user_app = 0x99};

    sw_packet_frame_t pf;
    memset(&pf, 0xA5, sizeof(pf));

    sw_packet_init(NULL, &config);
    sw_packet_init(&pf, NULL);

    sw_packet_init(&pf, &config);
    ASSERT_EQ_INT(0x55, pf.logical_addr);
    ASSERT_EQ_INT(0x99, pf.user_app);
    ASSERT_EQ_INT(0, pf.path_len);
    ASSERT_EQ_INT(0, pf.packet.ph.version);
    ASSERT_EQ_INT(SP_PACKET_TYPE_TC, pf.packet.ph.type);
    ASSERT_EQ_INT(0, pf.packet.ph.sec_hdr_flag);
    ASSERT_EQ_INT(0, pf.packet.ph.apid);
    return 0;
}

static int test_packet_encode_error_paths(void)
{
    static const uint8_t data[4] = {1, 2, 3, 4};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];

    ASSERT_EQ_INT(0, sw_packet_encode(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, sw_packet_encode(&pf, NULL, sizeof(buf)));

    /* CCSDS data pointer NULL with non-zero length. */
    make_sample(&pf, NULL, 4);
    ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

    /* Empty CCSDS data field is below the minimum packet length. */
    make_sample(&pf, data, 0);
    ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

    /* path_len set but path pointer NULL. */
    make_sample(&pf, data, sizeof(data));
    pf.path = NULL;
    pf.path_len = 2;
    ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

    /* path octet outside the valid 0..31 range. */
    static const uint8_t bad_path[1] = {32};
    make_sample(&pf, data, sizeof(data));
    pf.path = bad_path;
    pf.path_len = 1;
    ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, sizeof(buf)));

    /* Output buffer too small. */
    make_sample(&pf, data, sizeof(data));
    ASSERT_EQ_INT(0, sw_packet_encode(&pf, buf, 5));
    return 0;
}

static int test_packet_decode_error_paths(void)
{
    const uint8_t buf[16] = {0};
    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_OK;

    ASSERT_EQ_INT(0, sw_packet_decode(NULL, buf, sizeof(buf), SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_PTP_STATUS_INVALID, status);

    status = SW_PTP_STATUS_OK;
    ASSERT_EQ_INT(0, sw_packet_decode(&out, NULL, sizeof(buf), SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_PTP_STATUS_INVALID, status);

    /* Shorter than header + minimal CCSDS packet (11 octets). */
    status = SW_PTP_STATUS_OK;
    ASSERT_EQ_INT(0, sw_packet_decode(&out, buf, 10, SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_PTP_STATUS_INVALID, status);

    /* A NULL status pointer is tolerated. */
    ASSERT_EQ_INT(0, sw_packet_decode(&out, buf, 10, SW_END_EOP, NULL));
    return 0;
}

static int test_packet_create_convenience(void)
{
    const uint8_t payload[3] = {0x11, 0x22, 0x33};
    uint8_t buf[64];

    size_t n = sw_packet_create(0xFE, 0x07, 0x0042, payload, sizeof(payload), buf, sizeof(buf));
    ASSERT_EQ_INT(4 + 6 + 3, (int)n);
    ASSERT_EQ_INT(0xFE, buf[0]);
    ASSERT_EQ_INT(0x02, buf[1]);

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_INVALID;
    ASSERT_EQ_INT(1, sw_packet_decode(&out, buf, n, SW_END_EOP, &status));
    ASSERT_EQ_INT(SW_PTP_STATUS_OK, status);
    ASSERT_EQ_INT(0x07, out.user_app);
    ASSERT_EQ_INT(0x0042, out.packet.ph.apid);

    ASSERT_EQ_INT(0, sw_packet_create(0xFE, 0, 0x0042, payload, sizeof(payload), NULL, sizeof(buf)));
    return 0;
}

static int test_packet_statistics(void)
{
    static const uint8_t data[4] = {1, 2, 3, 4};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    uint8_t buf[64];

    sw_reset_statistics();
    sw_statistics_t stats;
    sw_get_statistics(NULL); /* tolerate NULL */
    sw_get_statistics(&stats);
    ASSERT_EQ_INT(0, stats.packets_sent);

    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));
    sw_packet_frame_t out;
    ASSERT_EQ_INT(1, sw_packet_decode(&out, buf, n, SW_END_EOP, NULL));
    ASSERT_EQ_INT(0, sw_packet_decode(&out, buf, n, SW_END_EEP, NULL)); /* discarded */

    sw_get_statistics(&stats);
    ASSERT_EQ_INT(1, stats.packets_sent);
    ASSERT_EQ_INT(1, stats.packets_received);
    ASSERT_EQ_INT(1, stats.packets_discarded);
    ASSERT_TRUE(stats.bytes_sent > 0);
    ASSERT_TRUE(stats.bytes_received > 0);

    sw_reset_statistics();
    sw_get_statistics(&stats);
    ASSERT_EQ_INT(0, stats.packets_received);
    return 0;
}

/* The virtual channel rides in the User Application field (clause 5.3.5 NOTE 2). */
static int test_packet_virtual_channel(void)
{
    static const uint8_t data[4] = {1, 2, 3, 4};
    sw_packet_frame_t pf;
    make_sample(&pf, data, sizeof(data));

    sw_packet_set_virtual_channel(&pf, 0x07);
    ASSERT_EQ_INT(0x07, sw_packet_virtual_channel(&pf));
    ASSERT_EQ_INT(0x07, pf.user_app);

    uint8_t buf[64];
    size_t n = sw_packet_encode(&pf, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    sw_packet_frame_t out;
    sw_ptp_status_t status = SW_PTP_STATUS_INVALID;
    ASSERT_EQ_INT(1, sw_packet_decode(&out, buf, n, SW_END_EOP, &status));
    ASSERT_EQ_INT(0x07, sw_packet_virtual_channel(&out));

    sw_packet_set_virtual_channel(NULL, 0x09); /* no-op, must not crash */
    ASSERT_EQ_INT(0, sw_packet_virtual_channel(NULL));
    return 0;
}

test_result_t test_spacewire_packet_run_all(void)
{
    RUN_TEST(test_packet_wire_format);
    RUN_TEST(test_packet_encode_decode_roundtrip);
    RUN_TEST(test_packet_with_path);
    RUN_TEST(test_packet_decode_eep_discarded);
    RUN_TEST(test_packet_decode_reserved_nonzero);
    RUN_TEST(test_packet_decode_bad_protocol_id);
    RUN_TEST(test_packet_decode_malformed_ccsds);
    RUN_TEST(test_packet_init_defaults);
    RUN_TEST(test_packet_encode_error_paths);
    RUN_TEST(test_packet_decode_error_paths);
    RUN_TEST(test_packet_create_convenience);
    RUN_TEST(test_packet_virtual_channel);
    RUN_TEST(test_packet_statistics);
    return (test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
