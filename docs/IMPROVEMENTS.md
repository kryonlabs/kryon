# Flint UI: Improvements and Unified Architecture

This document analyzes the current Flint codebase for code duplicity, architectural issues, and proposes a unified model for improvement.

## Executive Summary

Flint UI is a lightweight C UI library with ~15 modules and 12 UI components. While functional, it exhibits significant code duplication, inconsistent patterns, and opportunities for architectural consolidation. This document identifies specific issues and proposes a unified architecture.

---

## Current Architecture Analysis

### Module Structure

```
Core Modules (15):
├── flint_color      - Color manipulation
├── flint_scaling    - DPI scaling
├── flint_dpi        - DPI state management
├── flint_layout     - Layout utilities
├── flint_clip       - Clipping utilities
├── flint_text       - Text rendering
├── flint_text_layout - Text layout with icons
├── flint_icons      - Icon loading
├── flint_theme      - Theme management
├── flint_theme_meta - Theme metadata
├── flint_locale     - Localization
├── flint_transition - Transition effects
├── flint_web        - Web utilities
└── flint_runtime_assets - Runtime asset downloading

UI Components (12):
├── bottom_nav       - Bottom navigation bar
├── toolbar          - Top toolbar
├── tab_bar          - Tab bar
├── dropdown         - Dropdown menu
├── modal            - Modal dialogs
├── scroll           - Scrollable containers
├── rows             - Info/button rows
├── theme_picker     - Theme picker
├── icon_controls    - Icon controls
├── guide            - Guide/tutorial overlay
└── tutorial         - Tutorial helpers
```

---

## Issues Identified

### 1. DPI/Scaling Duplication

**Problem:** Two separate modules handle DPI scaling with overlapping responsibilities.

- `flint_scaling.c`: Simple DPI scale factor (`g_dpi_scale`)
- `flint_dpi.c`: Complex DPI state with camera zoom (`FlintDpiState`)

**Evidence:**
```c
// flint_scaling.c
static float g_dpi_scale = 1.0f;
int flint_px(int px) { return (int)(px * g_dpi_scale + 0.5f); }

// flint_dpi.c
extern FlintDpiState flint_dpi_state;
static inline float flint_dpi_scale(void) { return flint_dpi_state.ui_scale_clamped; }
```

**Impact:** Confusing API, potential for inconsistent behavior

### 2. Button Drawing Duplication

**Problem:** Button drawing code is duplicated across at least 6 functions:

- `flint_ui_button()` - in flint_ui.c (767-809)
- `flint_ui_icon_button()` - in flint_ui.c (812-868)
- `ui_draw_icon_btn()` - in flint_ui.c (1125-1165)
- `ui_draw_icon_btn_padded()` - in flint_ui.c (1168-1205)
- `ui_draw_text_btn()` - in flint_ui.c (1208-1250)
- `ui_draw_generic_button()` - in flint_ui.c (1253-1340)

**Common Pattern:**
```c
// Each function repeats:
// 1. Mouse detection
// 2. Hover state calculation
// 3. Color selection
// 4. Rectangle drawing with bevel
// 5. Content drawing (text/icon)
// 6. Click detection
```

### 3. Bevel Drawing Duplication

**Problem:** Bevel drawing code exists in multiple places:

- `ui_draw_bevel()` in flint_ui.c (1097-1100)
- `flint_ui_draw_bevel()` in flint_ui.c (994-1000)
- Inline bevel drawing in button functions

### 4. Color Calculation Duplication

**Problem:** Darken/lighten operations scattered throughout:

```c
// In many files:
flint_darken(c_bg, 14)  // repeated ~50+ times
flint_lighten(fill, 40)  // repeated ~30+ times
```

### 5. Size Calculation Duplication

**Problem:** Icon button size calculations repeated:

```c
// flint_ui.c:1012-1021 (flint_ui_icon_btn_size)
// flint_ui.c:1113-1116 (ui_icon_btn_size)
// Both implement identical switch statements
```

### 6. Text Input State Duplication

**Problem:** Text input logic scattered:

- `FlintUITextInput` - display only
- `FlintUITextField` - editable with buffer
- `FlintUITextEdit` - edit operations only
- `flint_ui_text_edit()` - edit logic
- `flint_ui_text_field()` - field logic
- Internal queues for codepoints, backspace, enter

### 7. Layout Calculation Duplication

**Problem:** Similar layout calculations in multiple components:

- Centered column: `flint_centered_column()`, inline in bottom_nav.c
- Modal sizing: `ui_draw_modal()`, `ui_draw_modal_3btn()`
- Tab width: `ui_tab_bar_tab_width()`, inline in subtab_bar.c

### 8. Scroll Container Duplication

**Problem:** Three separate scroll APIs with overlapping functionality:

