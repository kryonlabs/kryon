## Nim bindings for Kryon IR Canvas system
## These functions are implemented in core/canvas.c using the IR command buffer

import std/math

# Canvas draw mode (matches kryon_draw_mode_t)
type
  CanvasDrawMode* {.size: sizeof(cint).} = enum
    CANVAS_DRAW_FILL = 0
    CANVAS_DRAW_LINE = 1

  DrawMode* = CanvasDrawMode  # Alias for API compatibility

  Point* = tuple[x: float, y: float]

# Import kryon_draw_mode_t type
type kryon_draw_mode_t* {.importc: "kryon_draw_mode_t", header: "kryon_canvas.h", size: sizeof(cint).} = enum
  KRYON_DRAW_FILL = 0
  KRYON_DRAW_LINE = 1

# Import kryon_fp_t (on desktop it's float)
type kryon_fp_t* {.importc: "kryon_fp_t", header: "kryon.h".} = cfloat

# Canvas lifecycle
proc kryon_canvas_init*(width, height: uint16) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_shutdown*() {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_resize*(width, height: uint16) {.importc, cdecl, header: "kryon_canvas.h".}

# Canvas state
proc kryon_canvas_set_command_buffer*(buf: pointer) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_set_offset*(x, y: int16) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_clear*() {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_clear_color*(color: uint32) {.importc, cdecl, header: "kryon_canvas.h".}

# Color management
proc kryon_canvas_set_color*(color: uint32) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_set_color_rgba*(r, g, b, a: uint8) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_set_color_rgb*(r, g, b: uint8) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_get_color*(): uint32 {.importc, cdecl, header: "kryon_canvas.h".}

proc kryon_canvas_set_background_color*(color: uint32) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_set_background_color_rgba*(r, g, b, a: uint8) {.importc, cdecl, header: "kryon_canvas.h".}

# Line properties
proc kryon_canvas_set_line_width*(width: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_get_line_width*(): kryon_fp_t {.importc, cdecl, header: "kryon_canvas.h".}

# Shape drawing
proc kryon_canvas_rectangle*(mode: kryon_draw_mode_t, x, y, width, height: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_circle*(mode: kryon_draw_mode_t, x, y, radius: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_ellipse*(mode: kryon_draw_mode_t, x, y, rx, ry: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_polygon*(mode: kryon_draw_mode_t, vertices: ptr kryon_fp_t, vertex_count: uint16) {.importc, cdecl, header: "kryon_canvas.h".}

# Line drawing
proc kryon_canvas_line*(x1, y1, x2, y2: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_line_points*(points: ptr kryon_fp_t, point_count: uint16) {.importc, cdecl, header: "kryon_canvas.h".}

# Arc drawing
proc kryon_canvas_arc*(mode: kryon_draw_mode_t, cx, cy, radius, start_angle, end_angle: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}

# Text rendering
proc kryon_canvas_print*(text: cstring, x, y: kryon_fp_t) {.importc, cdecl, header: "kryon_canvas.h".}
proc kryon_canvas_get_text_width*(text: cstring): kryon_fp_t {.importc, cdecl, header: "kryon_canvas.h".}

# High-level API wrappers (maintain compatibility with old API)

proc background*(r, g, b: int) =
  ## Clear background with opaque color
  kryon_canvas_set_background_color_rgba(uint8(r), uint8(g), uint8(b), 255)
  kryon_canvas_clear()

proc background*(r, g, b, a: int) =
  ## Clear background with transparent color
  kryon_canvas_set_background_color_rgba(uint8(r), uint8(g), uint8(b), uint8(a))
  kryon_canvas_clear()

proc fill*(r, g, b: int) =
  ## Set fill color (opaque)
  kryon_canvas_set_color_rgba(uint8(r), uint8(g), uint8(b), 255)

proc fill*(r, g, b, a: int) =
  ## Set fill color (with alpha)
  kryon_canvas_set_color_rgba(uint8(r), uint8(g), uint8(b), uint8(a))

proc stroke*(r, g, b: int) =
  ## Set stroke color (opaque) - same as fill in new system
  kryon_canvas_set_color_rgba(uint8(r), uint8(g), uint8(b), 255)

proc stroke*(r, g, b, a: int) =
  ## Set stroke color (with alpha) - same as fill in new system
  kryon_canvas_set_color_rgba(uint8(r), uint8(g), uint8(b), uint8(a))

proc strokeWeight*(weight: float) =
  ## Set line width
  kryon_canvas_set_line_width(kryon_fp_t(weight))

proc rect*(x, y, width, height: float) =
  ## Draw filled rectangle (legacy API - always filled)
  kryon_canvas_rectangle(KRYON_DRAW_FILL, kryon_fp_t(x), kryon_fp_t(y), kryon_fp_t(width), kryon_fp_t(height))

proc rect*(x, y, width, height: int) =
  rect(float(x), float(y), float(width), float(height))

proc rectangle*(mode: DrawMode, x, y, width, height: float) =
  ## Draw rectangle with fill/line mode
  kryon_canvas_rectangle(kryon_draw_mode_t(mode), kryon_fp_t(x), kryon_fp_t(y), kryon_fp_t(width), kryon_fp_t(height))

proc rectangle*(mode: DrawMode, x, y, width, height: int) =
  rectangle(mode, float(x), float(y), float(width), float(height))

proc circle*(mode: DrawMode, x, y, radius: float) =
  ## Draw circle with fill/line mode
  kryon_canvas_circle(kryon_draw_mode_t(mode), kryon_fp_t(x), kryon_fp_t(y), kryon_fp_t(radius))

proc circle*(mode: DrawMode, x, y, radius: int) =
  circle(mode, float(x), float(y), float(radius))

proc ellipse*(mode: DrawMode, x, y, rx, ry: float) =
  ## Draw ellipse with fill/line mode
  kryon_canvas_ellipse(kryon_draw_mode_t(mode), kryon_fp_t(x), kryon_fp_t(y), kryon_fp_t(rx), kryon_fp_t(ry))

proc ellipse*(mode: DrawMode, x, y, rx, ry: int) =
  ellipse(mode, float(x), float(y), float(rx), float(ry))

proc polygon*(mode: DrawMode, vertices: openArray[Point]) =
  ## Draw polygon with fill/line mode
  if vertices.len < 3: return

  var flatVertices = newSeq[kryon_fp_t](vertices.len * 2)
  for i, vertex in vertices:
    flatVertices[i * 2] = kryon_fp_t(vertex.x)
    flatVertices[i * 2 + 1] = kryon_fp_t(vertex.y)

  kryon_canvas_polygon(kryon_draw_mode_t(mode), addr flatVertices[0], uint16(vertices.len))

proc line*(x1, y1, x2, y2: float) =
  ## Draw single line
  kryon_canvas_line(kryon_fp_t(x1), kryon_fp_t(y1), kryon_fp_t(x2), kryon_fp_t(y2))

proc line*(x1, y1, x2, y2: int) =
  line(float(x1), float(y1), float(x2), float(y2))

proc line*(points: openArray[Point]) =
  ## Draw connected lines
  if points.len < 2: return

  var flatPoints = newSeq[kryon_fp_t](points.len * 2)
  for i, point in points:
    flatPoints[i * 2] = kryon_fp_t(point.x)
    flatPoints[i * 2 + 1] = kryon_fp_t(point.y)

  kryon_canvas_line_points(addr flatPoints[0], uint16(points.len))

proc arc*(mode: DrawMode, cx, cy, radius: float, startAngle, endAngle: float) =
  ## Draw arc with fill/line mode
  kryon_canvas_arc(kryon_draw_mode_t(mode), kryon_fp_t(cx), kryon_fp_t(cy), kryon_fp_t(radius), kryon_fp_t(startAngle), kryon_fp_t(endAngle))

proc arc*(mode: DrawMode, cx, cy, radius: int, startAngle, endAngle: float) =
  arc(mode, float(cx), float(cy), float(radius), startAngle, endAngle)

proc print*(text: string, x, y: float) =
  ## Print text at position
  kryon_canvas_print(cstring(text), kryon_fp_t(x), kryon_fp_t(y))

proc print*(text: string, x, y: int) =
  print(text, float(x), float(y))

# Aliases for compatibility
const
  Fill* = CANVAS_DRAW_FILL
  Line* = CANVAS_DRAW_LINE

# Export math for convenience
export math

# Converters for seamless int/float operations (Love2D-style API convenience)
converter intToFloat*(x: int): float = float(x)
converter intToFloat32*(x: int): float32 = float32(x)
