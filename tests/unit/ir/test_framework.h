#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// ANSI color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Global test statistics
static int g_test_count = 0;
static int g_test_passed = 0;
static int g_test_failed = 0;
static const char* g_current_test_name = NULL;
static const char* g_current_suite_name = NULL;

// Test suite management
#define BEGIN_SUITE(name) \
  do { \
    g_current_suite_name = name; \
    printf("\n" COLOR_BOLD COLOR_CYAN "═══════════════════════════════════════════════\n"); \
    printf("  Test Suite: %s\n", name); \
    printf("═══════════════════════════════════════════════" COLOR_RESET "\n"); \
  } while(0)

#define END_SUITE() \
  do { \
    printf("\n" COLOR_BOLD COLOR_CYAN "═══════════════════════════════════════════════" COLOR_RESET "\n"); \
    if (g_test_failed == 0) { \
      printf(COLOR_BOLD COLOR_GREEN "✓ All tests passed: %d/%d" COLOR_RESET "\n", \
             g_test_passed, g_test_count); \
    } else { \
      printf(COLOR_BOLD COLOR_RED "✗ Some tests failed: %d/%d passed, %d failed" COLOR_RESET "\n", \
             g_test_passed, g_test_count, g_test_failed); \
    } \
    printf(COLOR_BOLD COLOR_CYAN "═══════════════════════════════════════════════" COLOR_RESET "\n\n"); \
  } while(0)

// Test definition macro
#define TEST(test_name) \
  static void test_name(void); \
  static void test_name(void)

// Run a test function
#define RUN_TEST(test_func) \
  do { \
    g_current_test_name = #test_func; \
    g_test_count++; \
    printf(COLOR_BLUE "► Running: %s..." COLOR_RESET, #test_func); \
    fflush(stdout); \
    test_func(); \
    g_test_passed++; \
    printf("\r" COLOR_GREEN "✓ Passed: %s" COLOR_RESET "    \n", #test_func); \
  } while(0)

// Test failure handling
#define TEST_FAIL(message) \
  do { \
    g_test_failed++; \
    g_test_passed--; \
    printf("\r" COLOR_RED "✗ Failed: %s" COLOR_RESET "\n", g_current_test_name); \
    fprintf(stderr, COLOR_RED "  Error at %s:%d\n  %s" COLOR_RESET "\n", \
            __FILE__, __LINE__, message); \
    return; \
  } while(0)

#define TEST_FAIL_FMT(fmt, ...) \
  do { \
    g_test_failed++; \
    g_test_passed--; \
    printf("\r" COLOR_RED "✗ Failed: %s" COLOR_RESET "\n", g_current_test_name); \
    fprintf(stderr, COLOR_RED "  Error at %s:%d\n  " fmt COLOR_RESET "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__); \
    return; \
  } while(0)

// Assertion macros
#define ASSERT_TRUE(condition) \
  do { \
    if (!(condition)) { \
      TEST_FAIL_FMT("Expected true: %s", #condition); \
    } \
  } while(0)

#define ASSERT_FALSE(condition) \
  do { \
    if (condition) { \
      TEST_FAIL_FMT("Expected false: %s", #condition); \
    } \
  } while(0)

#define ASSERT_EQ(expected, actual) \
  do { \
    if ((expected) != (actual)) { \
      TEST_FAIL_FMT("Expected: %lld, Actual: %lld", \
                    (long long)(expected), (long long)(actual)); \
    } \
  } while(0)

#define ASSERT_NEQ(unexpected, actual) \
  do { \
    if ((unexpected) == (actual)) { \
      TEST_FAIL_FMT("Expected NOT equal to: %lld", (long long)(unexpected)); \
    } \
  } while(0)

#define ASSERT_NULL(ptr) \
  do { \
    if ((ptr) != NULL) { \
      TEST_FAIL_FMT("Expected NULL, got: %p", (void*)(ptr)); \
    } \
  } while(0)

#define ASSERT_NONNULL(ptr) \
  do { \
    if ((ptr) == NULL) { \
      TEST_FAIL("Expected non-NULL pointer"); \
    } \
  } while(0)

#define ASSERT_STR_EQ(expected, actual) \
  do { \
    if (strcmp((expected), (actual)) != 0) { \
      TEST_FAIL_FMT("Expected string: \"%s\", Actual: \"%s\"", \
                    (expected), (actual)); \
    } \
  } while(0)

#define ASSERT_STR_NEQ(unexpected, actual) \
  do { \
    if (strcmp((unexpected), (actual)) == 0) { \
      TEST_FAIL_FMT("Expected string NOT equal to: \"%s\"", (unexpected)); \
    } \
  } while(0)

#define ASSERT_FLOAT_EQ(expected, actual, epsilon) \
  do { \
    double diff = fabs((double)(expected) - (double)(actual)); \
    if (diff > (epsilon)) { \
      TEST_FAIL_FMT("Expected: %.6f, Actual: %.6f (diff: %.6f > %.6f)", \
                    (double)(expected), (double)(actual), diff, (double)(epsilon)); \
    } \
  } while(0)

#define ASSERT_MEM_EQ(expected, actual, size) \
  do { \
    if (memcmp((expected), (actual), (size)) != 0) { \
      TEST_FAIL_FMT("Memory comparison failed (%zu bytes)", (size_t)(size)); \
    } \
  } while(0)

// Range assertions
#define ASSERT_GT(value, min) \
  do { \
    if (!((value) > (min))) { \
      TEST_FAIL_FMT("Expected %lld > %lld", \
                    (long long)(value), (long long)(min)); \
    } \
  } while(0)

#define ASSERT_GTE(value, min) \
  do { \
    if (!((value) >= (min))) { \
      TEST_FAIL_FMT("Expected %lld >= %lld", \
                    (long long)(value), (long long)(min)); \
    } \
  } while(0)

#define ASSERT_LT(value, max) \
  do { \
    if (!((value) < (max))) { \
      TEST_FAIL_FMT("Expected %lld < %lld", \
                    (long long)(value), (long long)(max)); \
    } \
  } while(0)

#define ASSERT_LTE(value, max) \
  do { \
    if (!((value) <= (max))) { \
      TEST_FAIL_FMT("Expected %lld <= %lld", \
                    (long long)(value), (long long)(max)); \
    } \
  } while(0)

#define ASSERT_IN_RANGE(value, min, max) \
  do { \
    if (!((value) >= (min) && (value) <= (max))) { \
      TEST_FAIL_FMT("Expected %lld in range [%lld, %lld]", \
                    (long long)(value), (long long)(min), (long long)(max)); \
    } \
  } while(0)

// Main test runner
#define RUN_TEST_SUITE(suite_func) \
  int main(void) { \
    suite_func(); \
    return (g_test_failed == 0) ? 0 : 1; \
  }

#endif // TEST_FRAMEWORK_H
