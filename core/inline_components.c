#include "include/kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Inline Component Helper - Get Text Content
// ============================================================================

// Helper to extract text from component children or text property
static const char* get_component_text_content(kryon_component_t* component, uint16_t* length_out) {
    if (!component) return NULL;

    // Check if component has direct text in state
    // For now, we'll assume inline components store text as first child or have a text field
    // This is a simplified implementation

    *length_out = 0;
    return NULL;  // Placeholder - will be filled by actual implementation
}

// ============================================================================
// Span Component (Inline Container)
// ============================================================================

static void span_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    // Span doesn't render directly - it contributes to parent Text component
    (void)self;
    (void)buf;
}

static bool span_on_event(kryon_component_t* self, kryon_event_t* event) {
    // Events are handled by parent Text component
    (void)self;
    (void)event;
    return false;
}

static void span_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    // Layout is handled by parent Text component
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_span_ops = {
    .render = span_render,
    .on_event = span_on_event,
    .destroy = NULL,
    .layout = span_layout
};

// ============================================================================
// Bold Component
// ============================================================================

static void bold_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static void bold_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_bold_ops = {
    .render = bold_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = bold_layout
};

// ============================================================================
// Italic Component
// ============================================================================

static void italic_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static void italic_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_italic_ops = {
    .render = italic_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = italic_layout
};

// ============================================================================
// Underline Component
// ============================================================================

static void underline_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static void underline_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_underline_ops = {
    .render = underline_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = underline_layout
};

// ============================================================================
// Strikethrough Component
// ============================================================================

static void strikethrough_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static void strikethrough_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_strikethrough_ops = {
    .render = strikethrough_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = strikethrough_layout
};

// ============================================================================
// Code Component (Inline Code Span)
// ============================================================================

static void code_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static void code_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_code_ops = {
    .render = code_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = code_layout
};

// ============================================================================
// Link Component (Clickable Hyperlink)
// ============================================================================

static void link_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static bool link_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (!self || !event || !self->state) return false;

    kryon_link_state_t* link_state = (kryon_link_state_t*)self->state;

    if (event->type == KRYON_EVT_CLICK) {
        const bool is_press = (event->param & 0x80000000u) == 0;
        if (!is_press && link_state->on_click && link_state->url) {
            link_state->on_click(self, link_state->url, event);
            return true;
        }
    }

    return false;
}

static void link_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_link_ops = {
    .render = link_render,
    .on_event = link_on_event,
    .destroy = NULL,
    .layout = link_layout
};

// ============================================================================
// Highlight Component (Background Highlight)
// ============================================================================

static void highlight_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    (void)self;
    (void)buf;
}

static void highlight_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    (void)self;
    (void)available_width;
    (void)available_height;
}

const kryon_component_ops_t kryon_highlight_ops = {
    .render = highlight_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = highlight_layout
};
