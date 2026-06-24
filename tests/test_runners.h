#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

typedef struct
{
    int passed;
    int total;
} pus_test_result_t;

pus_test_result_t test_spacewire_codec_run_all(void);
pus_test_result_t test_spacewire_frame_run_all(void);
pus_test_result_t test_spacewire_router_run_all(void);
pus_test_result_t test_spacewire_packet_run_all(void);

#endif /* TEST_RUNNERS_H */
