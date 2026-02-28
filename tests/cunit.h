/* Tiny C unit test helpers. Suitable for embedding in small projects.
 * Usage: include this header in a single C test file and implement test
 * functions returning 0 on success, non-zero on failure. Use RUN_TEST(fn).
 */
#ifndef CUNIT_H
#define CUNIT_H

#include <stdio.h>
#include <string.h>

static int cunit_overall_failures = 0;

#define ASSERT_EQ_INT(expected, actual)                                        \
  do {                                                                         \
    if ((int)(expected) != (int)(actual)) {                                    \
      printf("ASSERT_EQ_INT failed: %s:%d: expected %d got %d\n", __FILE__,    \
             __LINE__, (int)(expected), (int)(actual));                        \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_EQ_MEM(a, b, len)                                               \
  do {                                                                         \
    if (memcmp((a), (b), (len)) != 0) {                                        \
      printf("ASSERT_EQ_MEM failed: %s:%d\n", __FILE__, __LINE__);             \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("ASSERT_TRUE failed: %s:%d\n", __FILE__, __LINE__);               \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define RUN_TEST(fn)                                                           \
  do {                                                                         \
    printf("RUN %s\n", #fn);                                                   \
    int r = fn();                                                              \
    if (r == 0)                                                                \
      printf("PASS %s\n", #fn);                                                \
    else {                                                                     \
      printf("FAIL %s\n", #fn);                                                \
      cunit_overall_failures++;                                                \
    }                                                                          \
  } while (0)

#endif /* CUNIT_H */
