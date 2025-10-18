## Skia FFI Bindings
##
## This module provides Nim FFI bindings to the Skia C library.
## Using Skia C API (skia-c)

{.passL: "-lskia".}

# ============================================================================
# Core Types
# ============================================================================

type
  SkCanvas* = ptr object
  SkSurface* = ptr object
  SkPaint* = ptr object
  SkFont* = ptr object
  SkTypeface* = ptr object
  SkData* = ptr object
  SkImage* = ptr object
  SkPath* = ptr object
  SkRect* = ptr object
  SkColor* = uint32

  GrDirectContext* = ptr object
  GrGLInterface* = ptr object

  # Enums
  SkPaintStyle* {.size: sizeof(cint).} = enum
    kFill_Style = 0
    kStroke_Style = 1
    kStrokeAndFill_Style = 2

  SkTextEncoding* {.size: sizeof(cint).} = enum
    kUTF8_Encoding = 0
    kUTF16_Encoding = 1
    kUTF32_Encoding = 2
    kGlyphID_Encoding = 3

  SkTextAlign* {.size: sizeof(cint).} = enum
    kLeft_Align = 0
    kCenter_Align = 1
    kRight_Align = 2

  # Rectangle structure (C-compatible)
  SkRectC* {.bycopy.} = object
    left*: cfloat
    top*: cfloat
    right*: cfloat
    bottom*: cfloat

  # Point structure
  SkPoint* {.bycopy.} = object
    x*: cfloat
    y*: cfloat

# ============================================================================
# Color Helpers
# ============================================================================

proc SkColorSetARGB*(a, r, g, b: uint8): SkColor {.inline.} =
  ## Create a SkColor from ARGB components
  (a.uint32 shl 24) or (r.uint32 shl 16) or (g.uint32 shl 8) or b.uint32

proc SkColorSetRGB*(r, g, b: uint8): SkColor {.inline.} =
  ## Create a SkColor from RGB components (alpha = 255)
  SkColorSetARGB(255, r, g, b)

# ============================================================================
# Rectangle Helpers
# ============================================================================

proc SkRectMakeLTRB*(left, top, right, bottom: cfloat): SkRectC {.inline.} =
  ## Create a rectangle from left, top, right, bottom
  SkRectC(left: left, top: top, right: right, bottom: bottom)

proc SkRectMakeXYWH*(x, y, width, height: cfloat): SkRectC {.inline.} =
  ## Create a rectangle from x, y, width, height
  SkRectC(left: x, top: y, right: x + width, bottom: y + height)

# ============================================================================
# Canvas Functions
# ============================================================================

proc sk_canvas_clear*(canvas: SkCanvas, color: SkColor) {.importc: "sk_canvas_clear", cdecl.}
proc sk_canvas_draw_rect*(canvas: SkCanvas, rect: ptr SkRectC, paint: SkPaint) {.importc: "sk_canvas_draw_rect", cdecl.}
proc sk_canvas_draw_text*(canvas: SkCanvas, text: cstring, length: csize_t, x, y: cfloat, font: SkFont, paint: SkPaint) {.importc: "sk_canvas_draw_text", cdecl.}
proc sk_canvas_draw_line*(canvas: SkCanvas, x0, y0, x1, y1: cfloat, paint: SkPaint) {.importc: "sk_canvas_draw_line", cdecl.}
proc sk_canvas_save*(canvas: SkCanvas): cint {.importc: "sk_canvas_save", cdecl.}
proc sk_canvas_restore*(canvas: SkCanvas) {.importc: "sk_canvas_restore", cdecl.}
proc sk_canvas_clip_rect*(canvas: SkCanvas, rect: ptr SkRectC, antialias: bool) {.importc: "sk_canvas_clip_rect", cdecl.}
proc sk_canvas_flush*(canvas: SkCanvas) {.importc: "sk_canvas_flush", cdecl.}

# ============================================================================
# Paint Functions
# ============================================================================

proc sk_paint_new*(): SkPaint {.importc: "sk_paint_new", cdecl.}
proc sk_paint_delete*(paint: SkPaint) {.importc: "sk_paint_delete", cdecl.}
proc sk_paint_set_color*(paint: SkPaint, color: SkColor) {.importc: "sk_paint_set_color", cdecl.}
proc sk_paint_set_antialias*(paint: SkPaint, antialias: bool) {.importc: "sk_paint_set_antialias", cdecl.}
proc sk_paint_set_style*(paint: SkPaint, style: SkPaintStyle) {.importc: "sk_paint_set_style", cdecl.}
proc sk_paint_set_stroke_width*(paint: SkPaint, width: cfloat) {.importc: "sk_paint_set_stroke_width", cdecl.}

