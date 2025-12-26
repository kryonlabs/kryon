## NativeCanvas Component for Nim
##
## Provides a canvas element that allows direct access to the active rendering backend.
## Users can call backend-specific rendering functions (raylib, SDL3, etc.) within
## the onRender callback.
##
## Example usage with raylib:
## ```nim
## import native_canvas
## import raylib  # Use external raylib bindings
##
## let canvas = NativeCanvas(
##   width = 800,
##   height = 600,
##   onRender = proc() =
##     # Call raylib functions directly
##     BeginMode3D(camera)
##     DrawCube(Vector3(x: 0, y: 0, z: 0), 1.0, 1.0, 1.0, RED)
##     EndMode3D()
## )
## ```

import macros
import tables
import ir_core

# ============================================================================
# C Bindings for NativeCanvas
# ============================================================================

{.passC: "-I../../ir".}
{.compile: "../../ir/ir_native_canvas.c".}

type
  IRNativeCanvasData* {.importc: "IRNativeCanvasData", header: "ir_native_canvas.h".} = object
    width*: uint16
    height*: uint16
    background_color*: uint32
    render_callback*: proc(component_id: uint32) {.cdecl.}
    user_data*: pointer

# C function imports
proc ir_create_native_canvas*(width: uint16, height: uint16): ptr IRComponent
  {.importc, header: "ir_native_canvas.h".}

proc ir_native_canvas_set_callback*(component_id: uint32, callback: proc(component_id: uint32) {.cdecl.})
  {.importc, header: "ir_native_canvas.h".}

proc ir_native_canvas_set_user_data*(component_id: uint32, user_data: pointer)
  {.importc, header: "ir_native_canvas.h".}

proc ir_native_canvas_get_user_data*(component_id: uint32): pointer
  {.importc, header: "ir_native_canvas.h".}

proc ir_native_canvas_invoke_callback*(component_id: uint32): bool
  {.importc, header: "ir_native_canvas.h".}

proc ir_native_canvas_set_background_color*(component_id: uint32, color: uint32)
  {.importc, header: "ir_native_canvas.h".}

proc ir_native_canvas_get_data*(component: ptr IRComponent): ptr IRNativeCanvasData
  {.importc, header: "ir_native_canvas.h".}

# ============================================================================
# Callback Registry
# ============================================================================

# Global table mapping component IDs to Nim callbacks
var g_native_canvas_callbacks {.global.} = initTable[uint32, proc()]()

# C callback bridge - exported for C to call
proc native_canvas_callback_bridge*(componentId: uint32) {.exportc, cdecl, dynlib.} =
  ## Bridge function called by C code to invoke Nim callbacks
  if g_native_canvas_callbacks.hasKey(componentId):
    g_native_canvas_callbacks[componentId]()

# ============================================================================
# Nim API
# ============================================================================

proc registerNativeCanvasCallback*(componentId: uint32, callback: proc()) =
  ## Register a Nim callback for a NativeCanvas component
  g_native_canvas_callbacks[componentId] = callback

proc createNativeCanvas*(width: int, height: int, onRender: proc() = nil): ptr IRComponent =
  ## Create a NativeCanvas component programmatically
  ##
  ## Args:
  ##   width: Canvas width in pixels
  ##   height: Canvas height in pixels
  ##   onRender: Optional render callback
  ##
  ## Returns:
  ##   Pointer to the created IR component

  result = ir_create_native_canvas(width.uint16, height.uint16)

  if onRender != nil:
    # Register the Nim callback
    registerNativeCanvasCallback(result.id, onRender)

    # Set the C callback bridge
    ir_native_canvas_set_callback(result.id, native_canvas_callback_bridge)

# ============================================================================
# DSL Macro
# ============================================================================

macro NativeCanvas*(body: untyped): untyped =
  ## DSL macro for creating NativeCanvas components
  ##
  ## Example:
  ## ```nim
  ## NativeCanvas:
  ##   width = 800
  ##   height = 600
  ##   backgroundColor = 0x000000FF
  ##   onRender = proc() =
  ##     # Render with backend-specific functions
  ##     discard
  ## ```

  var width = 800
  var height = 600
  var backgroundColor: uint32 = 0x000000FF
  var onRenderCallback: NimNode = nil

  # Parse body statements
  if body.kind == nnkStmtList:
    for stmt in body:
      if stmt.kind == nnkAsgn or stmt.kind == nnkCall:
        let key = if stmt.kind == nnkAsgn: stmt[0] else: stmt[0]
        let val = if stmt.kind == nnkAsgn: stmt[1] else: stmt[1][0]

        case $key
        of "width":
          width = val.intVal.int
        of "height":
          height = val.intVal.int
        of "backgroundColor":
          if val.kind == nnkIntLit:
            backgroundColor = val.intVal.uint32
          else:
            backgroundColor = val.intVal.uint32
        of "onRender":
          onRenderCallback = val
        else:
          discard

  # Generate code
  result = quote do:
    block:
      let comp = ir_create_native_canvas(`width`.uint16, `height`.uint16)

      # Set background color if specified
      when `backgroundColor` != 0x000000FF'u32:
        ir_native_canvas_set_background_color(comp.id, `backgroundColor`)

      # Register callback if provided
      when `onRenderCallback` != nil:
        let callback = `onRenderCallback`
        registerNativeCanvasCallback(comp.id, callback)
        ir_native_canvas_set_callback(comp.id, native_canvas_callback_bridge)

      comp

# ============================================================================
# Helper Utilities
# ============================================================================

proc setCanvasUserData*(component: ptr IRComponent, userData: pointer) =
  ## Set user data for a NativeCanvas component
  if component != nil:
    ir_native_canvas_set_user_data(component.id, userData)

proc getCanvasUserData*(component: ptr IRComponent): pointer =
  ## Get user data from a NativeCanvas component
  if component != nil:
    return ir_native_canvas_get_user_data(component.id)
  return nil

proc getCanvasData*(component: ptr IRComponent): ptr IRNativeCanvasData =
  ## Get the NativeCanvas data structure
  return ir_native_canvas_get_data(component)

# ============================================================================
# Color Utilities
# ============================================================================

proc rgba*(r, g, b: uint8, a: uint8 = 255): uint32 {.inline.} =
  ## Create RGBA color value (0xRRGGBBAA format)
  result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or a.uint32

proc rgb*(r, g, b: uint8): uint32 {.inline.} =
  ## Create RGB color value with full opacity
  rgba(r, g, b, 255)
