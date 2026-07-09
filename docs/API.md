# UI API Documentation

File UI Toolkit is a lightweight C UI component library for embedded applications and runtime environments. It provides core UI primitives and icon asset management without external dependencies.

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
  - [Lyra Sync](#lyra-sync)
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

### `InitUI`

Initialize the UI system with viewport dimensions and DPI scale.

```c
void InitUI(int width, int height, float dpi);
```

**Parameters:**
- `width` - Viewport width in pixels
- `height` - Viewport height in pixels
- `dpi` - DPI scale factor (1.0 = 96 DPI)

### `SetUIColors`

Set the global color scheme for UI elements.

```c
void SetUIColors(Color text, Color bg, Color surface, Color circle, Color button, Color button_hover, Color icon);
```

### `SetUIFrame`

Update the UI camera and reset per-frame state.

```c
void SetUIFrame(Camera2D camera);
```

`SetUIFrame` sanitizes invalid cameras before storing them. A zero-initialized
`Camera2D` is treated as an untransformed UI camera with `zoom = 1.0f`, so controls
continue to receive pointer input. If an application does not need a transformed UI
camera, prefer `BeginUIFrame`.

### `GetUIDefaultCamera`

Return the canonical untransformed UI camera.

```c
Camera2D GetUIDefaultCamera(void);
```

### `BeginUIFrame`

Convenience frame entry point for normal screen-space UI. It updates the viewport/DPI
state, updates the layout view size, and begins a frame with
`GetUIDefaultCamera()`.

```c
void BeginUIFrame(int width, int height, float dpi);
```

---

## Core Modules

### Color

#### `LightenUIColor`

Lighten a color by adding to each RGB component.

```c
Color LightenUIColor(Color c, int amount);
```

**Parameters:**
- `c` - Source color
- `amount` - Amount to add (0-255)

**Returns:** Lightened color

#### `DarkenUIColor`

Darken a color by subtracting from each RGB component.

```c
Color DarkenUIColor(Color c, int amount);
```

---

### Scaling

#### `SetUIScale`

Set the DPI scale factor (call once at startup).

```c
void SetUIScale(float scale);
```

#### `GetUIScale`

Get the current DPI scale factor.

```c
float GetUIScale(void);
```

#### `ScaleUIPx`

Scale a pixel value by the DPI factor.

```c
int ScaleUIPx(int px);
```

#### `ClampUIPx`

Scale and clamp a pixel value between min and max.

```c
int ClampUIPx(int px, int min_px, int max_px);
```

---

### DPI

#### `ui_dpi_state`

Global DPI state structure.

```c
typedef struct UIDPIState {
    int view_width;
    int view_height;
    float ui_scale;
    float ui_scale_clamped;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} UIDPIState;
```

#### `InitUIDPI`

Initialize DPI system.

```c
void InitUIDPI(void);
```

#### `UpdateUIDPI`

Update DPI state for new viewport size.

```c
void UpdateUIDPI(int view_width, int view_height);
```

---

### Layout

#### `SetUIViewSize`

Set the view dimensions.

```c
void SetUIViewSize(int width, int height);
```

#### `GetUIViewWidth` / `GetUIViewHeight`

Get current view dimensions.

```c
int GetUIViewWidth(void);
int GetUIViewHeight(void);
```

#### `GetUICenteredColumn`

Calculate centered column dimensions.

```c
void GetUICenteredColumn(int max_w, int side_pad, int *x, int *w);
```

**Parameters:**
- `max_w` - Maximum width
- `side_pad` - Side padding
- `x` - Output: x position (can be NULL)
- `w` - Output: width (can be NULL)

#### `GetUIPageSidePadding`

Calculate page side padding based on current view width.

```c
int GetUIPageSidePadding(void);
```

---

### Clipping

#### `GetUIClipIntersection`

Calculate intersection of two rectangles.

```c
Rectangle GetUIClipIntersection(Rectangle a, Rectangle b);
```

#### `BeginUIClip`

Begin a clipping region.

```c
void BeginUIClip(int x, int y, int w, int h);
```

#### `EndUIClip`

End the current clipping region.

```c
void EndUIClip(void);
```

#### `ResetUIClip`

Reset all clipping.

```c
void ResetUIClip(void);
```

---

### Text

#### Font Management

```c
void SetUIFont(Font font);
void SetUISmallFont(Font font);
Font GetUIFont(void);
```

#### Font Loading

```c
Font LoadUIChoppedFont(const char *png_path, const char *dat_path, int base_size);
Font LoadUIChoppedFontFromMemory(const unsigned char *png_data, unsigned int png_size,
                                              const unsigned char *dat_data, unsigned int dat_size, int base_size);
void UnloadUIFont(Font *font);
```

#### Text Measurement

```c
int MeasureUIText(const char *text, int font_size);
int GetUITextHeight(const char *text, int font_size);
int GetUITextLineHeight(int font_size);
```

#### Text Drawing

```c
void DrawUIText(const char *text, int x, int y, int font_size, Color color);
void DrawScaledUIText(const char *text, int x, int y, int scale, Color color);
void DrawCenteredUIText(const char *text, int center_x, int center_y, int font_size, Color color);
void DrawUITextInRect(const char *text, Rectangle rect, int font_size, Color color);
```

#### Vertical Centering

```c
int GetUITextY(const char *text, int box_y, int box_h, int font_size);
```

---

### Text Layout

Layout text with embedded icons and line breaks.

#### `UITextLayout`

```c
typedef struct UITextLayout {
    UITextElement *elements;
    int element_count;
    int *line_breaks;
    int line_count;
    int *line_widths;
    int total_height;
    int line_height;
    int last_reflow_width;
} UITextLayout;
```

#### `ParseUITextLayout`

Parse text input into a layout.

```c
UITextLayout ParseUITextLayout(const char *input, Texture2D icon, UIIconType icon_type, int icon_size);
```

#### `ReflowUITextLayout`

Reflow layout for a given width.

```c
void ReflowUITextLayout(UITextLayout *layout, int max_width, int font_size, int line_height);
```

#### `DrawUITextLayout`

Draw the layout.

```c
void DrawUITextLayout(UITextLayout *layout, int x, int *y, int font_size, Color color);
```

#### `GetUITextLayoutHeight` / `FreeUITextLayout`

```c
int GetUITextLayoutHeight(UITextLayout *layout);
void FreeUITextLayout(UITextLayout *layout);
```

---

### Icons

#### `GetUIIconAsset`

Get icon asset by type or name.

```c
const UIIconAsset *GetUIIconAsset(UIIconType type);
const UIIconAsset *GetUIIconAssetByName(const char *name);
```

#### `LoadUIIconTexture`

Load an icon texture.

```c
Texture2D LoadUIIconTexture(UIIconType type);
Texture2D LoadUIIconTextureByName(const char *name);
```

#### `LoadAllUIIconTextures` / `UnloadAllUIIconTextures`

```c
void LoadAllUIIconTextures(Texture2D *icons);
void UnloadAllUIIconTextures(Texture2D *icons);
```

---

### Theme

Theme management for colors and appearance.

#### `ResetTheme` / `RegisterThemeScope`

```c
void ResetTheme(void);
ThemeScope *RegisterThemeScope(const char *name, const char *path);
ThemeScope *RegisterDarkThemeScope(const char *name, const char *path, const char *dark_path);
```

#### `GetThemeColor` / `SetThemeColor`

```c
Color GetThemeColor(const char *scope, const char *key);
bool SetThemeColor(const char *scope, const char *key, Color color);
```

#### `SaveThemeScope` / `SaveAllThemes`

```c
bool SaveThemeScope(const char *scope);
bool SaveAllThemes(void);
```

#### Theme Export/Import

```c
bool ExportTheme(const char *path);
bool ImportTheme(const char *path);
```

#### Dark Mode

```c
void SetThemeDarkMode(bool dark);
bool GetThemeDarkMode(void);
void SetCurrentTheme(int theme_id, int dark_mode);
```

#### Theme Colors

```c
Color GetCurrentThemeColor(const char *key);
Color GetThemeText(void);
Color GetThemeBackground(void);
Color GetThemeSurface(void);
Color GetThemeCircle(void);
Color GetThemeButton(void);
Color GetThemeButtonHover(void);
Color GetThemeIcon(void);
```

---

### Locale

Localization support.

#### `InitLocale` / `SetLocale`

```c
void InitLocale(void);
int SetLocale(const char *code);
```

#### `GetLocaleText` / `FormatLocaleText`

```c
const char *GetLocaleText(const char *key);
void FormatLocaleText(char *dst, size_t dst_size, const char *key, ...);
```

#### Locale Information

```c
int GetLocaleCount(void);
const char *GetLocaleCode(int index);
const char *GetLocaleLabel(int index);
int GetLocaleIndex(const char *code);
const char *GetCurrentLocaleCode(void);
int GetCurrentLocaleIndex(void);
```

---

### Lyra Sync

Common Lyra sync protocol helpers. File UI Toolkit owns URL handling, token auth,
challenge/login, bearer requests, sync posting, account deletion, and small JSON
helpers. Applications still own their local data model and provide callbacks to
build sync payloads, apply sync responses, store auth tokens, and perform
platform HTTP.

#### `LyraSyncResult`

```c
typedef enum LyraSyncResult {
    LYRA_SYNC_OK = 0,
    LYRA_SYNC_INVALID_URL,
    LYRA_SYNC_NO_ACCOUNT,
    LYRA_SYNC_PAYLOAD_FAILED,
    LYRA_SYNC_CHALLENGE_FAILED,
    LYRA_SYNC_SIGN_FAILED,
    LYRA_SYNC_REQUEST_FAILED,
    LYRA_SYNC_AUTH_FAILED
} LyraSyncResult;
```

#### `LyraSyncConfig`

```c
typedef struct LyraSyncConfig {
    const char *base_url;
    const LyraAccount *account;
    const char *client_id;
    LyraSyncHttpRequestFn http_request;
    LyraSyncGetTextFn get_text;
    LyraSyncSetTextFn set_text;
    LyraSyncBuildPayloadFn build_payload;
    LyraSyncFreePayloadFn free_payload;
    LyraSyncApplyResponseFn apply_response;
    LyraSyncVoidFn purge_synced_deleted;
    LyraSyncLogFn log_http_failure;
    void *user;
} LyraSyncConfig;
```

`http_request` is platform-owned. Native apps can implement it with libcurl,
Android apps can bridge through JNI, and web apps can bridge to JavaScript fetch.
`get_text` and `set_text` store `sync_auth_token` and
`sync_auth_token_expires_at`.

#### URL Helpers

```c
int IsLyraSyncURLValid(const char *url);
int NormalizeLyraSyncURL(const char *input, char *out, size_t out_size);
int JoinLyraSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinLyraSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
```

Remote sync URLs must be HTTPS. HTTP is accepted only for loopback hosts such as
`localhost`, `127.0.0.1`, and Android emulator host `10.0.2.2`.

#### Buffer And JSON Helpers

```c
int AppendLyraSyncBuffer(LyraSyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendLyraSyncBufferJSONString(LyraSyncBuffer *buffer,
                                              const char *text);
void FreeLyraSyncBuffer(LyraSyncBuffer *buffer);
int FindLyraSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindLyraSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
```

These are intentionally small helpers for Lyra protocol payload construction and
simple response fields. Applications that already have a full JSON parser should
keep using it for domain data.

#### Auth And Sync

```c
void ClearLyraSyncAuthToken(const LyraSyncConfig *cfg);
LyraSyncResult LoginLyraSync(const LyraSyncConfig *cfg);
LyraSyncResult RunLyraSync(const LyraSyncConfig *cfg);
LyraSyncResult RequestLyraSyncBearer(const LyraSyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
LyraSyncResult DeleteLyraSyncAccount(const LyraSyncConfig *cfg);
const char *GetLyraSyncResultName(LyraSyncResult result);
```

`RunLyraSync` loads or refreshes an auth token, asks the app callback for
a local-first payload, posts it to `/api/v1/sync`, applies the response through
the callback, and purges synced tombstones on success. `RequestLyraSyncBearer`
is for app-specific Lyra endpoints that use the same account token.

---

### Transitions

Transition effects for screen changes.

#### `UITransition`

```c
typedef struct UITransition {
    int active;
    int phase;
    float elapsed_seconds;
    float duration_seconds;
} UITransition;
```

#### `ResetUITransition` / `BeginUITransition`

```c
void ResetUITransition(UITransition *transition);
void BeginUITransition(UITransition *transition, float duration_seconds);
```

#### `ReverseUITransitionToOut`

```c
void ReverseUITransitionToOut(UITransition *transition);
```

#### `GetUITransitionAlpha` / `StepUITransition`

```c
float GetUITransitionAlpha(const UITransition *transition);
int StepUITransition(UITransition *transition, float delta_seconds);
```

#### `DrawUITransitionFade`

```c
void DrawUITransitionFade(const UITransition *transition, int width, int height, Color color);
```

---

### Runtime Assets

Download and cache runtime assets.

#### `InitRuntimeAssets`

Initialize runtime asset system.

```c
int InitRuntimeAssets(const char *app_id);
```

#### `GetRuntimeAssetCacheRoot`

Get cache root directory.

```c
int GetRuntimeAssetCacheRoot(const char *app_id, char *out, size_t out_size);
```

#### `DownloadRuntimeAsset`

Download an asset.

```c
int DownloadRuntimeAsset(RuntimeAssetDownload *download, const char *url, const char *path);
```

#### `SetRuntimeAssetDownloadBackend`

Set custom download backend.

```c
void SetRuntimeAssetDownloadBackend(RuntimeAssetDownloadBackend backend);
```

---

### Web Utilities

Web platform specific utilities.

#### `GetWebViewportSize`

Get browser viewport size.

```c
void GetWebViewportSize(int fallback_width, int fallback_height, int *width, int *height);
```

#### `GetWebWindowFlags` / `SyncWebWindowSize`

```c
unsigned int GetWebWindowFlags(void);
int SyncWebWindowSize(void);
```

---

## UI Components

### Buttons

#### `UIButton`

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
} UIButton;
```

#### `DrawUIButton`

Draw and handle a button.

```c
int DrawUIButton(UIButton button);
```

**Returns:** 1 if clicked, 0 otherwise

#### `UIIconButton`

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
} UIIconButton;
```

