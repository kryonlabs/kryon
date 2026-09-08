# UI API Documentation

Kryon is a lightweight C UI component library for embedded applications and runtime environments. It provides core UI primitives and icon asset management without external dependencies.

## Carousel navigation

`CarouselControls(CarouselControlsProps)` draws 48-unit side-arrow targets and
tappable pagination dots. It returns the selected index, wrapping at either end.
Pass `move` from the existing swipe recognizer or keyboard navigation. The app
owns the images, selection persistence, and routes. Zero items return -1; one
item hides navigation. `disabled` blocks both taps and movement. `id` reserves
`count + 2` consecutive focus IDs. The widget is available in native C and Go.

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

### Disabled content blocks in `.kry`

Use a lexical block to disable ordinary child controls without manually pairing
runtime calls:

```kry
Disabled saving: {
    when = busy
    Button {
        label = "Save"
        bounds = {20, 20, 100, 32}
    }
}
```

The name is optional. `when` is a boolean expression, evaluated once on entry;
omitting it means `true`. Nested blocks inherit an outer disabled state even
when their own condition is false. Only `when` is accepted as a scope property.
The shared compiler cleanup pass restores the previous state at the closing
brace and on `return`, `break`, or `continue` leaving the block. This is authoring
syntax over the existing `BeginDisabled`/`EndDisabled` runtime operations, not a
new public widget or props type. As with explicit `defer`, functions containing
this scope currently reject raw C/preprocessor regions, `goto`/labels, and
`guard` exits; use structured control flow and explicit `if`/`return` instead.

### Scrolling blocks in native `.kry` code

```kry
Scroll content: {
    bounds = {20, 20, 240, 160}
    content_height = 600
    scroll_offset = &offset
    Button {
        bounds = {content.x, content.y, 200, 32}
        label = "Scrollable action"
    }
}
```

`bounds` is required and accepts a rectangle expression or `{x,y,w,h}`.
`content_height` defaults to zero; `scroll_offset` defaults to `nil` and, when
provided, points to caller-owned integer state. A block name optionally binds
the content rectangle returned by `BeginScroll`, visible only inside that block.
Child widgets use this rectangle for scrolled positioning; the scope does not
silently transform explicit coordinates. Nested blocks intersect clips.

The compiler pairs `BeginScroll` and `EndScroll` through lexical cleanup,
including return, break and continue. The same structured-control-flow
restrictions as `Disabled` apply. Generated C and native Go parity tests exercise
nested input/clipping, wheel scrolling, scrollbar dragging and parent restoration;
C++ syntax tests cover the shared lowering. This native scope is not yet
behaviorally verified in the JS runner.

### Composed combo blocks in native `.kry` code

Use a `Combo` block when the popup contains caller-defined widgets:

```kry
Combo commands: {
    bounds = {20, 20, 180, 32}
    popup_size = (Vector2){260, 160}
    preview = "Commands"
    id = 4200
    open = &commands_open
    flags = ComboPopupAlignLeft | ComboHeightSmall

    Row actions: {
        Button { label = "Run" }
        TextField { text = query }
    }
}
```

The block conditionally submits its children only while open. The compiler
emits `BeginCombo` and a matching `EndCombo` through the shared lexical cleanup
pass, including `return`, `break`, and `continue`, so application source cannot
forget the closing call. Call `CloseCombo()` inside the block for explicit
dismissal. The properties are the fields of `ComboProps`; generated C, C++ and
native Go target the same runtime scope.

Use a `Popup` block for arbitrary caller-owned popup contents without a combo
owner:

```kry
Popup tools: {
    bounds = {170, 50, 240, 140}
    id = 4201
    open = &tools_open

    Column content: {
        Text { text = "Tools" }
        Button { label = "Apply" }
    }
}
```

`Popup` has the same conditional, cleanup-managed block behavior as `Combo`.
Call `ClosePopup()` inside the block for explicit dismissal. Its properties are
the fields of `PopupProps`.

### Runtime surface

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

Use canonical widget names when declaring controls:

```c
IconButton(button);
TextField(field);
Slider(id, x, y, w, "Volume", 0, 100, &volume, "%", NULL);
Overlays();
```

Normal application UI is declared in `#ui` functions. The root is an explicit
`Screen` node and containers own lexical blocks. The compiler lowers named
blocks to retained runtime nodes with stable path keys; reconciliation, layout,
input routing, updates, painting, and overlays remain runtime-owned:

```kry
Settings :: (viewport: Rectangle) #ui {
    Screen root: {
        bounds = viewport

        Column body: {
            bounds = {20, 20, 360, 420}
            gap = 12
            padding = 16

            Text((TextProps){
                .text = "Account"
                .font = Text24
            })
            TextField(account_field)
            Button(save_button)
        }
    }
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

### Page

Kryon page helpers layer browser-facing semantics over the normal UI runtime.
On the DOM backend they set document metadata, route state, and semantic DOM
annotations; on other backends they render through the same Kryon UI widgets
and ignore web-only metadata.

```c
SetPageTitle("Kryon");
SetPageDescription("Native-feeling apps across desktop and web.");
ReplaceRoute("/docs#getting-started");
int route_version = GetRouteVersion();

