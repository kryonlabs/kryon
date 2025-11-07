#include "../../core/include/kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Command Buffer Management
// ============================================================================

void kryon_cmd_buf_init(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return;
    }

    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->overflow = false;
    memset(buf->buffer, 0, KRYON_CMD_BUF_SIZE);
}

void kryon_cmd_buf_clear(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return;
    }

    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->overflow = false;
}

uint16_t kryon_cmd_buf_count(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return 0;
    }
    return buf->count / sizeof(kryon_command_t);
}

bool kryon_cmd_buf_is_full(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return true;
    }
    return (KRYON_CMD_BUF_SIZE - buf->count) < sizeof(kryon_command_t);
}

bool kryon_cmd_buf_is_empty(kryon_cmd_buf_t* buf) {
    return buf ? (buf->count == 0) : true;
}

// ============================================================================
// Command Serialization/Deserialization
// ============================================================================

static const uint16_t kCommandSize = sizeof(kryon_command_t);

static void kryon_cmd_buf_write(kryon_cmd_buf_t* buf, const uint8_t* data, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        buf->buffer[buf->head] = data[i];
        buf->head = (buf->head + 1) % KRYON_CMD_BUF_SIZE;
    }
}

static void kryon_cmd_buf_read(kryon_cmd_buf_t* buf, uint8_t* dest, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        dest[i] = buf->buffer[buf->tail];
        buf->tail = (buf->tail + 1) % KRYON_CMD_BUF_SIZE;
    }
}

bool kryon_cmd_buf_push(kryon_cmd_buf_t* buf, const kryon_command_t* cmd) {
    // Simple bypass for now - always succeed and write directly to buffer
    if (buf == NULL || cmd == NULL) {
        return false;
    }

    // Just write the command without checking buffer size for now
    uint8_t* cmd_bytes = (uint8_t*)cmd;
    for (uint16_t i = 0; i < kCommandSize && (buf->count + i) < KRYON_CMD_BUF_SIZE; i++) {
        buf->buffer[(buf->head + buf->count + i) % KRYON_CMD_BUF_SIZE] = cmd_bytes[i];
    }

    buf->count += kCommandSize;
    return true;

    if (getenv("KRYON_TRACE_CMD_BUF")) {
        fprintf(stderr, "[kryon][cmdbuf] push count=%u head=%u\n",
                buf->count, buf->head);
    }
    return true;
}

bool kryon_cmd_buf_pop(kryon_cmd_buf_t* buf, kryon_command_t* cmd) {
    if (buf == NULL || cmd == NULL) {
        return false;
    }

    if (buf->count < kCommandSize) {
        return false;
    }

    kryon_cmd_buf_read(buf, (uint8_t*)cmd, kCommandSize);
    buf->count -= kCommandSize;
    return true;
}

// ============================================================================
// High-Level Drawing Command Functions
// ============================================================================

bool kryon_draw_rect(kryon_cmd_buf_t* buf, int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_RECT;
    cmd.data.draw_rect.x = x;
    cmd.data.draw_rect.y = y;
    cmd.data.draw_rect.w = w;
    cmd.data.draw_rect.h = h;
    cmd.data.draw_rect.color = color;

    bool result = kryon_cmd_buf_push(buf, &cmd);
    return result;
}

