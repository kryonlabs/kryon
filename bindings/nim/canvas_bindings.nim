## Nim bindings for C canvas SDL3 backend
## These functions are implemented in backends/desktop/canvas_sdl3.c

import std/math

# SDL renderer type (opaque)
type SDL_Renderer* {.importc: "SDL_Renderer", header: "canvas_sdl3.h", incompleteStruct.} = object

# Canvas draw mode
type
  CanvasDrawMode* {.size: sizeof(cint).} = enum
    CANVAS_DRAW_FILL = 0
    CANVAS_DRAW_LINE = 1

  DrawMode* = CanvasDrawMode  # Alias for API compatibility

  Point* = tuple[x: float, y: float]

# Context management
proc canvas_sdl3_set_renderer*(renderer: ptr SDL_Renderer) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_get_renderer*(): ptr SDL_Renderer {.importc, cdecl, header: "canvas_sdl3.h".}

# Color management
proc canvas_sdl3_background*(r, g, b, a: uint8) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_fill*(r, g, b, a: uint8) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_stroke*(r, g, b, a: uint8) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_stroke_weight*(weight: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}

# Basic shapes
proc canvas_sdl3_rect*(x, y, width, height: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_rectangle*(mode: CanvasDrawMode, x, y, width, height: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_circle*(mode: CanvasDrawMode, x, y, radius: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_ellipse*(mode: CanvasDrawMode, x, y, rx, ry: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_polygon*(mode: CanvasDrawMode, vertices: ptr cfloat, vertex_count: cint) {.importc, cdecl, header: "canvas_sdl3.h".}

# Lines
proc canvas_sdl3_line*(x1, y1, x2, y2: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}
proc canvas_sdl3_lines*(points: ptr cfloat, point_count: cint) {.importc, cdecl, header: "canvas_sdl3.h".}

# Arcs
proc canvas_sdl3_arc*(mode: CanvasDrawMode, cx, cy, radius, start_angle, end_angle: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}

# Text
proc canvas_sdl3_print*(text: cstring, x, y: cfloat) {.importc, cdecl, header: "canvas_sdl3.h".}

# High-level API wrappers
proc background*(r, g, b: int) =
  canvas_sdl3_background(uint8(r), uint8(g), uint8(b), 255)

proc background*(r, g, b, a: int) =
  canvas_sdl3_background(uint8(r), uint8(g), uint8(b), uint8(a))

proc fill*(r, g, b: int) =
  canvas_sdl3_fill(uint8(r), uint8(g), uint8(b), 255)

proc fill*(r, g, b, a: int) =
  canvas_sdl3_fill(uint8(r), uint8(g), uint8(b), uint8(a))

proc stroke*(r, g, b: int) =
  canvas_sdl3_stroke(uint8(r), uint8(g), uint8(b), 255)

proc stroke*(r, g, b, a: int) =
  canvas_sdl3_stroke(uint8(r), uint8(g), uint8(b), uint8(a))

proc strokeWeight*(weight: float) =
  canvas_sdl3_stroke_weight(cfloat(weight))

proc rect*(x, y, width, height: float) =
  canvas_sdl3_rect(cfloat(x), cfloat(y), cfloat(width), cfloat(height))

proc rect*(x, y, width, height: int) =
  rect(float(x), float(y), float(width), float(height))

proc rectangle*(mode: DrawMode, x, y, width, height: float) =
  canvas_sdl3_rectangle(mode, cfloat(x), cfloat(y), cfloat(width), cfloat(height))

proc rectangle*(mode: DrawMode, x, y, width, height: int) =
  rectangle(mode, float(x), float(y), float(width), float(height))

proc circle*(mode: DrawMode, x, y, radius: float) =
  canvas_sdl3_circle(mode, cfloat(x), cfloat(y), cfloat(radius))

proc circle*(mode: DrawMode, x, y, radius: int) =
  circle(mode, float(x), float(y), float(radius))

proc ellipse*(mode: DrawMode, x, y, rx, ry: float) =
  canvas_sdl3_ellipse(mode, cfloat(x), cfloat(y), cfloat(rx), cfloat(ry))

proc ellipse*(mode: DrawMode, x, y, rx, ry: int) =
  ellipse(mode, float(x), float(y), float(rx), float(ry))

proc polygon*(mode: DrawMode, vertices: openArray[Point]) =
  if vertices.len < 3: return

  var flatVertices = newSeq[cfloat](vertices.len * 2)
  for i, vertex in vertices:
    flatVertices[i * 2] = cfloat(vertex.x)
    flatVertices[i * 2 + 1] = cfloat(vertex.y)

  canvas_sdl3_polygon(mode, addr flatVertices[0], cint(vertices.len))

proc line*(x1, y1, x2, y2: float) =
  canvas_sdl3_line(cfloat(x1), cfloat(y1), cfloat(x2), cfloat(y2))

proc line*(x1, y1, x2, y2: int) =
  line(float(x1), float(y1), float(x2), float(y2))

proc line*(points: openArray[Point]) =
  if points.len < 2: return

  var flatPoints = newSeq[cfloat](points.len * 2)
  for i, point in points:
    flatPoints[i * 2] = cfloat(point.x)
    flatPoints[i * 2 + 1] = cfloat(point.y)

  canvas_sdl3_lines(addr flatPoints[0], cint(points.len))

proc arc*(mode: DrawMode, cx, cy, radius: float, startAngle, endAngle: float) =
  canvas_sdl3_arc(mode, cfloat(cx), cfloat(cy), cfloat(radius), cfloat(startAngle), cfloat(endAngle))

proc arc*(mode: DrawMode, cx, cy, radius: int, startAngle, endAngle: float) =
  arc(mode, float(cx), float(cy), float(radius), startAngle, endAngle)

proc print*(text: string, x, y: float) =
  canvas_sdl3_print(cstring(text), cfloat(x), cfloat(y))

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
