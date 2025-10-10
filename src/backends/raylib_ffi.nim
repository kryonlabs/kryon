## Raylib FFI Bindings
##
## This module provides Nim FFI bindings to the Raylib C library.
## Using Raylib 5.0.0

{.passL: "-lraylib".}

# ============================================================================
# Core Types
# ============================================================================

type
  RColor* {.importc: "Color", header: "raylib.h", bycopy.} = object
    r*: uint8
    g*: uint8
    b*: uint8
    a*: uint8

  RVector2* {.importc: "Vector2", header: "raylib.h", bycopy.} = object
    x*: cfloat
    y*: cfloat

  RRectangle* {.importc: "Rectangle", header: "raylib.h", bycopy.} = object
    x*: cfloat
    y*: cfloat
    width*: cfloat
    height*: cfloat

  RFont* {.importc: "Font", header: "raylib.h", bycopy.} = object
    baseSize*: cint
    glyphCount*: cint
    glyphPadding*: cint
    # ... other fields omitted for simplicity

# ============================================================================
# Color Constants
# ============================================================================

# Define common colors directly
const
  RAYWHITE* = RColor(r: 245, g: 245, b: 245, a: 255)
  BLACK* = RColor(r: 0, g: 0, b: 0, a: 255)
  WHITE* = RColor(r: 255, g: 255, b: 255, a: 255)
  DARKGRAY* = RColor(r: 80, g: 80, b: 80, a: 255)
  LIGHTGRAY* = RColor(r: 200, g: 200, b: 200, a: 255)
  GRAY* = RColor(r: 130, g: 130, b: 130, a: 255)

# Helper to create colors
proc rcolor*(r, g, b, a: uint8): RColor {.inline.} =
  RColor(r: r, g: g, b: b, a: a)

# ============================================================================
# Window Management
# ============================================================================

proc InitWindow*(width, height: cint, title: cstring) {.importc: "InitWindow", header: "raylib.h".}
proc CloseWindow*() {.importc: "CloseWindow", header: "raylib.h".}
proc WindowShouldClose*(): bool {.importc: "WindowShouldClose", header: "raylib.h".}
proc SetTargetFPS*(fps: cint) {.importc: "SetTargetFPS", header: "raylib.h".}

# ============================================================================
# Drawing
# ============================================================================

proc BeginDrawing*() {.importc: "BeginDrawing", header: "raylib.h".}
proc EndDrawing*() {.importc: "EndDrawing", header: "raylib.h".}
proc ClearBackground*(color: RColor) {.importc: "ClearBackground", header: "raylib.h".}

# ============================================================================
# Shapes Drawing
# ============================================================================

proc DrawRectangle*(posX, posY, width, height: cint, color: RColor) {.importc: "DrawRectangle", header: "raylib.h".}
proc DrawRectangleRec*(rec: RRectangle, color: RColor) {.importc: "DrawRectangleRec", header: "raylib.h".}
proc DrawRectangleLines*(posX, posY, width, height: cint, color: RColor) {.importc: "DrawRectangleLines", header: "raylib.h".}
proc DrawRectangleLinesEx*(rec: RRectangle, lineThick: cfloat, color: RColor) {.importc: "DrawRectangleLinesEx", header: "raylib.h".}

# ============================================================================
# Text Drawing
# ============================================================================

proc DrawText*(text: cstring, posX, posY, fontSize: cint, color: RColor) {.importc: "DrawText", header: "raylib.h".}
proc DrawTextEx*(font: RFont, text: cstring, position: RVector2, fontSize, spacing: cfloat, tint: RColor) {.importc: "DrawTextEx", header: "raylib.h".}
proc MeasureText*(text: cstring, fontSize: cint): cint {.importc: "MeasureText", header: "raylib.h".}
proc MeasureTextEx*(font: RFont, text: cstring, fontSize, spacing: cfloat): RVector2 {.importc: "MeasureTextEx", header: "raylib.h".}

# ============================================================================
# Font Management
# ============================================================================

proc GetFontDefault*(): RFont {.importc: "GetFontDefault", header: "raylib.h".}

# ============================================================================
# Input Handling - Mouse
# ============================================================================

proc GetMousePosition*(): RVector2 {.importc: "GetMousePosition", header: "raylib.h".}
proc IsMouseButtonPressed*(button: cint): bool {.importc: "IsMouseButtonPressed", header: "raylib.h".}
proc IsMouseButtonDown*(button: cint): bool {.importc: "IsMouseButtonDown", header: "raylib.h".}
proc IsMouseButtonReleased*(button: cint): bool {.importc: "IsMouseButtonReleased", header: "raylib.h".}

# Mouse button constants
const
  MOUSE_BUTTON_LEFT* = 0.cint
  MOUSE_BUTTON_RIGHT* = 1.cint
  MOUSE_BUTTON_MIDDLE* = 2.cint

# Mouse cursor functions
proc SetMouseCursor*(cursor: cint) {.importc: "SetMouseCursor", header: "raylib.h".}

# Mouse cursor constants
const
  MOUSE_CURSOR_DEFAULT* = 0.cint
  MOUSE_CURSOR_ARROW* = 1.cint
  MOUSE_CURSOR_IBEAM* = 2.cint
  MOUSE_CURSOR_CROSSHAIR* = 3.cint
  MOUSE_CURSOR_POINTING_HAND* = 4.cint
  MOUSE_CURSOR_RESIZE_EW* = 5.cint
  MOUSE_CURSOR_RESIZE_NS* = 6.cint
  MOUSE_CURSOR_RESIZE_NWSE* = 7.cint
  MOUSE_CURSOR_RESIZE_NESW* = 8.cint
  MOUSE_CURSOR_RESIZE_ALL* = 9.cint
  MOUSE_CURSOR_NOT_ALLOWED* = 10.cint

# ============================================================================
# Input Handling - Keyboard
# ============================================================================

proc GetCharPressed*(): cint {.importc: "GetCharPressed", header: "raylib.h".}
proc IsKeyPressed*(key: cint): bool {.importc: "IsKeyPressed", header: "raylib.h".}
proc IsKeyDown*(key: cint): bool {.importc: "IsKeyDown", header: "raylib.h".}
proc IsKeyReleased*(key: cint): bool {.importc: "IsKeyReleased", header: "raylib.h".}

# Key constants (commonly used)
const
  KEY_NULL* = 0.cint
  KEY_BACKSPACE* = 259.cint
  KEY_ENTER* = 257.cint
  KEY_TAB* = 258.cint
  KEY_ESCAPE* = 256.cint
  KEY_DELETE* = 261.cint
  KEY_LEFT* = 263.cint
  KEY_RIGHT* = 262.cint
  KEY_UP* = 265.cint
  KEY_DOWN* = 264.cint
  KEY_HOME* = 268.cint
  KEY_END* = 269.cint
  KEY_LEFT_CONTROL* = 341.cint
  KEY_LEFT_SHIFT* = 340.cint
  KEY_LEFT_ALT* = 342.cint

# ============================================================================
# Collision Detection
# ============================================================================

proc CheckCollisionPointRec*(point: RVector2, rec: RRectangle): bool {.importc: "CheckCollisionPointRec", header: "raylib.h".}

# ============================================================================
# Helpers
# ============================================================================

proc rvec2*(x, y: float): RVector2 {.inline.} =
  RVector2(x: x.cfloat, y: y.cfloat)

proc rrect*(x, y, width, height: float): RRectangle {.inline.} =
  RRectangle(x: x.cfloat, y: y.cfloat, width: width.cfloat, height: height.cfloat)
