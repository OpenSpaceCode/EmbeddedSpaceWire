/**
 * @file    cunit.h
 * @brief   Tiny C unit-test helpers: assertions and a test-runner macro
 *
 * Include in a C test file and implement test functions returning 0 on
 * success, non-zero on failure. Use RUN_TEST(fn). The counters are static,
 * so each translation unit keeps its own totals.
 *
 * Copyright 2026 OpenSpaceCode contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */
#ifndef CUNIT_H
#define CUNIT_H

#include <stdio.h>
#include <string.h>

/** @brief Number of failed tests in this translation unit. */
static int cunit_overall_failures = 0;

/** @brief Number of tests run in this translation unit. */
static int cunit_total_tests = 0;

/** @brief Fail the test unless two values are equal when converted to int. */
#define ASSERT_EQ_INT(expected, actual)                                                            \
    do                                                                                             \
    {                                                                                              \
        if ((int)(expected) != (int)(actual))                                                      \
        {                                                                                          \
            printf("ASSERT_EQ_INT failed: %s:%d: expected %d got %d\n",                            \
                   __FILE__,                                                                       \
                   __LINE__,                                                                       \
                   (int)(expected),                                                                \
                   (int)(actual));                                                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/** @brief Fail the test unless two unsigned values are equal, compared without truncation. */
#define ASSERT_EQ_UINT(expected, actual)                                                           \
    do                                                                                             \
    {                                                                                              \
        if ((unsigned long long)(expected) != (unsigned long long)(actual))                        \
        {                                                                                          \
            printf("ASSERT_EQ_UINT failed: %s:%d: expected %llu got %llu\n",                       \
                   __FILE__,                                                                       \
                   __LINE__,                                                                       \
                   (unsigned long long)(expected),                                                 \
                   (unsigned long long)(actual));                                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/** @brief Fail the test unless the first @p len octets of @p a and @p b are equal. */
#define ASSERT_EQ_MEM(a, b, len)                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (memcmp((a), (b), (len)) != 0)                                                          \
        {                                                                                          \
            printf("ASSERT_EQ_MEM failed: %s:%d\n", __FILE__, __LINE__);                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/** @brief Fail the test unless @p cond is true. */
#define ASSERT_TRUE(cond)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            printf("ASSERT_TRUE failed: %s:%d\n", __FILE__, __LINE__);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/** @brief Run test function @p fn, print RUN/PASS/FAIL and update the counters. */
#define RUN_TEST(fn)                                                                               \
    do                                                                                             \
    {                                                                                              \
        printf("RUN %s\n", #fn);                                                                   \
        cunit_total_tests++;                                                                       \
        int r = fn();                                                                              \
        if (r == 0)                                                                                \
        {                                                                                          \
            printf("PASS %s\n", #fn);                                                              \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("FAIL %s\n", #fn);                                                              \
            cunit_overall_failures++;                                                              \
        }                                                                                          \
    } while (0)

#endif /* CUNIT_H */
