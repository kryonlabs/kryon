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

    // Debug: Log buffer size and platform detection at initialization (disabled - too verbose)
    // fprintf(stderr, "[kryon][cmdbuf] INIT: platform=%d, buffer_size=%u, no_float=%d, no_heap=%d\n",
    //         KRYON_TARGET_PLATFORM, KRYON_CMD_BUF_SIZE, KRYON_NO_FLOAT, KRYON_NO_HEAP);
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
    if (buf == NULL || cmd == NULL) {
        return false;
    }

    // Special debug for polygon commands to track corruption
    if (cmd->type == KRYON_CMD_DRAW_POLYGON) {
        fprintf(stderr, "[kryon][cmdbuf] POLYGON DEBUG: Before push, vertex[0]=(%.2f,%.2f) vertex[1]=(%.2f,%.2f) vertex[2]=(%.2f,%.2f)\n",
                kryon_fp_to_float(cmd->data.draw_polygon.vertex_storage[0]),
                kryon_fp_to_float(cmd->data.draw_polygon.vertex_storage[1]),
                kryon_fp_to_float(cmd->data.draw_polygon.vertex_storage[2]),
                kryon_fp_to_float(cmd->data.draw_polygon.vertex_storage[3]),
                kryon_fp_to_float(cmd->data.draw_polygon.vertex_storage[4]),
                kryon_fp_to_float(cmd->data.draw_polygon.vertex_storage[5]));
    }

    // Check if there's enough space for the full command
    if (buf->count + kCommandSize > KRYON_CMD_BUF_SIZE) {
        // Always show buffer overflow warnings - this is critical for debugging
        fprintf(stderr, "[kryon][cmdbuf] BUFFER FULL! count=%u size=%u capacity=%u\n",
                buf->count, kCommandSize, KRYON_CMD_BUF_SIZE);
        fprintf(stderr, "[kryon][cmdbuf] CRITICAL: Command dropped - increase KRYON_CMD_BUF_SIZE\n");
        return false;
    }

    // Write the full command to the buffer
    uint8_t* cmd_bytes = (uint8_t*)cmd;
    // Write at head position, then advance head
    for (uint16_t i = 0; i < kCommandSize; i++) {
        buf->buffer[buf->head] = cmd_bytes[i];
        buf->head = (buf->head + 1) % KRYON_CMD_BUF_SIZE;
    }

    buf->count += kCommandSize;

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

bool kryon_draw_polygon(kryon_cmd_buf_t* buf, const kryon_fp_t* vertices, uint16_t vertex_count,
                       uint32_t color, bool filled) {
    // Always debug entry to see if function is called
    fprintf(stderr, "[kryon][cmdbuf] ENTRY: kryon_draw_polygon called with vertex_count=%d\n", vertex_count);

    if (buf == NULL || vertices == NULL || vertex_count < 3) {
        fprintf(stderr, "[kryon][cmdbuf] ERROR: buf=%p vertices=%p vertex_count=%d\n",
                (void*)buf, (void*)vertices, vertex_count);
        return false;
    }

    // Limit vertex count to prevent buffer overflow
    if (vertex_count > 16) {
        fprintf(stderr, "[kryon][cmdbuf] ERROR: Too many vertices (%d), max supported is 16\n", vertex_count);
        return false;
    }

    kryon_command_t cmd = {0};
    cmd.type = KRYON_CMD_DRAW_POLYGON;
    cmd.data.draw_polygon.vertices = vertices;
    cmd.data.draw_polygon.vertex_count = vertex_count;
    cmd.data.draw_polygon.color = color;
    cmd.data.draw_polygon.filled = filled;

    // Copy vertices inline to fix pointer dereference issues
    // Use vertex_storage instead of external pointer to avoid invalid memory access
    uint16_t num_floats = vertex_count * 2; // x,y pairs

    // Cap at our storage size to prevent buffer overflow
    if (vertex_count > 16 || num_floats > 32) {
        fprintf(stderr, "[kryon][cmdbuf] ERROR: Too many vertices (%d), max supported is 16\n", vertex_count);
        return false;
    }

    // Debug: Trace vertex copying
    if (getenv("KRYON_TRACE_POLYGON")) {
        fprintf(stderr, "[kryon][cmdbuf] About to copy %d vertices (%d floats):\n", vertex_count, num_floats);
        for (uint16_t i = 0; i < num_floats; i++) {
            fprintf(stderr, "[kryon][cmdbuf]   input[%d]: raw=%d float=%.2f\n",
                    i, (int32_t)vertices[i], kryon_fp_to_float(vertices[i]));
        }
    }

    // Copy vertices into inline storage
    for (uint16_t i = 0; i < num_floats; i++) {
        if (getenv("KRYON_TRACE_POLYGON")) {
            fprintf(stderr, "[kryon][cmdbuf] COPY[%d]: input=%.2f (raw=%d) -> storage\n",
                    i, kryon_fp_to_float(vertices[i]), (int32_t)vertices[i]);
        }
        cmd.data.draw_polygon.vertex_storage[i] = vertices[i];
        if (getenv("KRYON_TRACE_POLYGON")) {
            fprintf(stderr, "[kryon][cmdbuf] COPY[%d]: stored=%.2f (raw=%d)\n",
                    i, kryon_fp_to_float(cmd.data.draw_polygon.vertex_storage[i]),
                    (int32_t)cmd.data.draw_polygon.vertex_storage[i]);
        }
    }

    // Point to the copied vertex data
    cmd.data.draw_polygon.vertices = cmd.data.draw_polygon.vertex_storage;

    if (getenv("KRYON_TRACE_POLYGON")) {
        fprintf(stderr, "[kryon][cmdbuf] Copied %d vertices to storage:\n", vertex_count);
        for (uint16_t i = 0; i < vertex_count; i++) {
            fprintf(stderr, "[kryon][cmdbuf]   vertex[%d]: storage=(%.2f,%.2f)\n",
                    i, kryon_fp_to_float(cmd.data.draw_polygon.vertex_storage[i * 2]),
                          kryon_fp_to_float(cmd.data.draw_polygon.vertex_storage[i * 2 + 1]));
        }
    }

    if (getenv("KRYON_TRACE_CMD_BUF")) {
        fprintf(stderr, "[kryon][cmdbuf] polygon: vertices=%d color=0x%08x filled=%d\n",
                vertex_count, color, filled);
    }

    bool push_result = kryon_cmd_buf_push(buf, &cmd);
    fprintf(stderr, "[kryon][cmdbuf] POLYGON PUSH: result=%d cmd.type=%d\n", push_result, cmd.type);
    return push_result;
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

    uint16_t start_pos = iter->position;
    for (uint16_t i = 0; i < kCommandSize; i++) {
        ((uint8_t*)cmd)[i] = iter->buf->buffer[iter->position];
        iter->position = (iter->position + 1) % KRYON_CMD_BUF_SIZE;
    }
    iter->remaining -= kCommandSize;

    // CRITICAL FIX: For polygon commands, fix the vertices pointer
    // After copying bytes from buffer, the vertices pointer is invalid
    // It needs to point to the vertex_storage within THIS command
    if (cmd->type == KRYON_CMD_DRAW_POLYGON) {
        cmd->data.draw_polygon.vertices = cmd->data.draw_polygon.vertex_storage;
    }

    return true;
}

// ============================================================================
// Command Buffer Statistics
// ============================================================================
// kryon_cmd_stats_t is now defined in kryon.h

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
                case KRYON_CMD_DRAW_POLYGON:
                    stats.draw_polygon_count++;
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
