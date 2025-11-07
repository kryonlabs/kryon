#include "include/kryon.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// Canvas Component
// ============================================================================

// Canvas state structure - defined in header

static void canvas_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible) {
        return;
    }

    kryon_canvas_state_t* canvas_state = (kryon_canvas_state_t*)self->state;
    if (canvas_state == NULL) return;

    // Draw background
    if ((canvas_state->background_color & 0xFF) != 0) {
        kryon_draw_rect(buf,
                       KRYON_FP_TO_INT(self->x),
                       KRYON_FP_TO_INT(self->y),
                       KRYON_FP_TO_INT(self->width),
                       KRYON_FP_TO_INT(self->height),
                       canvas_state->background_color);
    }

    // Call custom draw function
    if (canvas_state->on_draw != NULL) {
        canvas_state->on_draw(self, buf);
    }
}

static void canvas_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) return;

    kryon_canvas_state_t* canvas_state = (kryon_canvas_state_t*)self->state;

    // Use explicit size if set, otherwise use available space
    if (canvas_state->width > 0) {
        self->width = KRYON_FP_FROM_INT(canvas_state->width);
    } else {
        self->width = available_width;
        canvas_state->width = KRYON_FP_TO_INT(available_width);
    }

    if (canvas_state->height > 0) {
        self->height = KRYON_FP_FROM_INT(canvas_state->height);
    } else {
        self->height = available_height;
        canvas_state->height = KRYON_FP_TO_INT(available_height);
    }

    kryon_component_mark_clean(self);
}

static bool canvas_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (self == NULL || event == NULL || self->state == NULL) return false;

    kryon_canvas_state_t* canvas_state = (kryon_canvas_state_t*)self->state;

    switch (event->type) {
        case KRYON_EVT_TIMER:
            // Update animation timer
            if (canvas_state->on_update != NULL) {
                float delta_time = 0.016f; // 60 FPS assumption
                canvas_state->on_update(self, delta_time);
            }
            return true;

        default:
            break;
    }

    return false;
}

const kryon_component_ops_t kryon_canvas_ops = {
    .render = canvas_render,
    .on_event = canvas_on_event,
    .destroy = NULL,
    .layout = canvas_layout
};

// ============================================================================
// Input Component
// ============================================================================

// Input state structure - defined in header

static void input_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible) {
        return;
    }

    kryon_input_state_t* input_state = (kryon_input_state_t*)self->state;
    if (input_state == NULL) return;

    // Draw background
    kryon_draw_rect(buf,
                   KRYON_FP_TO_INT(self->x),
                   KRYON_FP_TO_INT(self->y),
                   KRYON_FP_TO_INT(self->width),
                   KRYON_FP_TO_INT(self->height),
                   input_state->background_color);

    // Draw border
    if (input_state->border_width > 0) {
        uint16_t bw = input_state->border_width;
        // Top border
        kryon_draw_rect(buf,
                       KRYON_FP_TO_INT(self->x), KRYON_FP_TO_INT(self->y),
                       KRYON_FP_TO_INT(self->width), bw,
                       input_state->border_color);
        // Bottom border
        kryon_draw_rect(buf,
                       KRYON_FP_TO_INT(self->x),
                       KRYON_FP_TO_INT(self->y + self->height) - bw,
                       KRYON_FP_TO_INT(self->width), bw,
                       input_state->border_color);
        // Left border
        kryon_draw_rect(buf,
                       KRYON_FP_TO_INT(self->x), KRYON_FP_TO_INT(self->y),
                       bw, KRYON_FP_TO_INT(self->height),
                       input_state->border_color);
        // Right border
        kryon_draw_rect(buf,
                       KRYON_FP_TO_INT(self->x + self->width) - bw,
                       KRYON_FP_TO_INT(self->y),
                       bw, KRYON_FP_TO_INT(self->height),
                       input_state->border_color);
    }

    // Draw text or placeholder
    const char* text_to_draw = input_state->text;
    uint32_t text_color = input_state->text_color;

    if (strlen(text_to_draw) == 0) {
        text_to_draw = input_state->placeholder;
        text_color = input_state->placeholder_color;
    }

    if (input_state->password_mode && strlen(text_to_draw) > 0) {
        // Show asterisks for password mode
        static char masked_text[KRYON_MAX_TEXT_LENGTH];
        for (size_t i = 0; i < strlen(text_to_draw) && i < KRYON_MAX_TEXT_LENGTH - 1; i++) {
            masked_text[i] = '*';
        }
        masked_text[strlen(text_to_draw)] = '\0';
        text_to_draw = masked_text;
    }

    int16_t text_x = KRYON_FP_TO_INT(self->x) + input_state->padding + input_state->border_width;
    int16_t text_y = KRYON_FP_TO_INT(self->y) + input_state->padding + input_state->border_width;

    kryon_draw_text(buf, text_to_draw, text_x, text_y, input_state->font_id, text_color);

    // Draw cursor if focused
    if (input_state->focused && input_state->cursor_visible) {
        // Calculate cursor position (simplified)
        int16_t cursor_x = text_x + (input_state->cursor_position * 8); // 8 pixels per character
        int16_t cursor_y = text_y;
        int16_t cursor_height = 16; // Font height estimate

        kryon_draw_rect(buf, cursor_x, cursor_y, 2, cursor_height, text_color);
    }
}