# ============================================================================
# Font Functions
# ============================================================================

proc sk_font_new*(): SkFont {.importc: "sk_font_new", cdecl.}
proc sk_font_new_with_values*(typeface: SkTypeface, size: cfloat, scaleX: cfloat, skewX: cfloat): SkFont {.importc: "sk_font_new_with_values", cdecl.}
proc sk_font_delete*(font: SkFont) {.importc: "sk_font_delete", cdecl.}
proc sk_font_set_size*(font: SkFont, size: cfloat) {.importc: "sk_font_set_size", cdecl.}
proc sk_font_set_typeface*(font: SkFont, typeface: SkTypeface) {.importc: "sk_font_set_typeface", cdecl.}
proc sk_font_measure_text*(font: SkFont, text: cstring, byteLength: csize_t, encoding: SkTextEncoding, bounds: ptr SkRectC, paint: SkPaint): cfloat {.importc: "sk_font_measure_text", cdecl.}

# ============================================================================
# Typeface Functions
# ============================================================================

proc sk_typeface_create_default*(): SkTypeface {.importc: "sk_typeface_create_default", cdecl.}
proc sk_typeface_create_from_name*(familyName: cstring, style: cint): SkTypeface {.importc: "sk_typeface_create_from_name", cdecl.}
proc sk_typeface_create_from_file*(path: cstring, index: cint): SkTypeface {.importc: "sk_typeface_create_from_file", cdecl.}
proc sk_typeface_unref*(typeface: SkTypeface) {.importc: "sk_typeface_unref", cdecl.}

# ============================================================================
# Surface Functions
# ============================================================================

proc sk_surface_new_backend_render_target*(
  context: GrDirectContext,
  renderTarget: pointer,
  origin: cint,
  colorType: cint,
  colorSpace: pointer,
  props: pointer
): SkSurface {.importc: "sk_surface_new_backend_render_target", cdecl.}

proc sk_surface_new_raster*(
  width: cint,
  height: cint,
  colorType: cint,
  alphaType: cint,
  props: pointer
): SkSurface {.importc: "sk_surface_new_raster", cdecl.}

proc sk_surface_new_raster_direct*(
  width: cint,
  height: cint,
  colorType: cint,
  alphaType: cint,
  pixels: pointer,
  rowBytes: csize_t,
  props: pointer
): SkSurface {.importc: "sk_surface_new_raster_direct", cdecl.}

proc sk_surface_unref*(surface: SkSurface) {.importc: "sk_surface_unref", cdecl.}
proc sk_surface_get_canvas*(surface: SkSurface): SkCanvas {.importc: "sk_surface_get_canvas", cdecl.}

# ============================================================================
# OpenGL Integration (for GPU-accelerated rendering)
# ============================================================================

proc gr_glinterface_create_native_interface*(): GrGLInterface {.importc: "gr_glinterface_create_native_interface", cdecl.}
proc gr_glinterface_unref*(glInterface: GrGLInterface) {.importc: "gr_glinterface_unref", cdecl.}

proc gr_direct_context_make_gl*(glInterface: GrGLInterface): GrDirectContext {.importc: "gr_direct_context_make_gl", cdecl.}
proc gr_direct_context_unref*(context: GrDirectContext) {.importc: "gr_direct_context_unref", cdecl.}
proc gr_direct_context_flush*(context: GrDirectContext) {.importc: "gr_direct_context_flush", cdecl.}

# ============================================================================
# Constants
# ============================================================================

const
  # Color Types
  kUnknown_ColorType* = 0.cint
  kAlpha_8_ColorType* = 1.cint
  kRGB_565_ColorType* = 2.cint
  kARGB_4444_ColorType* = 3.cint
  kRGBA_8888_ColorType* = 4.cint
  kBGRA_8888_ColorType* = 5.cint

  # Alpha Types
  kUnknown_AlphaType* = 0.cint
  kOpaque_AlphaType* = 1.cint
  kPremul_AlphaType* = 2.cint
  kUnpremul_AlphaType* = 3.cint

  # Surface Origin
  kTopLeft_GrSurfaceOrigin* = 0.cint
  kBottomLeft_GrSurfaceOrigin* = 1.cint

  # Font Style
  kNormal_Style* = 0.cint
  kBold_Style* = 1.cint
  kItalic_Style* = 2.cint
  kBoldItalic_Style* = 3.cint
