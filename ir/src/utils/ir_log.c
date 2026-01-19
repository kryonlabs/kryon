/**
 * @file ir_log.c
 * @brief Unified logging system implementation
 */

#ifndef PLAN9
#define _GNU_SOURCE
#endif
#include "../include/ir_log.h"
#include "../include/ir_platform.h"
#include <stdarg.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static IRLogLevel g_current_level = IR_LOG_INFO;

void ir_log_set_level(IRLogLevel level) {
    g_current_level = level;
}

IRLogLevel ir_log_get_level(void) {
    return g_current_level;
}

static const char* level_to_string(IRLogLevel level) {
    switch (level) {
        case IR_LOG_DEBUG: return "DEBUG";
        case IR_LOG_INFO:  return "INFO";
        case IR_LOG_WARN:  return "WARN";
        case IR_LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void ir_log(IRLogLevel level, const char* tag, const char* fmt, ...) {
    if (level < g_current_level) return;

    va_list args;
    va_start(args, fmt);

#ifdef __ANDROID__
    int android_level = ANDROID_LOG_INFO;
    switch (level) {
        case IR_LOG_DEBUG: android_level = ANDROID_LOG_DEBUG; break;
        case IR_LOG_INFO:  android_level = ANDROID_LOG_INFO; break;
        case IR_LOG_WARN:  android_level = ANDROID_LOG_WARN; break;
        case IR_LOG_ERROR: android_level = ANDROID_LOG_ERROR; break;
    }
    __android_log_vprint(android_level, tag, fmt, args);
#else
    fprintf(stderr, "[%s] [%s] ", level_to_string(level), tag);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
#endif

    va_end(args);
}