- `ui_scroll_container_*()` - Basic scroll container
- `ui_scroll_page_*()` - Multi-pass measuring scroll page
- `ui_draw_scrollbar()` - Standalone scrollbar

### 9. Focus System Inconsistency

**Problem:** Focus handling inconsistent across components:

- Some components use `focus_id`, others don't
- Focus registration optional but pattern unclear
- Keyboard navigation (Tab) only works when `ui_focus_begin()` called
- No clear documentation on when to use focus

### 10. Theme API Complexity

**Problem:** Theme system has multiple overlapping APIs:

- `flint_theme_get(scope, key)` - scoped colors
- `flint_theme_current_color(key)` - current theme
- `flint_theme_get_text()` etc. - predefined colors
- `flint_theme_meta` - theme metadata
- Theme scopes vs. aggregate variables vs. current theme

---

## Proposed Unified Architecture

### Principles

1. **Single Responsibility:** Each module has one clear purpose
2. **Don't Repeat Yourself:** Common code extracted to shared functions
3. **Consistent Patterns:** Similar components use similar patterns
4. **Clear Hierarchy:** Public API → Internal API → Implementation
5. **Separation of Concerns:** State, rendering, and interaction separated

### Unified Architecture

```
flint/
├── core/                    # Core utilities (no UI dependencies)
│   ├── flint_color.c       # Color utilities
│   ├── flint_math.c        # Math helpers (NEW)
│   ├── flint_rect.c        # Rectangle utilities (NEW)
│   └── flint_string.c      # String utilities (NEW)
│
├── platform/               # Platform abstraction
│   ├── flint_platform.c    # Platform detection and utilities
│   ├── flint_time.c        # Time utilities (NEW)
│   └── flint_http.c        # HTTP/download backend (NEW)
│
├── graphics/               # Graphics abstraction
│   ├── flint_renderer.c    # Renderer interface (NEW)
│   ├── flint_clip.c        # Clipping
│   ├── flint_texture.c     # Texture management (NEW)
│   └── flint_font.c        # Font management (NEW)
│
├── text/                   # Text subsystem
│   ├── flint_text.c        # Text rendering
│   ├── flint_text_layout.c # Text layout
│   └── flint_locale.c      # Localization
│
├── theme/                  # Theme subsystem
│   ├── flint_theme.c       # Theme management
│   └── flint_theme_colors.c # Color definitions (NEW)
│
├── layout/                 # Layout subsystem
│   ├── flint_dpi.c         # DPI/scaling (UNIFIED)
│   ├── flint_layout.c      # Layout utilities
│   └── flint_constraints.c # Layout constraints (NEW)
│
├── input/                  # Input handling
│   ├── flint_input.c       # Input state (NEW)
│   ├── flint_pointer.c     # Pointer/touch (NEW)
│   ├── flint_keyboard.c    # Keyboard/focus (NEW)
│   └── flint_text_edit.c   # Text editing (NEW)
│
├── assets/                 # Asset management
│   ├── flint_icons.c       # Icon loading
│   └── flint_runtime_assets.c # Runtime assets
│
├── animation/              # Animation subsystem
│   └── flint_transition.c  # Transitions
│
├── ui/                     # UI components
│   ├── flint_ui.c          # UI context/init
│   ├── flint_widget.c      # Base widget (NEW)
│   ├── flint_button.c      # Button (UNIFIED)
│   ├── flint_icon_button.c # Icon button (NEW)
│   ├── flint_text_input.c  # Text input (UNIFIED)
│   ├── flint_slider.c      # Sliders (UNIFIED)
│   ├── flint_container.c   # Scroll/panel (UNIFIED)
│   ├── flint_nav.c         # Navigation (UNIFIED from nav bars)
│   ├── flint_picker.c      # Pickers/dropdowns (UNIFIED)
│   ├── flint_modal.c       # Modals (UNIFIED)
│   └── flint_overlay.c     # Guides/tutorials (UNIFIED)
│
└── include/                # Public headers
    └── flint/
        ├── flint.h         # Main header
        ├── flint_*.h       # Module headers
        └── ui/
            └── flint_ui_*.h # UI component headers
```

### Key Unifications

#### 1. DPI/Scaling Unification

**Before:** `flint_scaling.c` + `flint_dpi.c`

**After:** Single `flint_dpi.c` module:

```c
// Unified DPI state
typedef struct {
    float scale;              // Raw DPI scale
    float scale_clamped;      // Clamped for UI
    int viewport_width;
    int viewport_height;
    Vector2i base_size;       // Base design size
} FlintDpiState;

// Unified API
void flint_dpi_init(float scale, Vector2i base_size);
void flint_dpi_update_viewport(int width, int height);
float flint_dpi_scale(void);
float flint_dpi_scale_clamped(void);
int flint_dpi_px(int px);              // Replaces flint_px()
int flint_dpi_px_clamp(int px, int min, int max); // Replaces flint_clamp_px()
```

