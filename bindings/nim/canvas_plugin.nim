# 0BSD
## Canvas Plugin Bindings for Nim
## Provides access to canvas plugin drawing primitives

import os, strutils

# Link canvas plugin
{.passL: "-Lbuild -lkryon_desktop -lkryon_ir".}

# Canvas initialization
proc kryon_canvas_init*(width, height: uint16) {.
  importc: "kryon_canvas_init",
  header: "../../core/include/kryon_canvas.h".}

# Canvas plugin drawing functions
proc kryon_canvas_draw_circle*(cx, cy, radius: int32, color: uint32, filled: bool) {.
  importc: "kryon_canvas_draw_circle",
  header: "../../plugins/canvas/canvas_plugin.h".}

proc kryon_canvas_draw_ellipse*(cx, cy, rx, ry: int32, color: uint32, filled: bool) {.
  importc: "kryon_canvas_draw_ellipse",
  header: "../../plugins/canvas/canvas_plugin.h".}

proc kryon_canvas_draw_arc*(cx, cy, radius: int32, start_angle, end_angle: int32, color: uint32) {.
  importc: "kryon_canvas_draw_arc",
  header: "../../plugins/canvas/canvas_plugin.h".}

# Helper function to convert hex color string to uint32 RGBA
proc parseColor*(hex: string): uint32 =
  var h = hex
  if h.startsWith("#"):
    h = h[1..^1]

  # Parse RGB or RGBA
  if h.len == 6:
    # RGB - add full alpha
    let r = parseHexInt(h[0..1])
    let g = parseHexInt(h[2..3])
    let b = parseHexInt(h[4..5])
    result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or 0xFF'u32
  elif h.len == 8:
    # RGBA
    let r = parseHexInt(h[0..1])
    let g = parseHexInt(h[2..3])
    let b = parseHexInt(h[4..5])
    let a = parseHexInt(h[6..7])
    result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or a.uint32
  else:
    # Default to white
    result = 0xFFFFFFFF'u32

# High-level wrappers with Nim-friendly API
proc drawCircle*(cx, cy, radius: int, color: string, filled: bool = true) =
  kryon_canvas_draw_circle(cx.int32, cy.int32, radius.int32, parseColor(color), filled)

proc drawEllipse*(cx, cy, rx, ry: int, color: string, filled: bool = true) =
  kryon_canvas_draw_ellipse(cx.int32, cy.int32, rx.int32, ry.int32, parseColor(color), filled)

proc drawArc*(cx, cy, radius: int, startAngle, endAngle: int, color: string) =
  kryon_canvas_draw_arc(cx.int32, cy.int32, radius.int32, startAngle.int32, endAngle.int32, parseColor(color))