static void input_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) return;

    // Set default input size
    if (self->width == 0) self->width = KRYON_FP_FROM_INT(200);
    if (self->height == 0) self->height = KRYON_FP_FROM_INT(30);

    kryon_component_mark_clean(self);
}

static bool input_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (self == NULL || event == NULL || self->state == NULL) return false;

    kryon_input_state_t* input_state = (kryon_input_state_t*)self->state;

    switch (event->type) {
        case KRYON_EVT_CLICK:
            input_state->focused = true;
            return true;

        case KRYON_EVT_KEY:
            {
                uint32_t key_code = kryon_event_get_key_code(event);
                bool pressed = kryon_event_is_key_pressed(event);

                if (!pressed) return false; // Only handle key press events

                if (key_code == 8) { // Backspace
                    size_t len = strlen(input_state->text);
                    if (len > 0 && input_state->cursor_position > 0) {
                        input_state->text[input_state->cursor_position - 1] = '\0';
                        input_state->cursor_position--;
                        if (input_state->on_change != NULL) {
                            input_state->on_change(self, input_state->text);
                        }
                    }
                } else if (key_code == 13) { // Enter
                    if (input_state->on_submit != NULL) {
                        input_state->on_submit(self, input_state->text);
                    }
                } else if (key_code >= 32 && key_code <= 126) { // Printable characters
                    size_t len = strlen(input_state->text);
                    if (len < KRYON_MAX_TEXT_LENGTH - 1) {
                        input_state->text[input_state->cursor_position] = (char)key_code;
                        input_state->text[input_state->cursor_position + 1] = '\0';
                        input_state->cursor_position++;
                        if (input_state->on_change != NULL) {
                            input_state->on_change(self, input_state->text);
                        }
                    }
                }
                return true;
            }

        case KRYON_EVT_TIMER:
            // Handle cursor blinking
            input_state->cursor_blink_timer += 0.5f; // 500ms intervals
            if (input_state->cursor_blink_timer >= 1.0f) {
                input_state->cursor_visible = !input_state->cursor_visible;
                input_state->cursor_blink_timer = 0.0f;
            }
            return true;

        default:
            break;
    }

    return false;
}

const kryon_component_ops_t kryon_input_ops = {
    .render = input_render,
    .on_event = input_on_event,
    .destroy = NULL,
    .layout = input_layout
};

// ============================================================================
// Checkbox Component
// ============================================================================

// Checkbox state structure - defined in header