Page((PageProps){
    .bounds = {0, 0, GetUIViewWidth(), GetUIViewHeight()},
    .title = "Kryon",
    .description = "Native-feeling apps across desktop and web.",
    .gap = Scale(16),
    .padding = GetUIPageSidePadding(),
    .key = Key("home")
});
Heading((HeadingProps){{40, 40, 520, 40}, "Kryon", 1, Text32, GetThemeText(), 0});
Link((LinkProps){{40, 96, 160, 28}, "Docs", "/docs", Text16, 101, 0, GetThemeLink(), {0}});
End();
```

Use `Page`, `Section`, `Heading`, `ParagraphText`, `Link`, `PagePicture`,
`Flow`, and `PageGrid` for Kryon-authored website surfaces. `PagePicture` is
named separately because `Image` is already the decoded-image type in the
raylib compatibility surface; `PageGrid` avoids the existing `Grid` type name.
The Go runtime mirrors these helpers and records semantic `FrameOp` metadata,
so `.kry` files lowered through `k2go` can use the same page API.
`GetRouteVersion()` increments when the browser route changes through
`PushRoute`, `ReplaceRoute`, `popstate`, or `hashchange`; non-DOM backends
return `0`.

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

#### `Scale`

Scale a pixel value by the DPI factor.

```c
int Scale(int px);
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
int RegisterUIFontSourceForText(const char *name, const char *file_type, const unsigned char *font_data, unsigned int font_size, const char *text);
int RegisterUIFontFileSourceForText(const char *name, const char *path, const char *text);
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
Use `RegisterUIFontSourceForText` or `RegisterUIFontFileSourceForText` when
the font should include Kryon's standard UI coverage plus the unique
codepoints found in a UTF-8 corpus, such as localized strings.

#### Text Measurement

```c
int TextWidth(const char *text, int font_size);
int TextHeight(const char *text, int font_size);
int TextLineHeight(int font_size);
```

#### Text Widgets

```c
typedef enum { TextWrapAuto, TextWrapNone } TextWrap;
typedef enum { TextAlignStart, TextAlignCenter, TextAlignEnd } TextAlign;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    Color color;
    TextWrap wrap;
    TextAlign align;
    TextAlign vertical_align;
    int disabled;
} TextProps;

void Text(TextProps text);
```

`Text` is the single read-only text widget. A positive bounds width enables
word wrapping by default, and a positive height clips the result. Set
`wrap = TextWrapNone` for one line; `align` and `vertical_align` control its
placement inside the bounds.
Zero width measures the line intrinsically. Zero font and transparent color
select the current UI defaults. Color and disabled presentation are properties,
not separate widget entry points.

Native `.kry` can use the same properties without a compound literal:

```kry
Text message: {
    bounds = {24, 24, 240, 64}
    text = "This wraps and clips inside its bounds."
    font = Text16
    color = GetThemeText()
}
```

#### Vertical Centering

```c
int TextBaselineY(const char *text, int box_y, int box_h, int font_size);
```

---

### Text Layout

Layout text with embedded icons and line breaks.

#### `TextLayout`

```c
typedef struct TextLayout {
    TextElement *elements;
    int element_count;
    int *line_breaks;
    int line_count;
    int *line_widths;
    int total_height;
    int line_height;
    int last_reflow_width;
} TextLayout;
```

#### `ParseTextLayout`

Parse text input into a layout.

```c
TextLayout ParseTextLayout(const char *input, Texture2D icon, UIIconType icon_type, int icon_size);
```

#### `ReflowTextLayout`

Reflow layout for a given width.

```c
void ReflowTextLayout(TextLayout *layout, int max_width, int font_size, int line_height);
```

#### `GetTextLayoutHeight` / `FreeTextLayout`

```c
int GetTextLayoutHeight(TextLayout *layout);
void FreeTextLayout(TextLayout *layout);
```

---

### Icons

#### `GetUIIconAsset`

Get icon asset by type or name.

```c
const UIIconAsset *GetUIIconAsset(UIIconType type);
const UIIconAsset *GetUIIconAssetByName(const char *name);
```

#### `LoadIconSheet` / `UnloadIconSheets`

Load a generated spritesheet or unload all icon textures. `DrawIcon` selects
and loads the correct sheet lazily, so normal widget code does not need to call
these functions directly.

```c
Texture2D LoadIconSheet(UIIconSheet sheet);
void UnloadIconSheets(void);
```

#### `DrawIcon` / `DrawIconByName`

Draw an indexed atlas glyph. UI icons accept the runtime tint; PFP, platform,
payment, language, and tile icons retain their full source colors and only use the
tint's alpha. Every entry uses one fixed 64×64 source cell.

