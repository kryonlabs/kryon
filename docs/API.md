# Flint UI API Documentation

Flint is a lightweight C UI component library for embedded applications and runtime environments. It provides core UI primitives and icon asset management without external dependencies.

## Table of Contents

- [Initialization](#initialization)
- [Core Modules](#core-modules)
  - [Color](#color)
  - [Scaling](#scaling)
  - [DPI](#dpi)
  - [Layout](#layout)
  - [Clipping](#clipping)
  - [Text](#text)
  - [Text Layout](#text-layout)
  - [Icons](#icons)
  - [Theme](#theme)
  - [Locale](#locale)
  - [Transitions](#transitions)
  - [Runtime Assets](#runtime-assets)
  - [Web Utilities](#web-utilities)
- [UI Components](#ui-components)
  - [Buttons](#buttons)
  - [Text Input](#text-input)
  - [Navigation](#navigation)
  - [Modals](#modals)
  - [Scrolling](#scrolling)
  - [Controls](#controls)
  - [Layout Components](#layout-components)
- [Input Handling](#input-handling)
- [Focus System](#focus-system)

---

## Initialization

### `ui_init`

Initialize the UI system with viewport dimensions and DPI scale.

```c
void ui_init(int width, int height, float dpi);
```

**Parameters:**
- `width` - Viewport width in pixels
- `height` - Viewport height in pixels
- `dpi` - DPI scale factor (1.0 = 96 DPI)

### `ui_set_colors`

Set the global color scheme for UI elements.

```c
void ui_set_colors(Color text, Color bg, Color surface, Color circle, Color button, Color button_hover, Color icon);
```

### `ui_set_frame`

Update the UI camera and reset per-frame state.

```c
void ui_set_frame(Camera2D camera);
```

---

## Core Modules

### Color

#### `flint_lighten`

Lighten a color by adding to each RGB component.

```c
Color flint_lighten(Color c, int amount);
```

**Parameters:**
- `c` - Source color
- `amount` - Amount to add (0-255)

**Returns:** Lightened color

#### `flint_darken`

Darken a color by subtracting from each RGB component.

```c
Color flint_darken(Color c, int amount);
```

---

### Scaling

#### `flint_set_dpi_scale`

Set the DPI scale factor (call once at startup).

```c
void flint_set_dpi_scale(float scale);
```

#### `flint_get_dpi_scale`

Get the current DPI scale factor.

```c
float flint_get_dpi_scale(void);
```

#### `flint_px`

Scale a pixel value by the DPI factor.

```c
int flint_px(int px);
```

#### `flint_clamp_px`

Scale and clamp a pixel value between min and max.

```c
int flint_clamp_px(int px, int min_px, int max_px);
```

---

### DPI

#### `flint_dpi_state`

Global DPI state structure.

```c
typedef struct FlintDpiState {
    int view_width;
    int view_height;
    float ui_scale;
    float ui_scale_clamped;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} FlintDpiState;
```

#### `flint_dpi_init`

Initialize DPI system.

```c
void flint_dpi_init(void);
```

#### `flint_dpi_update`

Update DPI state for new viewport size.

```c
void flint_dpi_update(int view_width, int view_height);
```

---

### Layout

#### `flint_set_view_size`

Set the view dimensions.

```c
void flint_set_view_size(int width, int height);
```

#### `flint_view_width` / `flint_view_height`

Get current view dimensions.

```c
int flint_view_width(void);
int flint_view_height(void);
```

#### `flint_centered_column`

Calculate centered column dimensions.

```c
void flint_centered_column(int max_w, int side_pad, int *x, int *w);
```

**Parameters:**
- `max_w` - Maximum width
- `side_pad` - Side padding
- `x` - Output: x position (can be NULL)
- `w` - Output: width (can be NULL)

#### `flint_page_side_padding`

Calculate page side padding based on current view width.

```c
int flint_page_side_padding(void);
```

---

### Clipping

#### `flint_clip_intersection`

Calculate intersection of two rectangles.

```c
Rectangle flint_clip_intersection(Rectangle a, Rectangle b);
```

#### `flint_clip_begin`

Begin a clipping region.

```c
void flint_clip_begin(int x, int y, int w, int h);
```

#### `flint_clip_end`

End the current clipping region.

```c
void flint_clip_end(void);
```

#### `flint_clip_reset`

Reset all clipping.

```c
void flint_clip_reset(void);
```

---

### Text

#### Font Management

```c
void flint_text_set_font(Font font);
void flint_text_set_small_font(Font font);
Font flint_text_font(void);
```

#### Font Loading

```c
Font flint_text_load_chopped_font(const char *png_path, const char *dat_path, int base_size);
Font flint_text_load_chopped_font_from_memory(const unsigned char *png_data, unsigned int png_size,
                                              const unsigned char *dat_data, unsigned int dat_size, int base_size);
void flint_text_unload_font(Font *font);
```

#### Text Measurement

```c
int flint_text_measure(const char *text, int font_size);
int flint_text_height(const char *text, int font_size);
int flint_text_line_height(int font_size);
```

#### Text Drawing

```c
void flint_text_draw(const char *text, int x, int y, int font_size, Color color);
void flint_text_draw_scaled(const char *text, int x, int y, int scale, Color color);
void flint_text_draw_centered(const char *text, int center_x, int center_y, int font_size, Color color);
void flint_text_draw_in_rect(const char *text, Rectangle rect, int font_size, Color color);
```

#### Vertical Centering

```c
int flint_text_y(const char *text, int box_y, int box_h, int font_size);
```

---

### Text Layout

Layout text with embedded icons and line breaks.

#### `FlintTextLayout`

```c
typedef struct FlintTextLayout {
    FlintTextElement *elements;
    int element_count;
    int *line_breaks;
    int line_count;
    int *line_widths;
    int total_height;
    int line_height;
    int last_reflow_width;
} FlintTextLayout;
```

#### `flint_text_layout_parse`

Parse text input into a layout.

```c
FlintTextLayout flint_text_layout_parse(const char *input, Texture2D icon, UIIconType icon_type, int icon_size);
```

#### `flint_text_layout_reflow`

Reflow layout for a given width.

```c
void flint_text_layout_reflow(FlintTextLayout *layout, int max_width, int font_size, int line_height);
```

#### `flint_text_layout_draw`

Draw the layout.

```c
void flint_text_layout_draw(FlintTextLayout *layout, int x, int *y, int font_size, Color color);
```

#### `flint_text_layout_get_height` / `flint_text_layout_free`

```c
int flint_text_layout_get_height(FlintTextLayout *layout);
void flint_text_layout_free(FlintTextLayout *layout);
```

---

### Icons

#### `flint_icon_asset`

Get icon asset by type or name.

```c
const FlintIconAsset *flint_icon_asset(UIIconType type);
const FlintIconAsset *flint_icon_asset_by_name(const char *name);
```

#### `flint_load_icon_texture`

Load an icon texture.

```c
Texture2D flint_load_icon_texture(UIIconType type);
Texture2D flint_load_icon_texture_by_name(const char *name);
```

#### `flint_load_all_icons` / `flint_unload_all_icons`

```c
void flint_load_all_icons(Texture2D *icons);
void flint_unload_all_icons(Texture2D *icons);
```

---

### Theme

Theme management for colors and appearance.

#### `flint_theme_reset` / `flint_theme_register_scope`

```c
void flint_theme_reset(void);
FlintThemeScope *flint_theme_register_scope(const char *name, const char *path);
FlintThemeScope *flint_theme_register_scope_dark(const char *name, const char *path, const char *dark_path);
```

#### `flint_theme_get` / `flint_theme_set_color`

```c
Color flint_theme_get(const char *scope, const char *key);
bool flint_theme_set_color(const char *scope, const char *key, Color color);
```

#### `flint_theme_save_scope` / `flint_theme_save_all`

```c
bool flint_theme_save_scope(const char *scope);
bool flint_theme_save_all(void);
```

#### Theme Export/Import

```c
bool flint_theme_export_theme(const char *path);
bool flint_theme_import_theme(const char *path);
```

#### Dark Mode

```c
void flint_theme_set_dark_mode(bool dark);
bool flint_theme_get_dark_mode(void);
void flint_theme_set_current(int theme_id, int dark_mode);
```

#### Theme Colors

```c
Color flint_theme_current_color(const char *key);
Color flint_theme_get_text(void);
Color flint_theme_get_bg(void);
Color flint_theme_get_surface(void);
Color flint_theme_get_circle(void);
Color flint_theme_get_button(void);
Color flint_theme_get_button_hover(void);
Color flint_theme_get_icon(void);
```

---

### Locale

Localization support.

#### `locale_init` / `locale_set`

```c
void locale_init(void);
int locale_set(const char *code);
```

#### `locale_get` / `locale_format`

```c
const char *locale_get(const char *key);
void locale_format(char *dst, size_t dst_size, const char *key, ...);
```

#### Locale Information

```c
int locale_count(void);
const char *locale_code_at(int index);
const char *locale_label_at(int index);
int locale_index_of(const char *code);
const char *locale_current_code(void);
int locale_current_index(void);
```

---

### Transitions

Transition effects for screen changes.

#### `FlintTransition`

```c
typedef struct FlintTransition {
    int active;
    int phase;
    int ticks;
    int duration;
} FlintTransition;
```

#### `flint_transition_reset` / `flint_transition_begin`

```c
void flint_transition_reset(FlintTransition *transition);
void flint_transition_begin(FlintTransition *transition, int duration);
```

#### `flint_transition_reverse_to_out`

```c
void flint_transition_reverse_to_out(FlintTransition *transition);
```

#### `flint_transition_alpha` / `flint_transition_step`

```c
float flint_transition_alpha(const FlintTransition *transition);
int flint_transition_step(FlintTransition *transition);
```

#### `flint_transition_draw_fade`

```c
void flint_transition_draw_fade(const FlintTransition *transition, int width, int height, Color color);
```

---

### Runtime Assets

Download and cache runtime assets.

#### `flint_runtime_assets_init`

Initialize runtime asset system.

```c
int flint_runtime_assets_init(const char *app_id);
```

#### `flint_runtime_asset_cache_root`

Get cache root directory.

```c
int flint_runtime_asset_cache_root(const char *app_id, char *out, size_t out_size);
```

#### `flint_runtime_asset_download`

Download an asset.

```c
int flint_runtime_asset_download(FlintRuntimeAssetDownload *download, const char *url, const char *path);
```

#### `flint_runtime_asset_set_download_backend`

Set custom download backend.

```c
void flint_runtime_asset_set_download_backend(FlintRuntimeAssetDownloadBackend backend);
```

---

### Web Utilities

Web platform specific utilities.

#### `flint_web_viewport_size`

Get browser viewport size.

```c
void flint_web_viewport_size(int fallback_width, int fallback_height, int *width, int *height);
```

#### `flint_web_window_flags` / `flint_web_sync_window_size`

```c
unsigned int flint_web_window_flags(void);
int flint_web_sync_window_size(void);
```

---

## UI Components

### Buttons

#### `FlintUIButton`

```c
typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color text;
    Color border;
    float radius;
} FlintUIButton;
```

#### `flint_ui_button`

Draw and handle a button.

```c
int flint_ui_button(FlintUIButton button);
```

**Returns:** 1 if clicked, 0 otherwise

#### `FlintUIIconButton`

```c
typedef struct {
    Rectangle bounds;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int icon_padding;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color icon_color;
    Color border;
    float radius;
} FlintUIIconButton;
```

#### `flint_ui_icon_button`

```c
int flint_ui_icon_button(FlintUIIconButton button);
```

---

### Text Input

#### `FlintUITextInputStyle`

```c
typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
} FlintUITextInputStyle;
```

#### `FlintUITextInput`

```c
typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    FlintUITextInputStyle style;
} FlintUITextInput;
```

#### `FlintUITextField`

```c
typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int max_codepoints;
    int font;
    int focus_id;
    FlintUITextInputStyle style;
    FlintUITextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} FlintUITextField;
```

#### `flint_ui_text_input` / `flint_ui_text_field`

```c
int flint_ui_text_input(FlintUITextInput input);
int flint_ui_text_field(FlintUITextField field);
```

---

### Navigation

#### Bottom Navigation

```c
typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int active;
    int disabled;
} FlintUIBottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const FlintUIBottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} FlintUIBottomNav;

FlintUIBottomNavResult ui_draw_bottom_nav(FlintUIBottomNav nav);
int ui_bottom_nav_height(void);
```

#### Toolbar

```c
typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    int draw_menu;
    const char **options;
    int option_count;
    int *selected_index;
    // ... more fields
} FlintUIToolbar;

FlintUIToolbarResult ui_draw_toolbar(FlintUIToolbar toolbar);
FlintUIToolbarHeaderResult ui_draw_toolbar_header(FlintUIToolbarHeader header);
```

#### Tab Bar

```c
typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} FlintUITab;

typedef struct {
    Rectangle bounds;
    const FlintUITab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
} FlintUITabBar;

int ui_draw_tab_bar(FlintUITabBar bar);
int ui_tab_bar_height(void);
```

#### Dropdown

```c
int ui_draw_dropdown_button(int id, int x, int y, int w, int h,
                            const char **options, int option_count, int *selected_index);
int ui_draw_dropdown_menu(int id);
int ui_dropdown_captures_click(Vector2 point);
```

---

### Modals

#### `ui_draw_modal`

Simple two-button modal.

```c
int ui_draw_modal(const char *title, const char *message,
                  const char *cancel_btn, const char *confirm_btn);
```

**Returns:** 1 for cancel, 2 for confirm

Backdrop clicks are blocked automatically for the current frame and the next frame.

#### `ui_draw_modal_3btn`

Three-button modal.

```c
int ui_draw_modal_3btn(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn, const char *right_btn);
```

Backdrop clicks are blocked automatically for the current frame and the next frame.

#### `FlintUIPanelFrame` / `ui_draw_modal_frame`

```c
typedef struct {
    int x;
    int y;
    int w;
    int h;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int left_clicked;
    int right_clicked;
} FlintUIPanelFrame;

FlintUIPanelFrame ui_draw_modal_frame(int width, int height, const char *title,
                                     Texture2D left_icon, Texture2D right_icon);
```

`ui_draw_modal_frame` also updates the modal capture bounds automatically for the
current frame and the next frame.

---

### Scrolling

#### Scroll Container

```c
typedef struct {
    Rectangle bounds;
    int content_height;
    int content_x;
    int content_width;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
} FlintUIScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} FlintUIScrollView;

FlintUIScrollView ui_scroll_container_measure(FlintUIScrollArea area);
FlintUIScrollView ui_scroll_container_begin(FlintUIScrollArea area);
void ui_scroll_container_end(FlintUIScrollArea area, FlintUIScrollView view);
```

#### Scroll Page

```c
typedef int (*FlintUIScrollPageHeightFn)(int content_width, void *user_data);

typedef struct {
    int y;
    int height;
    int max_content_width;
    int min_content_width;
    int side_padding;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
    int measure_passes;
    FlintUIScrollPageHeightFn content_height;
    void *user_data;
} FlintUIScrollPageSpec;

typedef struct {
    FlintUIScrollArea area;
    FlintUIScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} FlintUIScrollPage;

FlintUIScrollPage ui_scroll_page_begin(FlintUIScrollPageSpec spec);
void ui_scroll_page_end(FlintUIScrollPage page);
```

#### Scrollbar

```c
int ui_draw_scrollbar(int x, int y, int viewport_h, int content_h,
                     int *scroll_offset, int max_scroll);
```

---

### Controls

#### Sliders

```c
int ui_draw_slider(int id, int x, int y, int w, const char *label,
                   int min, int max, int *value, const char *suffix);
int ui_draw_slider_vertical(int id, int x, int y, int h,
                            int min, int max, int *value);
```

#### Toggle Switch

```c
int ui_draw_toggle_switch(int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
```

#### Checkbox

```c
int ui_draw_checkbox_toggle(int x, int y, const char *label, int *value);
int ui_draw_checkbox_toggle_disabled(int x, int y, const char *label,
                                     int *value, int disabled);
```

---

### Layout Components

#### Info Rows

```c
typedef struct {
    const char *text;
    int font;
    Color color;
} FlintUIInfoRow;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const FlintUIInfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} FlintUIInfoRows;

void ui_draw_info_rows(FlintUIInfoRows rows);
```

#### Button Rows

```c
typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} FlintUIButtonRowItem;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const FlintUIButtonRowItem *items;
    int count;
} FlintUIButtonRow;

int ui_button_row_height(FlintUIButtonRow row);
int ui_draw_button_row(FlintUIButtonRow row);
```

---

## Input Handling

### Input Capture

```c
int ui_input_captures_click(Vector2 point);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
void ui_set_modal_capture(Rectangle bounds);
```

`ui_set_modal_capture` defines the active modal rectangle for the current frame and the
next frame. Any click outside that bounds is treated as captured, so the rest of the
screen does not receive pointer events while the modal is rendered.

Built-in modal helpers (`ui_draw_modal`, `ui_draw_modal_3btn`, `ui_draw_modal_frame`)
register their bounds automatically.

### Input Blocking

```c
void ui_set_input_blocked(int blocked);
```

### Hover Effects

```c
int ui_hover_effects_enabled(void);
```

### Text Input Queuing

```c
void flint_ui_text_input_queue_codepoint(int codepoint);
void flint_ui_text_input_queue_backspace(void);
void flint_ui_text_input_queue_enter(void);
```

---

## Focus System

Keyboard navigation and focus management.

### Focus Begin/End

```c
void ui_focus_begin(void);
void ui_focus_end(void);
```

### Focus Registration

```c
int ui_focus_register(int id, Rectangle bounds);
```

**Returns:** 1 if this element has focus

### Focus State

```c
int ui_focus_is_active(int id);
int ui_focus_activate_pressed(int id);
```

### Focus Control

```c
void ui_focus_set(int id);
void ui_focus_clear(void);
void ui_focus_set_text_input_active(int active);
```

### Focus Indicator

```c
void ui_focus_draw(Rectangle bounds);
```

---

## Utility Functions

### Bevel Drawing

```c
void ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark);
```

### Icon Buttons

```c
int ui_icon_btn_size(UIIconSize size);
int ui_icon_btn_padding(UIIconSize size);
int ui_draw_icon_btn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int ui_draw_icon_btn_padded(int x, int y, int size, int padding, Texture2D icon, int *hover);
```

### Generic Button

```c
int ui_draw_generic_button(int x, int y, int w, int h, const char *label,
                           UIButtonStyle style, int disabled, int *hover);
```

### Text Drawing Helpers

```c
void flint_ui_draw_text_centered(const char *text, int center_x, int center_y, int font, Color color);
void flint_ui_draw_text_left_in_rect(const char *text, Rectangle rect, int font_size, Color color);
void ui_draw_fitted_text_in_rect(const char *text, Rectangle rect, int preferred_size, int min_size, Color color);
```

---

## Button Styles

```c
typedef enum {
    UI_BUTTON_STYLE_PRIMARY,
    UI_BUTTON_STYLE_SECONDARY,
    UI_BUTTON_STYLE_DANGER,
    UI_BUTTON_STYLE_TAB,
    UI_BUTTON_STYLE_TAB_SELECTED
} UIButtonStyle;
```

---

## Icon Sizes

```c
typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;
```

---

## Theme IDs

```c
typedef enum {
    FLINT_THEME_SKY,
    FLINT_THEME_OCEAN,
    FLINT_THEME_FOREST,
    FLINT_THEME_SUNSET,
    FLINT_THEME_LAVENDER,
    FLINT_THEME_CHERRY,
    FLINT_THEME_DAWN,
    FLINT_THEME_SAGE,
    FLINT_THEME_INK,
    FLINT_THEME_MONO,
    FLINT_THEME_MINT,
    FLINT_THEME_COBALT
} FlintThemeId;
```

---

## Text Sizes

```c
#define FLINT_TEXT_8 8
#define FLINT_TEXT_12 12
#define FLINT_TEXT_16 16
#define FLINT_TEXT_24 24
#define FLINT_TEXT_BASE_SIZE 16
```

---

## Integration Example

```c
#include "flint.h"

int main(void) {
    // Initialize window with Raylib
    InitWindow(320, 560, "Flint Demo");
    SetTargetFPS(60);

    // Initialize Flint UI
    float dpi = 1.0f;  // Get from platform
    ui_init(320, 560, dpi);
    ui_set_colors(
        (Color){240, 240, 240, 255},  // text
        (Color){40, 40, 40, 255},     // bg
        (Color){60, 60, 60, 255},     // surface
        (Color){80, 80, 80, 255},     // circle
        (Color){160, 160, 160, 255},  // button
        (Color){180, 180, 180, 255},  // button_hover
        (Color){200, 200, 200, 255}   // icon
    );

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // Set camera
        Camera2D camera = {0};
        ui_set_frame(camera);

        // Draw UI
        if (ui_draw_generic_button(10, 10, 100, 36, "Click Me",
                                   UI_BUTTON_STYLE_PRIMARY, 0, NULL)) {
            // Button clicked
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```
