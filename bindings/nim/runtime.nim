## Kryon Runtime - Nim<->C ABI Glue
##
## Provides the runtime bridge between Nim DSL and C core engine

import core_kryon, core_kryon_canvas, os, strutils, tables

template runtimeTrace(msg: string) =
  if getEnv("KRYON_TRACE_RUNTIME", "") != "":
    echo msg

# ============================================================================
# Kryon Runtime Types
# ============================================================================

type
  KryonApp* = ref object
    root*: KryonComponent
    renderer*: KryonRenderer
    window*: KryonWindow
    running*: bool
    rendererPreinitialized*: bool

  KryonWindow* = ref object
    width*: int
    height*: int
    title*: string

  ComponentProps* = object
    x*: int = -1
    y*: int = -1
    width*: int = 0
    height*: int = 0
    text*: string
    textColor*: uint32
    backgroundColor*: uint32
    onClick*: proc()
    onDraw*: proc(ctx: DrawingContext)
    fontSize*: int

  DrawingContext* = ref object
    width*: int
    height*: int
    # Canvas drawing operations would go here

  # Missing component types
  KryonCanvas* = ref object
  KryonSpacer* = ref object

proc isHeadlessEnvironment(): bool =
  ## Detect whether we are running without a graphical display available.
  when defined(linux):
    let hasX11 = existsEnv("DISPLAY")
    let hasWayland = existsEnv("WAYLAND_DISPLAY")
    let videoDriverSet = getEnv("SDL_VIDEODRIVER", "") != ""
    result = (not hasX11) and (not hasWayland) and (not videoDriverSet)
  else:
    result = false

proc toUint16OrDefault(value: int; defaultValue: uint16): uint16 =
  ## Clamp potentially invalid dimensions to a sensible default.
  if value <= 0 or value > int(high(uint16)):
    result = defaultValue
  else:
    result = uint16(value)

var activeRendererBackend = "sdl3"
var rendererPreinitializedHint = false
var currentHoverTarget: KryonComponent = nil

when defined(KRYON_SDL3):
  const
    cursorArrow = 0'u8
    cursorHand = 1'u8
  var currentCursorShape: uint8 = cursorArrow

proc updateCursorShape(target: KryonComponent) =
  when defined(KRYON_SDL3):
    if activeRendererBackend == "sdl3":
      let desired = if not target.isNil and kryon_component_is_button(target): cursorHand else: cursorArrow
      if desired != currentCursorShape:
        kryon_sdl3_apply_cursor_shape(desired)
        currentCursorShape = desired

