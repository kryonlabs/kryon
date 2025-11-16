#include "include/kryon.h"
#include "include/kryon_canvas.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Global Renderer for Text Measurement
// ============================================================================

static kryon_renderer_t* g_renderer = NULL;

void kryon_set_global_renderer(kryon_renderer_t* renderer) {
    g_renderer = renderer;
    if (getenv("KRYON_TRACE_LAYOUT")) {
        fprintf(stderr, "[kryon][global] Set global renderer: %p (measure_text=%p)\n",
                (void*)renderer, (void*)(renderer ? renderer->measure_text : NULL));
    }
}

kryon_renderer_t* kryon_get_global_renderer(void) {
    return g_renderer;
}

// ============================================================================
// Coordinate Transformation Utilities
// ============================================================================

// Calculate absolute position by accumulating parent positions
void calculate_absolute_position(const kryon_component_t* component,
                                     kryon_fp_t* abs_x, kryon_fp_t* abs_y) {
    if (component == NULL) {
        *abs_x = 0;
        *abs_y = 0;
        return;
    }

    *abs_x = component->x;
    *abs_y = component->y;

    // If layout system already set absolute coordinates, don't add parents again
    if (component->layout_flags & KRYON_COMPONENT_FLAG_HAS_ABSOLUTE) {
        return;
    }

    // Add parent positions recursively
    kryon_component_t* parent = component->parent;
    while (parent != NULL) {
        *abs_x += parent->x;
        *abs_y += parent->y;
        parent = parent->parent;
    }
}