```c
void DrawIcon(UIIconType type, Rectangle bounds, Color tint);
void DrawIconByName(const char *name, Rectangle bounds, Color tint);
```

Kryon's checked-in `icons/` directory is the finished spritesheet package.
Regular UI entries—including workbook controls—use the rounded MingCute Core
family and are packed into the monochrome `icons/ui.png`. The manifest
records each upstream SVG mapping and revision. There are no prebuilt flat-color
variants; runtime drawing can tint the clean alpha artwork on demand.
Product and project marks retain their brand colors, stay out of the generic UI
atlas, and are packed into the separate `icons/logos.png` sheet.
Entries use names like `platforms_freebsd.png`, `proj_kryon.png`, and matching
`UI_ICON_TYPE_PLATFORMS_*` / `UI_ICON_TYPE_PROJ_*` enum values. Downstream
websites can sync shared assets from a vendored Kryon copy with
`vendor/kryon/scripts/sync-icons.sh`. Embedded C assets are refreshed with
`make icons-embed`.

Profile-picture, platform, payment, and language artwork is packed into the
separate full-color `icons/pfp.png`, `icons/platforms.png`,
`icons/payments.png`, `icons/language.png`, and `icons/tiles.png` sheets. They remain in the same
indexed icon catalog with their existing `UI_ICON_TYPE_*` values. Use
`GetUIProfilePictureIconCount`,
`GetUIProfilePictureIconType`, and `GetUIProfilePictureIconName` to enumerate
the standard profile-picture options.

Kryon also exposes stable `UI_SYNC_PROFILE_ICON_*` IDs and mapping helpers:

```c
UIIconType GetUIProfilePictureIconTypeForSyncID(int sync_id);
int GetUISyncIDForProfilePictureIconType(UIIconType type);
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
    THEME_STYLE_CLASSIC,
    THEME_STYLE_DEFAULT
} ThemeStyle;

void SetThemeStyle(ThemeStyle style);
ThemeStyle GetThemeStyle(void);
ThemeStyle GetEffectiveThemeStyle(void);
ThemeStyle GetDefaultPlatformThemeStyle(void);
int GetDefaultThemeForThemeStyle(ThemeStyle style);
const char *GetThemeStyleLabel(ThemeStyle style);
```

`THEME_STYLE_SYSTEM` resolves to Default on Android builds and Classic elsewhere.
Default uses Default 3 style tokens: 48px touch targets, rounded controls,
state layers/ripple feedback, elevation shadows, and theme-derived Default color
roles. Classic keeps the original beveled Kryon look.

