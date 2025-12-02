0BSD# 0BSD
## canvas Plugin Bindings for Nim
## Auto-generated from bindings.json - DO NOT EDIT MANUALLY
##
## 2D drawing primitives (circles, ellipses, arcs)
## Version: 1.0.0

import strutils

# Link plugin libraries
{.passL: "-lkryon_desktop -lkryon_ir".}

# ============================================================================
# C Function Imports
# ============================================================================

proc kryon_canvas_draw_circle*(cx: int32, cy: int32, radius: int32, color: uint32, filled: bool) {.
  importc: "kryon_canvas_draw_circle",
  header: "../../plugins/canvas/canvas_plugin.h".}

proc kryon_canvas_draw_ellipse*(cx: int32, cy: int32, rx: int32, ry: int32, color: uint32, filled: bool) {.
  importc: "kryon_canvas_draw_ellipse",
  header: "../../plugins/canvas/canvas_plugin.h".}

proc kryon_canvas_draw_arc*(cx: int32, cy: int32, radius: int32, start_angle: int32, end_angle: int32, color: uint32) {.
  importc: "kryon_canvas_draw_arc",
  header: "../../plugins/canvas/canvas_plugin.h".}

# ============================================================================
# Helper Functions
# ============================================================================

proc parseColor*(hex: string): uint32 =
  ## Convert hex color string (#RRGGBB or #RRGGBBAA) to uint32 RGBA
  var h = hex
  if h.startsWith("#"):
    h = h[1..^1]

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

# ============================================================================
# High-Level Wrappers
# ============================================================================

proc drawCircle*(cx: int32, cy: int32, radius: int32, color: string, filled: bool = true) =
  ## Draw a circle (filled or outline)
  ## - cx: Center X coordinate
  ## - cy: Center Y coordinate
  ## - radius: Circle radius
  ## - color: RGBA color
  ## - filled: Fill the circle (true) or outline only (false)
  kryon_canvas_draw_circle(cx.int32, cy.int32, radius.int32, parseColor(color), filled)

proc drawEllipse*(cx: int32, cy: int32, rx: int32, ry: int32, color: string, filled: bool = true) =
  ## Draw an ellipse (filled or outline)
  ## - cx: Center X coordinate
  ## - cy: Center Y coordinate
  ## - rx: Horizontal radius
  ## - ry: Vertical radius
  ## - color: RGBA color
  ## - filled: Fill the ellipse (true) or outline only (false)
  kryon_canvas_draw_ellipse(cx.int32, cy.int32, rx.int32, ry.int32, parseColor(color), filled)

proc drawArc*(cx: int32, cy: int32, radius: int32, start_angle: int32, end_angle: int32, color: string) =
  ## Draw an arc (portion of a circle)
  ## - cx: Center X coordinate
  ## - cy: Center Y coordinate
  ## - radius: Arc radius
  ## - start_angle: Start angle in degrees
  ## - end_angle: End angle in degrees
  ## - color: RGBA color
  kryon_canvas_draw_arc(cx.int32, cy.int32, radius.int32, start_angle.int32, end_angle.int32, parseColor(color))

