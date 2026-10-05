#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

/*
 * Minimal test harness. A failed check prints its location and the test continues; TEST_FINISH() returns the
 * process exit code. Include it in one test source per executable.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long test_check_count;
static unsigned long test_failure_count;

static inline bool test_check(bool is_passed, const char* expression, const char* file, int line) {
  ++test_check_count;
  if (!is_passed) {
    ++test_failure_count;
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
  }
  return is_passed;
}

static inline bool test_check_equal(unsigned long long expected, unsigned long long actual, const char* expression,
                                    const char* file, int line) {
  ++test_check_count;
  if (expected != actual) {
    ++test_failure_count;
    fprintf(stderr, "%s:%d: %s: expected %llu (0x%llX), got %llu (0x%llX)\n", file, line, expression, expected,
            expected, actual, actual);
    return false;
  }
  return true;
}

static inline int test_finish(const char* program) {
  printf("%s: %lu checks, %lu failed\n", program, test_check_count, test_failure_count);
  return (test_failure_count == 0U) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#define TEST_ASSERT(condition) test_check((condition), #condition, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL(expected, actual) \
  test_check_equal((unsigned long long)(expected), (unsigned long long)(actual), #actual, __FILE__, __LINE__)
#define TEST_RUN(test_function)          \
  do {                                   \
    printf("RUN  %s\n", #test_function); \
    test_function();                     \
  } while (0)
#define TEST_FINISH() test_finish(__FILE__)

#endif /* TEST_ASSERT_H */