```c
UIStyleTokens GetUIStyleTokens(void);
UIDefaultScheme GetUIDefaultScheme(void);
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

### Sync

Sync Sync is Kryon's common sync protocol layer. Kryon owns URL handling, token auth,
challenge/login, bearer requests, sync posting, account deletion, and small JSON
helpers, and default platform transport. Applications still own their local data
model and provide callbacks to build sync payloads, apply sync responses, and
store auth tokens.

#### `SyncResult`

```c
typedef enum SyncResult {
    SYNC_OK = 0,
    SYNC_INVALID_URL,
    SYNC_NO_ACCOUNT,
    SYNC_PAYLOAD_FAILED,
    SYNC_CHALLENGE_FAILED,
    SYNC_SIGN_FAILED,
    SYNC_REQUEST_FAILED,
    SYNC_AUTH_FAILED
} SyncResult;
```

#### `SyncConfig`

```c
typedef struct SyncConfig {
    const char *base_url;
    const SyncAccount *account;
    const char *client_id;
    SyncHttpRequestFn http_request;
    SyncGetTextFn get_text;
    SyncSetTextFn set_text;
    SyncBuildPayloadFn build_payload;
    SyncFreePayloadFn free_payload;
    SyncApplyResponseFn apply_response;
    SyncVoidFn purge_synced_deleted;
    SyncLogFn log_http_failure;
    void *user;
} SyncConfig;
```

`http_request` can be app-provided, or set to `DefaultSyncHttpRequest` for
Kryon's built-in libcurl/JNI/fetch transport.
`get_text` and `set_text` store `sync_auth_token` and
`sync_auth_token_expires_at`.

#### URL Helpers

```c
int IsSyncURLValid(const char *url);
int NormalizeSyncURL(const char *input, char *out, size_t out_size);
int JoinSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
```

Remote sync URLs must be HTTPS. HTTP is accepted only for loopback hosts such as
`localhost`, `127.0.0.1`, and Android emulator host `10.0.2.2`.

#### Buffer And JSON Helpers

```c
int AppendSyncBuffer(SyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendSyncBufferJSONString(SyncBuffer *buffer,
                                              const char *text);
void FreeSyncBuffer(SyncBuffer *buffer);
int FindSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
```

These are intentionally small helpers for Sync protocol payload construction and
simple response fields. Applications that already have a full JSON parser should
keep using it for domain data.

#### Auth And Sync

```c
void ClearSyncAuthToken(const SyncConfig *cfg);
SyncResult LoginSync(const SyncConfig *cfg);
SyncResult RunSync(const SyncConfig *cfg);
SyncResult RequestSyncBearer(const SyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
SyncResult DeleteSyncAccount(const SyncConfig *cfg);
const char *GetSyncResultName(SyncResult result);
```

`RunSync` loads or refreshes an auth token, asks the app callback for
a local-first payload, posts it to `/api/v1/sync`, applies the response through
the callback, and purges synced tombstones on success. `RequestSyncBearer`
is for app-specific Sync endpoints that use the same account token.

#### Default Transport And Events

```c
int DefaultSyncHttpRequest(const char *method, const char *url,
                            const char *body,
                            const char *const *headers,
                            int header_count,
                            SyncBuffer *response,
                            long *status, void *user);
SyncResult WaitForRemoteSyncEvent(const SyncConfig *cfg,
                                     const char *path);
#if defined(__EMSCRIPTEN__)
int StartWebSync(const SyncConfig *cfg);
int PollWebSync(SyncResult *result, int *changed);
int StartWebRemoteEvents(const SyncConfig *cfg, const char *path);
int PollWebRemoteEvents(void);
#endif
```

`DefaultSyncHttpRequest` provides the common platform HTTP transport. Native
builds use libcurl, Android builds call `syncHttpRequest`/`syncWebSocketWait` on
the activity through JNI, and web builds use JavaScript `fetch`.

`WaitForRemoteSyncEvent` waits for one Sync WebSocket sync-change event using the
stored bearer token. Web builds use the nonblocking `StartWebRemoteEvents`
and `PollWebRemoteEvents` pair instead.

`StartWebSync` and `PollWebSync` run the same login/token/sync flow as
`RunSync` without blocking the browser frame loop.

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
typedef enum PictureFit {
    PICTURE_FIT_STRETCH,
    PICTURE_FIT_CONTAIN,
    PICTURE_FIT_COVER
} PictureFit;

typedef struct PictureProps {
    const char *asset_path;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    Color tint;
    PictureFit fit;
    PictureStyle style;
} PictureProps;

void Picture(PictureProps picture);
```

Pictures are image-backed UI widget nodes. `asset_path` is resolved first as a
runtime file path and then as an embedded asset path. `Picture` uses the full
image with contain fitting and exposes source rect, origin, rotation, tint, fit
mode, and optional material-style image treatment through `style`. The
`Sprite2D` scene node shares the same texture cache for world-space game
sprites. Named
`Picture` rather than `Image` because raylib already owns `Image` as a
decoded-image-in-memory struct type.

### Buttons

#### `Button`

```c
typedef struct {
    Rectangle bounds;
    const char *label;
    ButtonStyle style;
    int font;
    int id;
    int disabled;
} ButtonProps;
```

#### `Button`

Draw and handle a button.

```c
int Button(ButtonProps button);
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

#### `TextInputStyle`

```c
typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
} TextInputStyle;
```

#### `TextInput`

```c
typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    TextInputStyle style;
} TextInput;
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
    TextInputStyle style;
    TextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} TextField;
```

#### `TextField`

```c
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
} BottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const BottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} BottomNavProps;

BottomNavResult BottomNav(BottomNavProps nav);
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
} ToolbarProps;

ToolbarResult Toolbar(ToolbarProps toolbar);
ToolbarHeaderResult ToolbarHeader(ToolbarHeaderProps header);
```

#### Sidebar Account Header

```c
SidebarAccountHeaderResult SidebarAccountHeader(SidebarAccountHeaderProps header);
ProfilePicturePickerResult ProfilePicturePicker(ProfilePicturePickerProps modal);
```

`SidebarAccountHeader` draws the standard account top area with banner,
username, subtitle, friends summary, and pfp. `content_padding_x` overrides
the internal horizontal inset while the header bounds can fill its parent. It
returns separate click flags
for pfp, username, and friends so applications keep ownership of route changes
and persistence.

`ProfilePicturePicker` draws the shared pfp selection modal over the
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
} Tab;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
} TabBarProps;

int TabBar(TabBarProps bar);
int BeginTabBar(TabBarProps bar, int *selected_index);
int BeginTabItem(int index);
void EndTabItem(void);
void EndTabBar(void);
```

Use `TabBar` when only the header interaction result is needed. Use
`BeginTabBar` for arbitrary tab contents: pass the same canonical
`TabBarProps` plus caller-owned selection state, conditionally submit each
item's children when `BeginTabItem(index)` returns true, and balance successful
item and bar beginnings with their corresponding endings. A header selection
updates `selected_index` before the item checks in that frame. The scope does
not introduce a second renderer; it delegates the complete header behavior to
`TabBar`.

#### Dropdown

```c
int Dropdown(int id, int x, int y, int w, int h,
                            const char **options, int option_count, int *selected_index);
void Overlays(void);
```