#### `DrawUIIconButton`

```c
int DrawUIIconButton(UIIconButton button);
```

---

### Text Input

#### `UITextInputStyle`

```c
typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
} UITextInputStyle;
```

#### `UITextInput`

```c
typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    UITextInputStyle style;
} UITextInput;
```

#### `UITextField`

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
    UITextInputStyle style;
    UITextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} UITextField;
```

#### `DrawUITextInputControl` / `DrawUITextField`

```c
int DrawUITextInputControl(UITextInput input);
int DrawUITextField(UITextField field);
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
} UIBottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const UIBottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} UIBottomNav;

UIBottomNavResult DrawUIBottomNav(UIBottomNav nav);
int GetUIBottomNavHeight(void);
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
} UIToolbar;

UIToolbarResult DrawUIToolbar(UIToolbar toolbar);
UIToolbarHeaderResult DrawUIToolbarHeader(UIToolbarHeader header);
```

#### Tab Bar

```c
typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} UITab;

typedef struct {
    Rectangle bounds;
    const UITab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
} UITabBar;

int DrawUITabBar(UITabBar bar);
int GetUITabBarHeight(void);
```

#### Dropdown

```c
int DrawUIDropdownButton(int id, int x, int y, int w, int h,
                            const char **options, int option_count, int *selected_index);
