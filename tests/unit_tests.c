#include "test_runners.h"

#include <stdio.h>

#define REPORT(label, r) printf("  %-14s Passed %d/%d\n\n", label ":", (r).passed, (r).total)

int main(void)
{
    test_result_t r;
    int total_passed = 0;
    int total_tests = 0;

    r = test_spacewire_frame_run_all();
    REPORT("frame", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_spacewire_router_run_all();
    REPORT("router", r);
    total_passed += r.passed;
    total_tests += r.total;

    r = test_spacewire_packet_run_all();
    REPORT("packet", r);
    total_passed += r.passed;
    total_tests += r.total;

    printf("  ------------------------------\n");
    printf("  %-14s Passed %d/%d\n", "All UT:", total_passed, total_tests);

    return (total_passed == total_tests) ? 0 : 1;
}
