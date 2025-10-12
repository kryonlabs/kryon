## SDL2 FFI Bindings
##
## This module provides Nim FFI bindings to the SDL2 C library.
## Using SDL2 for cross-platform window management, rendering, and input.

{.passL: "-lSDL2".}

# ============================================================================
# Core Types
# ============================================================================

type
  SDL_Window* {.importc: "SDL_Window", header: "SDL_video.h", bycopy.} = object

  SDL_Renderer* {.importc: "SDL_Renderer", header: "SDL_render.h", bycopy.} = object

  SDL_Surface* {.importc: "SDL_Surface", header: "SDL_surface.h", bycopy.} = object

  SDL_Texture* {.importc: "SDL_Texture", header: "SDL_render.h", bycopy.} = object

  SDL_Color* {.importc: "SDL_Color", header: "SDL_pixels.h", bycopy.} = object
    r*: uint8
    g*: uint8
    b*: uint8
    a*: uint8

  SDL_Rect* {.importc: "SDL_Rect", header: "SDL_rect.h", bycopy.} = object
    x*: cint
    y*: cint
    w*: cint
    h*: cint

  SDL_Point* {.importc: "SDL_Point", header: "SDL_rect.h", bycopy.} = object
    x*: cint
    y*: cint

  SDL_FPoint* {.importc: "SDL_FPoint", header: "SDL_rect.h", bycopy.} = object
    x*: cfloat
    y*: cfloat

# Font handling - we'll use SDL_ttf for text rendering
type
  TTF_Font* {.importc: "TTF_Font", header: "SDL_ttf.h", bycopy.} = object

# ============================================================================
# Color Constants (matching Raylib)
# ============================================================================

# Helper to create colors
proc rcolor*(r, g, b, a: uint8): SDL_Color {.inline.} =
  SDL_Color(r: r, g: g, b: b, a: a)

# ============================================================================
# SDL2 Initialization and Cleanup
# ============================================================================

proc SDL_Init*(flags: uint32): cint {.importc: "SDL_Init", header: "SDL.h".}
proc SDL_Quit*() {.importc: "SDL_Quit", header: "SDL.h".}

# SDL initialization flags
const
  SDL_INIT_VIDEO* = 0x00000020'u32

# SDL_ttf initialization
proc TTF_Init*(): cint {.importc: "TTF_Init", header: "SDL_ttf.h".}
proc TTF_Quit*() {.importc: "TTF_Quit", header: "SDL_ttf.h".}

# ============================================================================
# Window Management
# ============================================================================

proc SDL_CreateWindow*(title: cstring; x, y, w, h: cint; flags: uint32): ptr SDL_Window {.importc: "SDL_CreateWindow", header: "SDL_video.h".}
proc SDL_DestroyWindow*(window: ptr SDL_Window) {.importc: "SDL_DestroyWindow", header: "SDL_video.h".}

# Window flags
const
  SDL_WINDOW_SHOWN* = 0x00000004'u32
  SDL_WINDOWPOS_UNDEFINED* = 0x1FFF0000'i32

# ============================================================================
# Renderer Management
# ============================================================================

proc SDL_CreateRenderer*(window: ptr SDL_Window; index: cint; flags: uint32): ptr SDL_Renderer {.importc: "SDL_CreateRenderer", header: "SDL_render.h".}
proc SDL_DestroyRenderer*(renderer: ptr SDL_Renderer) {.importc: "SDL_DestroyRenderer", header: "SDL_render.h".}

# Renderer flags
const
  SDL_RENDERER_ACCELERATED* = 0x00000002'u32
  SDL_RENDERER_PRESENTVSYNC* = 0x00000004'u32

# ============================================================================
# Drawing and Rendering
# ============================================================================

proc SDL_SetRenderDrawColor*(renderer: ptr SDL_Renderer; r, g, b, a: uint8): cint {.importc: "SDL_SetRenderDrawColor", header: "SDL_render.h".}
proc SDL_RenderClear*(renderer: ptr SDL_Renderer): cint {.importc: "SDL_RenderClear", header: "SDL_render.h".}
proc SDL_RenderPresent*(renderer: ptr SDL_Renderer) {.importc: "SDL_RenderPresent", header: "SDL_render.h".}
proc SDL_RenderFillRect*(renderer: ptr SDL_Renderer; rect: ptr SDL_Rect): cint {.importc: "SDL_RenderFillRect", header: "SDL_render.h".}
proc SDL_RenderDrawRect*(renderer: ptr SDL_Renderer; rect: ptr SDL_Rect): cint {.importc: "SDL_RenderDrawRect", header: "SDL_render.h".}

# ============================================================================
# Event Handling
# ============================================================================