In native C and Go, a focused, enabled `Dropdown`/`Combobox` with a positive ID
opens with Enter, keypad Enter, Space, or Down. The opening key does not move
the highlight or commit a selection. Focused controls display a focus indicator;
disabled controls neither open from the keyboard nor display that indicator.
An open `Dropdown`/`Combobox` supports Up/Down to move the
highlight, Home/End to jump to the first/last option, and Enter to commit and
close. Escape closes without committing the highlight. Navigation clamps to
the current option list. Both native runtimes constrain the popup vertically,
flip it above the control when needed, and scroll the highlighted row into view.
Popup width is capped to the window width and its horizontal position is shifted
inside the window; the owner button keeps its declared bounds. Painting and
input capture use the same shifted rectangle.
Go uses the shared scroll container for wheel input, scrollbar dragging and
row clipping, painting only visible rows. These behaviors do not yet provide general keyboard-focus
isolation for arbitrary popup children or complete ImGui navigation semantics.

For caller-defined contents, use the native composed combo scope:

```c
typedef enum {
    ComboFlagsNone = 0,
    ComboPopupAlignLeft = 1 << 0,
    ComboHeightSmall = 1 << 1,
    ComboHeightRegular = 1 << 2,
    ComboHeightLarge = 1 << 3,
    ComboHeightLargest = 1 << 4,
    ComboNoArrowButton = 1 << 5,
    ComboNoPreview = 1 << 6,
    ComboWidthFitPreview = 1 << 7
} ComboFlags;

typedef struct {
    Rectangle bounds;
    Vector2 popup_size;
    const char *preview;
    int id;
    bool *open;
    unsigned int flags;
    int disabled;
} ComboProps;

int BeginCombo(ComboProps combo);
void EndCombo(void);
void CloseCombo(void);
```

Call `EndCombo` exactly once when `BeginCombo` returns nonzero. Between those
calls, ordinary widgets and nested `Row` or `Column` layouts are clipped,
painted, and routed as popup contents; no popup-specific widget variants are
needed. `CloseCombo` closes the current scope immediately and updates the
caller-owned `open` value. Disabling the combo, pressing Escape, releasing the
pointer outside its popup, or omitting its owner on a later frame also closes
it. Nested combo scopes are supported.

At most one height flag may be supplied. With a zero popup height, Small,
Regular/default, Large, and Largest select approximately 4, 8, 20, and 32 owner
rows. A zero width uses the owner width. Popups are constrained to the current
view and flip above the owner when necessary. The default aligns right edges;
`ComboPopupAlignLeft` aligns left edges. The remaining flags suppress the arrow
or preview and optionally expand the owner to fit its preview.

The begin/end pair marks the lexical lifetime of arbitrary child declarations;
frame ownership and paint-target management remain internal to Kryon. Generated
C, C++ and native Go use this same clean surface.

For an arbitrary popup that is not owned by a combo, use:

```c
typedef struct {
    Rectangle bounds;
    int id;
    bool *open;
    int disabled;
    Rectangle trigger;
    unsigned int flags;
} PopupProps;

typedef enum {
    PopupFlagsNone = 0,
    PopupTooltip = 1 << 0,
    PopupModal = 1 << 1,
    PopupContext = 1 << 2
} PopupFlags;

int BeginPopup(PopupProps popup);
void EndPopup(void);
void ClosePopup(void);
```

`BeginPopup` returns nonzero only while the caller-owned `open` value is true.
Its children use the same overlay painting, clipping, nested layout and input
capture as composed combos. Escape, a pointer release outside the popup,
disabling it, or omitting its owner on a later frame closes it. Outside releases
are consumed so the background widget underneath is not activated.

With `PopupTooltip`, `open` is optional and visibility is derived from pointer
hover over `trigger`. The tooltip uses the same arbitrary-child paint and layout
scope, but does not enter popup input capture: controls beneath it continue to
receive input. Tooltip bounds and child positions are explicit, keeping sizing
and placement in the retained layout rather than creating a second text-only
renderer. `ClosePopup` may hide the tooltip for its current frame.

With `PopupModal`, the same scope accepts arbitrary native children while
drawing a full-view dimming backdrop and owning pointer and keyboard input over
the background. Pointer releases outside the panel are blocked without closing
it; Escape, `ClosePopup`, disabling it, or omitting its owner closes it. Modal
and tooltip flags are mutually exclusive.

With `PopupContext`, a right-button release inside `trigger` sets the
caller-owned `open` value and enters the same arbitrary-child popup scope.
The caller supplies the panel position in `bounds`; this keeps placement stable
after the pointer moves. Outside left-button dismissal, Escape, explicit close,
disabled state, and missing-owner cleanup use the ordinary popup lifecycle.
Context, modal, and tooltip presentation flags are mutually exclusive.

#### Segmented Control