void kryon_component_get_absolute_bounds(kryon_component_t* component,
                                         kryon_fp_t* abs_x, kryon_fp_t* abs_y,
                                         kryon_fp_t* width, kryon_fp_t* height) {
    if (abs_x) *abs_x = 0;
    if (abs_y) *abs_y = 0;
    if (width) *width = 0;
    if (height) *height = 0;

    if (component == NULL) {
        return;
    }

    kryon_fp_t ax = 0, ay = 0;
    calculate_absolute_position(component, &ax, &ay);

    if (abs_x) *abs_x = ax;
    if (abs_y) *abs_y = ay;
    if (width) *width = component->width;
    if (height) *height = component->height;
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

void kryon_component_set_z_index(kryon_component_t* component, uint16_t z_index) {
    if (component == NULL) {
        return;
    }
    component->z_index = z_index;
    kryon_component_mark_dirty(component);
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

void kryon_component_set_scrollable(kryon_component_t* component, bool scrollable) {
    if (component == NULL) {
        return;
    }
    component->scrollable = scrollable;
    if (scrollable) {
        component->scroll_offset_x = 0;
        component->scroll_offset_y = 0;
    }
    kryon_component_mark_dirty(component);
}

void kryon_component_set_scroll_offset(kryon_component_t* component, kryon_fp_t offset_x, kryon_fp_t offset_y) {
    if (component == NULL || !component->scrollable) {
        return;
    }
    component->scroll_offset_x = offset_x;
    component->scroll_offset_y = offset_y;
    kryon_component_mark_dirty(component);
}

void kryon_component_get_scroll_offset(kryon_component_t* component, kryon_fp_t* offset_x, kryon_fp_t* offset_y) {
    if (component == NULL) {
        if (offset_x) *offset_x = 0;
        if (offset_y) *offset_y = 0;
        return;
    }
    if (offset_x) *offset_x = component->scroll_offset_x;
    if (offset_y) *offset_y = component->scroll_offset_y;
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
}

void kryon_component_set_layout_alignment(kryon_component_t* component,
                                         kryon_alignment_t justify,
                                         kryon_alignment_t align) {
    if (component == NULL) {
        return;
    }

    // fprintf(stderr, "[kryon][component] set_layout_alignment %p justify=%d align=%d\n", (void*)component, (int)justify, (int)align);  // Disabled - too verbose
    component->justify_content = (uint8_t)justify;
    component->align_items = (uint8_t)align;
    kryon_component_mark_dirty(component);
}

void kryon_component_set_layout_direction(kryon_component_t* component, uint8_t direction) {
    if (component == NULL) {
        return;
    }

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

    bool handled = false;

    // Send to component-specific handlers first
    if (component->ops && component->ops->on_event) {
        handled = component->ops->on_event(component, event);
    }

    // Send to registered event handlers
    for (uint8_t i = 0; i < component->handler_count; i++) {
        if (component->event_handlers[i] != NULL) {
            component->event_handlers[i](component, event);
        }
    }

    if (handled) {
        return;
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

    // Calculate absolute position for rendering
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(self, &abs_x, &abs_y);
    int16_t x = KRYON_FP_TO_INT(abs_x);
    int16_t y = KRYON_FP_TO_INT(abs_y);
    uint16_t w = (uint16_t)KRYON_FP_TO_INT(self->width);
    uint16_t h = (uint16_t)KRYON_FP_TO_INT(self->height);

    // Only use explicitly set background color, don't render inherited backgrounds
    // This prevents double-rendering when a child inherits parent's background
    uint32_t bg_color = self->background_color;
    const bool has_border = self->border_width > 0 && (self->border_color & 0xFF) != 0;
    const bool has_background = (bg_color & 0xFF) != 0;

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

    // Render all children sorted by z-index
    // Create temporary array for z-index sorted rendering
    // This preserves the original layout order while rendering by z-index
    kryon_component_t* sorted_children[255];

    // Copy children to temporary array
    for (uint8_t i = 0; i < self->child_count; i++) {
        sorted_children[i] = self->children[i];
    }

    // Debug: Print BEFORE sorting
    if (getenv("KRYON_TRACE_ZINDEX")) {
        fprintf(stderr, "[kryon][zindex] BEFORE sort (%d children):\n", self->child_count);
        for (uint8_t i = 0; i < self->child_count; i++) {
            fprintf(stderr, "[kryon][zindex]   [%d] z=%d\n", i, sorted_children[i]->z_index);
        }
    }

    // Sort temporary array by z_index (insertion sort - efficient for small arrays)
    // Higher z_index renders last (appears on top)
    if (self->child_count > 1) {
        for (uint8_t i = 1; i < self->child_count; i++) {
            kryon_component_t* key = sorted_children[i];
            uint16_t key_z = key->z_index;
            int j = i - 1;

            while (j >= 0 && sorted_children[j]->z_index > key_z) {
                sorted_children[j + 1] = sorted_children[j];
                j--;
            }
            sorted_children[j + 1] = key;
        }
    }

    // Debug: Print AFTER sorting
    if (getenv("KRYON_TRACE_ZINDEX")) {
        fprintf(stderr, "[kryon][zindex] AFTER sort:\n");
        for (uint8_t i = 0; i < self->child_count; i++) {
            fprintf(stderr, "[kryon][zindex]   [%d] z=%d\n", i, sorted_children[i]->z_index);
        }
    }

    // Apply clipping and scroll offset for scrollable containers
    if (self->scrollable) {
        // Recalculate content dimensions now that all children are laid out
        kryon_fp_t max_right = 0;
        kryon_fp_t max_bottom = 0;
        for (uint8_t i = 0; i < self->child_count; i++) {
            kryon_component_t* child = self->children[i];
            if (child != NULL && child->visible) {
                kryon_fp_t child_right = child->x + child->width +
                                        kryon_fp_from_int(child->margin_right);
                kryon_fp_t child_bottom = child->y + child->height +
                                         kryon_fp_from_int(child->margin_bottom);
                if (child_right > max_right) max_right = child_right;
                if (child_bottom > max_bottom) max_bottom = child_bottom;
            }
        }
        self->content_width = max_right;
        self->content_height = max_bottom;

        // Set and push clip rectangle to constrain content to container bounds
        kryon_set_clip(buf, x, y, w, h);
        kryon_push_clip(buf);

        // Temporarily offset children by scroll amount
        for (uint8_t i = 0; i < self->child_count; i++) {
            kryon_component_t* child = self->children[i];
            if (child != NULL) {
                child->y -= self->scroll_offset_y;
                child->x -= self->scroll_offset_x;
            }
        }
    }

    // Debug: Print z-index order
    if (getenv("KRYON_TRACE_ZINDEX")) {
        fprintf(stderr, "[kryon][zindex] Rendering %d children in z-index order:\n", self->child_count);
        for (uint8_t i = 0; i < self->child_count; i++) {
            fprintf(stderr, "[kryon][zindex]   [%d] z_index=%d component=%p\n",
                    i, sorted_children[i]->z_index, (void*)sorted_children[i]);
        }
    }

    // Render from sorted array (preserves original children order)
    for (uint8_t i = 0; i < self->child_count; i++) {
        kryon_component_t* child = sorted_children[i];
        if (child->ops && child->ops->render) {
            child->ops->render(child, buf);
        } else {
        }
    }

    // Restore child positions and pop clip for scrollable containers
    if (self->scrollable) {
        // Restore original child positions
        for (uint8_t i = 0; i < self->child_count; i++) {
            kryon_component_t* child = self->children[i];
            if (child != NULL) {
                child->y += self->scroll_offset_y;
                child->x += self->scroll_offset_x;
            }
        }

        // Pop clip rectangle
        kryon_pop_clip(buf);

        // Draw scrollbar if content overflows
        if (self->content_height > self->height) {
            // Scrollbar dimensions
            const uint16_t scrollbar_width = 12;
            const int16_t scrollbar_x = x + w - scrollbar_width;
            const int16_t scrollbar_y = y;
            const uint16_t scrollbar_height = h;

            // Draw scrollbar track
            uint32_t track_color = 0xE0E0E0FF;  // Light gray
            kryon_draw_rect(buf, scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height, track_color);

            // Calculate thumb size and position
            kryon_fp_t content_h = self->content_height;
            kryon_fp_t visible_h = self->height;
            kryon_fp_t scroll_range = content_h - visible_h;

            // Thumb height proportional to visible:content ratio
            uint16_t thumb_height = (uint16_t)((visible_h * kryon_fp_from_int(h)) / content_h);
            if (thumb_height < 20) thumb_height = 20;  // Minimum thumb size
            if (thumb_height > scrollbar_height) thumb_height = scrollbar_height;

            // Thumb position based on scroll offset
            uint16_t thumb_y_offset = 0;
            if (scroll_range > 0) {
                thumb_y_offset = (uint16_t)((self->scroll_offset_y * kryon_fp_from_int(scrollbar_height - thumb_height)) / scroll_range);
            }

            // Draw scrollbar thumb
            uint32_t thumb_color = 0xA0A0A0FF;  // Dark gray
            kryon_draw_rect(buf, scrollbar_x + 2, scrollbar_y + thumb_y_offset + 2,
                          scrollbar_width - 4, thumb_height - 4, thumb_color);
        }
    }
}

static bool container_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (self == NULL || event == NULL) {
        return false;
    }

    // Handle click-outside-to-close for dropdown children
    if (event->type == KRYON_EVT_CLICK) {
        const bool is_press = (event->param & 0x80000000u) == 0;
        if (is_press) {
            // Check all children for open dropdowns
            for (uint8_t i = 0; i < self->child_count; i++) {
                kryon_component_t* child = self->children[i];
                if (child != NULL && child->ops == &kryon_dropdown_ops && child->state != NULL) {
                    kryon_dropdown_state_t* dropdown_state = (kryon_dropdown_state_t*)child->state;
                    if (dropdown_state->is_open) {
                        // Check if click is outside the dropdown
                        // We need to account for the dropdown menu which extends below the component
                        if (!kryon_event_is_point_in_component(child, event->x, event->y)) {
                            // Click is outside dropdown, close it
                            dropdown_state->is_open = false;
                            child->z_index = 0;  // Restore normal z-index
                            kryon_component_mark_dirty(child);
                        }
                    }
                }
            }
        }
    }

    // Handle scroll events for scrollable containers
    if (self->scrollable && event->type == KRYON_EVT_SCROLL) {
        // Check if event is within this container's bounds
        if (kryon_event_is_point_in_component(self, event->x, event->y)) {
            // Extract scroll deltas from event
            int16_t delta_x, delta_y;
            kryon_event_get_scroll_delta(event, &delta_x, &delta_y);

            // Update scroll offset (delta_y is typically negative when scrolling down)
            // Use 40 pixels per scroll unit for natural browser-like scrolling
            kryon_fp_t new_offset_y = self->scroll_offset_y - kryon_fp_from_int(delta_y * 40);
            kryon_fp_t new_offset_x = self->scroll_offset_x - kryon_fp_from_int(delta_x * 40);

            // Clamp vertical scroll to valid range [0, max_scroll]
            kryon_fp_t max_scroll_y = self->content_height > self->height ?
                                     self->content_height - self->height : 0;
            if (new_offset_y < 0) new_offset_y = 0;
            if (new_offset_y > max_scroll_y) new_offset_y = max_scroll_y;

            // Clamp horizontal scroll to valid range [0, max_scroll]
            kryon_fp_t max_scroll_x = self->content_width > self->width ?
                                     self->content_width - self->width : 0;
            if (new_offset_x < 0) new_offset_x = 0;
            if (new_offset_x > max_scroll_x) new_offset_x = max_scroll_x;

            // Apply new offsets
            self->scroll_offset_y = new_offset_y;
            self->scroll_offset_x = new_offset_x;
            kryon_component_mark_dirty(self);

            // Event was handled
            return true;
        }
    }

    // Return false to allow normal event propagation
    return false;
}

static void container_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL) {
        return;
    }


    if (self->width == 0 && available_width > 0) {
        self->width = available_width;
    }
    // Don't set height from available_height - let layout calculation determine it from content

    kryon_layout_component(self,
        self->width > 0 ? self->width : available_width,
        self->height > 0 ? self->height : available_height);

    // Calculate content dimensions for scrollable containers
    // Note: This is done again in render phase for accuracy after all layout completes
    if (self->scrollable && self->child_count > 0) {
        kryon_fp_t max_right = 0;
        kryon_fp_t max_bottom = 0;

        for (uint8_t i = 0; i < self->child_count; i++) {
            kryon_component_t* child = self->children[i];
            if (child != NULL && child->visible) {
                kryon_fp_t child_right = child->x + child->width +
                                        kryon_fp_from_int(child->margin_right);
                kryon_fp_t child_bottom = child->y + child->height +
                                         kryon_fp_from_int(child->margin_bottom);

                if (child_right > max_right) max_right = child_right;
                if (child_bottom > max_bottom) max_bottom = child_bottom;
            }
        }

        self->content_width = max_right;
        self->content_height = max_bottom;
    }

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_container_ops = {
    .render = container_render,
    .on_event = container_on_event,
    .destroy = NULL,
    .layout = container_layout
};

// Text Component
static void text_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible || self->state == NULL) {
        return;
    }

    kryon_text_state_t* text_state = (kryon_text_state_t*)self->state;

    // Check if using rich text mode
    if (text_state->uses_rich_text && text_state->rich_text) {
        kryon_rich_text_render(self, text_state->rich_text, buf);
        return;
    }

    // Simple text mode
    if (text_state->text == NULL) {
        return;
    }

    // Calculate absolute position for rendering (used for both background and text)
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(self, &abs_x, &abs_y);
    int16_t render_x = KRYON_FP_TO_INT(abs_x);
    int16_t render_y = KRYON_FP_TO_INT(abs_y);
    uint16_t render_w = (uint16_t)KRYON_FP_TO_INT(self->width);
    uint16_t render_h = (uint16_t)KRYON_FP_TO_INT(self->height);

    // Draw background using inherited color with absolute coordinates
    uint32_t bg_color = kryon_component_get_effective_background_color(self);
    if ((bg_color & 0xFF) != 0) {
        kryon_draw_rect(buf, render_x, render_y, render_w, render_h, bg_color);
    }

    // Draw text using inherited color
    uint32_t text_color = kryon_component_get_effective_text_color(self);

    kryon_draw_text(buf,
                   text_state->text,
                   render_x,
                   render_y,
                   text_state->font_id,
                   text_state->font_size,
                   text_state->font_weight,
                   text_state->font_style,
                   text_color);
}

