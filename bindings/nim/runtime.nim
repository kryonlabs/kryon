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
  KryonAlignment* = IRAlignment  # Type alias for compatibility

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

# Alignment constants for compatibility
const
  kaStart* = IR_ALIGNMENT_START
  kaCenter* = IR_ALIGNMENT_CENTER
  kaEnd* = IR_ALIGNMENT_END
  kaStretch* = IR_ALIGNMENT_STRETCH
  # Legacy alignment modes - map to IR equivalents
  kaSpaceEvenly* = IR_ALIGNMENT_STRETCH  # Fallback to stretch
  kaSpaceAround* = IR_ALIGNMENT_STRETCH  # Fallback to stretch
  kaSpaceBetween* = IR_ALIGNMENT_STRETCH  # Fallback to stretch

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

proc setWindow*(app: KryonApp, window: KryonWindow) =
  ## Set the window configuration for the app
  app.window = window

proc setRenderer*(app: KryonApp, renderer: KryonRenderer) =
  ## Set the renderer for the app (not used in IR architecture)
  app.renderer = renderer

proc setRoot*(app: KryonApp, root: ptr IRComponent) =
  ## Set the root component for the app
  app.root = root

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

# IR uses floats directly - toFixed is for DSL compatibility with old fixed-point system
proc toFixed*(value: int): cfloat {.inline.} = cfloat(value)
proc toFixed*(value: float): cfloat {.inline.} = cfloat(value)

# ============================================================================
# Compatibility Layer - Map old API to IR
# ============================================================================

# Component tree management (compatibility with old API)
proc kryon_component_add_child*(parent, child: ptr IRComponent): bool =
  ir_add_child(parent, child)
  return true  # IR always succeeds unless nil pointers

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

# Canvas/Spacer - actual IR components
proc newKryonCanvas*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CANVAS)

proc newKryonSpacer*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)

# Dropdown - not yet implemented in IR, placeholder for compatibility
proc newKryonDropdown*(options: seq[string] = @[], selected: int = 0, onChange: proc(index: int) = nil): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)

# Legacy compatibility for markdown and other modules
type
  KryonCmdBuf* = pointer

proc kryon_component_set_layout_direction*(component: ptr IRComponent, direction: int) =
  # Layout direction is implicitly handled by ROW vs COLUMN component types
  discard

proc kryon_component_set_layout_alignment*(component: ptr IRComponent, justify: KryonAlignment, align: KryonAlignment) =
  # Layout alignment for containers - create or get layout and set alignment
  var layout = ir_get_layout(component)
  if layout.isNil:
    layout = ir_create_layout()
    ir_set_layout(component, layout)
  ir_set_justify_content(layout, justify)
  ir_set_align_items(layout, align)

proc kryon_component_set_gap*(component: ptr IRComponent, gap: uint8) =
  # Gap is part of flexbox layout
  var layout = ir_get_layout(component)
  if layout.isNil:
    layout = ir_create_layout()
    ir_set_layout(component, layout)
  ir_set_gap(layout, uint32(gap))

proc kryon_component_set_bounds_mask*(component: ptr IRComponent, x, y, w, h: cfloat, mask: uint8) =
  # Set component bounds using IR style system
  # Mask parameter ignored - all bounds are set
  kryon_component_set_bounds(component, int(x), int(y), int(w), int(h))

proc kryon_component_set_flex*(component: ptr IRComponent, flexGrow, flexShrink: uint8) =
  # Flex grow/shrink in IR would be handled by flex layout dimensions
  # For now, stub - full flex implementation needs more IR layout work
  discard

proc kryon_component_mark_dirty*(component: ptr IRComponent) =
  # Dirty marking not needed in IR - rendering happens on each frame
  discard

# ============================================================================
# Nim-to-C Event Bridge - Actual Implementation
# ============================================================================

# Button event handling using IR event system
var buttonHandlers = initTable[uint32, proc()]()

proc nimButtonBridge*(componentId: uint32) {.exportc: "nimButtonBridge", cdecl, dynlib.} =
  ## Bridge function for button clicks - called from C IR event system
  ## This is exported to C so it can be called when buttons are clicked
  if buttonHandlers.hasKey(componentId):
    buttonHandlers[componentId]()

