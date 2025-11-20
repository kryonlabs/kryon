## Kryon IR Runtime - Pure IR-based Runtime
## This is the NEW runtime that uses ONLY the IR system
## NO LEGACY CODE - everything uses IR

import ir_core, ir_desktop, os, strutils, tables, math, parseutils

# Import reactive system for compatibility
import reactive_system
export reactive_system

# ============================================================================
# Runtime Types - IR-based
# ============================================================================

type
  KryonComponent* = ptr IRComponent  # Type alias for compatibility
  KryonRenderer* = DesktopIRRenderer  # Type alias for compatibility

  KryonApp* = ref object
    root*: ptr IRComponent
    renderer*: KryonRenderer  # Using IR renderer
    window*: KryonWindow
    running*: bool
    config*: DesktopRendererConfig

  KryonWindow* = ref object
    width*: int
    height*: int
    title*: string

# ============================================================================
# Component Creation - IR-based
# ============================================================================

proc newKryonContainer*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)

proc newKryonButton*(text: string): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_BUTTON)
  ir_set_text_content(result, cstring(text))

proc newKryonText*(text: string): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_TEXT)
  ir_set_text_content(result, cstring(text))

proc newKryonRow*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_ROW)

proc newKryonColumn*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_COLUMN)

proc newKryonCenter*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CENTER)

proc newKryonInput*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_INPUT)

proc newKryonCheckbox*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CHECKBOX)

# ============================================================================
# Component Property Setters - IR-based
# ============================================================================

proc setWidth*(component: ptr IRComponent, width: int) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_width(newStyle, IR_DIMENSION_PX, cfloat(width))
    ir_set_style(component, newStyle)
  else:
    ir_set_width(style, IR_DIMENSION_PX, cfloat(width))

proc setHeight*(component: ptr IRComponent, height: int) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_height(newStyle, IR_DIMENSION_PX, cfloat(height))
    ir_set_style(component, newStyle)
  else:
    ir_set_height(style, IR_DIMENSION_PX, cfloat(height))

proc setBackgroundColor*(component: ptr IRComponent, color: uint32) =
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_background_color(newStyle, r, g, b, a)
    ir_set_style(component, newStyle)
  else:
    ir_set_background_color(style, r, g, b, a)

proc setTextColor*(component: ptr IRComponent, color: uint32) =
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_font(newStyle, 16.0, "sans-serif", r, g, b, a, false, false)
    ir_set_style(component, newStyle)
  else:
    ir_set_font(style, 16.0, "sans-serif", r, g, b, a, false, false)

proc setText*(component: ptr IRComponent, text: string) =
  ir_set_text_content(component, cstring(text))

# ============================================================================
# Application Runtime - IR-based
# ============================================================================

proc newKryonApp*(): KryonApp =
  ## Create new Kryon application (IR-based)
  result = KryonApp()
  result.running = false

proc initRenderer*(width, height: int; title: string): KryonRenderer =
  ## Initialize IR desktop renderer with window properties
  echo "Initializing IR desktop renderer..."
  echo "  Window: ", width, "x", height
  echo "  Title: ", title

  # Create renderer config
  var config = desktop_renderer_config_sdl3(int32(width), int32(height), cstring(title))
  config.resizable = true
  config.vsync_enabled = true
  config.target_fps = 60

  # Note: Renderer will be created during run()
  # For now, return nil as placeholder (not used in IR architecture)
  result = nil

proc run*(app: KryonApp) =
  ## Run the IR-based Kryon application
  if app.root.isNil:
    echo "ERROR: No root component set"
    quit(1)

  echo "Running Kryon IR application..."

  # Create renderer config if not set
  if app.window.isNil:
    app.window = KryonWindow(width: 800, height: 600, title: "Kryon App")

  app.config = desktop_renderer_config_sdl3(
    int32(app.window.width),
    int32(app.window.height),
    cstring(app.window.title)
  )
  app.config.resizable = true
  app.config.vsync_enabled = true
  app.config.target_fps = 60

  echo "  Window: ", app.window.width, "x", app.window.height
  echo "  Title: ", app.window.title

  # Render using IR desktop renderer
  if not desktop_render_ir_component(app.root, addr app.config):
    echo "ERROR: Failed to render IR component"
    quit(1)

  echo "Application closed"

# ============================================================================
# Helper Functions for DSL Compatibility
# ============================================================================

proc parseColor*(colorStr: string): uint32 =
  ## Parse hex color string like "#ff0000" or "#ff0000ff"
  if colorStr.len == 0 or colorStr[0] != '#':
    return 0xFFFFFFFF'u32

  var hex = colorStr[1..^1]
  if hex.len == 6:
    hex = hex & "FF"  # Add alpha if not present

  if hex.len != 8:
    return 0xFFFFFFFF'u32

  try:
    let r = fromHex[uint8](hex[0..1])
    let g = fromHex[uint8](hex[2..3])
    let b = fromHex[uint8](hex[4..5])
    let a = fromHex[uint8](hex[6..7])
    result = (uint32(r) shl 24) or (uint32(g) shl 16) or (uint32(b) shl 8) or uint32(a)
  except:
    result = 0xFFFFFFFF'u32

# ============================================================================
# Compatibility Layer - Map old API to IR
# ============================================================================

# Component tree management (compatibility with old API)
proc kryon_component_add_child*(parent, child: ptr IRComponent) =
  ir_add_child(parent, child)

# Component property setters (compatibility with old API)
proc kryon_component_set_background_color*(component: ptr IRComponent, color: uint32) =
  setBackgroundColor(component, color)

proc kryon_component_set_text_color*(component: ptr IRComponent, color: uint32) =
  setTextColor(component, color)

proc kryon_component_set_bounds*(component: ptr IRComponent, x, y, w, h: int) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_width(newStyle, IR_DIMENSION_PX, cfloat(w))
    ir_set_height(newStyle, IR_DIMENSION_PX, cfloat(h))
    ir_set_style(component, newStyle)
  else:
    ir_set_width(style, IR_DIMENSION_PX, cfloat(w))
    ir_set_height(style, IR_DIMENSION_PX, cfloat(h))

# Canvas/Spacer stubs (not yet implemented in IR)
proc newKryonCanvas*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CANVAS)

proc newKryonSpacer*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)  # Use container as spacer for now

# Dropdown stub (not yet implemented)
proc newKryonDropdown*(options: seq[string] = @[], selected: int = 0, onChange: proc(index: int) = nil): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)  # Stub for now

# Export IR functions for DSL use
export ir_add_child, ir_create_component, ir_set_text_content
export IR_COMPONENT_CONTAINER, IR_COMPONENT_BUTTON, IR_COMPONENT_TEXT
export IR_COMPONENT_ROW, IR_COMPONENT_COLUMN, IR_COMPONENT_CENTER
export IR_COMPONENT_INPUT, IR_COMPONENT_CHECKBOX, IR_COMPONENT_CANVAS

# Export compatibility types
export KryonComponent, KryonRenderer, KryonApp