static void checkbox_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible) {
        return;
    }

    kryon_checkbox_state_t* checkbox_state = (kryon_checkbox_state_t*)self->state;
    if (checkbox_state == NULL) {
        return;
    }

    int16_t box_size = checkbox_state->check_size;
    int16_t x = KRYON_FP_TO_INT(self->x);
    int16_t y = KRYON_FP_TO_INT(self->y);

    // Draw checkbox box
    kryon_draw_rect(buf, x, y, box_size, box_size, checkbox_state->box_color);

    // Draw checkmark if checked
    if (checkbox_state->checked) {
        // Draw proper checkmark with visible line segments
        // Using grid-based approach for consistent ✓ shape
        int16_t third = box_size / 3;
        int16_t two_thirds = (box_size * 2) / 3;

        // First line: from top-left to middle
        kryon_draw_line(buf,
                       x + third, y + third,              // Start: 1/3 from left, 1/3 from top
                       x + box_size/2, y + two_thirds,     // End: middle, 2/3 from top
                       checkbox_state->check_color);

        // Second line: from middle to bottom-right
        kryon_draw_line(buf,
                       x + box_size/2, y + two_thirds,     // Start: middle, 2/3 from top
                       x + two_thirds + 1, y + third,       // End: 2/3+1 from left, 1/3 from top
                       checkbox_state->check_color);
    }

    // ALWAYS draw label text - debug version to ensure visibility
    if (strlen(checkbox_state->label) > 0) {
        int16_t text_x = x + box_size + 8; // 8 pixel spacing
        int16_t text_y = y + (box_size / 2) - 8; // Center text vertically

        // Use standard black text color for readability
        uint32_t text_color = checkbox_state->text_color; // Use the component's text color

                kryon_draw_text(buf, checkbox_state->label, text_x, text_y,
                       checkbox_state->font_id, text_color);
    }
}

static void checkbox_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) return;

    kryon_checkbox_state_t* checkbox_state = (kryon_checkbox_state_t*)self->state;

    // Set default checkbox size
    if (checkbox_state->check_size == 0) {
        checkbox_state->check_size = 16;
    }

    // Calculate component size based on check size and label
    uint16_t total_width = checkbox_state->check_size;
    if (strlen(checkbox_state->label) > 0) {
        total_width += 8; // Spacing
        total_width += strlen(checkbox_state->label) * 8; // Text width estimate
    }

    if (self->width == 0) self->width = KRYON_FP_FROM_INT(total_width);
    if (self->height == 0) self->height = KRYON_FP_FROM_INT(checkbox_state->check_size);

    kryon_component_mark_clean(self);
}

static bool checkbox_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (self == NULL || event == NULL || self->state == NULL) return false;

    kryon_checkbox_state_t* checkbox_state = (kryon_checkbox_state_t*)self->state;

    switch (event->type) {
        case KRYON_EVT_CLICK:
            checkbox_state->checked = !checkbox_state->checked;
            if (checkbox_state->on_change != NULL) {
                checkbox_state->on_change(self, checkbox_state->checked);
            }
            return true;

        default:
            break;
    }

    return false;
}

const kryon_component_ops_t kryon_checkbox_ops = {
    .render = checkbox_render,
    .on_event = checkbox_on_event,
    .destroy = NULL,
    .layout = checkbox_layout
};

// ============================================================================
// Spacer Component
// ============================================================================

// Spacer state structure - defined in header

static void spacer_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    // Spacer doesn't render anything visible
    (void)self;
    (void)buf;
}

static void spacer_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) return;

    kryon_spacer_state_t* spacer_state = (kryon_spacer_state_t*)self->state;

    // Set flex grow property
    self->flex_grow = spacer_state->flex_grow;

    // Use explicit size if set, otherwise use defaults
    if (spacer_state->width > 0) {
        self->width = KRYON_FP_FROM_INT(spacer_state->width);
    }
    if (spacer_state->height > 0) {
        self->height = KRYON_FP_FROM_INT(spacer_state->height);
    }

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_spacer_ops = {
    .render = spacer_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = spacer_layout
};

// ============================================================================
// Component Factory Functions
// ============================================================================

