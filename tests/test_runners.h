/**
 * @file test_runners.h
 * @brief Declarations of the per-suite test runners.
 */
#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

typedef struct
{
    int passed;
    int total;
} test_result_t;

test_result_t test_spacewire_spw_packet_run_all(void);
test_result_t test_spacewire_router_run_all(void);
test_result_t test_spacewire_packet_run_all(void);

#endif /* TEST_RUNNERS_H */