Responsive choice control for mutually exclusive compact options. It measures
and draws through the same input, focus, text, and button primitives as other
Kryon controls, so it works across backends and can wrap onto multiple rows on
narrow screens.

```c
typedef struct {
    const char *label;
    int disabled;
} SegmentOption;

typedef struct {
    Rectangle bounds;
    int id;
    const SegmentOption *options;
    int option_count;
    int *selected_index;
    int wrap;
} SegmentedControlProps;

int GetSegmentedControlHeight(SegmentedControlProps control);
SegmentedControlResult SegmentedControl(SegmentedControlProps control);
```

#### Score Control

Responsive signed score selector for compact voting, rating, and priority
inputs. It uses Kryon button, focus, text, and wrapping layout primitives, and
stores the selected integer through the supplied value pointer.

```c
typedef struct {
    Rectangle bounds;
    int id;
    int min_value;
    int max_value;
    int *value;
    int font;
    int gap;
    int height;
    int min_item_width;
    int wrap;
} ScoreControlProps;

typedef struct {
    int value;
    int clicked;
    int clicked_value;
    int changed;
    int height;
} ScoreControlResult;

int GetScoreControlHeight(ScoreControlProps control);
ScoreControlResult ScoreControl(ScoreControlProps control);
```

---

### Modals

#### `ActionModal`

Adaptive action modal for a title, message, optional close icon, and one to
three action buttons.

```c
typedef struct {
    const char *label;
    ButtonStyle style;
    int disabled;
} ModalAction;

typedef struct {
    const char *title;
    const char *message;
    const ModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} ModalProps;

int ActionModal(ModalProps modal);
```

**Returns:** `-1` when the close icon is clicked, `0` for no action, or the
1-based action index.

The modal width is capped to the viewport and `max_width`, body text reflows to
the content width, and action buttons measure their labels. Button text is fitted
inside the button, and the action row wraps to multiple rows when labels do not
fit. Backdrop clicks are blocked automatically for the current frame and the next
frame.

#### `Modal`

Simple two-button modal.

```c
int Modal(const char *title, const char *message,
          const char *cancel_btn, const char *confirm_btn);
```

**Returns:** 1 for cancel, 2 for confirm

Uses the same adaptive modal behavior as `ActionModal`: adaptive width,
reflowed message text, fitted button labels, wrapped actions when needed, and
automatic backdrop capture for the current frame and the next frame.

#### `Modal3Button`

Three-button modal.

```c
int Modal3Button(const char *title, const char *message,
                 const char *left_btn, const char *middle_btn, const char *right_btn);
```

Uses the same adaptive modal behavior as `ActionModal`: adaptive width,
reflowed message text, fitted button labels, wrapped actions when needed, and
automatic backdrop capture for the current frame and the next frame.

#### `UIPanelFrame` / `ModalFrame`

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

UIPanelFrame ModalFrame(int width, int height, const char *title,
                        Texture2D left_icon, Texture2D right_icon);
```

`ModalFrame` also updates the modal capture bounds automatically for the current
frame and the next frame.

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
int GetNodeHeight(const UIWidgetNode *node);
int GetNodeHeightById(int id);
```

---

### Controls

#### Sliders

```c
int UISliderNode(int id, int x, int y, int w, const char *label,
                   int min, int max, int *value, const char *suffix,
                   const char *value_text_override);
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
    ButtonStyle style;
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

#### Form Cursor

```c
typedef struct {
    int x;
    int y;
    int width;
    int cursor_y;
    int gap;
    Rectangle last_bounds;
    int focused_rect_valid;
    Rectangle focused_rect;
} Form;

Form FormBegin(int x, int y, int width);
int FormY(const Form *form);
Rectangle FormTakeRect(Form *form, int height);
int FormSection(Form *form, SectionLabelProps row);
int FormTextField(Form *form, LabelTextFieldProps row);
int FormCheckbox(Form *form, CheckboxRowProps row);
int FormSpinbox(Form *form, SpinboxRowProps row);
int FormButtons(Form *form, ButtonRowProps row);
int FormEnsureFocusedVisible(Form *form, UIScrollArea area, int margin);
```

`Form` is a small immediate-mode cursor for settings and data-entry pages.
It centralizes row advancement, default row heights, and focused-field
scrolling so apps do not need parallel `draw_*` and `content_height_*`
arithmetic for simple forms.

---

## App Framework Helpers

### Route Stack And Shell Layout

```c
typedef struct KryRouteStack {
    int *routes;
    int count;
    int capacity;
    int root_route;
} KryRouteStack;

void KryRouteStackInit(KryRouteStack *stack, int *routes, int capacity,
                       int root_route);
int KryRouteStackCurrent(const KryRouteStack *stack);
int KryRouteStackPush(KryRouteStack *stack, int route);
int KryRouteStackPop(KryRouteStack *stack);
void KryRouteStackReset(KryRouteStack *stack, int root_route);