int DrawUIDropdownMenu(int id);
int UIDropdownCapturesClick(Vector2 point);
```

---

### Modals

#### `DrawUIModal`

Simple two-button modal.

```c
int DrawUIModal(const char *title, const char *message,
                  const char *cancel_btn, const char *confirm_btn);
```

**Returns:** 1 for cancel, 2 for confirm

Backdrop clicks are blocked automatically for the current frame and the next frame.

#### `DrawUIModal3Button`

Three-button modal.

```c
int DrawUIModal3Button(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn, const char *right_btn);
```

Backdrop clicks are blocked automatically for the current frame and the next frame.

#### `UIPanelFrame` / `DrawUIModalFrame`

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
} UIPanelFrame;

UIPanelFrame DrawUIModalFrame(int width, int height, const char *title,
                                     Texture2D left_icon, Texture2D right_icon);
```

`DrawUIModalFrame` also updates the modal capture bounds automatically for the
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
} UIScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} UIScrollView;

UIScrollView MeasureUIScrollContainer(UIScrollArea area);
UIScrollView BeginUIScrollContainer(UIScrollArea area);
void EndUIScrollContainer(UIScrollArea area, UIScrollView view);
```

#### Scroll Page

```c
typedef int (*UIScrollPageHeightFn)(int content_width, void *user_data);

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
    UIScrollPageHeightFn content_height;
    void *user_data;
} UIScrollPageSpec;