#### 2. Button Unification

**Before:** 6 separate button drawing functions

**After:** Single widget base + specialized renderers:

```c
// Base widget structure
typedef struct {
    FlintRect bounds;          // Geometry
    FlintWidgetState state;   // Internal state
    FlintWidgetStyle *style;  // Visual style
    int flags;                // Widget flags
    const char *id;           // Widget ID for focus
} FlintWidget;

// Unified button API
typedef struct {
    FlintWidget widget;
    const char *label;
    Texture2D icon;
    FlintButtonVariant variant;
} FlintButton;

FlintButton *flint_button_new(const char *id, FlintRect bounds);
void flint_button_set_label(FlintButton *btn, const char *label);
void flint_button_set_icon(FlintButton *btn, Texture2D icon);
int flint_button_draw(FlintButton *btn);  // Returns clicked
```

**Benefits:**
- Single code path for button logic
- Consistent state management
- Easier to add new button types
- Built-in focus support

#### 3. Bevel/Border Unification

**Before:** Scattered bevel drawing

**After:** Unified style system:

```c
// Border styles
typedef enum {
    FLINT_BORDER_NONE,
    FLINT_BORDER_BEVEL,
    FLINT_BORDER_LINE,
    FLINT_BORDER_ROUNDED
} FlintBorderStyle;

typedef struct {
    FlintBorderStyle style;
    Color light;
    Color dark;
    float radius;
    float width;
} FlintBorder;

// Unified border drawing
void flint_draw_border(FlintRect bounds, const FlintBorder *border);
```

#### 4. Text Input Unification

**Before:** `FlintUITextInput`, `FlintUITextField`, `FlintUITextEdit`

**After:** Single text field widget:

```c
typedef struct {
    FlintWidget widget;
    char *buffer;
    size_t buffer_size;
    int cursor_pos;
    int selection_start;
    int selection_end;
    FlintTextEditFilter filter;
    void *filter_user_data;
    int flags;  // FLINT_TEXT_FLAG_MULTILINE, etc.
} FlintTextField;

// Unified API
FlintTextField *flint_text_field_new(const char *id, FlintRect bounds, size_t buffer_size);
int flint_text_field_draw(FlintTextField *field);  // Returns changed
void flint_text_field_set_text(FlintTextField *field, const char *text);
void flint_text_field_select_all(FlintTextField *field);
```

#### 5. Container/Scroll Unification

**Before:** `ui_scroll_container_*()`, `ui_scroll_page_*()`, `ui_draw_scrollbar()`

**After:** Single container widget:

```c
typedef struct {
    FlintWidget widget;
    FlintRect content_bounds;
    int *scroll_offset;
    int wheel_step;
    FlintScrollbarStyle scrollbar;
    FlintOverflow overflow;  // HIDDEN, SCROLL, AUTO
} FlintContainer;

FlintContainer *flint_container_new(const char *id, FlintRect bounds);
void flint_container_begin(FlintContainer *container);
void flint_container_end(FlintContainer *container);
int flint_container_get_scroll_y(FlintContainer *container);
```

#### 6. Navigation Unification

**Before:** `bottom_nav.c`, `toolbar.c`, `tab_bar.c`

**After:** Unified navigation widget:

```c
typedef enum {
    FLINT_NAV_STYLE_BOTTOM,
    FLINT_NAV_STYLE_TOP,
    FLINT_NAV_STYLE_TABS,
    FLINT_NAV_STYLE_SIDEBAR
} FlintNavStyle;

typedef struct {
    FlintNavStyle style;
    FlintNavItem *items;
    int item_count;
    int selected_index;
} FlintNavigation;

FlintNavigation *flint_nav_new(FlintNavStyle style);
int flint_nav_draw(FlintNavigation *nav);  // Returns clicked index
void flint_nav_set_selected(FlintNavigation *nav, int index);
```

#### 7. Focus System Unification

**Before:** Scattered focus handling

**After:** Centralized focus manager:

```c
// Focus manager (internal)
typedef struct {
    int focused_id;
    FlintVector prev_ids;    // Focus history
    FlintFocusMode mode;      // AUTO, MANUAL
} FlintFocusManager;

// Unified API
void flint_focus_begin(void);
void flint_focus_end(void);
void flint_focus_set(const char *id);
const char *flint_focus_get(void);
int flint_focus_is_focused(const char *id);
int flint_focus_navigate(FlintFocusDirection dir);  // Tab navigation
```

---

## Migration Path

### Phase 1: Core Unification (Low Risk)

1. **Merge DPI modules** → Single `flint_dpi.c`
2. **Extract bevel/border** → `flint_border.c`
3. **Unify color utilities** → `flint_color.c` enhancements
4. **Create math utilities** → `flint_math.c`