KryAppShellLayout KryAppShellMeasure(KryAppShellLayoutSpec spec);
```

These helpers cover app-neutral navigation state: a bounded route history and
a safe-area-aware shell measurement for bottom navigation plus optional
wide-screen sidebars.

### Capabilities And Settings

```c
int KryCapabilitiesHas(int capabilities, KryCapability capability);
const char *KryCapabilityName(KryCapability capability);
Rectangle KrySafeContentRect(KryViewportSpec spec);

int KryClampInt(int value, int min_value, int max_value);
int KryNormalizeIntSetting(KryIntSetting setting);
int KryNormalizeBoolSetting(KryBoolSetting setting);
```

Capabilities give `.kry` apps a shared vocabulary for platform features such
as file picking, secure storage, biometrics, notifications, wakelock, and
clipboard support. Setting helpers normalize common persisted state values
before an app applies or saves them.

---

## Input Handling

### Text composition

Platform adapters submit UTF-8 IME preedit and commit events through the shared
input front-end. C retained `TextField` and `TextArea` consume these events;
commits use the editor's insertion rules and cannot mutate a read-only buffer.
C retained `TextField` displays preedit separately from committed text; C
TextArea preedit rendering and immediate C composition remain incomplete.

```c
SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "nihon", 5, 0);
SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "日本", 2, 0);

KryTextCompositionEvent event;
while (PollTextComposition(&event)) {
    /* Custom editors may consume the same backend-neutral event stream. */
}
```

`ClearTextComposition` discards pending composition events, including commits;
it does not cancel preedit already stored in an editor. Submit a `CANCEL` event
to cancel that preedit. Android `InputConnection` and the DOM backend feed this
C API directly.

Native Go exposes the same phase names and `SubmitTextComposition`,
`PollTextComposition`, and `ClearTextComposition`, both as package functions
and Runtime methods. Its queue belongs to the runtime. Go TextField/TextArea
display preedit without changing the caller buffer, apply commits with UTF-8
cursor/length handling, and discard preedit on focus loss, removal, disabling
or popup capture. Unconsumed Go events expire at frame end. Both queues accept
up to 16 events with at most 255 text bytes per event; submission returns 1 on
success and 0 for an invalid phase or full queue. Native Go OS-window IME event
delivery and detailed preedit cursor/selection rendering remain incomplete.

Native Go `TextFieldProps.ReadOnly` and `TextAreaProps.ReadOnly` mirror C's
`read_only` property. Read-only editors remain focusable and allow selection,
navigation and copying, but reject typing, cut/paste mutations, deletion and
IME commits. Switching a Go editor to read-only cancels its preedit; its frame
operation carries `ReadOnly` so rendering suppresses the insertion caret without
removing focus styling. Re-enabling editing does not replay rejected input.

### Retained accessibility

`GetAccessibilitySnapshot` projects the committed retained UI tree into
backend-neutral roles, labels, bounds, focus, disabled, and checked state.
`SetAccessibilitySink` installs a host callback invoked after every retained
frame. The DOM backend additionally publishes the snapshot as ARIA nodes.

```c
UIAccessibilityNode nodes[64];
int count = GetAccessibilitySnapshot(nodes, 64);
```

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

Built-in modal helpers (`ActionModal`, `Modal`, `Modal3Button`, `ModalFrame`)
register their bounds automatically.

Applications should use `ActionModal` for standard title/message/action dialogs
and `ModalFrame` for custom modal content instead of manually drawing a backdrop
and calling `SetUIModalCapture`. Manual capture remains available for
specialized overlays, but the helpers keep modal bounds, backdrop, and input
capture consistent across projects.

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

### Text Drawing Helpers

```c
void DrawLeftUIControlTextInRect(const char *text, Rectangle rect, int font_size, Color color);
void DrawFittedTextInRect(const char *text, Rectangle rect, int preferred_size, int min_size, Color color);
```

---

## Button Styles

```c
typedef enum {
    ButtonStylePrimary,
    ButtonStyleSecondary,
    ButtonStyleDanger,
    ButtonStyleTab,
    ButtonStyleTabSelected
} ButtonStyle;
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
#define Text8 8
#define Text12 12
#define Text14 14
#define Text16 16
#define Text18 18
#define Text20 20
#define Text24 24
#define Text32 32
#define Text48 48
#define TextBaseSize 16
```

---

## Pragmatic Tk Toolkit

`ui_tk.h` adds Kryon's Tk-replacement layer. The rule is one simple way to use
each widget: prepare a plain struct, keep state in caller variables, and call the
matching immediate-mode function each frame.

```c
FrameBox frame = BeginFrameBox((Rectangle){40, 40, 320, 200}, 12, 12, 8);
Rectangle row = FramePack(&frame, SideTop, 32);

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
uses the same one-call shape: draw between `BeginCanvas` and `EndCanvas`;
scroll and zoom in the `Canvas` struct are applied to canvas drawing and hit
coordinates.