static void text_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) {
        return;
    }

    kryon_text_state_t* text_state = (kryon_text_state_t*)self->state;

    // DEBUG: Always log child count and ops
    if (getenv("KRYON_TRACE_LAYOUT")) {
        fprintf(stderr, "[kryon][text_layout] child_count=%d ops=%p text_ops=%p text='%s'\n",
                self->child_count, (void*)self->ops, (void*)&kryon_text_ops,
                text_state->text ? text_state->text : "(null)");
    }

    // Check if we have inline children (rich text mode)
    if (self->child_count > 0 && text_state) {
        // Rich text mode - flatten children to runs and layout
        if (getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "[kryon][richtext] Text component has %d children, entering rich text mode\n",
                    self->child_count);
        }

        // DEBUG: Verify execution continues
        if (getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "[kryon][richtext] After rich text mode msg, text_state=%p rich_text=%p\n",
                    (void*)text_state, (void*)(text_state ? text_state->rich_text : NULL));
        }

        // Create rich text if not already exists
        if (!text_state->rich_text) {
            text_state->rich_text = kryon_rich_text_create(1024, 32, 16);
            text_state->uses_rich_text = true;
            if (getenv("KRYON_TRACE_LAYOUT")) {
                fprintf(stderr, "[kryon][richtext] Created rich_text=%p\n", (void*)text_state->rich_text);
            }
        }

        if (text_state->rich_text) {
            if (getenv("KRYON_TRACE_LAYOUT")) {
                fprintf(stderr, "[kryon][richtext] About to flatten children\n");
            }
            // Flatten children to text runs
            kryon_text_flatten_children_to_runs(self, text_state->rich_text);

            // Layout the lines
            uint16_t max_width = available_width > 0 ? KRYON_FP_TO_INT(available_width) : 10000;
            uint8_t font_size = text_state->font_size > 0 ? text_state->font_size : 16;
            kryon_rich_text_layout_lines(text_state->rich_text, max_width, font_size, g_renderer);

            // Calculate total dimensions from lines
            uint16_t total_height = 0;
            uint16_t max_line_width = 0;

            for (uint16_t i = 0; i < text_state->rich_text->line_count; i++) {
                kryon_text_line_t* line = &text_state->rich_text->lines[i];
                total_height += line->height;
                if (line->width > max_line_width) {
                    max_line_width = line->width;
                }
            }

            self->width = KRYON_FP_FROM_INT(max_line_width);
            self->height = KRYON_FP_FROM_INT(total_height);
        }
    } else if (text_state && text_state->text) {
        // Simple text mode
        uint32_t text_len = strlen(text_state->text);
        uint8_t font_size = text_state->font_size > 0 ? text_state->font_size : 16;

        // Use same formula as layout.c get_component_intrinsic_width for consistency
        float char_width = font_size * 0.5f;

        // Account for bold (20% wider) and italic (5% wider)
        if (text_state->font_weight == KRYON_FONT_WEIGHT_BOLD) {
            char_width *= 1.2f;
        }
        if (text_state->font_style == KRYON_FONT_STYLE_ITALIC) {
            char_width *= 1.05f;
        }

        self->width = KRYON_FP_FROM_INT((int)(text_len * char_width));
        self->height = KRYON_FP_FROM_INT((int)(font_size * 1.2f));
    } else {
        // Fallback for empty text
        self->width = KRYON_FP_FROM_INT(50);
        self->height = KRYON_FP_FROM_INT(20);
    }

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_text_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

