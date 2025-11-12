## Kryon Canvas C API Bindings
##
## Nim bindings for the enhanced C canvas implementation

import core_kryon

# ============================================================================
# Canvas Types and Enums (matching C definitions)
# ============================================================================

type
  kryon_draw_mode_t* = enum
    KRYON_DRAW_FILL = 0
    KRYON_DRAW_LINE = 1

  kryon_line_style_t* = enum
    KRYON_LINE_SOLID = 0
    KRYON_LINE_ROUGH = 1
    KRYON_LINE_SMOOTH = 2

  kryon_line_join_t* = enum
    KRYON_LINE_JOIN_MITER = 0
    KRYON_LINE_JOIN_BEVEL = 1
    KRYON_LINE_JOIN_ROUND = 2

  kryon_blend_mode_t* = enum
    KRYON_BLEND_ALPHA = 0
    KRYON_BLEND_ADDITIVE = 1
    KRYON_BLEND_MULTIPLY = 2
    KRYON_BLEND_SUBTRACT = 3
    KRYON_BLEND_SCREEN = 4
    KRYON_BLEND_REPLACE = 5

  kryon_canvas_state_t* = object
    # Canvas state structure (opaque for now)
    data: pointer

# ============================================================================
# Canvas Lifecycle and State Management
# ============================================================================

proc kryonCanvasInit*(width, height: uint16) {.importc: "kryon_canvas_init", header: "kryon_canvas.h".}
proc kryonCanvasShutdown*() {.importc: "kryon_canvas_shutdown", header: "kryon_canvas.h".}
proc kryonCanvasResize*(width, height: uint16) {.importc: "kryon_canvas_resize", header: "kryon_canvas.h".}
proc kryonCanvasClear*() {.importc: "kryon_canvas_clear", header: "kryon_canvas.h".}
proc kryonCanvasClearColor*(color: uint32) {.importc: "kryon_canvas_clear_color", header: "kryon_canvas.h".}

# ============================================================================
# Drawing State Management
# ============================================================================

proc kryonCanvasSetColor*(color: uint32) {.importc: "kryon_canvas_set_color", header: "kryon_canvas.h".}
proc kryonCanvasSetColorRgb*(r, g, b: uint8) {.importc: "kryon_canvas_set_color_rgb", header: "kryon_canvas.h".}
proc kryonCanvasSetColorRgba*(r, g, b, a: uint8) {.importc: "kryon_canvas_set_color_rgba", header: "kryon_canvas.h".}
proc kryonCanvasGetColor*(): uint32 {.importc: "kryon_canvas_get_color", header: "kryon_canvas.h".}

proc kryonCanvasSetBackgroundColor*(color: uint32) {.importc: "kryon_canvas_set_background_color", header: "kryon_canvas.h".}
proc kryonCanvasSetBackgroundColorRgba*(r, g, b, a: uint8) {.importc: "kryon_canvas_set_background_color_rgba", header: "kryon_canvas.h".}

proc kryonCanvasSetLineWidth*(width: KryonFp) {.importc: "kryon_canvas_set_line_width", header: "kryon_canvas.h".}
proc kryonCanvasGetLineWidth*(): KryonFp {.importc: "kryon_canvas_get_line_width", header: "kryon_canvas.h".}

proc kryonCanvasSetLineStyle*(style: kryon_line_style_t) {.importc: "kryon_canvas_set_line_style", header: "kryon_canvas.h".}
proc kryonCanvasGetLineStyle*(): kryon_line_style_t {.importc: "kryon_canvas_get_line_style", header: "kryon_canvas.h".}

proc kryonCanvasSetLineJoin*(join: kryon_line_join_t) {.importc: "kryon_canvas_set_line_join", header: "kryon_canvas.h".}
proc kryonCanvasGetLineJoin*(): kryon_line_join_t {.importc: "kryon_canvas_get_line_join", header: "kryon_canvas.h".}

proc kryonCanvasSetFont*(fontId: uint16) {.importc: "kryon_canvas_set_font", header: "kryon_canvas.h".}
proc kryonCanvasGetFont*(): uint16 {.importc: "kryon_canvas_get_font", header: "kryon_canvas.h".}