type
  SDL_EventType* = uint32

  SDL_KeyboardEvent* {.importc: "SDL_KeyboardEvent", header: "SDL_events.h", bycopy.} = object
    eventType*: SDL_EventType
    timestamp*: uint32
    windowID*: uint32
    state*: uint8
    repeat*: uint8
    padding2*: uint8
    padding3*: uint8
    keysym*: SDL_Keysym

  SDL_MouseButtonEvent* {.importc: "SDL_MouseButtonEvent", header: "SDL_events.h", bycopy.} = object
    eventType*: SDL_EventType
    timestamp*: uint32
    windowID*: uint32
    which*: uint32
    button*: uint8
    state*: uint8
    clicks*: uint8
    padding1*: uint8
    x*: cint
    y*: cint

  SDL_MouseMotionEvent* {.importc: "SDL_MouseMotionEvent", header: "SDL_events.h", bycopy.} = object
    eventType*: SDL_EventType
    timestamp*: uint32
    windowID*: uint32
    which*: uint32
    state*: uint32
    x*: cint
    y*: cint
    xrel*: cint
    yrel*: cint

  SDL_TextInputEvent* {.importc: "SDL_TextInputEvent", header: "SDL_events.h", bycopy.} = object
    eventType*: SDL_EventType
    timestamp*: uint32
    windowID*: uint32
    text*: array[32, char]

  SDL_Keysym* {.importc: "SDL_Keysym", header: "SDL_scancode.h", bycopy.} = object
    scancode*: cint
    sym*: cint
    modifiers*: uint16
    unused*: uint32

  SDL_Event* {.importc: "SDL_Event", header: "SDL_events.h", bycopy.} = object
    data*: array[56, byte]

# Event types
const
  SDL_EVENT_QUIT* = 0x100'u32
  SDL_EVENT_KEYDOWN* = 0x300'u32
  SDL_EVENT_KEYUP* = 0x301'u32
  SDL_EVENT_TEXTINPUT* = 0x303'u32
  SDL_EVENT_MOUSEBUTTONDOWN* = 0x401'u32
  SDL_EVENT_MOUSEBUTTONUP* = 0x402'u32
  SDL_EVENT_MOUSEMOTION* = 0x400'u32

# Keyboard state
const
  SDL_PRESSED* = 1'u8
  SDL_RELEASED* = 0'u8

# Key codes (matching Raylib's key codes)
proc SDL_GetScancodeFromKey*(key: cint): cint {.importc: "SDL_GetScancodeFromKey", header: "SDL_scancode.h".}

# Scancodes
const
  SDL_SCANCODE_BACKSPACE* = 42
  SDL_SCANCODE_TAB* = 43
  SDL_SCANCODE_RETURN* = 40
  SDL_SCANCODE_ESCAPE* = 41
  SDL_SCANCODE_DELETE* = 76
  SDL_SCANCODE_LEFT* = 80
  SDL_SCANCODE_RIGHT* = 79
  SDL_SCANCODE_UP* = 82
  SDL_SCANCODE_DOWN* = 81
  SDL_SCANCODE_LCTRL* = 224
  SDL_SCANCODE_LSHIFT* = 225
  SDL_SCANCODE_LALT* = 226

# Mouse buttons
const
  SDL_BUTTON_LEFT* = 1'u8
  SDL_BUTTON_RIGHT* = 2'u8
  SDL_BUTTON_MIDDLE* = 3'u8

# Event handling functions
proc SDL_PollEvent*(event: ptr SDL_Event): cint {.importc: "SDL_PollEvent", header: "SDL_events.h".}
proc SDL_GetMouseState*(x: ptr cint; y: ptr cint): uint32 {.importc: "SDL_GetMouseState", header: "SDL_mouse.h".}

# ============================================================================
# Text Rendering with SDL_ttf
# ============================================================================

proc TTF_OpenFont*(file: cstring; ptsize: cint): ptr TTF_Font {.importc: "TTF_OpenFont", header: "SDL_ttf.h".}
proc TTF_CloseFont*(font: ptr TTF_Font) {.importc: "TTF_CloseFont", header: "SDL_ttf.h".}

proc TTF_RenderUTF8_Blended*(font: ptr TTF_Font; text: cstring; fg: SDL_Color): ptr SDL_Surface {.importc: "TTF_RenderUTF8_Blended", header: "SDL_ttf.h".}
proc TTF_SizeUTF8*(font: ptr TTF_Font; text: cstring; w: ptr cint; h: ptr cint): cint {.importc: "TTF_SizeUTF8", header: "SDL_ttf.h".}

proc SDL_CreateTextureFromSurface*(renderer: ptr SDL_Renderer; surface: ptr SDL_Surface): ptr SDL_Texture {.importc: "SDL_CreateTextureFromSurface", header: "SDL_render.h".}
proc SDL_FreeSurface*(surface: ptr SDL_Surface) {.importc: "SDL_FreeSurface", header: "SDL_surface.h".}
proc SDL_DestroyTexture*(texture: ptr SDL_Texture) {.importc: "SDL_DestroyTexture", header: "SDL_render.h".}

proc SDL_RenderCopy*(renderer: ptr SDL_Renderer; texture: ptr SDL_Texture; srcrect, dstrect: ptr SDL_Rect): cint {.importc: "SDL_RenderCopy", header: "SDL_render.h".}

