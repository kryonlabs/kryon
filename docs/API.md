# UI API Documentation

Kryon is a lightweight C UI component library for embedded applications and runtime environments. It provides core UI primitives and icon asset management without external dependencies.

## Table of Contents

- [Initialization](#initialization)
- [Canonical App API](#canonical-app-api)
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
  - [Sync](#sync)
  - [Transitions](#transitions)
  - [Runtime Assets](#runtime-assets)
  - [Desktop App Integration](#desktop-app-integration)
  - [File Dialogs](#file-dialogs)
  - [Web Utilities](#web-utilities)
- [UI Components](#ui-components)
  - [Buttons](#buttons)
  - [Pictures](#pictures)
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

### `SetCurrentTheme`

Set the active theme. UI controls automatically use the active theme colors.

```c
void SetCurrentTheme(int theme_id, int current_dark_mode);
void SetUILinkColor(Color link);
```

### `SetUIFrame`

Update the UI camera and reset per-frame state.

```c
void SetUIFrame(Camera2D camera);
```

`SetUIFrame` sanitizes invalid cameras before storing them. A zero-initialized
`Camera2D` is treated as an untransformed UI camera with `zoom = 1.0f`, so controls
continue to receive pointer input. It also closes the previous focus pass and
starts a new one, so registered focus controls are reset every frame. If an
application does not need a transformed UI camera, prefer `BeginUIFrame`.

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

### `EndUIFrame`

Finish the current UI frame after all widgets have been drawn and before the
backend drawing pass ends. This draws deferred overlays, including dropdowns and
text-input context menus, and finalizes focus and inspection state.

```c
void EndUIFrame(void);
```

---

## Canonical App API

Kryon owns the public app-facing API. The default backend still implements much
of the surface through raylib, but application and generated code should include
Kryon headers and call Kryon-owned names directly. That keeps apps portable to
future backends.

Use the raylib-style drawing, input, texture, window, and math names provided by
Kryon, such as:

```c
DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
Vector2 mouse = GetMousePosition();
if(IsKeyPressed(KEY_ESCAPE))
    CloseWindow();
```

Use canonical widget drawing names when drawing a concrete widget immediately:

```c
DrawUIIconButton(button);
DrawUITextField(field);
DrawUISlider(id, x, y, w, "Volume", 0, 100, &volume, "%");
DrawUIOverlays();
```

Normal application UI is declared between `BeginUI` and `EndUI`. Containers
use one matching `End`, independent of their kind. Reconciliation, layout,
input routing, updates, painting, and overlays are runtime-owned and are not
application lifecycle calls:

```c
BeginUI(Key("settings"));
Column((ColumnProps){
    .bounds = {20, 20, 360, 420},
    .gap = 12,
    .padding = 16,
    .key = Key("settings/body"),
});
Text("Account", 0, 0, UI_TEXT_24, GetThemeText());
TextField(account_field);
Button(save_button);
End();
EndUI();

while(NextUIEvent(&event)) {
    if(event.key == Key("settings/save") &&
       event.kind == UI_EVENT_CLICK)
        SaveSettings();
}
```

Migrate callers to the current Kryon API directly so the backend boundary stays
simple.

Cartridges (`.krb`) are portable Kryon render artifacts produced from KIR. The
runtime loads packed node data, state schema, source metadata, portable logic,
capabilities, and explicit host imports. Load the image, bind host functions by
import name when the cartridge declares them, and draw through `KryBackend`:

```c
KrbImage img;
KrbLoadFile(&img, "02_buttons.krb");
KrbBind(&img, "primary_button", on_primary, app);
KrbDraw(&img, 0, 0, width, height);
KrbFree(&img);
```

`k2b` is the cartridge compiler. It accepts `.kry` or `.kir`; `.kry` input is
lowered through KIR before the KRB sections are written. `KryBackendDraw`
implements the rendering table with the public Kryon draw/input API;
`KryBackendNull` is the headless stand-in. Mount live C fields so a cartridge
can read them as files:

```c
KrbField fields[] = {
    { "score", offsetof(App, score), KRB_I32, 4 },
    { NULL }
};
KrbMount(&img, "/app", app, fields);
KrbReadI32(&img, "/app/score", &n);
```

`OP_CALL_HOST` and `OP_SET_I32` run from `KrbExec`. The portable path is
designed around KIR-owned logic plus a capability/import table; native C apps
should continue to use the C backend when direct library integration is the
right target. See `docs/KRB_FORMAT.md`.

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
Font GetUIFont(void);
int RegisterUIFont(const char *name, Font font);
int RegisterUISmallFont(const char *name, Font font);
int UseUIFont(const char *name);
int UIFontHasGlyph(Font font, int codepoint);
```

#### Font Loading

```c
Font LoadUIFontFromMemory(const char *file_type, const unsigned char *font_data, unsigned int font_size, int base_size);
Font LoadUIFontAsset(const char *path, int base_size);
void UnloadUIFont(Font *font);
void ClearUIFonts(void);
void UIFontMemoryReport(const char *tag);
```

`UIFontMemoryReport` prints per-font rasterization stats (codepoint counts,
rasterized sizes, glyph counts) to stderr. It is a no-op unless
`KRYON_MEM_DEBUG` is set in the environment.

Source fonts registered through `RegisterUIFontSource` rasterize their
declared codepoints at each requested physical size and retain bounded size
tiers. Their atlas coverage is immutable after registration: drawing or
typing text never reallocates a font texture. Supply every codepoint the
source is expected to render; omitting the list selects Kryon's standard UI
coverage. `RegisterUIFixedFontSource` is an equivalent explicit name.

#### Text Measurement

```c
int MeasureUIText(const char *text, int font_size);
int GetUITextHeight(const char *text, int font_size);
int GetUITextLineHeight(int font_size);
```

#### Text Widgets

```c
void Text(const char *text, int x, int y, int font_size, Color color);
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

Profile-picture PNGs in `pfp/` are embedded into the same icon catalog with
`UI_ICON_TYPE_PFP_*` enum values. Use `GetUIProfilePictureIconCount`,
`GetUIProfilePictureIconType`, and `GetUIProfilePictureIconName` to enumerate
the standard profile-picture options.

Kryon also exposes stable `UI_KSYNC_PROFILE_ICON_*` IDs and mapping helpers:

```c
UIIconType GetUIProfilePictureIconTypeForKsyncID(int ksync_id);
int GetUIKsyncIDForProfilePictureIconType(UIIconType type);
```

Use those IDs for server storage or sync payloads instead of generated
`UIIconType` ordinals.

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
Color GetThemeLink(void);
```

#### Theme Styles

Kryon supports three concrete widget styles plus a system selector:

```c
typedef enum ThemeStyle {
    THEME_STYLE_SYSTEM = 0,
    THEME_STYLE_RETRO,
    THEME_STYLE_MATERIAL,
    THEME_STYLE_AERO
} ThemeStyle;

void SetThemeStyle(ThemeStyle style);
ThemeStyle GetThemeStyle(void);
ThemeStyle GetEffectiveThemeStyle(void);
ThemeStyle GetDefaultPlatformThemeStyle(void);
int GetDefaultThemeForThemeStyle(ThemeStyle style);
const char *GetThemeStyleLabel(ThemeStyle style);
```

`THEME_STYLE_SYSTEM` resolves to Material on Android builds and Retro elsewhere.
Material uses Material 3 style tokens: 48px touch targets, rounded controls,
state layers/ripple feedback, elevation shadows, and theme-derived Material color
roles. Retro keeps the original beveled Kryon look.

Aero renders the Windows Vista/7 glass look — translucent control and panel
fills, gloss highlights, accent hover glow, soft layered shadows, inset text
fields and gradient tracks — using only plain drawing primitives, so it renders
identically on every backend including Android, web and the software renderer.
It defaults to the Sky palette but derives its glass tints from
whichever palette is active, so all built-in palettes work with it.

```c
UIStyleTokens GetUIStyleTokens(void);
UIMaterialScheme GetUIMaterialScheme(void);
UIAeroScheme GetUIAeroScheme(void);
```

`UIAeroScheme` exposes the derived Aero colors for the active palette (glass
fill, gradient top/bottom, border, highlight, hover glow, focus ring, inset
field colors) mirroring `UIMaterialScheme`.

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

### Sync

Ksync Sync is Kryon's common sync protocol layer. Kryon owns URL handling, token auth,
challenge/login, bearer requests, sync posting, account deletion, and small JSON
helpers, and default platform transport. Applications still own their local data
model and provide callbacks to build sync payloads, apply sync responses, and
store auth tokens.

#### `KsyncSyncResult`

```c
typedef enum KsyncSyncResult {
    KSYNC_SYNC_OK = 0,
    KSYNC_SYNC_INVALID_URL,
    KSYNC_SYNC_NO_ACCOUNT,
    KSYNC_SYNC_PAYLOAD_FAILED,
    KSYNC_SYNC_CHALLENGE_FAILED,
    KSYNC_SYNC_SIGN_FAILED,
    KSYNC_SYNC_REQUEST_FAILED,
    KSYNC_SYNC_AUTH_FAILED
} KsyncSyncResult;
```

#### `KsyncSyncConfig`

```c
typedef struct KsyncSyncConfig {
    const char *base_url;
    const KsyncAccount *account;
    const char *client_id;
    KsyncSyncHttpRequestFn http_request;
    KsyncSyncGetTextFn get_text;
    KsyncSyncSetTextFn set_text;
    KsyncSyncBuildPayloadFn build_payload;
    KsyncSyncFreePayloadFn free_payload;
    KsyncSyncApplyResponseFn apply_response;
    KsyncSyncVoidFn purge_synced_deleted;
    KsyncSyncLogFn log_http_failure;
    void *user;
} KsyncSyncConfig;
```

`http_request` can be app-provided, or set to `KsyncDefaultHttpRequest` for
Kryon's built-in libcurl/JNI/fetch transport.
`get_text` and `set_text` store `sync_auth_token` and
`sync_auth_token_expires_at`.

#### URL Helpers

```c
int IsKsyncSyncURLValid(const char *url);
int NormalizeKsyncSyncURL(const char *input, char *out, size_t out_size);
int JoinKsyncSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinKsyncSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
```

Remote sync URLs must be HTTPS. HTTP is accepted only for loopback hosts such as
`localhost`, `127.0.0.1`, and Android emulator host `10.0.2.2`.

#### Buffer And JSON Helpers

```c
int AppendKsyncSyncBuffer(KsyncSyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendKsyncSyncBufferJSONString(KsyncSyncBuffer *buffer,
                                              const char *text);
void FreeKsyncSyncBuffer(KsyncSyncBuffer *buffer);
int FindKsyncSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindKsyncSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
```

These are intentionally small helpers for Ksync protocol payload construction and
simple response fields. Applications that already have a full JSON parser should
keep using it for domain data.

#### Auth And Sync

```c
void ClearKsyncSyncAuthToken(const KsyncSyncConfig *cfg);
KsyncSyncResult LoginKsyncSync(const KsyncSyncConfig *cfg);
KsyncSyncResult RunKsyncSync(const KsyncSyncConfig *cfg);
KsyncSyncResult RequestKsyncSyncBearer(const KsyncSyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
KsyncSyncResult DeleteKsyncSyncAccount(const KsyncSyncConfig *cfg);
const char *GetKsyncSyncResultName(KsyncSyncResult result);
```

`RunKsyncSync` loads or refreshes an auth token, asks the app callback for
a local-first payload, posts it to `/api/v1/sync`, applies the response through
the callback, and purges synced tombstones on success. `RequestKsyncSyncBearer`
is for app-specific Ksync endpoints that use the same account token.

#### Default Transport And Events

```c
int KsyncDefaultHttpRequest(const char *method, const char *url,
                            const char *body,
                            const char *const *headers,
                            int header_count,
                            KsyncSyncBuffer *response,
                            long *status, void *user);
KsyncSyncResult KsyncRemoteEventWait(const KsyncSyncConfig *cfg,
                                     const char *path);
#if defined(__EMSCRIPTEN__)
int KsyncWebSyncStart(const KsyncSyncConfig *cfg);
int KsyncWebSyncPoll(KsyncSyncResult *result, int *changed);
int KsyncWebRemoteEventsStart(const KsyncSyncConfig *cfg, const char *path);
int KsyncWebRemoteEventsPoll(void);
#endif
```

`KsyncDefaultHttpRequest` provides the common platform HTTP transport. Native
builds use libcurl, Android builds call `syncHttpRequest`/`syncWebSocketWait` on
the activity through JNI, and web builds use JavaScript `fetch`.

`KsyncRemoteEventWait` waits for one Ksync WebSocket sync-change event using the
stored bearer token. Web builds use the nonblocking `KsyncWebRemoteEventsStart`
and `KsyncWebRemoteEventsPoll` pair instead.

`KsyncWebSyncStart` and `KsyncWebSyncPoll` run the same login/token/sync flow as
`RunKsyncSync` without blocking the browser frame loop.

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

Poll the download to copy the worker-owned status snapshot into
`RuntimeAssetDownload`. Release it with `FreeRuntimeAssetDownload`; native
builds wait for an active worker before freeing its state.

```c
RuntimeAssetStatus PollRuntimeAssetDownload(RuntimeAssetDownload *download);
void FreeRuntimeAssetDownload(RuntimeAssetDownload *download);
```

#### `SetRuntimeAssetDownloadBackend`

Set custom download backend.

```c
void SetRuntimeAssetDownloadBackend(RuntimeAssetDownloadBackend backend);
```

---

### Desktop App Integration

Desktop helpers centralize app identity, XDG paths, single-instance locks, and
file/URL open events for native desktop apps. Public names follow the normal
Kryon app-facing style, without a `Kry` prefix.

```c
typedef struct DesktopAppInfo {
    const char *app_id;
    const char *name;
    const char *display_name;
    const char *summary;
    const char *icon_name;
    const char *wm_class;
    int single_instance;
} DesktopAppInfo;

void InitDesktopApp(const DesktopAppInfo *info);
const DesktopAppInfo *GetDesktopAppInfo(void);
const char *GetDesktopAppID(void);
const char *GetDesktopDisplayName(void);
int GetDesktopConfigDir(char *out, int cap);
int GetDesktopDataDir(char *out, int cap);
int GetDesktopCacheDir(char *out, int cap);
int AcquireDesktopSingleInstance(const char *app_id, char *lock_path, int cap);
void ReleaseDesktopSingleInstance(void);
int QueueDesktopOpenPath(const char *path_or_url);
DesktopOpenEventKind PollDesktopOpenEvent(char *out, int cap);
```

`InitDesktopApp` also registers the desktop-entry id with the notification
backend so notification icons resolve through the installed desktop metadata.

---

### File Dialogs

Open native desktop file dialogs through the best available Linux backend. The
default backend order is XDG Desktop Portal, GTK, `zenity`, `kdialog`, then
`yad`. `KRYON_FILE_DIALOG_BACKEND` can force `portal`, `gtk`, `zenity`,
`kdialog`, `yad`, `auto`, or `none` for debugging and packaging checks. Explicit
forced backends fail closed when the requested backend is not available.

#### `GetFileDialogBackendName`

Return a stable backend name for logs, diagnostics, and examples.

```c
const char *GetFileDialogBackendName(void);
```

#### `LoadFileDialog` / `SaveFileDialog` / `SelectFileDialogFolder`

Open file, save file, or folder selection dialogs. They return `1` when the user
selects a path and `0` when the user cancels or no backend is available.

```c
int LoadFileDialog(FileDialog *dlg, const char *title);
int LoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter);
int SaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename);
int SelectFileDialogFolder(FileDialog *dlg, const char *title);
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

### Pictures

```c
typedef enum UIPictureFit {
    UI_PICTURE_FIT_STRETCH,
    UI_PICTURE_FIT_CONTAIN,
    UI_PICTURE_FIT_COVER
} UIPictureFit;

typedef struct PictureProps {
    const char *asset_path;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    Color tint;
    UIPictureFit fit;
} PictureProps;

void Picture(PictureProps picture);
```

Pictures are image-backed UI widget nodes. `asset_path` is resolved first as a
runtime file path and then as an embedded asset path. `Picture` uses the
full image with contain fitting and exposes source rect, origin,
rotation, tint, and fit mode. The `Sprite2D` scene node shares the same
texture cache (`KryLoadPictureTexture`) for world-space game sprites. Named
`Picture` rather than `Image` because raylib already owns `Image` as a
decoded-image-in-memory struct type.

### Buttons

#### `Button`

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
} UIButtonSpec;
```

#### `Button`

Draw and handle a button.

```c
int Button(UIButtonSpec button);
```

**Returns:** 1 if clicked, 0 otherwise

#### `IconButton`

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
} IconButton;
```

#### `UIIconButtonNode`

```c
int UIIconButtonNode(IconButton button);
```

#### `Href`

```c
typedef struct {
    Rectangle bounds;
    const char *text;
    const char *href;
    int font;
    int focus_id;
    int disabled;
    Color color;
    Color hover_color;
} Href;
```

#### `UIHrefNode`

Draw and handle a text link using the current theme link color by default.

```c
int UIHrefNode(Href link);
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

#### `TextField`

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
} TextField;
```

#### `UITextInputControlNode` / `TextField`

```c
int UITextInputControlNode(UITextInput input);
int TextField(TextField field);
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
} BottomNav;

UIBottomNavResult UIBottomNavNode(BottomNav nav);
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
} Toolbar;

UIToolbarResult UIToolbarNode(Toolbar toolbar);
UIToolbarHeaderResult UIToolbarHeaderNode(ToolbarHeader header);
```

#### Sidebar Account Header

```c
UISidebarAccountHeaderResult UISidebarAccountHeaderNode(SidebarAccountHeader header);
UIProfilePicturePickerResult UIProfilePicturePickerNode(UIProfilePicturePickerModal modal);
```

`UISidebarAccountHeaderNode` draws the standard account top area with banner,
username, subtitle, friends summary, and pfp. `content_padding_x` overrides
the internal horizontal inset while the header bounds can fill its parent. It
returns separate click flags
for pfp, username, and friends so applications keep ownership of route changes
and persistence.

`UIProfilePicturePickerNode` draws the shared pfp selection modal over the
standard built-in pfp icon set and writes the selected `UIIconType` when the
user chooses one.

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
} TabBar;

int TabBar(TabBar bar);
```

#### Dropdown

```c
int Dropdown(int id, int x, int y, int w, int h,
                            const char **options, int option_count, int *selected_index);
void DrawUIOverlays(void);
```

---

### Modals

#### `UIActionModalNode`

Adaptive action modal for a title, message, optional close icon, and one to
three action buttons.

```c
typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} UIModalAction;

typedef struct {
    const char *title;
    const char *message;
    const UIModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} ModalProps;

int UIActionModalNode(ModalProps modal);
```

**Returns:** `-1` when the close icon is clicked, `0` for no action, or the
1-based action index.

The modal width is capped to the viewport and `max_width`, body text reflows to
the content width, and action buttons measure their labels. Button text is fitted
inside the button, and the action row wraps to multiple rows when labels do not
fit. Backdrop clicks are blocked automatically for the current frame and the next
frame.

#### `UIModalNode`

Simple two-button modal.

```c
int UIModalNode(const char *title, const char *message,
                  const char *cancel_btn, const char *confirm_btn);
```

**Returns:** 1 for cancel, 2 for confirm

Uses the same adaptive modal behavior as `UIActionModalNode`: adaptive width,
reflowed message text, fitted button labels, wrapped actions when needed, and
automatic backdrop capture for the current frame and the next frame.

#### `UIModal3ButtonNode`

Three-button modal.

```c
int UIModal3ButtonNode(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn, const char *right_btn);
```

Uses the same adaptive modal behavior as `UIActionModalNode`: adaptive width,
reflowed message text, fitted button labels, wrapped actions when needed, and
automatic backdrop capture for the current frame and the next frame.

#### `UIPanelFrame` / `UIModalFrameNode`

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

UIPanelFrame UIModalFrameNode(int width, int height, const char *title,
                                     Texture2D left_icon, Texture2D right_icon);
```

`UIModalFrameNode` also updates the modal capture bounds automatically for the
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

#### Node Measurement

```c
int UIGetNodeHeight(const UIWidgetNode *node);
int UIGetNodeHeightById(int id);
```

---

### Controls

#### Sliders

```c
int UISliderNode(int id, int x, int y, int w, const char *label,
                   int min, int max, int *value, const char *suffix,
                   const char *value_text_override);
int UIVerticalSliderNode(int id, int x, int y, int h,
                            int min, int max, int *value);
```

#### Toggle Switch

```c
int Toggle(int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
```

#### Checkbox

```c
int Checkbox(int x, int y, const char *label, int *value);
int Checkbox(int x, int y, const char *label,
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
} InfoRows;

void UIInfoRowsNode(InfoRows rows);
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
} ButtonRow;

int UIButtonRowNode(ButtonRow row);
```

`UIButtonRowNode` measures labels, stores the final height on its node, fits
text inside each button, and wraps into additional rows when the configured
width cannot hold every action on one line.

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

Built-in modal helpers (`UIActionModalNode`, `UIModalNode`,
`UIModal3ButtonNode`, `UIModalFrameNode`) register their bounds automatically.

Applications should use `UIActionModalNode` for standard title/message/action
dialogs and `UIModalFrameNode` for custom modal content instead of manually
drawing a backdrop and calling `SetUIModalCapture`. Manual capture remains
available for specialized overlays, but the helpers keep modal bounds, backdrop,
and input capture consistent across projects.

### Input Blocking

```c
void ui_set_input_blocked(int blocked);
```

### Hover Effects

```c
int UIHoverEffectsEnabled(void);
void SetUITransitionCuesEnabled(int enabled);
int UITransitionCuesEnabled(void);
```

`SetUITransitionCuesEnabled` controls the extra subtle hover and selected-state cues
used by built-in controls. Leave it disabled when an application has transitions
turned off.

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

Normal UI code does not need to call these. `BeginUIFrame` and `SetUIFrame`
manage the focus pass automatically. Use these only for custom frame lifecycles
that do not go through Kryon's normal frame entry points.

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
void UIFocusNode(Rectangle bounds);
```

---

## Memory Diagnostics

```c
int KryonMemDebugEnabled(void);
void KryonMemReport(const char *tag);
```

Both are no-ops unless `KRYON_MEM_DEBUG` is set in the environment, so apps
can call them at interesting points unconditionally. `KryonMemReport` prints
the process RSS/high-water marks (Linux) and the glibc allocator arena
breakdown to stderr. `UIFontMemoryReport` (Text section) reports per-font
rasterization stats under the same switch.

## Utility Functions

### Icon Buttons

```c
int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);
int UIIconBtnNode(int id, int x, int y, UIIconSize size, Texture2D icon, int *hover);
int UIPaddedIconBtnNode(int id, int x, int y, int size, int padding, Texture2D icon, int *hover);
```

### Generic Button

```c
int UIGenericButtonNode(int x, int y, int w, int h, const char *label,
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

## Pragmatic Tk Toolkit

`ui_tk.h` adds Kryon's Tk-replacement layer. The rule is one simple way to use
each widget: prepare a plain struct, keep state in caller variables, and call the
matching immediate-mode function each frame.

```c
UIFrame frame = BeginUIFrameBox((Rectangle){40, 40, 320, 200}, 12, 12, 8);
Rectangle row = UIFramePack(&frame, UI_SIDE_TOP, 32);

int selected = 0;
UIListBoxNode((ListBox){
    .bounds = row,
    .id = 10,
    .items = items,
    .item_count = item_count,
    .selected_index = &selected,
    .row_height = 30
});
```

Collection widgets use `scroll_offset` as a caller-owned pixel offset. Canvas
uses the same one-call shape: draw between `BeginUICanvas` and `EndUICanvas`;
scroll and zoom in the `UICanvas` struct are applied to canvas drawing and hit
coordinates.

Text fields and text areas use the shared `EditUIText` core. Ctrl/Cmd+C copies
the field buffer, Ctrl/Cmd+X cuts it, and Ctrl/Cmd+V pastes clipboard text
through the existing codepoint filter.

Feature families:

- Geometry: `BeginUIFrameBox`, `UIFramePack`, `UIGridCell`, `UIPlace`, `UISeparatorNode`
- Menus: `UIMenuBarNode`, `UIPopupMenuNode`
- Basic controls: `Radio`, `Progress`, `Spinbox`, `Combobox`, `UILabelFrameNode`, `UIImageBoxNode`
- Collections: `UIListBoxNode`, `UITreeViewNode`, `UITableViewNode`
- Canvas: `BeginUICanvas`, `EndUICanvas`, `UICanvasGridNode`, `UICanvasHitTest`
- Containers: `UINotebookNode`, `PanedView`, `Collapsible`
- Dialogs/platform: `UIMessageDialogNode`, `UIConfirmDialogNode`, `UIPromptDialogNode`, `UIColorPickerNode`, `DispatchUIAccelerators`, clipboard helpers
- Accessibility/debug: `UIFocusDebugOverlayNode`

Examples `09_geometry` through `18_accessibility` demonstrate these APIs.

---

## Integration Example

```c
#include "kryon.h"

int main(void) {
    // Initialize window with Raylib
    InitWindow(320, 560, "Kryon Demo");
    SetTargetFPS(60);

    // Configure UI theme
    SetCurrentTheme(THEME_SKY, 0);
    float dpi = 1.0f;  // Get from platform

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), dpi);

        // Draw UI
        if (UIGenericButtonNode(10, 10, 100, 36, "Click Me",
                                   UI_BUTTON_STYLE_PRIMARY, 0, NULL)) {
            // Button clicked
        }

        EndUIFrame();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```
