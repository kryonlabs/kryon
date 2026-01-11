/**
 * @file ir_log.h
 * @brief Unified logging system for Kryon IR
 *
 * Platform-aware logging that works on desktop, Android, and other platforms.
 */

#ifndef IR_LOG_H
#define IR_LOG_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log levels for filtering messages
 */
typedef enum {
    IR_LOG_DEBUG = 0,
    IR_LOG_INFO,
    IR_LOG_WARN,
    IR_LOG_ERROR
} IRLogLevel;

/**
 * Set the minimum log level to display
 * Messages below this level will be filtered out
 */
void ir_log_set_level(IRLogLevel level);

/**
 * Get the current minimum log level
 */
IRLogLevel ir_log_get_level(void);

/**
 * Log a message with the specified level, tag, and format
 *
 * @param level The log level
 * @param tag A short tag identifying the source (e.g., "JSON", "LAYOUT")
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void ir_log(IRLogLevel level, const char* tag, const char* fmt, ...);

/**
 * Convenience macros for logging at specific levels
 */
#define IR_LOG_DEBUG(tag, ...) ir_log(IR_LOG_DEBUG, tag, __VA_ARGS__)
#define IR_LOG_INFO(tag, ...)  ir_log(IR_LOG_INFO, tag, __VA_ARGS__)
#define IR_LOG_WARN(tag, ...)  ir_log(IR_LOG_WARN, tag, __VA_ARGS__)
#define IR_LOG_ERROR(tag, ...) ir_log(IR_LOG_ERROR, tag, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // IR_LOG_H