proc kryonCanvasSetBlendMode*(mode: kryon_blend_mode_t) {.importc: "kryon_canvas_set_blend_mode", header: "kryon_canvas.h".}
proc kryonCanvasGetBlendMode*(): kryon_blend_mode_t {.importc: "kryon_canvas_get_blend_mode", header: "kryon_canvas.h".}

# ============================================================================
# Transform System
# ============================================================================

proc kryonCanvasOrigin*() {.importc: "kryon_canvas_origin", header: "kryon_canvas.h".}
proc kryonCanvasTranslate*(x, y: KryonFp) {.importc: "kryon_canvas_translate", header: "kryon_canvas.h".}
proc kryonCanvasRotate*(angle: KryonFp) {.importc: "kryon_canvas_rotate", header: "kryon_canvas.h".}
proc kryonCanvasScale*(sx, sy: KryonFp) {.importc: "kryon_canvas_scale", header: "kryon_canvas.h".}
proc kryonCanvasShear*(kx, ky: KryonFp) {.importc: "kryon_canvas_shear", header: "kryon_canvas.h".}
proc kryonCanvasPush*() {.importc: "kryon_canvas_push", header: "kryon_canvas.h".}
proc kryonCanvasPop*() {.importc: "kryon_canvas_pop", header: "kryon_canvas.h".}

# ============================================================================
# Basic Drawing Primitives
# ============================================================================

proc kryonCanvasRectangle*(mode: kryon_draw_mode_t, x, y, width, height: KryonFp) {.importc: "kryon_canvas_rectangle", header: "kryon_canvas.h".}
proc kryonCanvasCircle*(mode: kryon_draw_mode_t, x, y, radius: KryonFp) {.importc: "kryon_canvas_circle", header: "kryon_canvas.h".}
proc kryonCanvasEllipse*(mode: kryon_draw_mode_t, x, y, rx, ry: KryonFp) {.importc: "kryon_canvas_ellipse", header: "kryon_canvas.h".}
proc kryonCanvasPolygon*(mode: kryon_draw_mode_t, vertices: ptr KryonFp, vertexCount: uint16) {.importc: "kryon_canvas_polygon", header: "kryon_canvas.h".}
proc kryonCanvasLine*(x1, y1, x2, y2: KryonFp) {.importc: "kryon_canvas_line", header: "kryon_canvas.h".}
proc kryonCanvasLinePoints*(points: ptr KryonFp, pointCount: uint16) {.importc: "kryon_canvas_line_points", header: "kryon_canvas.h".}
proc kryonCanvasPoint*(x, y: KryonFp) {.importc: "kryon_canvas_point", header: "kryon_canvas.h".}
proc kryonCanvasPoints*(points: ptr KryonFp, pointCount: uint16) {.importc: "kryon_canvas_points", header: "kryon_canvas.h".}
proc kryonCanvasArc*(mode: kryon_draw_mode_t, cx, cy, radius, startAngle, endAngle: KryonFp) {.importc: "kryon_canvas_arc", header: "kryon_canvas.h".}

# ============================================================================
# Text Rendering
# ============================================================================

proc kryonCanvasPrint*(text: cstring, x, y: KryonFp) {.importc: "kryon_canvas_print", header: "kryon_canvas.h".}
proc kryonCanvasPrintf*(text: cstring, x, y, wrapLimit: KryonFp, align: int32) {.importc: "kryon_canvas_printf", header: "kryon_canvas.h".}
proc kryonCanvasGetTextWidth*(text: cstring): KryonFp {.importc: "kryon_canvas_get_text_width", header: "kryon_canvas.h".}
proc kryonCanvasGetTextHeight*(): KryonFp {.importc: "kryon_canvas_get_text_height", header: "kryon_canvas.h".}
proc kryonCanvasGetFontSize*(): KryonFp {.importc: "kryon_canvas_get_font_size", header: "kryon_canvas.h".}

# ============================================================================
# Canvas Command Buffer Access
# ============================================================================

proc kryonCanvasGetCommandBuffer*(): KryonCmdBuf {.importc: "kryon_canvas_get_command_buffer", header: "kryon_canvas.h".}