Text fields and text areas use the shared `EditText` core. Ctrl/Cmd+C copies
the field buffer, Ctrl/Cmd+X cuts it, and Ctrl/Cmd+V pastes clipboard text
through the existing codepoint filter.

Feature families:

- Geometry: `BeginFrameBox`, `FramePack`, `GridCell`, `Place`, `UISeparatorNode`
- Menus: `UIMenuBarNode`, `UIPopupMenuNode`
- Basic controls: `Radio`, `Progress`, `Spinbox`, `Combobox`, `UILabelFrameNode`, `UIImageBoxNode`
- Collections: `ListBox`, `TreeView`, `TableView`

`BeginListBox(ListBoxProps)` opens a framed, scrollable area for arbitrary
native children; finish it with `EndListBox()`. Its returned rectangle is the
scrolled content origin and available width, excluding the frame and scrollbar.
Set `content_height` (`ContentHeight` in Go) explicitly, or use `item_count`
times `row_height` (default 30). Place children using the returned bounds or a
nested Row/Column. The scope owns clipping, scrolling and disabled state; child
widgets own selection and editing. `items` and `selected_index` are used by the
string-list `ListBox` helper, not by this composition scope.
- Canvas: `BeginCanvas`, `EndCanvas`, `UICanvasGridNode`, `CanvasHitTest`
- Containers: `UINotebookNode`, `PanedView`, `Collapsible`

`TableViewProps.header_height` controls header height, with a minimum/default
of 30 logical pixels. `header_angle` rotates header labels in degrees, clamped
to -89 through 89; zero keeps ordinary horizontal text. Sorting and body-row
hit testing use the configured height. Nonzero angles create slanted header
cells; labels are clipped to those cells, and sorting/resizing follow their
slanted boundaries. Go uses the corresponding
`HeaderHeight` and `HeaderAngle` fields.

A nonzero table `id`/`ID` participates in focus traversal. While focused, the
standard table body moves through visible columns with the arrow keys or
Tab/Shift+Tab,
activates the current cell with Enter or F2, clears selection with Escape, and
scrolls the selected row into view. Popup ownership and disabled state suppress
those keys.

Ctrl/Cmd+C copies the selected cell, row, or column; `copy_text`/`CopyText` can
override that value for an editable cell. Ctrl/Cmd+V reports the clipboard text
and target through `pasted_text`, `pasted_row`, and `pasted_column` (the matching
capitalized fields in Go). C exposes the pasted string from runtime-owned
clipboard storage, valid until the clipboard is changed again.

For interactive cell content, set `TableViewProps.custom_cells` (`CustomCells`
in Go), draw `TableView`, then call `BeginTableCell(table, row, column)` for each
cell and finish each scope with `EndTableCell()`. The returned rectangle is the
cell's full bounds; place native child widgets using those coordinates. The
scope clips drawing and input to the visible cell, respects column order,
visibility, scrolling and frozen rows, and inherits table disabled state.
Always end the scope, including for hidden or invalid cells. The table and
children should use explicit bounds. In custom-cell mode, body selection,
activation and body keyboard handling belong to the children; header sorting
and resizing remain owned by the table. Keep row entries for geometry even
when their cell text arrays are empty.

Cell content may use `Row` or `Column` with the returned cell bounds and
zero-positioned child controls. Explicitly positioned children stay outside the
surrounding layout flow; ending the inner layout restores the outer cursor.

`CollapsibleProps.open` is a `bool*` in C and `Open *bool` in Go; `.kry`
callers should use boolean state. `Collapsible` returns whether that state
changed. Render child widgets conditionally on the open state. Set `tree` for
an unframed tree header, `depth` for 20-pixel-per-level header indentation,
`leaf` to show a non-expanding leaf marker, and `selected` for highlighting.
`disabled` prevents toggling and dims the label; `id` identifies the header.
Set optional `visible` / `Visible` state to show an ImGui-style close affordance.
When that state is false the header consumes no layout or input; closing sets it
false and returns changed without toggling `open`.
Children retain their own widget IDs and explicitly supplied bounds. Collapsing
a parent does not reset the caller's nested open state. Headers with a positive
ID participate in focus traversal: Left closes, Right opens, and Enter/Space
toggle the focused non-leaf header. In tree mode, Up/Down move through enabled
headers in the previous frame's visible order. Right on an open branch focuses
its first child; Left on a closed branch or leaf focuses its nearest ancestor.
Depth determines that hierarchy. Leaves and disabled content never expand;
disabled headers are skipped during directional traversal. Automatic child
indentation is not implemented.
- Dialogs/platform: `UIMessageDialogNode`, `UIConfirmDialogNode`, `UIPromptDialogNode`, `UIColorPickerNode`, `DispatchAccelerators`, clipboard helpers
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
        if (Button((ButtonProps){
                .bounds = {10, 10, 100, 36},
                .label = "Click Me",
                .style = ButtonStylePrimary,
                .id = 1,
        })) {
            // Button clicked
        }

        EndUIFrame();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```
