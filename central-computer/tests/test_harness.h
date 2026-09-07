#ifndef CENTRAL_COMPUTER_TEST_HARNESS_H
#define CENTRAL_COMPUTER_TEST_HARNESS_H

// Deliberately dependency-free (no gtest/catch2 download - this project
// builds offline) test harness, in the same spirit as the LNC firmware's
// own host-compiled test_main.c: plain asserts that print a clear message
// and make the process exit non-zero on the first failure, which is all
// `ctest` needs to report pass/fail.
#include <cstdio>
#include <cstdlib>

static int g_test_failures = 0;

#define CHECK(cond)                                                         \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
      g_test_failures++;                                                    \
    }                                                                       \
  } while (0)

#define TEST_MAIN_EXIT()                                                    \
  do {                                                                      \
    if (g_test_failures == 0) {                                             \
      std::printf("ALL CHECKS PASSED\n");                                   \
      return 0;                                                             \
    }                                                                       \
    std::printf("%d CHECK(S) FAILED\n", g_test_failures);                   \
    return 1;                                                               \
  } while (0)

#endif  // CENTRAL_COMPUTER_TEST_HARNESS_H
