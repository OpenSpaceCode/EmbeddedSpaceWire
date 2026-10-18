/**
 * @file    test_runners.h
 * @brief   Declarations of the per-suite test runners
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#ifndef TEST_RUNNERS_H
#define TEST_RUNNERS_H

/**
 * @brief Outcome of one test suite.
 */
typedef struct
{
    int passed; /**< Tests that passed. */
    int total;  /**< Tests that ran. */
} test_result_t;

/**
 * @brief Run every SpaceWire packet builder test.
 *
 * @return Passed and total test counts of the suite.
 */
test_result_t test_spacewire_spw_packet_run_all(void);

/**
 * @brief Run every routing switch test.
 *
 * @return Passed and total test counts of the suite.
 */
test_result_t test_spacewire_router_run_all(void);

/**
 * @brief Run every CCSDS packet transfer protocol test.
 *
 * @return Passed and total test counts of the suite.
 */
test_result_t test_spacewire_ptp_run_all(void);

/**
 * @brief Run every test of the default payload hooks.
 *
 * @return Passed and total test counts of the suite.
 */
test_result_t test_spacewire_payload_run_all(void);

#endif /* TEST_RUNNERS_H */
