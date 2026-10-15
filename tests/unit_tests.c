/**
 * @file    unit_tests.c
 * @brief   Unit-test entry point: runs each suite and reports totals
 *
 * Covers the default payload hooks; the overridden hooks are tested by the
 * separate binary built from test_payload_override.c.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#include "test_runners.h"

#include <stdio.h>

/** @brief Print one suite's pass count under a label. */
#define REPORT(label, r) printf("  %-14s Passed %d/%d\n\n", label ":", (r).passed, (r).total)

int main(void)
{
    test_result_t r;
    int total_passed = 0;
    int total_tests = 0;

    r = test_spacewire_spw_packet_run_all();
    REPORT("spw-packet", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_spacewire_router_run_all();
    REPORT("router", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_spacewire_ptp_run_all();
    REPORT("ptp", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_spacewire_payload_run_all();
    REPORT("payload", r);
    total_passed += r.passed;
    total_tests += r.total;

    printf("  ------------------------------\n");
    printf("  %-14s Passed %d/%d\n", "All UT:", total_passed, total_tests);

    return (total_passed == total_tests) ? 0 : 1;
}
