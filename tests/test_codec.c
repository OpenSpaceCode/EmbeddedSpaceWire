#include "cunit.h"
#include "spacewire.h"
#include "test_runners.h"

static int test_encode_decode(void)
{
    for (int i = 4; i < 256; i++)
    {
        uint8_t encoded;
        uint8_t parity = sw_encode_char((uint8_t)i, &encoded);

        uint8_t decoded;
        sw_char_result_t result = sw_decode_char(encoded, parity, &decoded);

        ASSERT_EQ_INT(SW_CHAR_OK, result);
        ASSERT_EQ_INT((uint8_t)i, decoded);
    }

    return 0;
}

static int test_parity_error(void)
{
    uint8_t encoded;
    uint8_t parity = sw_encode_char(0x55, &encoded);

    uint8_t decoded;
    uint8_t wrong_parity = parity ^ 1;
    sw_char_result_t result = sw_decode_char(encoded, wrong_parity, &decoded);

    ASSERT_EQ_INT(SW_CHAR_PARITY_ERROR, result);
    return 0;
}

static int test_crc_computation(void)
{
    const uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    uint16_t crc = sw_crc16(data, sizeof(data));

    ASSERT_TRUE(crc != 0);

    uint16_t crc2 = sw_crc16(data, sizeof(data));
    ASSERT_EQ_INT(crc, crc2);

    return 0;
}

static int test_codec_special_chars_and_invalid_args(void)
{
    uint8_t decoded = 0xAA;

    ASSERT_EQ_INT(SW_CHAR_INVALID, sw_decode_char(0x10, 0, NULL));

    uint8_t encoded;
    uint8_t parity;

    parity = sw_encode_char(SPACEWIRE_ESC, &encoded);
    ASSERT_EQ_INT(SW_CHAR_ESCAPE, sw_decode_char(encoded, parity, &decoded));

    parity = sw_encode_char(SPACEWIRE_FCT, &encoded);
    ASSERT_EQ_INT(SW_CHAR_FCT, sw_decode_char(encoded, parity, &decoded));

    parity = sw_encode_char(SPACEWIRE_EOP, &encoded);
    ASSERT_EQ_INT(SW_CHAR_EOP, sw_decode_char(encoded, parity, &decoded));

    parity = sw_encode_char(SPACEWIRE_EEP, &encoded);
    ASSERT_EQ_INT(SW_CHAR_EEP, sw_decode_char(encoded, parity, &decoded));

    ASSERT_EQ_INT(0, sw_encode_char(0xAB, NULL));
    ASSERT_EQ_INT(0xFFFF, sw_crc16(NULL, 12));

    return 0;
}

pus_test_result_t test_spacewire_codec_run_all(void)
{
    RUN_TEST(test_encode_decode);
    RUN_TEST(test_parity_error);
    RUN_TEST(test_crc_computation);
    RUN_TEST(test_codec_special_chars_and_invalid_args);
    return (pus_test_result_t){cunit_total_tests - cunit_overall_failures, cunit_total_tests};
}