typedef struct {
    UIScrollArea area;
    UIScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} UIScrollPage;

UIScrollPage BeginUIScrollPage(UIScrollPageSpec spec);
void EndUIScrollPage(UIScrollPage page);
```

#### Scrollbar

```c
int DrawUIScrollbar(int x, int y, int viewport_h, int content_h,
                     int *scroll_offset, int max_scroll);
```

---

### Controls

#### Sliders

```c
int DrawUISlider(int id, int x, int y, int w, const char *label,
                   int min, int max, int *value, const char *suffix);
int DrawUIVerticalSlider(int id, int x, int y, int h,
                            int min, int max, int *value);
```

#### Toggle Switch

```c
int DrawUIToggleSwitch(int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
```

#### Checkbox

```c
int DrawUICheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledUICheckboxToggle(int x, int y, const char *label,
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
} UIInfoRow;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const UIInfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} UIInfoRows;

void DrawUIInfoRows(UIInfoRows rows);
```

#### Button Rows

```c
typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} UIButtonRowItem;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const UIButtonRowItem *items;
    int count;
} UIButtonRow;

int GetUIButtonRowHeight(UIButtonRow row);
int DrawUIButtonRow(UIButtonRow row);
```

---

## Input Handling

### Input Capture

```c
int UIInputCapturesClick(Vector2 point);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
void SetUIModalCapture(Rectangle bounds);
```

`SetUIModalCapture` defines the active modal rectangle for the current frame and the
next frame. While a modal carried from the previous frame has not registered its current
bounds yet, all pointer input is captured. After registration, clicks outside the bounds
are captured while controls inside the modal remain usable.

Built-in modal helpers (`DrawUIModal`, `DrawUIModal3Button`, `DrawUIModalFrame`)
register their bounds automatically.

Applications should use `DrawUIModalFrame` for custom modal content instead of
manually drawing a backdrop and calling `SetUIModalCapture`. Manual capture remains
available for specialized overlays, but the helper keeps modal bounds, backdrop, and
input capture consistent across projects.

### Input Blocking

```c
void ui_set_input_blocked(int blocked);
```

### Hover Effects

```c
int UIHoverEffectsEnabled(void);
```

### Text Input Queuing

```c
void QueueUITextInputCodepoint(int codepoint);
void QueueUITextInputBackspace(void);
void QueueUITextInputEnter(void);
```

---

## Focus System

Keyboard navigation and focus management.

### Focus Begin/End

```c
void BeginUIFocus(void);
void EndUIFocus(void);
```

### Focus Registration

```c
int RegisterUIFocus(int id, Rectangle bounds);
```

**Returns:** 1 if this element has focus

### Focus State

```c
int IsUIFocusActive(int id);
int IsUIFocusActivatePressed(int id);
```

### Focus Control

```c
void SetUIFocus(int id);
void ClearUIFocus(void);
void SetUIFocusTextInputActive(int active);
```

### Focus Indicator

```c
void DrawUIFocus(Rectangle bounds);
```

---

## Utility Functions

### Bevel Drawing

```c
void DrawUIBevel(int x, int y, int w, int h, Color light, Color dark);
```

### Icon Buttons

```c
int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);
int DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon, int *hover);
```

### Generic Button

```c
int DrawUIGenericButton(int x, int y, int w, int h, const char *label,
                           UIButtonStyle style, int disabled, int *hover);