kryon_component_t* kryon_canvas_create(uint16_t width, uint16_t height, uint32_t background_color,
                                         bool (*on_draw)(kryon_component_t*, kryon_cmd_buf_t*),
                                         void (*on_update)(kryon_component_t*, float)) {
    kryon_canvas_state_t* state = (kryon_canvas_state_t*)malloc(sizeof(kryon_canvas_state_t));
    if (state == NULL) return NULL;

    memset(state, 0, sizeof(kryon_canvas_state_t));
    state->width = width;
    state->height = height;
    state->background_color = background_color;
    state->on_draw = on_draw;
    state->on_update = on_update;

    kryon_component_t* component = kryon_component_create(&kryon_canvas_ops, state);
    if (component == NULL) {
        free(state);
        return NULL;
    }

    return component;
}

kryon_component_t* kryon_input_create(const char* placeholder, const char* initial_text,
                                         uint16_t font_id, bool password_mode) {
    kryon_input_state_t* state = (kryon_input_state_t*)malloc(sizeof(kryon_input_state_t));
    if (state == NULL) return NULL;

    memset(state, 0, sizeof(kryon_input_state_t));
    if (placeholder != NULL) {
        strncpy(state->placeholder, placeholder, KRYON_MAX_TEXT_LENGTH - 1);
    }
    if (initial_text != NULL) {
        strncpy(state->text, initial_text, KRYON_MAX_TEXT_LENGTH - 1);
    }
    state->font_id = font_id;
    state->text_color = kryon_color_rgb(0, 0, 0);
    state->placeholder_color = kryon_color_rgb(128, 128, 128);
    state->background_color = kryon_color_rgb(255, 255, 255);
    state->border_color = kryon_color_rgb(200, 200, 200);
    state->border_width = 1;
    state->padding = 4;
    state->password_mode = password_mode;

    kryon_component_t* component = kryon_component_create(&kryon_input_ops, state);
    if (component == NULL) {
        free(state);
        return NULL;
    }

    return component;
}

kryon_component_t* kryon_checkbox_create(const char* label, bool initial_checked,
                                            uint16_t check_size, void (*on_change)(kryon_component_t*, bool)) {
    kryon_checkbox_state_t* state = (kryon_checkbox_state_t*)malloc(sizeof(kryon_checkbox_state_t));
    if (state == NULL) return NULL;

    memset(state, 0, sizeof(kryon_checkbox_state_t));
    state->checked = initial_checked;
    state->check_size = check_size > 0 ? check_size : 16;
    state->check_color = kryon_color_rgb(0, 0, 0); // Black checkmark
    state->box_color = kryon_color_rgb(128, 128, 128); // Gray box
    state->text_color = kryon_color_rgb(0, 0, 0); // Black text
    state->font_id = 0;
    if (label != NULL) {
        strncpy(state->label, label, KRYON_MAX_TEXT_LENGTH - 1);
    }
    state->on_change = on_change;

    kryon_component_t* component = kryon_component_create(&kryon_checkbox_ops, state);
    if (component == NULL) {
        free(state);
        return NULL;
    }

    return component;
}

kryon_component_t* kryon_spacer_create(uint16_t width, uint16_t height, uint8_t flex_grow) {
    kryon_spacer_state_t* state = (kryon_spacer_state_t*)malloc(sizeof(kryon_spacer_state_t));
    if (state == NULL) return NULL;

    memset(state, 0, sizeof(kryon_spacer_state_t));
    state->width = width;
    state->height = height;
    state->flex_grow = flex_grow;

    kryon_component_t* component = kryon_component_create(&kryon_spacer_ops, state);
    if (component == NULL) {
        free(state);
        return NULL;
    }

    return component;
}

// Component operation variables for external access
const kryon_component_ops_t kryon_canvas_ops_ext = kryon_canvas_ops;
const kryon_component_ops_t kryon_input_ops_ext = kryon_input_ops;
const kryon_component_ops_t kryon_checkbox_ops_ext = kryon_checkbox_ops;
const kryon_component_ops_t kryon_spacer_ops_ext = kryon_spacer_ops;