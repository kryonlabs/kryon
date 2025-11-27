#include "debug_backend.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Buffer growth size
#define BUFFER_GROW_SIZE 4096

// Internal helper to write to output
static void debug_write(DebugRenderer* renderer, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (renderer->config.output_mode == DEBUG_OUTPUT_BUFFER) {
        // Write to buffer
        char temp[1024];
        int len = vsnprintf(temp, sizeof(temp), fmt, args);
        if (len > 0) {
            // Ensure capacity
            while (renderer->buffer_size + len + 1 > renderer->buffer_capacity) {
                renderer->buffer_capacity += BUFFER_GROW_SIZE;
                renderer->buffer = realloc(renderer->buffer, renderer->buffer_capacity);
            }
            memcpy(renderer->buffer + renderer->buffer_size, temp, len);
            renderer->buffer_size += len;
            renderer->buffer[renderer->buffer_size] = '\0';
        }
    } else {
        // Write to file/stdout
        vfprintf(renderer->output, fmt, args);
    }

    va_end(args);
}

// Get component type as string
const char* debug_component_type_str(IRComponentType type) {
    switch (type) {
        case IR_COMPONENT_CONTAINER: return "Container";
        case IR_COMPONENT_TEXT: return "Text";
        case IR_COMPONENT_BUTTON: return "Button";
        case IR_COMPONENT_INPUT: return "Input";
        case IR_COMPONENT_CHECKBOX: return "Checkbox";
        case IR_COMPONENT_DROPDOWN: return "Dropdown";
        case IR_COMPONENT_ROW: return "Row";
        case IR_COMPONENT_COLUMN: return "Column";
        case IR_COMPONENT_CENTER: return "Center";
        case IR_COMPONENT_IMAGE: return "Image";
        case IR_COMPONENT_CANVAS: return "Canvas";
        case IR_COMPONENT_MARKDOWN: return "Markdown";
        case IR_COMPONENT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

// Format dimension to string
void debug_dimension_to_str(IRDimension dim, char* buffer, size_t size) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
            snprintf(buffer, size, "%.0fpx", dim.value);
            break;
        case IR_DIMENSION_PERCENT:
            snprintf(buffer, size, "%.0f%%", dim.value);
            break;
        case IR_DIMENSION_AUTO:
            snprintf(buffer, size, "auto");
            break;
        case IR_DIMENSION_FLEX:
            snprintf(buffer, size, "flex(%.0f)", dim.value);
            break;
        default:
            snprintf(buffer, size, "?");
    }
}

// Default configuration
DebugRendererConfig debug_renderer_config_default(void) {
    DebugRendererConfig config = {
        .output_mode = DEBUG_OUTPUT_STDOUT,
        .output_file = NULL,
        .max_depth = -1,
        .show_style = true,
        .show_bounds = true,
        .show_hidden = false,
        .compact = false
    };
    return config;
}

// Create debug renderer
DebugRenderer* debug_renderer_create(const DebugRendererConfig* config) {
    DebugRenderer* renderer = malloc(sizeof(DebugRenderer));
    if (!renderer) return NULL;

    renderer->config = config ? *config : debug_renderer_config_default();
    renderer->frame_count = 0;
    renderer->buffer = NULL;
    renderer->buffer_size = 0;
    renderer->buffer_capacity = 0;

    // Setup output
    switch (renderer->config.output_mode) {
        case DEBUG_OUTPUT_STDOUT:
            renderer->output = stdout;
            break;
        case DEBUG_OUTPUT_FILE:
            if (renderer->config.output_file) {
                renderer->output = fopen(renderer->config.output_file, "w");
                if (!renderer->output) {
                    renderer->output = stdout;
                }
            } else {
                renderer->output = stdout;
            }
            break;
        case DEBUG_OUTPUT_BUFFER:
            renderer->output = NULL;
            renderer->buffer_capacity = BUFFER_GROW_SIZE;
            renderer->buffer = malloc(renderer->buffer_capacity);
            renderer->buffer[0] = '\0';
            break;
    }

    return renderer;
}

// Destroy debug renderer
void debug_renderer_destroy(DebugRenderer* renderer) {
    if (!renderer) return;

    if (renderer->config.output_mode == DEBUG_OUTPUT_FILE &&
        renderer->output && renderer->output != stdout) {
        fclose(renderer->output);
    }

    if (renderer->buffer) {
        free(renderer->buffer);
    }

    free(renderer);
}

// Internal: print tree lines
static void print_tree_prefix(DebugRenderer* renderer, int* depths, int level, bool is_last) {
    for (int i = 0; i < level; i++) {
        if (i == level - 1) {
            debug_write(renderer, is_last ? "└── " : "├── ");
        } else {
            debug_write(renderer, depths[i] ? "│   " : "    ");
        }
    }
}

