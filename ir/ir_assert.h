#ifndef IR_ASSERT_H
#define IR_ASSERT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Assertion levels
#define IR_ASSERT_LEVEL_NONE     0  // No assertions (release builds)
#define IR_ASSERT_LEVEL_CRITICAL 1  // Only critical assertions (NULL checks, bounds)
#define IR_ASSERT_LEVEL_NORMAL   2  // Standard assertions (invariants, preconditions)
#define IR_ASSERT_LEVEL_PARANOID 3  // Paranoid mode (expensive checks)

// Set default assertion level based on DEBUG macro
#ifndef IR_ASSERT_LEVEL
  #ifdef DEBUG
    #define IR_ASSERT_LEVEL IR_ASSERT_LEVEL_NORMAL
  #else
    #define IR_ASSERT_LEVEL IR_ASSERT_LEVEL_CRITICAL
  #endif
#endif

// Runtime assertion macro
#if IR_ASSERT_LEVEL >= IR_ASSERT_LEVEL_CRITICAL
  #define IR_ASSERT(condition, message) \
    do { \
      if (!(condition)) { \
        fprintf(stderr, "[IR ASSERTION FAILED] %s:%d: %s\n  Condition: %s\n", \
                __FILE__, __LINE__, message, #condition); \
        abort(); \
      } \
    } while(0)
#else
  #define IR_ASSERT(condition, message) ((void)0)
#endif

// Critical assertion (always enabled unless explicitly disabled)
#if IR_ASSERT_LEVEL >= IR_ASSERT_LEVEL_CRITICAL
  #define IR_ASSERT_CRITICAL(condition, message) \
    do { \
      if (!(condition)) { \
        fprintf(stderr, "[IR CRITICAL] %s:%d: %s\n  Condition: %s\n", \
                __FILE__, __LINE__, message, #condition); \
        abort(); \
      } \
    } while(0)
#else
  #define IR_ASSERT_CRITICAL(condition, message) ((void)0)
#endif

// Paranoid assertion (expensive checks, only in paranoid mode)
#if IR_ASSERT_LEVEL >= IR_ASSERT_LEVEL_PARANOID
  #define IR_ASSERT_PARANOID(condition, message) IR_ASSERT(condition, message)
#else
  #define IR_ASSERT_PARANOID(condition, message) ((void)0)
#endif

// Parameter validation macros that return early on failure
#define IR_REQUIRE(condition, retval) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "[IR REQUIRE FAILED] %s:%d: %s\n", \
              __FILE__, __LINE__, #condition); \
      return retval; \
    } \
  } while(0)

#define IR_REQUIRE_VOID(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "[IR REQUIRE FAILED] %s:%d: %s\n", \
              __FILE__, __LINE__, #condition); \
      return; \
    } \
  } while(0)

// NULL pointer checks
#define IR_REQUIRE_NONNULL(ptr, retval) \
  IR_REQUIRE((ptr) != NULL, retval)

#define IR_REQUIRE_NONNULL_VOID(ptr) \
  IR_REQUIRE_VOID((ptr) != NULL)

// Bounds checking
#define IR_REQUIRE_BOUNDS(index, max, retval) \
  IR_REQUIRE((index) < (max), retval)

// Compile-time assertion (fails at compile time if condition is false)
#define IR_STATIC_ASSERT(condition, message) \
  _Static_assert(condition, message)

// Safe memory management macros
#define SAFE_FREE(ptr) \
  do { \
    if (ptr) { \
      free(ptr); \
      ptr = NULL; \
    } \
  } while(0)

#define SAFE_FREE_ARRAY(ptr, count, destroy_func) \
  do { \
    if (ptr) { \
      for (size_t i = 0; i < (count); i++) { \
        destroy_func(&(ptr)[i]); \
      } \
      free(ptr); \
      ptr = NULL; \
    } \
  } while(0)

// Unreachable code marker (helps compiler optimize)
#if defined(__GNUC__) || defined(__clang__)
  #define IR_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
  #define IR_UNREACHABLE() __assume(0)
#else
  #define IR_UNREACHABLE() abort()
#endif

// Mark intentionally unused variables/parameters
#define IR_UNUSED(x) ((void)(x))

// Check for integer overflow before allocation
#define IR_CHECK_OVERFLOW_MUL(a, b, retval) \
  do { \
    if ((a) > 0 && (b) > 0 && (a) > SIZE_MAX / (b)) { \
      fprintf(stderr, "[IR OVERFLOW] %s:%d: Integer overflow in multiplication\n", \
              __FILE__, __LINE__); \
      return retval; \
    } \
  } while(0)

#define IR_CHECK_OVERFLOW_ADD(a, b, retval) \
  do { \
    if ((a) > SIZE_MAX - (b)) { \
      fprintf(stderr, "[IR OVERFLOW] %s:%d: Integer overflow in addition\n", \
              __FILE__, __LINE__); \
      return retval; \
    } \
  } while(0)

#endif // IR_ASSERT_H
