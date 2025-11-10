#include "include/kryon.h"
#include "include/kryon_canvas.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Coordinate Transformation Utilities
// ============================================================================

// Calculate absolute position by accumulating parent positions
static void calculate_absolute_position(const kryon_component_t* component,
                                      kryon_fp_t* abs_x, kryon_fp_t* abs_y) {
    if (component == NULL) {
        *abs_x = 0;
        *abs_y = 0;
        return;
    }

    *abs_x = component->x;
    *abs_y = component->y;

    // Add parent positions recursively
    kryon_component_t* parent = component->parent;
    while (parent != NULL) {
        *abs_x += parent->x;
        *abs_y += parent->y;
        parent = parent->parent;
    }
}

// ============================================================================
// Static Component Pool (No Heap Allocations on MCU)
// ============================================================================

#if KRYON_NO_HEAP
// Static pool for MCU targets
static kryon_component_t component_pool[KRYON_MAX_COMPONENTS];
static bool component_used[KRYON_MAX_COMPONENTS];
static uint8_t next_component_index = 0;
#else
// Dynamic allocation for desktop
#include <stdlib.h>
#endif

// ============================================================================
// Component Creation and Management
// ============================================================================

static bool kryon_component_should_trace(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
        const char* env = getenv("KRYON_TRACE_COMPONENTS");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
#endif
    }

    return enabled;
}

static bool kryon_render_coordinates_should_trace(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
        const char* env = getenv("KRYON_TRACE_COORDINATES_FLOW");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
#endif
    }

    return enabled;
}

static void trace_render_coordinates(const char* component_type, kryon_component_t* component,
                                    int16_t render_x, int16_t render_y, uint16_t render_w, uint16_t render_h) {
    if (!kryon_render_coordinates_should_trace()) return;

    fprintf(stderr, "[kryon][render] %s: component=%p "
            "fixed_point(x=%f,y=%f,w=%f,h=%f) -> render(x=%d,y=%d,w=%d,h=%d)\n",
            component_type, (void*)component,
            kryon_fp_to_float(component->x), kryon_fp_to_float(component->y),
            kryon_fp_to_float(component->width), kryon_fp_to_float(component->height),
            render_x, render_y, render_w, render_h);
}

kryon_component_t* kryon_component_create(const kryon_component_ops_t* ops, void* initial_state) {
    if (ops == NULL) {
        return NULL;
    }

    kryon_component_t* component = NULL;

#if KRYON_NO_HEAP
    // Find unused component in pool
    for (uint8_t i = 0; i < KRYON_MAX_COMPONENTS; i++) {
        uint8_t index = (next_component_index + i) % KRYON_MAX_COMPONENTS;
        if (!component_used[index]) {
            component = &component_pool[index];
            component_used[index] = true;
            next_component_index = (index + 1) % KRYON_MAX_COMPONENTS;
            break;
        }
    }

    if (component == NULL) {
        return NULL; // Pool exhausted
    }
#else
    // Dynamic allocation for desktop
    component = (kryon_component_t*)malloc(sizeof(kryon_component_t));
    if (component == NULL) {
        return NULL;
    }
#endif

    // Initialize component
    memset(component, 0, sizeof(kryon_component_t));
    component->ops = ops;
    component->state = initial_state;
    component->visible = true;
    component->dirty = true;
    component->background_color = 0; // Transparent until styled
    component->text_color = 0;  // Transparent (inherit from parent)
    component->border_color = 0;
    component->border_width = 0;
    component->align_items = KRYON_ALIGN_START;
    component->justify_content = KRYON_ALIGN_START;

    return component;
}

void kryon_component_destroy(kryon_component_t* component) {
    if (component == NULL) {
        return;
    }

    // Destroy all children first
    for (uint8_t i = 0; i < component->child_count; i++) {
        kryon_component_destroy(component->children[i]);
    }

    // Call component-specific destroy function
    if (component->ops && component->ops->destroy) {
        component->ops->destroy(component);
    }

#if KRYON_NO_HEAP
    // Return to pool
    for (uint8_t i = 0; i < KRYON_MAX_COMPONENTS; i++) {
        if (&component_pool[i] == component) {
            component_used[i] = false;
            break;
        }
    }
#else
    // Free dynamic memory
    if (component->children) {
        free(component->children);
    }
    free(component);
#endif
}