// Helper to convert alignment to string
static const char* debug_alignment_to_str(IRAlignment align) {
    switch (align) {
        case IR_ALIGNMENT_START: return "start";
        case IR_ALIGNMENT_CENTER: return "center";
        case IR_ALIGNMENT_END: return "end";
        case IR_ALIGNMENT_STRETCH: return "stretch";
        case IR_ALIGNMENT_SPACE_BETWEEN: return "space-between";
        case IR_ALIGNMENT_SPACE_AROUND: return "space-around";
        case IR_ALIGNMENT_SPACE_EVENLY: return "space-evenly";
        default: return "?";
    }
}

// Internal: render component recursively
static void render_component(DebugRenderer* renderer, IRComponent* comp, int depth, int* depth_stack, bool is_last) {
    if (!comp) return;

    // Check depth limit
    if (renderer->config.max_depth >= 0 && depth > renderer->config.max_depth) {
        return;
    }

    // Check visibility
    if (!renderer->config.show_hidden && comp->style && !comp->style->visible) {
        return;
    }

    // Print prefix
    if (depth > 0) {
        print_tree_prefix(renderer, depth_stack, depth, is_last);
    }

    // Print component info
    debug_write(renderer, "%s", debug_component_type_str(comp->type));
    debug_write(renderer, " id=%u", comp->id);

    // Tag if present
    if (comp->tag && strlen(comp->tag) > 0) {
        debug_write(renderer, " tag=\"%s\"", comp->tag);
    }

    // Text content (truncated)
    if (comp->text_content && strlen(comp->text_content) > 0) {
        char truncated[32];
        strncpy(truncated, comp->text_content, 28);
        truncated[28] = '\0';
        if (strlen(comp->text_content) > 28) {
            strcat(truncated, "...");
        }
        // Replace newlines with spaces
        for (char* c = truncated; *c; c++) {
            if (*c == '\n' || *c == '\r') *c = ' ';
        }
        debug_write(renderer, " \"%s\"", truncated);
    }

    // Bounds
    if (renderer->config.show_bounds && comp->rendered_bounds.valid) {
        debug_write(renderer, " [%.0f,%.0f,%.0f,%.0f]",
            comp->rendered_bounds.x,
            comp->rendered_bounds.y,
            comp->rendered_bounds.width,
            comp->rendered_bounds.height);
    }

    // Style info
    if (renderer->config.show_style && comp->style && !renderer->config.compact) {
        // Width/height
        char w[32], h[32];
        debug_dimension_to_str(comp->style->width, w, sizeof(w));
        debug_dimension_to_str(comp->style->height, h, sizeof(h));
        if (comp->style->width.type != IR_DIMENSION_AUTO ||
            comp->style->height.type != IR_DIMENSION_AUTO) {
            debug_write(renderer, " size=%s×%s", w, h);
        }

        // Background color if non-transparent
        if (comp->style->background.data.a > 0) {
            debug_write(renderer, " bg=#%02X%02X%02X",
                comp->style->background.data.r,
                comp->style->background.data.g,
                comp->style->background.data.b);
            if (comp->style->background.data.a < 255) {
                debug_write(renderer, "%02X", comp->style->background.data.a);
            }
        }

        // Border if present
        if (comp->style->border.width > 0) {
            debug_write(renderer, " border(%dpx #%02X%02X%02X r=%d)",
                (int)comp->style->border.width,
                comp->style->border.color.data.r,
                comp->style->border.color.data.g,
                comp->style->border.color.data.b,
                comp->style->border.radius);
        }

        // Padding if non-zero
        if (comp->style->padding.top > 0 || comp->style->padding.right > 0 ||
            comp->style->padding.bottom > 0 || comp->style->padding.left > 0) {
            debug_write(renderer, " pad=%.0f,%.0f,%.0f,%.0f",
                comp->style->padding.top, comp->style->padding.right,
                comp->style->padding.bottom, comp->style->padding.left);
        }

        // Margin if non-zero
        if (comp->style->margin.top > 0 || comp->style->margin.right > 0 ||
            comp->style->margin.bottom > 0 || comp->style->margin.left > 0) {
            debug_write(renderer, " margin=%.0f,%.0f,%.0f,%.0f",
                comp->style->margin.top, comp->style->margin.right,
                comp->style->margin.bottom, comp->style->margin.left);
        }

        // Visibility
        if (!comp->style->visible) {
            debug_write(renderer, " [HIDDEN]");
        }

        // Z-index if non-zero
        if (comp->style->z_index > 0) {
            debug_write(renderer, " z=%u", comp->style->z_index);
        }
    }

    // Layout info
    if (renderer->config.show_style && comp->layout && !renderer->config.compact) {
        // Flex grow/shrink if set
        if (comp->layout->flex.grow > 0 || comp->layout->flex.shrink > 0) {
            debug_write(renderer, " flex(grow=%u,shrink=%u)",
                comp->layout->flex.grow, comp->layout->flex.shrink);
        }

        // Layout direction (horizontal=1, vertical=0)
        debug_write(renderer, " dir=%s", comp->layout->flex.direction ? "row" : "col");

        // Gap if non-zero
        if (comp->layout->flex.gap > 0) {
            debug_write(renderer, " gap=%u", comp->layout->flex.gap);
        }

        // Min/max constraints
        if (comp->layout->min_width.type != IR_DIMENSION_AUTO ||
            comp->layout->min_height.type != IR_DIMENSION_AUTO ||
            comp->layout->max_width.type != IR_DIMENSION_AUTO ||
            comp->layout->max_height.type != IR_DIMENSION_AUTO) {
            debug_write(renderer, " constraints(");
            bool first = true;
            if (comp->layout->min_width.type != IR_DIMENSION_AUTO) {
                char buf[32];
                debug_dimension_to_str(comp->layout->min_width, buf, sizeof(buf));
                debug_write(renderer, "min-w=%s", buf);
                first = false;
            }
            if (comp->layout->min_height.type != IR_DIMENSION_AUTO) {
                char buf[32];
                debug_dimension_to_str(comp->layout->min_height, buf, sizeof(buf));
                debug_write(renderer, "%smin-h=%s", first ? "" : " ", buf);
                first = false;
            }
            if (comp->layout->max_width.type != IR_DIMENSION_AUTO) {
                char buf[32];
                debug_dimension_to_str(comp->layout->max_width, buf, sizeof(buf));
                debug_write(renderer, "%smax-w=%s", first ? "" : " ", buf);
                first = false;
            }
            if (comp->layout->max_height.type != IR_DIMENSION_AUTO) {
                char buf[32];
                debug_dimension_to_str(comp->layout->max_height, buf, sizeof(buf));
                debug_write(renderer, "%smax-h=%s", first ? "" : " ", buf);
            }
            debug_write(renderer, ")");
        }

        // Alignment info for containers
        if (comp->child_count > 0) {
            debug_write(renderer, " justify=%s align=%s",
                debug_alignment_to_str(comp->layout->flex.justify_content),
                debug_alignment_to_str(comp->layout->flex.cross_axis));
        }
    }

    // Events
    if (comp->events) {
        debug_write(renderer, " [events]");
    }

    // Custom data indicator (useful for debugging TabGroupState, etc.)
    if (comp->custom_data) {
        debug_write(renderer, " [custom_data]");
    }

    // Child count
    if (comp->child_count > 0) {
        debug_write(renderer, " (%u children)", comp->child_count);
    }

    debug_write(renderer, "\n");

    // Recurse to children
    if (comp->children && comp->child_count > 0) {
        // Update depth stack
        if (depth > 0) {
            depth_stack[depth - 1] = !is_last;
        }

        for (uint32_t i = 0; i < comp->child_count; i++) {
            bool child_is_last = (i == comp->child_count - 1);
            render_component(renderer, comp->children[i], depth + 1, depth_stack, child_is_last);
        }
    }
}