// ============================================================================
// Heading Components (H1-H6) - Native first-class components
// ============================================================================

const kryon_component_ops_t kryon_h1_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

const kryon_component_ops_t kryon_h2_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

const kryon_component_ops_t kryon_h3_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

const kryon_component_ops_t kryon_h4_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

const kryon_component_ops_t kryon_h5_ops = {
    .render = text_render,
    .on_event = NULL,
    .destroy = NULL,
    .layout = text_layout
};

const kryon_component_ops_t kryon_h6_ops = {
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
        uint32_t text_len = strlen(button_state->text);
        int16_t char_width = button_state->font_size > 0
            ? (int16_t)(button_state->font_size * 0.5f)
            : 8;
        if (char_width <= 0) char_width = 8;
        int16_t estimated_text_width = (int16_t)(text_len * char_width);
        int16_t estimated_text_height = button_state->font_size > 0
            ? button_state->font_size
            : 16;

        int16_t available_width = (int16_t)(w - (self->padding_left + self->padding_right));
        if (available_width < 0) available_width = 0;
        int16_t available_height = (int16_t)(h - (self->padding_top + self->padding_bottom));
        if (available_height < 0) available_height = 0;

        // Base (text) position relative to button
        int16_t text_x = x + self->padding_left;
        int16_t text_y = y + self->padding_top +
            (available_height > estimated_text_height ?
                (available_height - estimated_text_height) / 2 : 0);

        const char* draw_text = button_state->text;
        char truncated_buffer[192];
        int16_t layout_text_width = estimated_text_width;

        if (button_state->ellipsize && available_width > 0 && estimated_text_width > available_width) {
            int16_t chars_fit = (int16_t)(available_width / char_width);
            if (chars_fit <= 0) chars_fit = 1;
            if (chars_fit < (int16_t)text_len) {
                bool add_ellipsis = chars_fit > 3;
                int16_t copy_len = add_ellipsis ? (chars_fit - 3) : chars_fit;
                if (copy_len < 0) copy_len = 0;
                if (copy_len > (int16_t)text_len) {
                    copy_len = (int16_t)text_len;
                    add_ellipsis = false;
                }
                if (copy_len > (int16_t)sizeof(truncated_buffer) - 4) {
                    copy_len = (int16_t)sizeof(truncated_buffer) - 4;
                }
                if (copy_len > 0) {
                    memcpy(truncated_buffer, button_state->text, (size_t)copy_len);
                }
                if (add_ellipsis && copy_len >= 0) {
                    truncated_buffer[copy_len] = '.';
                    truncated_buffer[copy_len + 1] = '.';
                    truncated_buffer[copy_len + 2] = '.';
                    truncated_buffer[copy_len + 3] = '\0';
                } else {
                    truncated_buffer[copy_len] = '\0';
                }
                draw_text = truncated_buffer;
                layout_text_width = available_width;
            }
        }

        if (button_state->center_text) {
            if (available_width > layout_text_width) {
                text_x += (available_width - layout_text_width) / 2;
            }
        } else {
            // Left align with a small inset to mimic Chromium tabs
            text_x += 4;
        }

        if (available_width > 0) {
            kryon_draw_text(buf,
                           draw_text,
                           text_x,
                           text_y,
                           button_state->font_id,
                           button_state->font_size,
                           KRYON_FONT_WEIGHT_NORMAL,
                           KRYON_FONT_STYLE_NORMAL,
                           self->text_color);
        }
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
            button_state->pressed = is_press;
            kryon_component_mark_dirty(self);

            if (!is_press && button_state->on_click != NULL) {
                button_state->on_click(self, event);
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

void kryon_button_set_center_text(kryon_component_t* component, bool center) {
    if (component == NULL || component->ops != &kryon_button_ops || component->state == NULL) {
        return;
    }
    kryon_button_state_t* state = (kryon_button_state_t*)component->state;
    state->center_text = center;
    kryon_component_mark_dirty(component);
}

void kryon_button_set_ellipsize(kryon_component_t* component, bool ellipsize) {
    if (component == NULL || component->ops != &kryon_button_ops || component->state == NULL) {
        return;
    }
    kryon_button_state_t* state = (kryon_button_state_t*)component->state;
    state->ellipsize = ellipsize;
    kryon_component_mark_dirty(component);
}

void kryon_button_set_font_size(kryon_component_t* component, uint8_t font_size) {
    if (component == NULL || component->ops != &kryon_button_ops || component->state == NULL) {
        return;
    }
    kryon_button_state_t* state = (kryon_button_state_t*)component->state;
    state->font_size = font_size;
    kryon_component_mark_dirty(component);
}

// ============================================================================
// Dropdown Component
// ============================================================================

static void dropdown_render(kryon_component_t* self, kryon_cmd_buf_t* buf) {
    if (self == NULL || buf == NULL || !self->visible) {
        return;
    }

    kryon_dropdown_state_t* dropdown_state = (kryon_dropdown_state_t*)self->state;
    if (dropdown_state == NULL) return;

    // Calculate absolute position for rendering
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(self, &abs_x, &abs_y);
    int16_t x = KRYON_FP_TO_INT(abs_x);
    int16_t y = KRYON_FP_TO_INT(abs_y);
    uint16_t w = (uint16_t)KRYON_FP_TO_INT(self->width);
    uint16_t h = (uint16_t)KRYON_FP_TO_INT(self->height);

    // Draw main dropdown box background
    kryon_draw_rect(buf, x, y, w, h, dropdown_state->background_color);

    // Draw border
    if (dropdown_state->border_width > 0) {
        uint16_t bw = dropdown_state->border_width;
        // Top border
        kryon_draw_rect(buf, x, y, w, bw, dropdown_state->border_color);
        // Bottom border
        kryon_draw_rect(buf, x, y + h - bw, w, bw, dropdown_state->border_color);
        // Left border
        kryon_draw_rect(buf, x, y, bw, h, dropdown_state->border_color);
        // Right border
        kryon_draw_rect(buf, x + w - bw, y, bw, h, dropdown_state->border_color);
    }

    // Draw selected option or placeholder
    const char* display_text = dropdown_state->placeholder;
    if (dropdown_state->selected_index >= 0 &&
        dropdown_state->selected_index < dropdown_state->option_count) {
        display_text = dropdown_state->options[dropdown_state->selected_index];
    }

    if (display_text != NULL) {
        int16_t text_x = x + 8;  // Left padding
        int16_t text_y = y + (h - 16) / 2;  // Vertically center
        kryon_draw_text(buf, display_text, text_x, text_y,
                       dropdown_state->font_id, 0, KRYON_FONT_WEIGHT_NORMAL,
                       KRYON_FONT_STYLE_NORMAL, dropdown_state->text_color);
    }

    // Draw dropdown arrow indicator
    int16_t arrow_x = x + w - 20;
    int16_t arrow_y = y + h / 2;
    const char* arrow = dropdown_state->is_open ? "▲" : "▼";
    kryon_draw_text(buf, arrow, arrow_x, arrow_y - 8,
                   dropdown_state->font_id, 0, KRYON_FONT_WEIGHT_NORMAL,
                   KRYON_FONT_STYLE_NORMAL, dropdown_state->text_color);

    // Draw dropdown menu if open
    if (dropdown_state->is_open) {
        uint16_t option_height = h;
        int16_t menu_y = y + h;

        for (uint8_t i = 0; i < dropdown_state->option_count; i++) {
            uint32_t bg_color = (i == dropdown_state->hovered_index) ?
                dropdown_state->hover_color : dropdown_state->background_color;

            kryon_draw_rect(buf, x, menu_y + (i * option_height),
                          w, option_height, bg_color);

            // Draw option border
            if (dropdown_state->border_width > 0) {
                uint16_t bw = dropdown_state->border_width;
                kryon_draw_rect(buf, x, menu_y + (i * option_height), w, bw, dropdown_state->border_color);
                kryon_draw_rect(buf, x, menu_y + ((i + 1) * option_height) - bw, w, bw, dropdown_state->border_color);
                kryon_draw_rect(buf, x, menu_y + (i * option_height), bw, option_height, dropdown_state->border_color);
                kryon_draw_rect(buf, x + w - bw, menu_y + (i * option_height), bw, option_height, dropdown_state->border_color);
            }

            // Draw option text
            if (dropdown_state->options[i] != NULL) {
                int16_t option_text_x = x + 8;
                int16_t option_text_y = menu_y + (i * option_height) + (option_height - 16) / 2;
                kryon_draw_text(buf, dropdown_state->options[i],
                              option_text_x, option_text_y,
                              dropdown_state->font_id, 0, KRYON_FONT_WEIGHT_NORMAL,
                              KRYON_FONT_STYLE_NORMAL, dropdown_state->text_color);
            }
        }
    }
}

static bool dropdown_on_event(kryon_component_t* self, kryon_event_t* event) {
    if (self == NULL || event == NULL || self->state == NULL) {
        return false;
    }

    kryon_dropdown_state_t* dropdown_state = (kryon_dropdown_state_t*)self->state;

    switch (event->type) {
        case KRYON_EVT_CLICK: {
            const bool is_press = (event->param & 0x80000000u) == 0;
            if (is_press) {
                // Toggle dropdown or select option
                if (dropdown_state->is_open) {
                    // Check if clicking on an option
                    if (dropdown_state->hovered_index >= 0) {
                        dropdown_state->selected_index = dropdown_state->hovered_index;
                        if (dropdown_state->on_change != NULL) {
                            dropdown_state->on_change(self, dropdown_state->selected_index);
                        }
                        dropdown_state->is_open = false;
                        // Restore normal z-index when closed
                        self->z_index = 0;
                    } else {
                        dropdown_state->is_open = false;
                        // Restore normal z-index when closed
                        self->z_index = 0;
                    }
                } else {
                    dropdown_state->is_open = true;
                    // Set high z-index when opened to render on top
                    self->z_index = 200;
                }
                kryon_component_mark_dirty(self);
            }
            return true;
        }

        case KRYON_EVT_HOVER: {
            // Update hovered index when dropdown is open
            if (dropdown_state->is_open) {
                // Calculate which option is being hovered
                kryon_fp_t abs_x, abs_y;
                calculate_absolute_position(self, &abs_x, &abs_y);
                int16_t y = KRYON_FP_TO_INT(abs_y);
                uint16_t h = (uint16_t)KRYON_FP_TO_INT(self->height);

                int16_t mouse_y = event->y;
                int16_t menu_y = y + h;
                int16_t menu_end = menu_y + (dropdown_state->option_count * h);

                if (mouse_y >= menu_y && mouse_y < menu_end) {
                    int8_t new_index = (mouse_y - menu_y) / h;
                    if (new_index >= 0 && new_index < dropdown_state->option_count) {
                        dropdown_state->hovered_index = new_index;
                    } else {
                        dropdown_state->hovered_index = -1;
                    }
                    kryon_component_mark_dirty(self);
                }
            }
            return true;
        }

        default:
            return false;
    }
}

static void dropdown_layout(kryon_component_t* self, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (self == NULL || self->state == NULL) {
        return;
    }

    // Use explicit dimensions if set
    if (self->width == 0) self->width = KRYON_FP_FROM_INT(200);
    if (self->height == 0) self->height = KRYON_FP_FROM_INT(40);

    kryon_component_mark_clean(self);
}

const kryon_component_ops_t kryon_dropdown_ops = {
    .render = dropdown_render,
    .on_event = dropdown_on_event,
    .destroy = NULL,
    .layout = dropdown_layout
};

// ============================================================================
// Canvas Component
// ============================================================================

// Note: canvas_render is now in components.c to avoid duplication
// The kryon_canvas_ops is also defined there

// Helper function to set canvas ops (for Nim bindings)
void kryon_component_set_canvas_ops(kryon_component_t* component) {
    if (component == NULL) return;

    // Allocate canvas state if not already present
    if (component->state == NULL) {
        kryon_canvas_state_t* state = (kryon_canvas_state_t*)malloc(sizeof(kryon_canvas_state_t));
        if (state == NULL) return;

        memset(state, 0, sizeof(kryon_canvas_state_t));
        state->width = 0;  // Will be set during layout
        state->height = 0;
        state->background_color = 0;  // Transparent
        state->on_draw = NULL;
        state->on_update = NULL;

        component->state = state;
    }

    component->ops = &kryon_canvas_ops;
}

void kryon_canvas_set_draw_callback(kryon_component_t* component,
                                    bool (*on_draw)(kryon_component_t*, kryon_cmd_buf_t*)) {
    if (component == NULL || component->state == NULL) return;
    kryon_canvas_state_t* state = (kryon_canvas_state_t*)component->state;
    state->on_draw = on_draw;
}

void kryon_canvas_set_size(kryon_component_t* component, uint16_t width, uint16_t height) {
    if (component == NULL || component->state == NULL) return;
    kryon_canvas_state_t* state = (kryon_canvas_state_t*)component->state;
    state->width = width;
    state->height = height;
}

void kryon_canvas_component_set_background_color(kryon_component_t* component, uint32_t color) {
    if (component == NULL || component->state == NULL) return;

    // Set both component and canvas state background colors
    component->background_color = color;

    kryon_canvas_state_t* state = (kryon_canvas_state_t*)component->state;
    state->background_color = color;

    kryon_component_mark_dirty(component);
}