when defined(KRYON_SDL3):
  proc dispatchPointerEvent(app: KryonApp; evt: var KryonEvent) =
    if app.root.isNil:
      return
    kryon_event_dispatch_to_point(app.root, evt.x, evt.y, addr evt)

  proc updateHoverState(app: KryonApp; evt: var KryonEvent) =
    if app.root.isNil:
      return

    let target = kryon_event_find_target_at_point(app.root, evt.x, evt.y)

    if target == currentHoverTarget:
      updateCursorShape(target)
      return

    if currentHoverTarget != nil:
      var leaveEvent = KryonEvent(`type`: KryonEventType.Hover, x: evt.x, y: evt.y, param: 0'u32)
      kryon_component_send_event(currentHoverTarget, addr leaveEvent)

    currentHoverTarget = target

    if target != nil:
      var enterEvent = KryonEvent(`type`: KryonEventType.Hover, x: evt.x, y: evt.y, param: 1'u32)
      kryon_component_send_event(target, addr enterEvent)

    updateCursorShape(target)

# ============================================================================
# Application Lifecycle
# ============================================================================

proc newKryonApp*(): KryonApp =
  ## Create new Kryon application
  result = KryonApp(
    running: false,
    rendererPreinitialized: false
  )

proc run*(app: KryonApp) =
  ## Run the application main loop
  if app.root == nil:
    echo "Error: No root component set"
    return

  if app.renderer == nil:
    echo "Error: No renderer set"
    return

  echo "Starting Kryon application: " & app.window.title
  app.running = true

  # Initialize the renderer
  var rendererReady = true
  if not app.rendererPreinitialized:
    rendererReady = kryon_renderer_init(app.renderer, nil)
  else:
    app.rendererPreinitialized = false
  if not rendererReady:
    echo "Failed to initialize renderer"

    if activeRendererBackend == "sdl3":
      when defined(linux):
        let currentDriver = getEnv("SDL_VIDEODRIVER", "")
        if currentDriver != "dummy":
          putEnv("SDL_VIDEODRIVER", "dummy")
          echo "Retrying SDL renderer using dummy video driver"
          rendererReady = kryon_renderer_init(app.renderer, nil)
          if rendererReady:
            echo "SDL renderer initialized with dummy video driver"

    if not rendererReady:
      when defined(KRYON_SDL3):
        if activeRendererBackend == "sdl3" and app.renderer != nil:
          kryon_sdl3_renderer_destroy(app.renderer)
          app.renderer = nil
      else:
        if activeRendererBackend == "sdl3" and app.renderer != nil:
          app.renderer = nil

      let fallback = kryon_framebuffer_renderer_create(
        uint16(app.window.width),
        uint16(app.window.height),
        4'u8)
      if fallback == nil:
        echo "Renderer initialization failed"
        return

      app.renderer = fallback
      activeRendererBackend = "framebuffer"
      app.rendererPreinitialized = true
      rendererReady = true

  # Main application loop
  while app.running:
    if app.root != nil:
      let wfp = toFixed(app.window.width)
      let hfp = toFixed(app.window.height)
      let wBase = app.window.width.int32 shl 16
      let hBase = app.window.height.int32 shl 16
      if getEnv("KRYON_TRACE_LAYOUT", "") != "":
        echo "[kryon][nim] layout width=", app.window.width, " height=", app.window.height,
             " wfp=", fromKryonFp(wfp), " hfp=", fromKryonFp(hfp), " raw=", wBase, "x", hBase,
             " noFloat=", KRYON_NO_FLOAT
      kryon_layout_tree(app.root, wfp, hfp)
      runtimeTrace("[kryon][runtime] layout complete")

    when defined(KRYON_SDL3):
      if activeRendererBackend == "sdl3":
        var evt: KryonEvent
        while kryon_sdl3_poll_event(addr evt):
          case evt.`type`
          of KryonEventType.Custom:
            app.running = false
            break
          of KryonEventType.Key:
            let pressedVal = if evt.data.isNil: 0'u else: cast[uint](evt.data)
            let isPressed = pressedVal == 1'u
            if isPressed and evt.param == 27'u32:
              app.running = false
              break
          of KryonEventType.Click:
            dispatchPointerEvent(app, evt)
          of KryonEventType.Hover:
            updateHoverState(app, evt)
          else:
            discard

        if not app.running:
          break

    if getEnv("KRYON_SKIP_RENDER", "") == "":
      kryon_render_frame(app.renderer, app.root)
    runtimeTrace("[kryon][runtime] render complete")

    # Frame timing
    sleep(16)  # ~60fps

  if currentHoverTarget != nil:
    var leaveEvt = KryonEvent(`type`: KryonEventType.Hover, x: 0'i16, y: 0'i16, param: 0'u32)
    kryon_component_send_event(currentHoverTarget, addr leaveEvt)
    currentHoverTarget = nil
  updateCursorShape(nil)
  runtimeTrace("[kryon][runtime] shutdown start")

  # Cleanup
  kryon_renderer_shutdown(app.renderer)
  echo "Application terminated"

proc appLayout*(app: KryonApp) =
  ## Perform layout on component tree
  if app.root != nil:
    kryon_layout_tree(app.root, toFixed(app.window.width), toFixed(app.window.height))

proc appRender*(app: KryonApp) =
  ## Render application frame
  if app.root != nil and app.renderer != nil:
    kryon_render_frame(app.renderer, app.root)

proc handleEvents*(app: KryonApp): bool =
  ## Handle application events
  ## Returns false if application should exit
  result = true

  # Simplified event handling
  # Real implementation would use platform-specific event systems

# ============================================================================
# Component Creation Functions
# ============================================================================

proc setRoot*(app: KryonApp, component: KryonComponent) =
  ## Set root component for application
  app.root = component

proc setRenderer*(app: KryonApp, renderer: KryonRenderer) =
  ## Set renderer for application
  app.renderer = renderer
  app.rendererPreinitialized = rendererPreinitializedHint
  rendererPreinitializedHint = false

proc setWindow*(app: KryonApp, window: KryonWindow) =
  ## Set window properties for application
  app.window = window

# ============================================================================
# Utility Functions
# ============================================================================

proc sleep*(ms: int) =
  ## Cross-platform sleep
  # Would use platform-specific sleep
  # For now, using Nim's sleep
  os.sleep(ms)

# ============================================================================
# Error Handling
# ============================================================================

proc handleKryonError*(error: string) =
  ## Handle Kryon framework errors
  echo "Kryon Error: " & error

proc checkKryonResult*(result: bool, operation: string) =
  ## Check result of Kryon operation
  if not result:
    handleKryonError("Operation failed: " & operation)

# ============================================================================
# Integration with C Core
# ============================================================================

proc initKryonCore*(): bool =
  ## Initialize Kryon C core
  # This would call any needed C core initialization
  result = true

proc shutdownKryonCore*() =
  ## Shutdown Kryon C core
  # This would call any needed C core cleanup
  discard

# ============================================================================
# Missing Component Functions
# ============================================================================

proc newKryonCanvas*(): KryonComponent =
  ## Create a new canvas component
  result = newKryonContainer()  # For now, use container as base

proc newKryonSpacer*(): KryonComponent =
  ## Create a new spacer component
  result = newKryonContainer()  # For now, use container as base

var buttonCallbacks*: Table[uint, proc () {.nimcall.}] = initTable[uint, proc () {.nimcall.}]()

proc registerButtonHandler*(button: KryonComponent; cb: proc () {.nimcall.}) =
  if cb.isNil or button.isNil:
    return
  buttonCallbacks[cast[uint](button)] = cb

proc nimButtonBridge*(component: KryonComponent; event: ptr KryonEvent) {.cdecl.} =
  let key = cast[uint](component)
  if buttonCallbacks.hasKey(key):
    let cb = buttonCallbacks[key]
    if not cb.isNil:
      cb()

# ============================================================================
# Renderer and System Functions
# ============================================================================

proc initRenderer*(width, height: int; title: string): KryonRenderer =
  ## Initialize renderer with window properties

  let requestedRenderer = getEnv("KRYON_RENDERER", "")
  rendererPreinitializedHint = false

  # Use explicitly requested renderer, no fallbacks
  when defined(KRYON_TERMINAL):
    if requestedRenderer == "terminal" or defined(KRYON_TERMINAL):
        echo "Using terminal renderer backend"
        result = kryon_terminal_renderer_create()
        activeRendererBackend = "terminal"
        return

  if requestedRenderer == "framebuffer":
    echo "Using framebuffer renderer backend"
    let w = toUint16OrDefault(width, 800'u16)
    let h = toUint16OrDefault(height, 600'u16)
    result = kryon_framebuffer_renderer_create(w, h, 4)
    activeRendererBackend = "framebuffer"
    rendererPreinitializedHint = true
    return

  when defined(KRYON_SDL3):
    if requestedRenderer == "sdl3":
        echo "Using SDL3 renderer backend"
        let w = toUint16OrDefault(width, 800'u16)
        let h = toUint16OrDefault(height, 600'u16)
        result = kryon_sdl3_renderer_create(w, h, title)
        activeRendererBackend = "sdl3"
        rendererPreinitializedHint = false
        return

  # Default behavior when no explicit renderer requested
  if isHeadlessEnvironment():
    echo "No display detected; using framebuffer renderer backend"
    let w = toUint16OrDefault(width, 800'u16)
    let h = toUint16OrDefault(height, 600'u16)
    result = kryon_framebuffer_renderer_create(w, h, 4)
    activeRendererBackend = "framebuffer"
    rendererPreinitializedHint = true
    return

  when defined(KRYON_SDL3):
      echo "Using SDL3 renderer backend (default)"
      let w = toUint16OrDefault(width, 800'u16)
      let h = toUint16OrDefault(height, 600'u16)
      result = kryon_sdl3_renderer_create(w, h, title)
      activeRendererBackend = "sdl3"
      rendererPreinitializedHint = false
  else:
      echo "SDL3 not available; using framebuffer renderer backend"
      let w = toUint16OrDefault(width, 800'u16)
      let h = toUint16OrDefault(height, 600'u16)
      result = kryon_framebuffer_renderer_create(w, h, 4)
      activeRendererBackend = "framebuffer"
      rendererPreinitializedHint = true

# ============================================================================
# Component Tree Management
# ============================================================================

proc createComponent*(componentType: string, props: NimNode): KryonComponent =
  ## Generic component creation function used by DSL
  ## This function creates components based on type string and properties
  case componentType.toLowerAscii():
  of "container":
    result = newKryonContainer()
  of "text":
    # Extract text content from props if available
    var textContent = "Text"
    # Note: This is a simplified implementation
    # In a full implementation, we'd parse the props NimNode to extract properties
    result = newKryonText(textContent)
  of "button":
    # Extract button text from props if available
    var buttonText = "Button"
    result = newKryonButton(buttonText)
  of "checkbox":
    # Extract checkbox properties from props if available
    var labelText = ""
    var checked = false
    result = newKryonCheckbox(labelText, checked, nil)
  of "canvas":
    result = newKryonCanvas()
  of "markdown":
    result = newKryonContainer()  # Markdown will be handled by component properties
  of "spacer":
    result = newKryonSpacer()
  else:
    echo "Unknown component type: " & componentType
    result = newKryonContainer()  # Default to container

proc createComponentTree*(parent: KryonComponent; components: NimNode) =
  ## Create component tree from nested components
  # This would be used by the Container macro
  discard

proc addChild*(parent: KryonComponent; child: KryonComponent) =
  ## Add child component to parent
  discard kryon_component_add_child(parent, child)

# ============================================================================
# Color Parsing
# ============================================================================

proc parseColor*(color: string): uint32 =
  ## Parse color from hex string or CSS name
  if color.startsWith("#"):
    let hex = color[1..^1]
    if hex.len == 6:
      let r = parseHexInt(hex[0..1])
      let g = parseHexInt(hex[2..3])
      let b = parseHexInt(hex[4..5])
      return rgba(r, g, b, 255)
  elif color.toLowerAscii() == "red":
    return rgba(255, 0, 0, 255)
  elif color.toLowerAscii() == "green":
    return rgba(0, 255, 0, 255)
  elif color.toLowerAscii() == "blue":
    return rgba(0, 0, 255, 255)
  elif color.toLowerAscii() == "white":
    return rgba(255, 255, 255, 255)
  elif color.toLowerAscii() == "black":
    return rgba(0, 0, 0, 255)

  return rgba(128, 128, 128, 255)  # Default gray

# ============================================================================
# Canvas Utility Functions
# ============================================================================

# Fixed-point conversion functions are available in core_kryon.nim
# No need to duplicate them here to avoid naming conflicts

# ============================================================================
# Canvas Component Integration
# ============================================================================

proc createCanvasComponent*(width, height: int, onDraw: proc(ctx: DrawingContext),
                           backgroundColor: uint32 = 0x000000FF): KryonComponent =
  ## Create a canvas component with Love2D-style drawing
  # TODO: Implement proper canvas component creation once canvas API is available
  result = newKryonContainer()  # Use container as placeholder for now

  # Store the drawing callback in component state
  if not onDraw.isNil:
    # This would need to be stored in component state for later use
    # For now, we'll just set up a basic component
    discard

# ============================================================================
# Canvas Runtime Integration
# ============================================================================

proc initCanvasForApp*(app: KryonApp, width, height: int) =
  ## Initialize canvas system for an application
  kryonCanvasInit(uint16(width), uint16(height))

proc getCanvasCommandBuffer*(): KryonCmdBuf =
  ## Get the current canvas command buffer for low-level access
  # This would return the global command buffer from the canvas system
  # For now, create a new one
  result = createKryonCmdBuf()

proc executeCanvasCommands*(app: KryonApp) =
  ## Execute pending canvas commands
  # This would flush the canvas command buffer to the renderer
  discard

# ============================================================================
# Enhanced Drawing Context for Canvas Components
# ============================================================================

proc newDrawingContext*(width, height: int): DrawingContext =
  ## Create a new drawing context
  result = DrawingContext(width: width, height: height)

proc clearBackground*(ctx: DrawingContext, width, height: int, color: uint32) =
  ## Clear the drawing context background
  kryonCanvasClearColor(color)