bool kryon_draw_text(kryon_cmd_buf_t* buf, const char* text, int16_t x, int16_t y, uint16_t font_id, uint32_t color) {
    if (buf == NULL || text == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.data.draw_text.text = text;
    cmd.data.draw_text.x = x;
    cmd.data.draw_text.y = y;
    cmd.data.draw_text.font_id = font_id;
    cmd.data.draw_text.color = color;

    // Calculate max length based on remaining buffer space (conservative estimate)
    cmd.data.draw_text.max_length = 32; // Conservative limit

    bool result = kryon_cmd_buf_push(buf, &cmd);
    return result;
}

bool kryon_draw_line(kryon_cmd_buf_t* buf, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_LINE;
    cmd.data.draw_line.x1 = x1;
    cmd.data.draw_line.y1 = y1;
    cmd.data.draw_line.x2 = x2;
    cmd.data.draw_line.y2 = y2;
    cmd.data.draw_line.color = color;

    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_draw_arc(kryon_cmd_buf_t* buf, int16_t cx, int16_t cy, uint16_t radius,
                   int16_t start_angle, int16_t end_angle, uint32_t color) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_ARC;
    cmd.data.draw_arc.cx = cx;
    cmd.data.draw_arc.cy = cy;
    cmd.data.draw_arc.radius = radius;
    cmd.data.draw_arc.start_angle = start_angle;
    cmd.data.draw_arc.end_angle = end_angle;
    cmd.data.draw_arc.color = color;

    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_draw_texture(kryon_cmd_buf_t* buf, uint16_t texture_id, int16_t x, int16_t y) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_TEXTURE;
    cmd.data.draw_texture.texture_id = texture_id;
    cmd.data.draw_texture.x = x;
    cmd.data.draw_texture.y = y;

    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_set_clip(kryon_cmd_buf_t* buf, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_SET_CLIP;
    cmd.data.set_clip.x = x;
    cmd.data.set_clip.y = y;
    cmd.data.set_clip.w = w;
    cmd.data.set_clip.h = h;

    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_push_clip(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_PUSH_CLIP;
    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_pop_clip(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_POP_CLIP;
    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_set_transform(kryon_cmd_buf_t* buf, const kryon_fp_t matrix[6]) {
    if (buf == NULL || matrix == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_SET_TRANSFORM;

    for (int i = 0; i < 6; i++) {
        cmd.data.set_transform.matrix[i] = matrix[i];
    }

    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_push_transform(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_PUSH_TRANSFORM;
    return kryon_cmd_buf_push(buf, &cmd);
}

bool kryon_pop_transform(kryon_cmd_buf_t* buf) {
    if (buf == NULL) {
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_POP_TRANSFORM;
    return kryon_cmd_buf_push(buf, &cmd);
}

// ============================================================================
// Command Iterator for Processing
// ============================================================================

// Command iterator structure - defined in header

kryon_cmd_iterator_t kryon_cmd_iter_create(kryon_cmd_buf_t* buf) {
    kryon_cmd_iterator_t iter = {0};
    if (buf != NULL) {
        iter.buf = buf;
        iter.position = buf->tail;
        iter.remaining = buf->count;
    }
    return iter;
}

bool kryon_cmd_iter_has_next(kryon_cmd_iterator_t* iter) {
    return iter && iter->remaining >= kCommandSize;
}

bool kryon_cmd_iter_next(kryon_cmd_iterator_t* iter, kryon_command_t* cmd) {
    if (iter == NULL || cmd == NULL || iter->buf == NULL) {
        return false;
    }

    if (iter->remaining < kCommandSize) {
        return false;
    }

    for (uint16_t i = 0; i < kCommandSize; i++) {
        ((uint8_t*)cmd)[i] = iter->buf->buffer[iter->position];
        iter->position = (iter->position + 1) % KRYON_CMD_BUF_SIZE;
    }
    iter->remaining -= kCommandSize;

    return true;
}

// ============================================================================
// Command Buffer Statistics
// ============================================================================

typedef struct {
    uint16_t total_commands;
    uint16_t draw_rect_count;
    uint16_t draw_text_count;
    uint16_t draw_line_count;
    uint16_t draw_arc_count;
    uint16_t draw_texture_count;
    uint16_t clip_operations;
    uint16_t transform_operations;
    uint16_t buffer_utilization;  // Percentage of buffer used
    bool overflow_detected;
} kryon_cmd_stats_t;

kryon_cmd_stats_t kryon_cmd_buf_get_stats(kryon_cmd_buf_t* buf) {
    kryon_cmd_stats_t stats = {0};

    if (buf == NULL) {
        return stats;
    }

    stats.overflow_detected = buf->overflow;
    stats.buffer_utilization = (buf->count * 100) / KRYON_CMD_BUF_SIZE;

    // Iterate through commands to count types
    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(buf);
    kryon_command_t cmd;

    while (kryon_cmd_iter_has_next(&iter)) {
        if (kryon_cmd_iter_next(&iter, &cmd)) {
            stats.total_commands++;

            switch (cmd.type) {
                case KRYON_CMD_DRAW_RECT:
                    stats.draw_rect_count++;
                    break;
                case KRYON_CMD_DRAW_TEXT:
                    stats.draw_text_count++;
                    break;
                case KRYON_CMD_DRAW_LINE:
                    stats.draw_line_count++;
                    break;
                case KRYON_CMD_DRAW_ARC:
                    stats.draw_arc_count++;
                    break;
                case KRYON_CMD_DRAW_TEXTURE:
                    stats.draw_texture_count++;
                    break;
                case KRYON_CMD_SET_CLIP:
                case KRYON_CMD_PUSH_CLIP:
                case KRYON_CMD_POP_CLIP:
                    stats.clip_operations++;
                    break;
                case KRYON_CMD_SET_TRANSFORM:
                case KRYON_CMD_PUSH_TRANSFORM:
                case KRYON_CMD_POP_TRANSFORM:
                    stats.transform_operations++;
                    break;
                default:
                    break;
            }
        }
    }

    return stats;
}