// Render IR tree as text
void debug_render_tree(DebugRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    // Depth stack for tree drawing (max 64 levels)
    int depth_stack[64] = {0};

    render_component(renderer, root, 0, depth_stack, true);
}

// Render single frame with header
void debug_render_frame(DebugRenderer* renderer, IRComponent* root) {
    if (!renderer) return;

    renderer->frame_count++;

    debug_write(renderer, "\n");
    debug_write(renderer, "═══════════════════════════════════════════════════════════════\n");
    debug_write(renderer, "  FRAME %u\n", renderer->frame_count);
    debug_write(renderer, "═══════════════════════════════════════════════════════════════\n");

    if (root) {
        debug_render_tree(renderer, root);
    } else {
        debug_write(renderer, "  (no root component)\n");
    }

    debug_write(renderer, "───────────────────────────────────────────────────────────────\n");

    // Flush output
    if (renderer->output) {
        fflush(renderer->output);
    }
}

// Get buffered output
const char* debug_get_output(DebugRenderer* renderer) {
    if (!renderer || renderer->config.output_mode != DEBUG_OUTPUT_BUFFER) {
        return NULL;
    }
    return renderer->buffer;
}

// Clear output buffer
void debug_clear_output(DebugRenderer* renderer) {
    if (!renderer || renderer->config.output_mode != DEBUG_OUTPUT_BUFFER) {
        return;
    }
    renderer->buffer_size = 0;
    if (renderer->buffer) {
        renderer->buffer[0] = '\0';
    }
}

// Convenience: render tree to stdout
void debug_print_tree(IRComponent* root) {
    DebugRendererConfig config = debug_renderer_config_default();
    DebugRenderer* renderer = debug_renderer_create(&config);
    if (renderer) {
        debug_render_frame(renderer, root);
        debug_renderer_destroy(renderer);
    }
}

// Convenience: render tree to file
void debug_print_tree_to_file(IRComponent* root, const char* filename) {
    DebugRendererConfig config = debug_renderer_config_default();
    config.output_mode = DEBUG_OUTPUT_FILE;
    config.output_file = filename;

    DebugRenderer* renderer = debug_renderer_create(&config);
    if (renderer) {
        debug_render_frame(renderer, root);
        debug_renderer_destroy(renderer);
    }
}