### Phase 2: Widget Foundation (Medium Risk)

1. **Create base widget** → `flint_widget.c`
2. **Unify buttons** → `flint_button.c`
3. **Unify text input** → `flint_text_input.c`
4. **Unify sliders** → `flint_slider.c`

### Phase 3: High-Level Unification (Higher Risk)

1. **Unify navigation** → `flint_navigation.c`
2. **Unify containers** → `flint_container.c`
3. **Unify modals** → `flint_modal.c`
4. **Migrate focus** → Centralized focus manager

### Phase 4: Cleanup

1. **Remove deprecated APIs**
2. **Update all examples**
3. **Update documentation**

---

## Code Reduction Estimates

| Module Type | Current (LOC) | Unified (LOC) | Reduction |
|-------------|--------------|---------------|-----------|
| Buttons     | ~400         | ~200          | 50%       |
| Text Input  | ~350         | ~250          | 29%       |
| Navigation  | ~500         | ~300          | 40%       |
| Scroll/Container | ~400  | ~250          | 38%       |
| Focus/Input | ~300         | ~200          | 33%       |
| **Total**   | **~1950**    | **~1200**     | **38%**   |

---

## New Features Enabled

### 1. Widget Pooling

```c
// Pre-allocate widgets to avoid runtime allocation
FlintWidgetPool *pool = flint_widget_pool_new(100);
FlintButton *btn = flint_widget_pool_acquire(pool, FLINT_WIDGET_BUTTON);
```

### 2. Style Inheritance

```c
// Child widgets inherit parent styles
FlintStyle *button_style = flint_style_new();
flint_style_inherit(button_style, parent_style);
```

### 3. Layout Constraints

```c
// Declarative layout
flint_constraint_width(btn, FLINT_CONSTRAINT_FILL);
flint_constraint_height(btn, FLINT_CONSTRAINT_WRAP);
flint_constraint_center(btn, FLINT_AXIS_BOTH);
```

### 4. Accessibility

```c
// Built-in accessibility support
flint_widget_set_label(btn, "Submit Form");
flint_widget_set_role(btn, FLINT_ROLE_BUTTON);
flint_widget_set_state(btn, FLINT_STATE_DISABLED);
```

---

## Implementation Checklist

### Immediate (Can start now)

- [ ] Create `core/flint_math.c` with utility functions
- [ ] Create `core/flint_rect.c` with rectangle utilities
- [ ] Extract bevel drawing to `flint_border.c`
- [ ] Document current button usage patterns

### Short-term (1-2 weeks)

- [ ] Design and implement `FlintWidget` base
- [ ] Implement unified DPI module
- [ ] Create widget pool allocator
- [ ] Write migration guide for existing code

### Medium-term (1-2 months)

- [ ] Implement unified button widget
- [ ] Implement unified text field widget
- [ ] Implement unified navigation widget
- [ ] Implement unified container widget

### Long-term (3-6 months)

- [ ] Migrate all existing components to new architecture
- [ ] Remove deprecated APIs
- [ ] Update all examples and documentation
- [ ] Add accessibility support

---

## Compatibility Strategy

### During Migration

1. **Keep old APIs** as wrappers around new implementation
2. **Mark deprecated** with `FLINT_DEPRECATED` macro
3. **Compile-time warnings** for deprecated usage
4. **Version flag** to enable/disable old APIs

### Post-Migration

1. **Deprecation period** of 2-3 minor versions
2. **Clear upgrade guide** with before/after examples
3. **Automated migration tool** if feasible

---

## Conclusion

The proposed unified architecture would:

1. **Reduce code duplication** by ~38%
2. **Improve consistency** across all UI components
3. **Enable new features** (pooling, accessibility, constraints)
4. **Simplify maintenance** with clear module boundaries
5. **Enhance extensibility** through widget composition

The migration is designed to be incremental, allowing existing code to continue working while new code adopts the unified patterns.

---

## Appendix: Example Code Comparison

### Before: Current API

```c
// Draw a button with current API
FlintUIButton btn = {
    .bounds = {10, 10, 100, 36},
    .label = "Click Me",
    .focus_id = 1,
    .disabled = 0,
    .background = c_button,
    .hover_background = c_button_hover,
    .text = c_text,
    .radius = 0.12f
};
if (flint_ui_button(btn)) {
    // Handle click
}
```

### After: Unified API

```c
// Draw a button with unified API
FlintButton *btn = flint_button_new("my_button", (FlintRect){10, 10, 100, 36});
flint_button_set_label(btn, "Click Me");
if (flint_button_draw(btn)) {
    // Handle click
}
// Widget auto-managed, cleaned up at end of frame
```

The unified API is cleaner, safer, and more consistent across all widget types.
