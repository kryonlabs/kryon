/**
 * @file ir_string_builder.c
 * @brief String builder implementation
 */

#define _GNU_SOURCE
#include "ir_string_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define DEFAULT_CAPACITY 4096
#define GROWTH_FACTOR 2

IRStringBuilder* ir_sb_create(size_t initial_capacity) {
    IRStringBuilder* sb = calloc(1, sizeof(IRStringBuilder));
    if (!sb) return NULL;

    if (initial_capacity == 0) initial_capacity = DEFAULT_CAPACITY;

    sb->buffer = malloc(initial_capacity);
    if (!sb->buffer) {
        free(sb);
        return NULL;
    }

    sb->buffer[0] = '\0';
    sb->size = 0;
    sb->capacity = initial_capacity;

    return sb;
}

void ir_sb_free(IRStringBuilder* sb) {
    if (!sb) return;
    free(sb->buffer);
    free(sb);
}

bool ir_sb_reserve(IRStringBuilder* sb, size_t capacity) {
    if (!sb) return false;

    if (capacity <= sb->capacity) return true;

    size_t new_capacity = sb->capacity;
    while (new_capacity < capacity) {
        new_capacity *= GROWTH_FACTOR;
    }

    char* new_buffer = realloc(sb->buffer, new_capacity);
    if (!new_buffer) return false;

    // Update buffer pointer (may have moved)
    size_t offset __attribute__((unused)) = sb->buffer - (char*)NULL;
    sb->buffer = new_buffer;
    sb->capacity = new_capacity;

    return true;
}

static bool sb_ensure_capacity(IRStringBuilder* sb, size_t needed) {
    if (!sb) return false;
    if (sb->size + needed < sb->capacity) return true;
    return ir_sb_reserve(sb, sb->capacity * GROWTH_FACTOR);
}

bool ir_sb_append(IRStringBuilder* sb, const char* str) {
    if (!sb || !str) return false;

    size_t len = strlen(str);
    if (!sb_ensure_capacity(sb, len + 1)) return false;

    memcpy(sb->buffer + sb->size, str, len + 1);
    sb->size += len;

    return true;
}

bool ir_sb_append_n(IRStringBuilder* sb, const char* str, size_t len) {
    if (!sb || !str || len == 0) return false;

    if (!sb_ensure_capacity(sb, len + 1)) return false;

    memcpy(sb->buffer + sb->size, str, len);
    sb->buffer[sb->size + len] = '\0';
    sb->size += len;

    return true;
}

bool ir_sb_appendf(IRStringBuilder* sb, const char* fmt, ...) {
    if (!sb || !fmt) return false;

    va_list args;
    va_start(args, fmt);

    char temp[4096];
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    if (len < 0) return false;
    if (len >= (int)sizeof(temp)) {
        // For very long strings, would need dynamic allocation
        len = sizeof(temp) - 1;
    }

    return ir_sb_append_n(sb, temp, len);
}

bool ir_sb_append_line(IRStringBuilder* sb, const char* line) {
    if (!ir_sb_append(sb, line)) return false;
    return ir_sb_append(sb, "\n");
}

bool ir_sb_append_linef(IRStringBuilder* sb, const char* fmt, ...) {
    if (!sb || !fmt) return false;

    va_list args;
    va_start(args, fmt);

    char temp[4096];
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    return ir_sb_append_line(sb, temp);
}

bool ir_sb_indent(IRStringBuilder* sb, int level) {
    if (!sb) return false;

    for (int i = 0; i < level; i++) {
        if (!ir_sb_append(sb, "  ")) return false;
    }

    return true;
}

char* ir_sb_build(IRStringBuilder* sb) {
    if (!sb) return NULL;
    return sb->buffer;  // Caller now owns the buffer
}

void ir_sb_clear(IRStringBuilder* sb) {
    if (!sb) return;
    sb->size = 0;
    if (sb->buffer) sb->buffer[0] = '\0';
}

size_t ir_sb_length(const IRStringBuilder* sb) {
    return sb ? sb->size : 0;
}

IRStringBuilder* ir_sb_clone(IRStringBuilder* sb) {
    if (!sb) return NULL;

    IRStringBuilder* clone = ir_sb_create(sb->capacity);
    if (!clone) return NULL;

    if (!ir_sb_append_n(clone, sb->buffer, sb->size)) {
        ir_sb_free(clone);
        return NULL;
    }

    return clone;
}