bool kryon_component_add_child(kryon_component_t* parent, kryon_component_t* child) {
    if (parent == NULL || child == NULL) {
        return false;
    }

    // Check if child already has a parent
    if (child->parent != NULL) {
        return false;
    }

    // Ensure children array has capacity
    if (parent->child_count >= parent->child_capacity) {
        uint8_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;

#if KRYON_NO_HEAP
        // For MCU, we have a fixed maximum
        if (new_capacity > 32) { // Reasonable limit for MCU
            return false;
        }
#endif

        kryon_component_t** new_children = (kryon_component_t**)realloc(
            parent->children,
            new_capacity * sizeof(kryon_component_t*)
        );

        if (new_children == NULL) {
            return false;
        }

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    // Add child
    parent->children[parent->child_count] = child;
    child->parent = parent;
    parent->child_count++;

    // Mark parent as dirty (layout needs recalculation)
    kryon_component_mark_dirty(parent);

    return true;
}

void kryon_component_remove_child(kryon_component_t* parent, kryon_component_t* child) {
    if (parent == NULL || child == NULL) {
        return;
    }

    // Find child index
    uint8_t child_index = 0;
    bool found = false;
    for (uint8_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            child_index = i;
            found = true;
            break;
        }
    }

    if (!found) {
        return; // Child not found
    }

    // Remove child from array
    for (uint8_t i = child_index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }

    parent->child_count--;
    child->parent = NULL;

    // Mark parent as dirty
    kryon_component_mark_dirty(parent);
}

kryon_component_t* kryon_component_get_parent(kryon_component_t* component) {
    return component ? component->parent : NULL;
}

kryon_component_t* kryon_component_get_child(kryon_component_t* component, uint8_t index) {
    if (component == NULL || index >= component->child_count) {
        return NULL;
    }
    return component->children[index];
}

uint8_t kryon_component_get_child_count(kryon_component_t* component) {
    return component ? component->child_count : 0;
}

// ============================================================================
// Component Properties
// ============================================================================

void kryon_component_set_bounds_mask(kryon_component_t* component, kryon_fp_t x, kryon_fp_t y,
                                     kryon_fp_t width, kryon_fp_t height, uint8_t explicit_mask) {
    if (component == NULL) {
        return;
    }

    component->x = x;
    component->y = y;
    component->width = width;
    component->height = height;

#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
    const char* trace_env = getenv("KRYON_TRACE_LAYOUT");
    if (trace_env != NULL && trace_env[0] != '\0' && trace_env[0] != '0') {
        fprintf(stderr,
                "[kryon][bounds] mask=%u x=%d y=%d w=%d h=%d\n",
                explicit_mask,
                KRYON_FP_TO_INT(x),
                KRYON_FP_TO_INT(y),
                KRYON_FP_TO_INT(width),
                KRYON_FP_TO_INT(height));
    }
#endif

    if (explicit_mask & KRYON_COMPONENT_FLAG_HAS_X) {
        component->explicit_x = x;
        component->layout_flags |= KRYON_COMPONENT_FLAG_HAS_X;
    } else {
        component->explicit_x = 0;
        component->layout_flags &= (uint8_t)~KRYON_COMPONENT_FLAG_HAS_X;
    }

    if (explicit_mask & KRYON_COMPONENT_FLAG_HAS_Y) {
        component->explicit_y = y;
        component->layout_flags |= KRYON_COMPONENT_FLAG_HAS_Y;
    } else {
        component->explicit_y = 0;
        component->layout_flags &= (uint8_t)~KRYON_COMPONENT_FLAG_HAS_Y;
    }

    if (explicit_mask & KRYON_COMPONENT_FLAG_HAS_WIDTH) {
        component->layout_flags |= KRYON_COMPONENT_FLAG_HAS_WIDTH;
    } else {
        component->layout_flags &= (uint8_t)~KRYON_COMPONENT_FLAG_HAS_WIDTH;
    }

    if (explicit_mask & KRYON_COMPONENT_FLAG_HAS_HEIGHT) {
        component->layout_flags |= KRYON_COMPONENT_FLAG_HAS_HEIGHT;
    } else {
        component->layout_flags &= (uint8_t)~KRYON_COMPONENT_FLAG_HAS_HEIGHT;
    }

#if defined(KRYON_TRACE_LAYOUT)
    fprintf(stderr, "[kryon][bounds] set %p x=%d w=%d raw=%d\n",
            (void*)component,
            KRYON_FP_TO_INT(width),
            KRYON_FP_TO_INT(height),
            (int)width);
#endif

    kryon_component_mark_dirty(component);
}