proc registerButtonHandler*(component: ptr IRComponent, handler: proc()) =
  ## Register a button click handler using IR event system
  if component.isNil:
    return

  buttonHandlers[component.id] = handler

  # Create IR event and attach to component
  let event = ir_create_event(IR_EVENT_CLICK, cstring("nim_button_" & $component.id), nil)
  ir_add_event(component, event)

proc kryon_component_set_border_color*(component: ptr IRComponent, color: uint32) =
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_border(newStyle, 1.0, r, g, b, a, 0)
    ir_set_style(component, newStyle)
  else:
    ir_set_border(style, 1.0, r, g, b, a, 0)

proc kryon_component_set_border_width*(component: ptr IRComponent, width: uint32) =
  let style = ir_get_style(component)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_border(newStyle, cfloat(width), 0, 0, 0, 255, 0)
    ir_set_style(component, newStyle)
  else:
    ir_set_border(style, cfloat(width), 0, 0, 0, 255, 0)

proc kryon_button_set_center_text*(button: ptr IRComponent, centered: bool) =
  # Text centering is handled automatically in button rendering
  discard

proc kryon_button_set_ellipsize*(button: ptr IRComponent, ellipsize: bool) =
  # Ellipsize not yet implemented in IR - would need font metrics
  discard

# Tab/Checkbox/Input stubs (not yet implemented in IR)
type
  TabVisualState* = object  # Stub for tab visual state
    backgroundColor*: uint32
    activeBackgroundColor*: uint32
    textColor*: uint32
    activeTextColor*: uint32

  TabGroupState* = ref object  # Stub
  CheckboxState* = ref object  # Stub
  InputState* = ref object  # Stub

proc createTabGroupState*(group, tabBar, tabContent: ptr IRComponent, selectedIndex: int, reorderable: bool): TabGroupState =
  # Stub for tab groups
  result = TabGroupState()

proc addTab*(state: TabGroupState, tab, panel: ptr IRComponent) =
  # Stub for adding tabs
  discard

proc finalizeTabGroup*(state: TabGroupState) =
  # Stub for finalizing tab group
  discard

proc registerTabBar*(tabBar: ptr IRComponent, state: TabGroupState) =
  # Stub for registering tab bar
  discard

proc registerTabComponent*(tab: ptr IRComponent, state: TabGroupState, index: int) =
  # Stub for registering tab component
  discard

proc registerTabContent*(content: ptr IRComponent, state: TabGroupState) =
  # Stub for registering tab content
  discard

proc registerTabPanel*(panel: ptr IRComponent, state: TabGroupState, index: int) =
  # Stub for registering tab panel
  discard

var canvasHandlers = initTable[uint32, proc()]()

proc registerCanvasHandler*(canvas: ptr IRComponent, handler: proc()) =
  ## Register a canvas onDraw handler - called each frame when canvas is rendered
  if canvas.isNil:
    return
  canvasHandlers[canvas.id] = handler

proc nimCanvasBridge*(componentId: uint32) {.exportc: "nimCanvasBridge", cdecl, dynlib.} =
  ## Bridge function for canvas rendering - called from C desktop renderer
  ## This is exported to C so it can be called when canvas components are rendered
  if canvasHandlers.hasKey(componentId):
    canvasHandlers[componentId]()

proc nimInputBridge*(component: ptr IRComponent, text: ptr cstring): bool =
  # Stub for input handling
  result = false

proc nimCheckboxBridge*(component: ptr IRComponent): bool =
  # Stub for checkbox handling
  result = false

proc registerCheckboxHandler*(component: ptr IRComponent, handler: proc(checked: bool)) =
  # Stub for checkbox handler registration
  discard

proc registerInputHandler*(component: ptr IRComponent, onChange: proc(text: string), onSubmit: proc(text: string) = nil) =
  # Stub for input handler registration
  discard

# Font management stubs
proc addFontSearchDir*(path: string) =
  # Stub - font management not yet implemented in IR
  discard

proc loadFont*(path: string, size: int): int =
  # Stub - font management not yet implemented in IR
  result = 0

proc getFontId*(path: string, size: int): int =
  # Stub - font management not yet implemented in IR
  result = 0

# Export IR functions for DSL use
export ir_add_child, ir_create_component, ir_set_text_content
export IRComponentType, IRAlignment  # Export the enum types

# Export compatibility types
export KryonComponent, KryonRenderer, KryonApp, KryonAlignment

# Export alignment constants
export kaStart, kaCenter, kaEnd, kaStretch, kaSpaceEvenly, kaSpaceAround, kaSpaceBetween