# ============================================================================
# Clipping (Scissor Mode)
# ============================================================================

proc SDL_RenderSetClipRect*(renderer: ptr SDL_Renderer; rect: ptr SDL_Rect): cint {.importc: "SDL_RenderSetClipRect", header: "SDL_render.h".}
proc SDL_RenderGetClipRect*(renderer: ptr SDL_Renderer; rect: ptr SDL_Rect) {.importc: "SDL_RenderGetClipRect", header: "SDL_render.h".}

# ============================================================================
# Cursor Management
# ============================================================================

type
  SDL_SystemCursor* = cint

proc SDL_CreateSystemCursor*(id: SDL_SystemCursor): pointer {.importc: "SDL_CreateSystemCursor", header: "SDL_mouse.h".}
proc SDL_FreeCursor*(cursor: pointer) {.importc: "SDL_FreeCursor", header: "SDL_mouse.h".}
proc SDL_SetCursor*(cursor: pointer) {.importc: "SDL_SetCursor", header: "SDL_mouse.h".}

# System cursor IDs
const
  SDL_SYSTEM_CURSOR_ARROW* = 0
  SDL_SYSTEM_CURSOR_IBEAM* = 2
  SDL_SYSTEM_CURSOR_HAND* = 21

# ============================================================================
# Helpers and Conversions
# ============================================================================

proc rvec2*(x, y: float): SDL_FPoint {.inline.} =
  SDL_FPoint(x: x.cfloat, y: y.cfloat)

proc rrect*(x, y, width, height: float): SDL_Rect {.inline.} =
  SDL_Rect(x: x.cint, y: y.cint, w: width.cint, h: height.cint)

proc rrecti*(x, y, width, height: cint): SDL_Rect {.inline.} =
  SDL_Rect(x: x, y: y, w: width, h: height)

# Event union helpers
proc getKeyEvent*(event: ptr SDL_Event): ptr SDL_KeyboardEvent {.inline.} =
  cast[ptr SDL_KeyboardEvent](event.addr)

proc getMouseButtonEvent*(event: ptr SDL_Event): ptr SDL_MouseButtonEvent {.inline.} =
  cast[ptr SDL_MouseButtonEvent](event.addr)

proc getMouseMotionEvent*(event: ptr SDL_Event): ptr SDL_MouseMotionEvent {.inline.} =
  cast[ptr SDL_MouseMotionEvent](event.addr)

proc getTextInputEvent*(event: ptr SDL_Event): ptr SDL_TextInputEvent {.inline.} =
  cast[ptr SDL_TextInputEvent](event.addr)

# ============================================================================
# Constants Matching Raylib Interface
# ============================================================================

# Define common colors directly (matching Raylib)
const
  RAYWHITE* = SDL_Color(r: 245, g: 245, b: 245, a: 255)
  BLACK* = SDL_Color(r: 0, g: 0, b: 0, a: 255)
  WHITE* = SDL_Color(r: 255, g: 255, b: 255, a: 255)
  DARKGRAY* = SDL_Color(r: 80, g: 80, b: 80, a: 255)
  LIGHTGRAY* = SDL_Color(r: 200, g: 200, b: 200, a: 255)
  GRAY* = SDL_Color(r: 130, g: 130, b: 130, a: 255)

# Mouse button constants (matching Raylib)
const
  MOUSE_BUTTON_LEFT* = SDL_BUTTON_LEFT
  MOUSE_BUTTON_RIGHT* = SDL_BUTTON_RIGHT
  MOUSE_BUTTON_MIDDLE* = SDL_BUTTON_MIDDLE

# Mouse cursor constants (matching Raylib)
const
  MOUSE_CURSOR_DEFAULT* = SDL_SYSTEM_CURSOR_ARROW
  MOUSE_CURSOR_IBEAM* = SDL_SYSTEM_CURSOR_IBEAM
  MOUSE_CURSOR_POINTING_HAND* = SDL_SYSTEM_CURSOR_HAND

# Key constants (matching Raylib)
const
  KEY_NULL* = 0
  KEY_BACKSPACE* = SDL_SCANCODE_BACKSPACE
  KEY_ENTER* = SDL_SCANCODE_RETURN
  KEY_TAB* = SDL_SCANCODE_TAB
  KEY_ESCAPE* = SDL_SCANCODE_ESCAPE
  KEY_DELETE* = SDL_SCANCODE_DELETE
  KEY_LEFT* = SDL_SCANCODE_LEFT
  KEY_RIGHT* = SDL_SCANCODE_RIGHT
  KEY_UP* = SDL_SCANCODE_UP
  KEY_DOWN* = SDL_SCANCODE_DOWN
  KEY_LEFT_CONTROL* = SDL_SCANCODE_LCTRL
  KEY_LEFT_SHIFT* = SDL_SCANCODE_LSHIFT
  KEY_LEFT_ALT* = SDL_SCANCODE_LALT