void kryon_component_set_bounds(kryon_component_t* component, kryon_fp_t x, kryon_fp_t y,
                               kryon_fp_t width, kryon_fp_t height) {
    kryon_component_set_bounds_mask(component, x, y, width, height,
        KRYON_COMPONENT_FLAG_HAS_X |
        KRYON_COMPONENT_FLAG_HAS_Y |
        KRYON_COMPONENT_FLAG_HAS_WIDTH |
        KRYON_COMPONENT_FLAG_HAS_HEIGHT);
}

void kryon_component_set_padding(kryon_component_t* component, uint8_t top, uint8_t right,
                                uint8_t bottom, uint8_t left) {
    if (component == NULL) {
        return;
    }

    component->padding_top = top;
    component->padding_right = right;
    component->padding_bottom = bottom;
    component->padding_left = left;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_margin(kryon_component_t* component, uint8_t top, uint8_t right,
                               uint8_t bottom, uint8_t left) {
    if (component == NULL) {
        return;
    }

    component->margin_top = top;
    component->margin_right = right;
    component->margin_bottom = bottom;
    component->margin_left = left;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_background_color(kryon_component_t* component, uint32_t color) {
    if (component == NULL) {
        return;
    }
    component->background_color = color;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_border_color(kryon_component_t* component, uint32_t color) {
    if (component == NULL) {
        return;
    }
    component->border_color = color;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_border_width(kryon_component_t* component, uint8_t width) {
    if (component == NULL) {
        return;
    }
    component->border_width = width;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_visible(kryon_component_t* component, bool visible) {
    if (component == NULL) {
        return;
    }
    component->visible = visible;
    kryon_component_mark_dirty(component);
}

// Color inheritance helpers
uint32_t kryon_component_get_effective_text_color(kryon_component_t* component) {
    if (component == NULL) {
        return kryon_color_rgb(0, 0, 0);  // Default: black
    }

    // Use component's color if alpha is non-zero (color is set)
    if ((component->text_color & 0xFF) != 0) {
        return component->text_color;
    }

    // Fallback to parent color recursively
    if (component->parent != NULL) {
        return kryon_component_get_effective_text_color(component->parent);
    }

    // Final default: black
    return kryon_color_rgb(0, 0, 0);
}

uint32_t kryon_component_get_effective_background_color(kryon_component_t* component) {
    if (component == NULL) {
        return 0;  // Default: transparent
    }

    // Use component's color if alpha is non-zero (color is set)
    if ((component->background_color & 0xFF) != 0) {
        return component->background_color;
    }

    // Fallback to parent color recursively
    if (component->parent != NULL) {
        return kryon_component_get_effective_background_color(component->parent);
    }

    // Final default: transparent
    return 0;
}

void kryon_component_set_flex(kryon_component_t* component, uint8_t flex_grow, uint8_t flex_shrink) {
    if (component == NULL) {
        return;
    }

    component->flex_grow = flex_grow;
    component->flex_shrink = flex_shrink;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_text_color(kryon_component_t* component, uint32_t color) {
    if (component == NULL) {
        return;
    }
    component->text_color = color;
    kryon_component_mark_dirty(component);

    if (kryon_component_should_trace()) {
        fprintf(stderr, "[kryon][component] set_text_color %p to 0x%08x (rgba: %d,%d,%d,%d)\n",
                (void*)component, color,
                (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    }
}

void kryon_component_set_text(kryon_component_t* component, const char* text) {
    if (component == NULL) {
        return;
    }

    // Check if component has text state (Text or Button component)
    if (component->state == NULL) {
        return;
    }

    // Try to cast to text state (works for both Text and Button components)
    kryon_text_state_t* text_state = (kryon_text_state_t*)component->state;

    // Free old text if it exists and was dynamically allocated
    if (text_state->text != NULL) {
        free((void*)text_state->text);
    }

    // Allocate and copy new text
    if (text != NULL) {
        size_t len = strlen(text);
        char* new_buffer = (char*)malloc(len + 1);
        if (new_buffer != NULL) {
            memcpy(new_buffer, text, len);
            new_buffer[len] = '\0';
            text_state->text = new_buffer;
        } else {
            // Memory allocation failed, set to empty string
            text_state->text = "";
        }
    } else {
        // Handle NULL text
        char* empty_buffer = (char*)malloc(1);
        if (empty_buffer != NULL) {
            empty_buffer[0] = '\0';
            text_state->text = empty_buffer;
        } else {
            text_state->text = "";
        }
    }

    // Mark component as dirty to trigger re-render
    kryon_component_mark_dirty(component);

    if (kryon_component_should_trace()) {
        fprintf(stderr, "[kryon][component] set_text %p to \"%s\"\n",
                (void*)component, text ? text : "(null)");
    }
}

void kryon_component_set_layout_alignment(kryon_component_t* component,
                                         kryon_alignment_t justify,
                                         kryon_alignment_t align) {
    if (component == NULL) {
        return;
    }

    fprintf(stderr, "[kryon][component] set_layout_alignment %p justify=%d align=%d\n", (void*)component, (int)justify, (int)align);
    component->justify_content = (uint8_t)justify;
    component->align_items = (uint8_t)align;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_layout_direction(kryon_component_t* component, uint8_t direction) {
    if (component == NULL) {
        return;
    }

    fprintf(stderr, "[kryon][component] set_layout_direction %p to %d\n", (void*)component, (int)direction);
    component->layout_direction = direction;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_gap(kryon_component_t* component, uint8_t gap) {
    if (component == NULL) {
        return;
    }

    component->gap = gap;
    kryon_component_mark_dirty(component);
}

void kryon_component_mark_dirty(kryon_component_t* component) {
    if (component == NULL) {
        return;
    }
    component->dirty = true;

    // Also mark parent as dirty
    if (component->parent != NULL) {
        kryon_component_mark_dirty(component->parent);
    }
}

void kryon_component_mark_clean(kryon_component_t* component) {
    if (component != NULL) {
        component->dirty = false;
    }
}

bool kryon_component_is_dirty(kryon_component_t* component) {
    return component ? component->dirty : false;
}

// ============================================================================
// Event System
// ============================================================================

void kryon_component_send_event(kryon_component_t* component, kryon_event_t* event) {
    if (component == NULL || event == NULL) {
        return;
    }

    // Send to component-specific handlers first
    if (component->ops && component->ops->on_event) {
        if (component->ops->on_event(component, event)) {
            return; // Event was handled
        }
    }

    // Send to registered event handlers
    for (uint8_t i = 0; i < component->handler_count; i++) {
        if (component->event_handlers[i] != NULL) {
            component->event_handlers[i](component, event);
        }
    }

    // Bubble up to parent (event propagation)
    if (component->parent != NULL) {
        kryon_component_send_event(component->parent, event);
    }
}

bool kryon_component_add_event_handler(kryon_component_t* component,
                                      void (*handler)(kryon_component_t*, kryon_event_t*)) {
    if (component == NULL || handler == NULL) {
        return false;
    }

    if (component->handler_count >= sizeof(component->event_handlers) / sizeof(component->event_handlers[0])) {
        return false; // Handler array full
    }

    component->event_handlers[component->handler_count] = handler;
    component->handler_count++;
    return true;
}

bool kryon_component_is_button(kryon_component_t* component) {
    return component != NULL && component->ops == &kryon_button_ops;
}

// ============================================================================
// Built-in Component Implementations
// ============================================================================

// Container Component
static void container_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible) {
        return;
    }

    const bool trace = kryon_component_should_trace();
    if (trace) {
        fprintf(stderr,
                "[kryon][component] render container=%p x=%d y=%d w=%d h=%d bg=%08X borderWidth=%u dirty=%d\n",
                (void*)self,
                KRYON_FP_TO_INT(self->x),
                KRYON_FP_TO_INT(self->y),
                KRYON_FP_TO_INT(self->width),
                KRYON_FP_TO_INT(self->height),
                self->background_color,
                self->border_width,
                self->dirty);
    }

    // Calculate absolute position for rendering
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(self, &abs_x, &abs_y);
    int16_t x = KRYON_FP_TO_INT(abs_x);
    int16_t y = KRYON_FP_TO_INT(abs_y);
    uint16_t w = (uint16_t)KRYON_FP_TO_INT(self->width);
    uint16_t h = (uint16_t)KRYON_FP_TO_INT(self->height);

    // Trace container render coordinates
    trace_render_coordinates("container", self, x, y, w, h);

    // Use inherited colors
    uint32_t bg_color = kryon_component_get_effective_background_color(self);
    const bool has_border = self->border_width > 0 && (self->border_color & 0xFF) != 0;
    const bool has_background = (bg_color & 0xFF) != 0;

    // Debug: Check background color values
    if (getenv("KRYON_TRACE_COLORS")) {
        fprintf(stderr, "[kryon][component] render_container %p: bg_color=0x%08x, alpha=%d, has_background=%d, w=%u, h=%u\n",
                (void*)self, bg_color, (int)(bg_color & 0xFF), (int)has_background, w, h);
    }

    if (has_border && w > 0 && h > 0) {
        kryon_draw_rect(buf, x, y, w, h, self->border_color);
    }

    if (has_background && w > 0 && h > 0) {
        if (has_border) {
            uint16_t bw = self->border_width;
            if (bw * 2 >= w || bw * 2 >= h) {
                // Border consumes the whole rect; nothing left to fill.
            } else {
                kryon_draw_rect(buf,
                    (int16_t)(x + bw),
                    (int16_t)(y + bw),
                    (uint16_t)(w - bw * 2),
                    (uint16_t)(h - bw * 2),
                    bg_color);
            }
        } else {
            kryon_draw_rect(buf, x, y, w, h, bg_color);
        }
    }

    // Render all children
    for (uint8_t i = 0; i < self->child_count; i++) {
        kryon_component_t* child = self->children[i];
        if (child->ops && child->ops->render) {
            child->ops->render(child, buf);
        } else {
        }
    }
}

static void container_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL) {
        return;
    }

    if (self->width == 0 && available_width > 0) {
        self->width = available_width;
    }
    if (self->height == 0 && available_height > 0) {
        self->height = available_height;
    }

    kryon_layout_apply_column(self,
        self->width > 0 ? self->width : available_width,
        self->height > 0 ? self->height : available_height);

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_container_ops = {
    .render = container_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = container_layout
};

// Text Component
static void text_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible || self->state == NULL) {
        return;
    }

    kryon_text_state_t* text_state = (kryon_text_state_t*)self->state;
    if (text_state->text == NULL) {
        return;
    }

    const char* trace_env = getenv("KRYON_TRACE_COMPONENTS");
    const bool trace_enabled = (trace_env != NULL && trace_env[0] != '\0' && trace_env[0] != '0');
    if (trace_enabled) {
        fprintf(stderr, "[kryon][component] text x=%d y=%d w=%d h=%d text=%s\n",
                KRYON_FP_TO_INT(self->x),
                KRYON_FP_TO_INT(self->y),
                KRYON_FP_TO_INT(self->width),
                KRYON_FP_TO_INT(self->height),
                text_state->text);
    }

    // Draw background using inherited color
    uint32_t bg_color = kryon_component_get_effective_background_color(self);
    if ((bg_color & 0xFF) != 0) {
        kryon_draw_rect(buf,
                       KRYON_FP_TO_INT(self->x),
                       KRYON_FP_TO_INT(self->y),
                       KRYON_FP_TO_INT(self->width),
                       KRYON_FP_TO_INT(self->height),
                       bg_color);
    }

    // Draw text using inherited color
    uint32_t text_color = kryon_component_get_effective_text_color(self);
    if (trace_enabled) {
        fprintf(stderr, "[kryon][component] text effective_color=0x%08x (rgba: %d,%d,%d,%d)\n",
                text_color,
                (text_color >> 24) & 0xFF, (text_color >> 16) & 0xFF, (text_color >> 8) & 0xFF, text_color & 0xFF);
    }

    // Calculate absolute position for rendering
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(self, &abs_x, &abs_y);
    int16_t render_x = KRYON_FP_TO_INT(abs_x);
    int16_t render_y = KRYON_FP_TO_INT(abs_y);

    // Trace final render coordinates before text drawing
    trace_render_coordinates("text", self, render_x, render_y,
                            KRYON_FP_TO_INT(self->width),
                            KRYON_FP_TO_INT(self->height));

    kryon_draw_text(buf,
                   text_state->text,
                   render_x,
                   render_y,
                   text_state->font_id,
                   text_color);
}

static void text_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) {
        return;
    }

    // For now, use fixed size. In a real implementation, we would measure text
    kryon_text_state_t* text_state = (kryon_text_state_t*)self->state;

    // Estimate text size (this would be platform-specific in real implementation)
    self->width = KRYON_FP_FROM_INT(100); // Placeholder
    self->height = KRYON_FP_FROM_INT(20); // Placeholder

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_text_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

static uint8_t clamp_color_channel(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static uint32_t button_tint_color(uint32_t color, int delta) {
    const int r = (int)((color >> 24) & 0xFF);
    const int g = (int)((color >> 16) & 0xFF);
    const int b = (int)((color >> 8) & 0xFF);
    const uint8_t a = (uint8_t)(color & 0xFF);

    return ((uint32_t)clamp_color_channel(r + delta) << 24) |
           ((uint32_t)clamp_color_channel(g + delta) << 16) |
           ((uint32_t)clamp_color_channel(b + delta) << 8) |
           a;
}

// Button Component
static void button_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible || self->state == NULL) {
        return;
    }

    kryon_button_state_t* button_state = (kryon_button_state_t*)self->state;

    uint32_t bg_color = self->background_color;
    if ((bg_color & 0xFF) == 0) {
        bg_color = kryon_color_rgba(64, 64, 64, 255);
    }

    if (button_state->hovered && !button_state->pressed) {
        bg_color = button_tint_color(bg_color, 18);
    }

    if (button_state->pressed) {
        bg_color = button_tint_color(bg_color, -24);
    }

    // Calculate absolute position for rendering
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(self, &abs_x, &abs_y);
    const int16_t x = KRYON_FP_TO_INT(abs_x);
    const int16_t y = KRYON_FP_TO_INT(abs_y);
    const uint16_t w = (uint16_t)KRYON_FP_TO_INT(self->width);
    const uint16_t h = (uint16_t)KRYON_FP_TO_INT(self->height);

    if (w == 0 || h == 0) {
        return;
    }

    const bool has_border = self->border_width > 0 && (self->border_color & 0xFF) != 0;

    if (has_border) {
        kryon_draw_rect(buf, x, y, w, h, self->border_color);
        const uint16_t bw = self->border_width;
        if (bw * 2 < w && bw * 2 < h) {
            kryon_draw_rect(buf,
                           (int16_t)(x + bw),
                           (int16_t)(y + bw),
                           (uint16_t)(w - bw * 2),
                           (uint16_t)(h - bw * 2),
                           bg_color);
        }
    } else {
        kryon_draw_rect(buf, x, y, w, h, bg_color);
    }

    if (button_state->text != NULL) {
        // Calculate text dimensions for centering
        uint32_t text_len = strlen(button_state->text);
        int16_t estimated_text_width = text_len * 8; // Same estimate as layout system
        int16_t estimated_text_height = 16; // Standard text height estimate

        // Calculate available space inside padding
        int16_t available_width = KRYON_FP_TO_INT(self->width) -
                                 (self->padding_left + self->padding_right);
        int16_t available_height = KRYON_FP_TO_INT(self->height) -
                                  (self->padding_top + self->padding_bottom);

        // Calculate absolute position for rendering
        kryon_fp_t abs_x, abs_y;
        calculate_absolute_position(self, &abs_x, &abs_y);
        int16_t base_x = KRYON_FP_TO_INT(abs_x);
        int16_t base_y = KRYON_FP_TO_INT(abs_y);

        // Center text horizontally and vertically (relative to button)
        int16_t text_x = base_x + self->padding_left +
                        (available_width > estimated_text_width ?
                         (available_width - estimated_text_width) / 2 : 0);
        int16_t text_y = base_y + self->padding_top +
                        (available_height > estimated_text_height ?
                         (available_height - estimated_text_height) / 2 : 0);

        // Trace final render coordinates before button text drawing
        trace_render_coordinates("button_text", self, text_x, text_y,
                                (uint16_t)(text_len * 8), 16);

    kryon_draw_text(buf,
                       button_state->text,
                       text_x,
                       text_y,
                       button_state->font_id,
                       self->text_color);
    }
}

static bool button_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (self == NULL || event == NULL || self->state == NULL) {
        return false;
    }

    kryon_button_state_t* button_state = (kryon_button_state_t*)self->state;

    switch (event->type) {
        case KRYON_EVT_CLICK: {
            const bool is_press = (event->param & 0x80000000u) != 0;
            fprintf(stderr, "[kryon][button] button_on_event: param=0x%08x is_press=%d on_click=%p\n",
                    event->param, is_press, (void*)button_state->on_click);
            button_state->pressed = is_press;
            kryon_component_mark_dirty(self);

            if (!is_press && button_state->on_click != NULL) {
                fprintf(stderr, "[kryon][button] Calling onclick handler!\n");
                button_state->on_click(self, event);
                fprintf(stderr, "[kryon][button] onclick handler returned\n");
            }
            return true;
        }

        case KRYON_EVT_HOVER:
            button_state->hovered = event->param != 0;
            kryon_component_mark_dirty(self);
            return true;

        case KRYON_EVT_TOUCH:
            return true;

        default:
            return false;
    }
}

static void button_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) {
        return;
    }

    // Default button size (would be based on text content in real implementation)
    if (self->width == 0) self->width = KRYON_FP_FROM_INT(80);
    if (self->height == 0) self->height = KRYON_FP_FROM_INT(30);

    // Default button styling
    if (self->border_width == 0) self->border_width = 1;  // Default 1px border
    if (self->border_color == 0) self->border_color = kryon_color_rgb(255, 255, 255);  // Default white border for maximum visibility

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_button_ops = {
    .render = button_render,
    .on_event = button_on_event,
    .destroy = NULL,
    .layout = button_layout
};

// ============================================================================
// Canvas Component
// ============================================================================

static void canvas_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible) {
        return;
    }

    // First render the container background
    container_render(self, buf);

    // Get the canvas command buffer from the global canvas state
    kryon_cmd_buf_t* canvas_buf = kryon_canvas_get_command_buffer();
    if (canvas_buf == NULL) {
        return;
    }

    // Copy all canvas commands to the renderer's buffer
    kryon_cmd_iterator_t iter = kryon_cmd_iter_create(canvas_buf);
    kryon_command_t cmd;

    while (kryon_cmd_iter_has_next(&iter)) {
        if (kryon_cmd_iter_next(&iter, &cmd)) {
            // Push the command to the renderer's buffer
            kryon_cmd_buf_push(buf, &cmd);
        }
    }

    // Clear the canvas buffer for next frame
    kryon_cmd_buf_clear(canvas_buf);
}

const kryon_component_ops_t kryon_canvas_ops = {
    .render = canvas_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = container_layout  // Use container layout
};

// Helper function to set canvas ops (for Nim bindings)
void kryon_component_set_canvas_ops(kryon_component_t* component) {
    if (component != NULL) {
        component->ops = &kryon_canvas_ops;
    }
}