```

### Text Drawing Helpers

```c
void DrawCenteredUIControlText(const char *text, int center_x, int center_y, int font, Color color);
void DrawLeftUIControlTextInRect(const char *text, Rectangle rect, int font_size, Color color);
void DrawFittedUITextInRect(const char *text, Rectangle rect, int preferred_size, int min_size, Color color);
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
    THEME_SKY,
    THEME_OCEAN,
    THEME_FOREST,
    THEME_SUNSET,
    THEME_LAVENDER,
    THEME_CHERRY,
    THEME_DAWN,
    THEME_SAGE,
    THEME_INK,
    THEME_MONO,
    THEME_MINT,
    THEME_COBALT
} ThemeId;
```

---

## Text Sizes

```c
#define UI_TEXT_8 8
#define UI_TEXT_12 12
#define UI_TEXT_16 16
#define UI_TEXT_24 24
#define UI_TEXT_BASE_SIZE 16
```

---

## Integration Example

```c
#include "flint.h"

int main(void) {
    // Initialize window with Raylib
    InitWindow(320, 560, "File UI Toolkit Demo");
    SetTargetFPS(60);

    // Configure UI colors
    float dpi = 1.0f;  // Get from platform
    SetUIColors(
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

        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), dpi);

        // Draw UI
        if (DrawUIGenericButton(10, 10, 100, 36, "Click Me",
                                   UI_BUTTON_STYLE_PRIMARY, 0, NULL)) {
            // Button clicked
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```
