## Kryon Runtime - Nim<->C ABI Glue
##
## Provides the runtime bridge between Nim DSL and C core engine

import core_kryon, core_kryon_canvas, os, strutils, tables, math
import reactive_system

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
    focusedComponent*: KryonComponent

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

  TabVisualState* = object
    backgroundColor*: uint32
    activeBackgroundColor*: uint32
    textColor*: uint32
    activeTextColor*: uint32

  TabGroupState* = ref object
    group*: KryonComponent
    tabBar*: KryonComponent
    tabContent*: KryonComponent
    tabs*: seq[KryonComponent]
    tabVisuals*: seq[TabVisualState]
    panels*: seq[KryonComponent]
    tabBaseZIndices*: seq[uint16]
    dragPending*: bool
    dragPendingIndex*: int
    dragPendingStartX*: int16
    selectedIndex*: int
    reorderable*: bool
    dragging*: bool
    dragTabIndex*: int
    dragPointerOffset*: float
    dragTabOriginalLeft*: float
    dragTabOriginalTop*: float
    dragTabOriginalWidth*: float
    dragTabOriginalHeight*: float
    dragTabBarLeft*: float
    dragTabBarTop*: float
    dragPointerX*: int16
    dragTabOriginalZIndex*: uint16
    dragOriginalLayoutFlags*: uint8
    dragTabHasManualBounds*: bool
    dragHadMovement*: bool
    dragLastReorderPointer*: int16
    suppressClick*: bool

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
var tabGroupRegistry*: Table[uint, TabGroupState] = initTable[uint, TabGroupState]()
var tabComponentRegistry*: Table[uint, tuple[state: TabGroupState, index: int]] = initTable[uint, tuple[state: TabGroupState, index: int]]()
var tabBarRegistry*: Table[uint, TabGroupState] = initTable[uint, TabGroupState]()
var lastPointerX: int16 = 0
var lastPointerY: int16 = 0

proc refreshActiveTabDrags()

proc relayoutTabBar(state: TabGroupState) =
  if state.isNil or state.tabBar.isNil:
    return
  var absX, absY, widthFp, heightFp: KryonFp
  kryon_component_get_absolute_bounds(state.tabBar, addr absX, addr absY, addr widthFp, addr heightFp)
  kryon_layout_component(state.tabBar, widthFp, heightFp)
  kryon_component_mark_dirty(state.tabBar)

when defined(KRYON_SDL3):
  const
    cursorArrow = 0'u8
    cursorHand = 1'u8
  var currentCursorShape: uint8 = cursorArrow

proc findTabAtPoint(state: TabGroupState; pointerX, pointerY: int16): int
proc reorderTabs(state: TabGroupState; fromIdx, toIdx: int)

proc updateCursorShape(target: KryonComponent) =
  when defined(KRYON_SDL3):
    if activeRendererBackend == "sdl3":
      var actualTarget = target
      if not actualTarget.isNil:
        let key = cast[uint](actualTarget)
        if tabGroupRegistry.hasKey(key):
          let state = tabGroupRegistry[key]
          let idx = findTabAtPoint(state, lastPointerX, lastPointerY)
          if idx >= 0 and idx < state.tabs.len:
            actualTarget = state.tabs[idx]
      let desired = if not actualTarget.isNil and kryon_component_is_button(actualTarget): cursorHand else: cursorArrow
      if desired != currentCursorShape:
        kryon_sdl3_apply_cursor_shape(desired)
        currentCursorShape = desired

when defined(KRYON_SDL3):
  proc dispatchPointerEvent(app: KryonApp; evt: var KryonEvent) =
    if app.root.isNil:
      runtimeTrace("[kryon][runtime] dispatchPointerEvent: root is nil!")
      return
    runtimeTrace("[kryon][runtime] dispatchPointerEvent: dispatching to root at (" & $evt.x & "," & $evt.y & ")")
    kryon_event_dispatch_to_point(app.root, evt.x, evt.y, addr evt)
    runtimeTrace("[kryon][runtime] dispatchPointerEvent: dispatch complete")

  proc updateHoverState(app: KryonApp; evt: var KryonEvent) =
    if app.root.isNil:
      return
    lastPointerX = evt.x
    lastPointerY = evt.y

    let target = kryon_event_find_target_at_point(app.root, evt.x, evt.y)

    # Handle target changes (send leave/enter events)
    if target != currentHoverTarget:
      if currentHoverTarget != nil:
        var leaveEvent = KryonEvent(`type`: KryonEventType.Hover, x: evt.x, y: evt.y, param: 0'u32)
        kryon_component_send_event(currentHoverTarget, addr leaveEvent)

      currentHoverTarget = target

      if target != nil:
        var enterEvent = KryonEvent(`type`: KryonEventType.Hover, x: evt.x, y: evt.y, param: 1'u32)
        kryon_component_send_event(target, addr enterEvent)

    # Always send hover event to current target (for components like dropdown that need continuous updates)
    if target != nil:
      var hoverEvent = KryonEvent(`type`: KryonEventType.Hover, x: evt.x, y: evt.y, param: 1'u32)
      kryon_component_send_event(target, addr hoverEvent)

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

# Forward declare canvas callback function
proc executeAllCanvasCallbacks*(root: KryonComponent)

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

  # Initialize reactive system
  if not isReactiveSystemInitialized():
    initReactiveSystem()

  # Initialize canvas system
  kryonCanvasInit(uint16(app.window.width), uint16(app.window.height))

  # Initialize the renderer
  var rendererReady = true
  if not app.rendererPreinitialized:
    rendererReady = kryon_renderer_init(app.renderer, nil)
    if rendererReady:
      # Set global renderer for accurate text measurement
      kryon_set_global_renderer(app.renderer)
  else:
    app.rendererPreinitialized = false
    # Set global renderer even if pre-initialized
    kryon_set_global_renderer(app.renderer)
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
            kryon_set_global_renderer(app.renderer)

    if not rendererReady:
      when defined(KRYON_SDL3):
        if activeRendererBackend == "sdl3" and app.renderer != nil:
          kryon_sdl3_renderer_destroy(app.renderer)
          app.renderer = nil
      else:
        if activeRendererBackend == "sdl3" and app.renderer != nil:
          app.renderer = nil

      echo "ERROR: SDL3 renderer initialization failed - no fallback available"
      echo "This application requires SDL3 to be properly installed and configured"
      quit(1)

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
      refreshActiveTabDrags()
      runtimeTrace("[kryon][runtime] layout complete")

    # Process reactive updates after layout but before rendering
    processReactiveUpdates()
    runtimeTrace("[kryon][runtime] reactive updates complete")

    when defined(KRYON_SDL3):
      if activeRendererBackend == "sdl3":
        var evt: KryonEvent
        while kryon_sdl3_poll_event(addr evt):
          runtimeTrace("[kryon][runtime] SDL3 event received: type=" & $evt.`type` & " x=" & $evt.x & " y=" & $evt.y)
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
            
            # Dispatch to focused component
            if app.focusedComponent != nil:
              kryon_component_send_event(app.focusedComponent, addr evt)

          of KryonEventType.Click:
            runtimeTrace("[kryon][runtime] Dispatching click event to components")
            
            # Update focus
            let target = kryon_event_find_target_at_point(app.root, evt.x, evt.y)
            app.focusedComponent = target
            
            dispatchPointerEvent(app, evt)
          of KryonEventType.Hover:
            updateHoverState(app, evt)
          of KryonEventType.Scroll:
            # Dispatch scroll event to component at mouse position
            dispatchPointerEvent(app, evt)
          else:
            discard

        if not app.running:
          break

    # Execute canvas drawing callbacks before rendering
    if app.root != nil:
      executeAllCanvasCallbacks(app.root)
      runtimeTrace("[kryon][runtime] canvas drawing complete")

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
# Font Management
# ============================================================================

# Global registry for loaded fonts (name -> font_id)
var loadedFonts*: Table[string, uint16] = initTable[string, uint16]()

proc addFontSearchDir*(path: string) =
  ## Add a directory to the font search path
  ## This allows fonts to be loaded from custom locations
  when defined(KRYON_SDL3):
    kryon_sdl3_add_font_search_path(cstring(path))
    runtimeTrace("[kryon][fonts] Added font search directory: " & path)

proc loadFont*(name: string, size: uint16 = 16): uint16 =
  ## Load a font by name and size
  ## Returns the font ID, or 0 if loading failed
  when defined(KRYON_SDL3):
    let fontId = kryon_sdl3_load_font(cstring(name), size)
    if fontId > 0:
      let key = name & "@" & $size
      loadedFonts[key] = fontId
      runtimeTrace("[kryon][fonts] Loaded font: " & name & " at size " & $size & " -> ID " & $fontId)
    else:
      runtimeTrace("[kryon][fonts] Failed to load font: " & name)
    return fontId
  else:
    return 0

proc getFontId*(name: string, size: uint16 = 16): uint16 =
  ## Get the font ID for a loaded font
  ## Returns 0 if font not loaded
  let key = name & "@" & $size
  if loadedFonts.hasKey(key):
    return loadedFonts[key]
  else:
    # Try to load it
    return loadFont(name, size)

# ============================================================================
# Missing Component Functions
# ============================================================================

var canvasCallbacks*: Table[uint, proc () {.closure.}] = initTable[uint, proc () {.closure.}]()

proc canvasDrawBridge(component: KryonComponent; buf: KryonCmdBuf): bool {.cdecl.} =
  ## C callback bridge for canvas drawing
  let key = cast[uint](component)
  if canvasCallbacks.hasKey(key):
    let cb = canvasCallbacks[key]
    if not cb.isNil:
      runtimeTrace("[kryon][canvas] Executing canvas draw callback for component " & $key)
      cb()
      return true
  return false

proc registerCanvasHandler*(canvas: KryonComponent; cb: proc () {.closure.}) =
  if cb.isNil or canvas.isNil:
    return
  let key = cast[uint](canvas)
  canvasCallbacks[key] = cb
  runtimeTrace("[kryon][runtime] Registered canvas onDraw handler for component " & $key)

  # Set the C-side callback
  kryon_canvas_set_draw_callback(canvas, canvasDrawBridge)

proc executeCanvasDrawing*(canvas: KryonComponent) =
  ## Execute the canvas drawing callback and render to the canvas component
  let key = cast[uint](canvas)
  if canvasCallbacks.hasKey(key):
    let cb = canvasCallbacks[key]
    if not cb.isNil:
      
      # Execute the user's drawing code
      cb()

      # Get the canvas command buffer with all drawing commands
      let cmdBuf = kryonCanvasGetCommandBuffer()
      if not cmdBuf.isNil:
        # The canvas commands need to be rendered to the component
        # For now, the commands will be executed during the next render pass
        runtimeTrace("[kryon][canvas] Canvas drawing executed, commands ready")

proc executeAllCanvasCallbacks*(root: KryonComponent) =
  ## Walk the component tree and execute all canvas drawing callbacks
  if root.isNil:
    return

  # Check if this component is a canvas with a callback
  # echo "[kryon][runtime] Checking component for canvas callback: ", cast[uint](root)  # Disabled - too verbose
  executeCanvasDrawing(root)

  # Recursively process children
  let childCount = int(kryon_component_get_child_count(root))
  for i in 0..<childCount:
    let child = kryon_component_get_child(root, uint8(i))
    if not child.isNil:
      executeAllCanvasCallbacks(child)

proc newKryonCanvas*(): KryonComponent =
  ## Create a new canvas component
  result = newKryonContainer()
  # Set canvas-specific ops for rendering
  kryon_component_set_canvas_ops(result)

proc newKryonSpacer*(): KryonComponent =
  ## Create a new spacer component
  result = newKryonContainer()  # For now, use container as base

var buttonCallbacks*: Table[uint, proc () {.closure.}] = initTable[uint, proc () {.closure.}]()

proc registerButtonHandler*(button: KryonComponent; cb: proc () {.closure.}) =
  if cb.isNil or button.isNil:
    return
  let key = cast[uint](button)
  buttonCallbacks[key] = cb
  echo "[kryon][runtime] Registered button handler for component: ", key

proc nimButtonBridge*(component: KryonComponent; event: ptr KryonEvent) {.cdecl.} =
  runtimeTrace("[kryon][runtime] Button clicked! Component: " & $cast[uint](component))
  let key = cast[uint](component)
  if buttonCallbacks.hasKey(key):
    runtimeTrace("[kryon][runtime] Found handler for button")
    let cb = buttonCallbacks[key]
    if not cb.isNil:
      runtimeTrace("[kryon][runtime] Calling handler")
      cb()
      runtimeTrace("[kryon][runtime] Handler completed")
    else:
      runtimeTrace("[kryon][runtime] Handler is nil!")
  else:
    runtimeTrace("[kryon][runtime] No handler registered for this button")

# ============================================================================
# Tab Group System
# ============================================================================
when defined(KRYON_SDL3):
  proc pointerPrimaryDown(): bool =
    kryon_sdl3_is_mouse_button_down(uint8(1))
else:
  proc pointerPrimaryDown(): bool = false

proc refreshTabComponentIndices(state: TabGroupState) =
  if state.isNil:
    return
  for idx, tab in state.tabs:
    if tab.isNil:
      continue
    tabComponentRegistry[cast[uint](tab)] = (state: state, index: idx)

proc reorderChildren(parent: KryonComponent; children: seq[KryonComponent]) =
  if parent.isNil:
    return
  for child in children:
    if not child.isNil:
      kryon_component_remove_child(parent, child)
  for child in children:
    if not child.isNil:
      discard kryon_component_add_child(parent, child)


proc clampIndex(value, maxVal: int): int =
  if maxVal < 0:
    return 0
  if value < 0:
    return 0
  if value > maxVal:
    return maxVal
  value

proc applyTabSelection(state: TabGroupState) =
  if state.isNil or state.tabs.len == 0:
    return
  state.selectedIndex = clampIndex(state.selectedIndex, state.tabs.len - 1)
  for idx, tab in state.tabs:
    if tab.isNil: continue
    if idx < state.tabVisuals.len:
      let visual = state.tabVisuals[idx]
      let isActive = idx == state.selectedIndex
      let bgColor = if isActive: visual.activeBackgroundColor else: visual.backgroundColor
      let textColor = if isActive: visual.activeTextColor else: visual.textColor
      kryon_component_set_background_color(tab, bgColor)
      kryon_component_set_text_color(tab, textColor)
      kryon_component_mark_dirty(tab)
    if idx < state.panels.len:
      let panel = state.panels[idx]
      if not panel.isNil:
        let active = idx == state.selectedIndex
        kryon_component_set_visible(panel, active)
        if active:
          kryon_component_mark_dirty(panel)


proc setActiveTab*(state: TabGroupState; index: int) =
  if state.isNil:
    return
  state.selectedIndex = clampIndex(index, state.tabs.len - 1)
  applyTabSelection(state)

const
  tabDragMaskBits = (0x01'u8 or 0x02'u8)
  tabReorderThreshold = 24.0
  tabDragStartThreshold = 6.0

proc computeTargetIndex(state: TabGroupState; pointerX: int16): int =
  if state.tabs.len == 0:
    return -1
  let px = float(pointerX)
  var fallback = state.tabs.len - 1

  if getEnv("KRYON_TRACE_TABS", "") != "":
    echo "[kryon][tabs] computeTargetIndex: px=", px, " dragTabIndex=", state.dragTabIndex

  for idx, tab in state.tabs:
    if tab.isNil: continue
    var absX, absY, width, height: KryonFp
    kryon_component_get_absolute_bounds(tab, addr absX, addr absY, addr width, addr height)
    let left = fromKryonFp(absX)
    let right = left + fromKryonFp(width)
    let midpoint = left + (right - left) / 2

    if getEnv("KRYON_TRACE_TABS", "") != "":
      echo "[kryon][tabs] tab#", idx, " bounds=(", left, "-", right, ") midpoint=", midpoint

    # Skip the tab being dragged to avoid self-targeting
    if idx == state.dragTabIndex:
      if getEnv("KRYON_TRACE_TABS", "") != "":
        echo "[kryon][tabs] skipping dragged tab at index ", idx
      continue

    if px < midpoint:
      if getEnv("KRYON_TRACE_TABS", "") != "":
        echo "[kryon][tabs] returning target index ", idx
      return idx
    fallback = idx

  if getEnv("KRYON_TRACE_TABS", "") != "":
    echo "[kryon][tabs] returning fallback index ", fallback
  fallback

proc clampDragLeft(state: TabGroupState; desiredLeft: float): float =
  ## Keep dragged tab within its tab bar bounds.
  result = desiredLeft
  if state.isNil or state.tabBar.isNil:
    return
  var absX, absY, width, height: KryonFp
  kryon_component_get_absolute_bounds(state.tabBar, addr absX, addr absY, addr width, addr height)
  let barLeft = fromKryonFp(absX)
  let barWidth = fromKryonFp(width)
  if barWidth <= 0 or state.dragTabOriginalWidth <= 0:
    return
  let barRight = barLeft + barWidth
  var minLeft = barLeft
  var maxLeft = barRight - state.dragTabOriginalWidth
  if maxLeft < minLeft:
    maxLeft = minLeft
  if result < minLeft:
    result = minLeft
  if result > maxLeft:
    result = maxLeft

proc applyTabDragVisual(state: TabGroupState; pointerX: int16; isRefresh = false) =
  ## Position the active tab directly under the cursor along the X axis.
  if state.isNil:
    return
  if state.dragTabIndex < 0 or state.dragTabIndex >= state.tabs.len:
    return
  let tab = state.tabs[state.dragTabIndex]
  if tab.isNil:
    return
  if not isRefresh and not state.dragging:
    return
  var desiredLeft = float(pointerX) - state.dragPointerOffset
  desiredLeft = clampDragLeft(state, desiredLeft)
  let relativeLeft = desiredLeft - state.dragTabBarLeft
  let relativeTop = state.dragTabOriginalTop - state.dragTabBarTop
  let dragMask = state.dragOriginalLayoutFlags or tabDragMaskBits
  kryon_component_set_bounds_mask(
    tab,
    toFixed(relativeLeft),
    toFixed(relativeTop),
    toFixed(state.dragTabOriginalWidth),
    toFixed(state.dragTabOriginalHeight),
    dragMask
  )
  state.dragTabHasManualBounds = true
  state.dragPointerX = pointerX
  if not isRefresh:
    state.dragHadMovement = true
  kryon_component_mark_dirty(tab)

proc maybeReorderTabs(state: TabGroupState; pointerX: int16) =
  if state.isNil or not state.dragging or not state.reorderable:
    return
  if state.tabs.len == 0:
    return
  let currentLeft = float(pointerX) - state.dragPointerOffset
  let currentRight = currentLeft + state.dragTabOriginalWidth
  let displacement = currentLeft - state.dragTabOriginalLeft
  if abs(displacement) < tabReorderThreshold:
    return
  let target = computeTargetIndex(state, pointerX)
  if target < 0 or target >= state.tabs.len or target == state.dragTabIndex:
    return

  # Increased dead zone to prevent rapid flipping when dragging to end positions
  if abs(float(pointerX - state.dragLastReorderPointer)) < tabReorderThreshold:
    return

  # Prevent immediate reversal - only allow reordering if moving consistently in one direction
  let pointerDiff = float(pointerX) - float(state.dragLastReorderPointer)
  if (target > state.dragTabIndex and pointerDiff < 0) or (target < state.dragTabIndex and pointerDiff > 0):
    if getEnv("KRYON_TRACE_TABS", "") != "":
      echo "[kryon][tabs] blocking reversal: target=", target, " dragTabIndex=", state.dragTabIndex, " pointerDiff=", pointerDiff
    return
  if target > state.dragTabIndex:
    let neighbor = state.tabs[target]
    if neighbor.isNil:
      return
    var nAbsX, nAbsY, nW, nH: KryonFp
    kryon_component_get_absolute_bounds(neighbor, addr nAbsX, addr nAbsY, addr nW, addr nH)
    let neighborLeft = fromKryonFp(nAbsX)
    if currentRight < neighborLeft + tabDragStartThreshold:
      return
  elif target < state.dragTabIndex:
    let neighbor = state.tabs[target]
    if neighbor.isNil:
      return
    var nAbsX, nAbsY, nW, nH: KryonFp
    kryon_component_get_absolute_bounds(neighbor, addr nAbsX, addr nAbsY, addr nW, addr nH)
    let neighborRight = fromKryonFp(nAbsX) + fromKryonFp(nW)
    if currentLeft > neighborRight - tabDragStartThreshold:
      return
  reorderTabs(state, state.dragTabIndex, target)
  state.dragTabIndex = target
  state.dragTabOriginalLeft = currentLeft
  state.dragLastReorderPointer = pointerX
  applyTabDragVisual(state, pointerX, true)

proc startTabDrag(state: TabGroupState; index: int; pointerX: int16) =
  ## Capture initial bounds for chrome-style dragging.
  if state.isNil or index < 0 or index >= state.tabs.len:
    return
  let tab = state.tabs[index]
  if tab.isNil:
    return
  var absX, absY, width, height: KryonFp
  kryon_component_get_absolute_bounds(tab, addr absX, addr absY, addr width, addr height)
  if not state.tabBar.isNil:
    var barAbsX, barAbsY, barWidth, barHeight: KryonFp
    kryon_component_get_absolute_bounds(state.tabBar, addr barAbsX, addr barAbsY, addr barWidth, addr barHeight)
    state.dragTabBarLeft = fromKryonFp(barAbsX)
    state.dragTabBarTop = fromKryonFp(barAbsY)
  else:
    state.dragTabBarLeft = 0.0
    state.dragTabBarTop = 0.0
  let left = fromKryonFp(absX)
  state.dragPointerOffset = float(pointerX) - left
  state.dragTabOriginalLeft = left
  state.dragTabOriginalTop = fromKryonFp(absY)
  state.dragTabOriginalWidth = fromKryonFp(width)
  state.dragTabOriginalHeight = fromKryonFp(height)
  state.dragTabIndex = clampIndex(index, state.tabs.len - 1)
  state.dragging = true
  state.dragTabHasManualBounds = false
  state.dragHadMovement = false
  state.dragPointerX = pointerX
  state.dragOriginalLayoutFlags = kryon_component_get_layout_flags(tab)
  state.dragPending = false
  state.dragPendingIndex = -1
  state.suppressClick = false
  state.dragLastReorderPointer = pointerX
  setActiveTab(state, state.dragTabIndex)
  if state.dragTabIndex >= 0 and state.dragTabIndex < state.tabBaseZIndices.len:
    state.dragTabOriginalZIndex = state.tabBaseZIndices[state.dragTabIndex]
  else:
    state.dragTabOriginalZIndex = 0'u16
  kryon_component_set_z_index(tab, 60000'u16)
  applyTabDragVisual(state, pointerX)

proc resetTabDragVisual(state: TabGroupState) =
  ## Give control back to the flex layout once dragging stops.
  if state.isNil or not state.dragTabHasManualBounds:
    return
  if state.dragTabIndex < 0 or state.dragTabIndex >= state.tabs.len:
    return
  let tab = state.tabs[state.dragTabIndex]
  if tab.isNil:
    return
  let relativeLeft = state.dragTabOriginalLeft - state.dragTabBarLeft
  let relativeTop = state.dragTabOriginalTop - state.dragTabBarTop
  kryon_component_set_bounds_mask(
    tab,
    toFixed(relativeLeft),
    toFixed(relativeTop),
    toFixed(state.dragTabOriginalWidth),
    toFixed(state.dragTabOriginalHeight),
    state.dragOriginalLayoutFlags
  )
  state.dragTabHasManualBounds = false
  kryon_component_mark_dirty(tab)
  kryon_component_set_z_index(tab, state.dragTabOriginalZIndex)
  if state.dragTabIndex >= 0 and state.dragTabIndex < state.tabBaseZIndices.len:
    state.tabBaseZIndices[state.dragTabIndex] = state.dragTabOriginalZIndex

proc pointInsideTabBar(state: TabGroupState; pointerX, pointerY: int16): bool =
  ## True when the pointer is within the tab bar's bounding box.
  if state.isNil or state.tabBar.isNil:
    return false
  let px = float(pointerX)
  let py = float(pointerY)
  var absX, absY, width, height: KryonFp
  kryon_component_get_absolute_bounds(state.tabBar, addr absX, addr absY, addr width, addr height)
  let left = fromKryonFp(absX)
  let top = fromKryonFp(absY)
  let right = left + fromKryonFp(width)
  let bottom = top + fromKryonFp(height)
  result = px >= left and px <= right and py >= top and py <= bottom

proc findTabAtPoint(state: TabGroupState; pointerX, pointerY: int16): int =
  if state.tabs.len == 0:
    return -1
  let px = float(pointerX)
  let py = float(pointerY)
  for idx, tab in state.tabs:
    if tab.isNil:
      continue
    var absX, absY, width, height: KryonFp
    kryon_component_get_absolute_bounds(tab, addr absX, addr absY, addr width, addr height)
    let left = fromKryonFp(absX)
    let top = fromKryonFp(absY)
    let right = left + fromKryonFp(width)
    let bottom = top + fromKryonFp(height)
    if px >= left and px <= right and py >= top and py <= bottom:
      return idx
  return -1

proc reorderTabs(state: TabGroupState; fromIdx, toIdx: int) =
  if state.isNil or state.tabs.len == 0:
    return
  let target = clampIndex(toIdx, state.tabs.len - 1)
  if fromIdx == target or fromIdx < 0 or fromIdx >= state.tabs.len:
    return
  let tabComp = state.tabs[fromIdx]
  state.tabs.delete(fromIdx)
  state.tabs.insert(tabComp, target)

  if fromIdx < state.tabVisuals.len:
    let visual = state.tabVisuals[fromIdx]
    state.tabVisuals.delete(fromIdx)
    state.tabVisuals.insert(visual, target)

  if fromIdx < state.panels.len:
    let panel = state.panels[fromIdx]
    state.panels.delete(fromIdx)
    if target <= state.panels.len:
      state.panels.insert(panel, target)
    else:
      state.panels.add(panel)
  if fromIdx < state.tabBaseZIndices.len:
    let baseZ = state.tabBaseZIndices[fromIdx]
    state.tabBaseZIndices.delete(fromIdx)
    state.tabBaseZIndices.insert(baseZ, target)

  if state.selectedIndex == fromIdx:
    state.selectedIndex = target
  elif state.selectedIndex > fromIdx and state.selectedIndex <= target:
    state.selectedIndex -= 1
  elif state.selectedIndex < fromIdx and state.selectedIndex >= target:
    state.selectedIndex += 1

  reorderChildren(state.tabBar, state.tabs)
  reorderChildren(state.tabContent, state.panels)
  refreshTabComponentIndices(state)
  applyTabSelection(state)

proc stopTabDrag(state: TabGroupState) =
  if state.isNil: return
  let hadMovement = state.dragHadMovement or state.dragTabHasManualBounds or state.dragging
  resetTabDragVisual(state)
  state.dragging = false
  state.dragTabIndex = -1
  state.dragPointerOffset = 0.0
  state.dragHadMovement = false
  state.dragTabOriginalZIndex = 0'u16
  state.dragOriginalLayoutFlags = 0'u8
  state.dragPending = false
  state.dragPendingIndex = -1
  state.dragPendingStartX = 0'i16
  state.dragLastReorderPointer = 0'i16
  state.suppressClick = hadMovement
  relayoutTabBar(state)

proc updateTabDrag(state: TabGroupState; pointerX: int16) =
  applyTabDragVisual(state, pointerX)
  maybeReorderTabs(state, pointerX)

proc refreshActiveTabDrags() =
  for _, state in tabGroupRegistry:
    if state.isNil: continue
    if state.dragging and not pointerPrimaryDown():
      stopTabDrag(state)
      continue
    if state.dragTabHasManualBounds and state.dragTabIndex >= 0 and state.dragTabIndex < state.tabs.len:
      applyTabDragVisual(state, state.dragPointerX, true)

proc handleTabHover(state: TabGroupState; index: int; event: ptr KryonEvent) =
  if state.isNil or event.isNil or not state.reorderable:
    return
  if pointerPrimaryDown():
    let targetIndex = clampIndex(index, state.tabs.len - 1)
    if not state.dragging:
      if not state.dragPending or state.dragPendingIndex != targetIndex:
        state.dragPending = true
        state.dragPendingIndex = targetIndex
        state.dragPendingStartX = event.x
      let delta = abs(float(event.x - state.dragPendingStartX))
      if delta >= tabDragStartThreshold:
        startTabDrag(state, targetIndex, event.x)
    if state.dragging:
      updateTabDrag(state, event.x)
  else:
    if state.dragging:
      stopTabDrag(state)
    state.dragPending = false
    state.dragPendingIndex = -1
    state.dragPendingStartX = 0'i16

proc tabComponentEventHandler(component: KryonComponent; event: ptr KryonEvent) {.cdecl.} =
  if component.isNil or event.isNil:
    return
  let key = cast[uint](component)
  if not tabComponentRegistry.hasKey(key):
    return
  let entry = tabComponentRegistry[key]
  let state = entry.state
  if state.isNil:
    return
  case event.`type`
  of KryonEventType.Click:
    if state.suppressClick:
      state.suppressClick = false
      return
    if state.reorderable and (state.dragging or state.dragTabHasManualBounds or state.dragHadMovement):
      return  # Let the tab group handler finalize the drag
    if state.reorderable:
      stopTabDrag(state)
    setActiveTab(state, entry.index)
  of KryonEventType.Hover:
    handleTabHover(state, entry.index, event)
  else:
    discard

proc tabBarEventHandler(component: KryonComponent; event: ptr KryonEvent) {.cdecl.} =
  if component.isNil or event.isNil:
    return
  let state = tabBarRegistry.getOrDefault(cast[uint](component))
  if state.isNil or not state.reorderable:
    return
  case event.`type`
  of KryonEventType.Hover:
    if state.dragging and pointerPrimaryDown():
      updateTabDrag(state, event.x)
  of KryonEventType.Click:
    if state.suppressClick:
      state.suppressClick = false
      return
    if not state.dragging and not state.dragTabHasManualBounds and not state.dragHadMovement:
      stopTabDrag(state)
  else:
    discard

proc tabGroupEventHandler(component: KryonComponent; event: ptr KryonEvent) {.cdecl.} =
  if component.isNil or event.isNil:
    return
  let state = tabGroupRegistry.getOrDefault(cast[uint](component))
  if state.isNil:
    return
  case event.`type`
  of KryonEventType.Click:
    if state.suppressClick:
      state.suppressClick = false
      return
    if state.reorderable and (state.dragging or state.dragTabHasManualBounds or state.dragHadMovement):
      stopTabDrag(state)
      return
    let directIdx = findTabAtPoint(state, event.x, event.y)
    var idx = directIdx
    if idx < 0 and pointInsideTabBar(state, event.x, event.y):
      idx = computeTargetIndex(state, event.x)
    if getEnv("KRYON_TRACE_TABS", "") != "":
      runtimeTrace("[kryon][tabs] click at " & $event.x & "," & $event.y &
           " direct=" & $directIdx & " target idx=" & $idx)
    if idx >= 0:
      setActiveTab(state, idx)
    stopTabDrag(state)
  of KryonEventType.Hover:
    if state.reorderable and state.dragging and pointerPrimaryDown():
      updateTabDrag(state, event.x)
  else:
    discard

proc createTabGroupState*(component: KryonComponent; selectedIndex: int): TabGroupState =
  if component.isNil:
    return nil
  result = TabGroupState(
    group: component,
    selectedIndex: if selectedIndex >= 0: selectedIndex else: 0,
    tabs: @[],
    tabVisuals: @[],
    panels: @[],
    tabBaseZIndices: @[],
    reorderable: false,
    dragging: false,
    dragTabIndex: -1,
    dragPointerOffset: 0.0,
    dragTabOriginalLeft: 0.0,
    dragTabOriginalTop: 0.0,
    dragTabOriginalWidth: 0.0,
    dragTabOriginalHeight: 0.0,
    dragTabBarLeft: 0.0,
    dragTabBarTop: 0.0,
    dragPointerX: 0'i16,
    dragTabOriginalZIndex: 0'u16,
    dragOriginalLayoutFlags: 0'u8,
    dragTabHasManualBounds: false,
    dragHadMovement: false,
    dragLastReorderPointer: 0'i16,
    suppressClick: false
  )
  let key = cast[uint](component)
  tabGroupRegistry[key] = result
  discard kryon_component_add_event_handler(component, tabGroupEventHandler)

proc registerTabBar*(state: TabGroupState; tabBar: KryonComponent; reorderable: bool) =
  if state.isNil or tabBar.isNil:
    return
  state.tabBar = tabBar
  state.reorderable = reorderable
  tabBarRegistry[cast[uint](tabBar)] = state
  discard kryon_component_add_event_handler(tabBar, tabBarEventHandler)

proc registerTabContent*(state: TabGroupState; content: KryonComponent) =
  if state.isNil or content.isNil:
    return
  state.tabContent = content

proc registerTabComponent*(state: TabGroupState; component: KryonComponent; visuals: TabVisualState) =
  if state.isNil or component.isNil:
    return
  state.tabs.add(component)
  state.tabVisuals.add(visuals)
  state.tabBaseZIndices.add(kryon_component_get_z_index(component))
  tabComponentRegistry[cast[uint](component)] = (state: state, index: state.tabs.len - 1)
  discard kryon_component_add_event_handler(component, tabComponentEventHandler)

  # CRITICAL: Add the tab component as a child to the TabBar
  if state.tabBar != nil:
    discard kryon_component_add_child(state.tabBar, component)
    echo "[kryon][tabs] Added tab component to TabBar: ", cast[uint](component), " -> TabBar: ", cast[uint](state.tabBar)
  else:
    echo "[kryon][tabs] Warning: TabBar is nil, cannot add tab as child"

  kryon_component_set_background_color(component, visuals.backgroundColor)
  kryon_component_set_text_color(component, visuals.textColor)
  kryon_component_mark_dirty(component)

proc registerTabPanel*(state: TabGroupState; panel: KryonComponent) =
  if state.isNil or panel.isNil:
    return
  state.panels.add(panel)
  kryon_component_set_visible(panel, false)

proc finalizeTabGroup*(state: TabGroupState) =
  if state.isNil:
    return
  if getEnv("KRYON_TRACE_TABS", "") != "":
    var absX, absY, w, h: KryonFp
    if state.tabBar != nil:
      kryon_component_get_absolute_bounds(state.tabBar, addr absX, addr absY, addr w, addr h)
    echo "[kryon][tabs] finalize group ", cast[uint](state.group),
         " tabs=", state.tabs.len, " panels=", state.panels.len,
         " tabBar=(" & $fromKryonFp(absX) & "," & $fromKryonFp(absY) & "," &
         $fromKryonFp(w) & "," & $fromKryonFp(h) & ")"
  refreshTabComponentIndices(state)
  applyTabSelection(state)

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
    echo "ERROR: Framebuffer renderer not supported - this application requires SDL3"
    echo "Please run without explicit renderer selection to use SDL3"
    quit(1)

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
    echo "ERROR: No display detected - this application requires SDL3 with a display"
    echo "Cannot run in headless environment without framebuffer fallback"
    quit(1)

  when defined(KRYON_SDL3):
      echo "Using SDL3 renderer backend (default)"
      let w = toUint16OrDefault(width, 800'u16)
      let h = toUint16OrDefault(height, 600'u16)
      result = kryon_sdl3_renderer_create(w, h, title)
      activeRendererBackend = "sdl3"
      rendererPreinitializedHint = false
  else:
      echo "ERROR: SDL3 not available - this application requires SDL3"
      echo "Please compile with SDL3 support or install SDL3 development libraries"
      quit(1)

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

# ============================================================================
# Input Component Implementation (Nim-side)
# ============================================================================

type
  KryonInputState* = ref object
    text*: string
    placeholder*: string
    onTextChange*: proc(text: string)
    textColor*: uint32
    backgroundColor*: uint32
    borderColor*: uint32
    cursorPos*: int
    scrollOffset*: float
    isFocused*: bool

var inputRegistry = initTable[uint, KryonInputState]()
var kryon_input_ops: KryonComponentOps

proc input_render(component: KryonComponent; buf: KryonCmdBuf) {.cdecl.} =
  let key = cast[uint](component)
  if not inputRegistry.hasKey(key): return
  let state = inputRegistry[key]
  
  var x, y, w, h: KryonFp
  kryon_component_get_absolute_bounds(component, addr x, addr y, addr w, addr h)
  let ix = int16(fromKryonFp(x))
  let iy = int16(fromKryonFp(y))
  let iw = uint16(fromKryonFp(w))
  let ih = uint16(fromKryonFp(h))
  
  # Draw background
  let bgColor = if state.backgroundColor != 0: state.backgroundColor else: kryon_color_rgba(255, 255, 255, 255)
  discard kryon_draw_rect(buf, ix, iy, iw, ih, bgColor)
  
  # Draw border - 1px width
  let borderColor = if state.borderColor != 0: state.borderColor else: kryon_color_rgba(204, 204, 204, 255)
  discard kryon_draw_rect(buf, ix, iy, iw, 1, borderColor) # Top
  discard kryon_draw_rect(buf, ix, iy + int16(ih) - 1, iw, 1, borderColor) # Bottom
  discard kryon_draw_rect(buf, ix, iy, 1, ih, borderColor) # Left
  discard kryon_draw_rect(buf, ix + int16(iw) - 1, iy, 1, ih, borderColor) # Right
  
  const CHAR_WIDTH = 8.0
  let padding = 5.0
  let contentWidth = float(iw) - (padding * 2)
  
  # Draw text with scrolling
  let displayText = if state.text.len > 0: state.text else: state.placeholder
  var textColor = state.textColor
  if textColor == 0:
    if state.text.len > 0:
      textColor = kryon_color_rgba(0, 0, 0, 255)
    else:
      textColor = kryon_color_rgba(136, 136, 136, 255)
  
  # Calculate visible range
  # We assume monospace font for now (approx 8px width)
  let startIdx = int(state.scrollOffset / CHAR_WIDTH)
  let visibleChars = int(contentWidth / CHAR_WIDTH) + 2 # +2 for partial chars
  let endIdx = min(startIdx + visibleChars, displayText.len)
  
  if startIdx < displayText.len:
    let visibleText = displayText[startIdx ..< endIdx]
    let textX = float(ix) + padding + (float(startIdx) * CHAR_WIDTH) - state.scrollOffset
    
    # Simple clipping check (only draw if within bounds)
    if textX + (float(visibleText.len) * CHAR_WIDTH) > float(ix) and textX < float(ix) + float(iw):
      discard kryon_draw_text(buf, cstring(visibleText), int16(textX), iy + 10, 0, 14, 0, 0, textColor)

  # Draw Cursor
  if state.isFocused and state.text.len >= 0: # Draw cursor even if empty
    let cursorX = (float(state.cursorPos) * CHAR_WIDTH) - state.scrollOffset
    let drawCursorX = float(ix) + padding + cursorX
    
    # Only draw cursor if visible within content area
    if drawCursorX >= float(ix) and drawCursorX <= float(ix) + float(iw):
      let cursorColor = kryon_color_rgba(0, 0, 0, 255)
      discard kryon_draw_rect(buf, int16(drawCursorX), iy + 5, 1, ih - 10, cursorColor)

proc input_onEvent(component: KryonComponent; evt: ptr KryonEvent): bool {.cdecl.} =
  let key = cast[uint](component)
  if not inputRegistry.hasKey(key): return false
  let state = inputRegistry[key]
  
  # Constants for key handling (SDL3 Keycodes)
  const
    SDLK_RIGHT = 1073741903'u32
    SDLK_LEFT = 1073741904'u32
    SDLK_HOME = 1073741898'u32
    SDLK_END = 1073741901'u32
    SDLK_DELETE = 127'u32
    CHAR_WIDTH = 8.0 # Approx width per char
  
  # Focus handling
  if evt.`type` == KryonEventType.Focus:
    state.isFocused = true
    kryon_component_mark_dirty(component)
    return true
  elif evt.`type` == KryonEventType.Blur:
    state.isFocused = false
    kryon_component_mark_dirty(component)
    return true
  
  if evt.`type` == KryonEventType.Click:
    var widthPtr, heightPtr: KryonFp
    var xPtr, yPtr: KryonFp
    kryon_component_get_absolute_bounds(component, addr xPtr, addr yPtr, addr widthPtr, addr heightPtr)
    
    let absX = float(fromKryonFp(xPtr))
    let padding = 5.0
    let clickX = float(evt.x) - absX - padding + state.scrollOffset
    
    # Calculate index from click position (approx)
    var newPos = int(round(clickX / CHAR_WIDTH))
    if newPos < 0: newPos = 0
    if newPos > state.text.len: newPos = state.text.len
    
    state.cursorPos = newPos
    kryon_component_mark_dirty(component)
    return true

  if evt.`type` == KryonEventType.Key:
    let pressedVal = if evt.data.isNil: 0'u else: cast[uint](evt.data)
    if pressedVal == 1'u: # Key down
      let keyCode = evt.param
      var handled = false
      
      # Navigation
      if keyCode == SDLK_LEFT:
        if state.cursorPos > 0:
          state.cursorPos.dec
          handled = true
      elif keyCode == SDLK_RIGHT:
        if state.cursorPos < state.text.len:
          state.cursorPos.inc
          handled = true
      elif keyCode == SDLK_HOME:
        state.cursorPos = 0
        handled = true
      elif keyCode == SDLK_END:
        state.cursorPos = state.text.len
        handled = true
        
      # Editing
      elif keyCode == 8: # Backspace
        if state.cursorPos > 0 and state.text.len > 0:
          state.text.delete(state.cursorPos - 1, state.cursorPos - 1)
          state.cursorPos.dec
          if state.onTextChange != nil: state.onTextChange(state.text)
          handled = true
      elif keyCode == SDLK_DELETE: # Delete
        if state.cursorPos < state.text.len:
          state.text.delete(state.cursorPos, state.cursorPos)
          if state.onTextChange != nil: state.onTextChange(state.text)
          handled = true
      # Printable characters (32-126)
      elif keyCode >= 32 and keyCode <= 126:
        state.text.insert($char(keyCode), state.cursorPos)
        state.cursorPos.inc
        if state.onTextChange != nil: state.onTextChange(state.text)
        handled = true
        
      if handled:
        # Update scroll offset to keep cursor visible
        var widthPtr, heightPtr: KryonFp
        var xPtr, yPtr: KryonFp
        kryon_component_get_absolute_bounds(component, addr xPtr, addr yPtr, addr widthPtr, addr heightPtr)
        let width = float(fromKryonFp(widthPtr)) - 16.0 # Subtract padding
        
        let cursorX = float(state.cursorPos) * CHAR_WIDTH
        
        if cursorX < state.scrollOffset:
          state.scrollOffset = cursorX
        elif cursorX > state.scrollOffset + width:
          state.scrollOffset = cursorX - width
          
        kryon_component_mark_dirty(component)
        return true
        
  return false

proc input_destroy(component: KryonComponent) {.cdecl.} =
  let key = cast[uint](component)
  if inputRegistry.hasKey(key):
    inputRegistry.del(key)

proc newKryonInput*(
    placeholder: string = "";
    value: string = "";
    onTextChange: proc(text: string) = nil;
    textColor: uint32 = 0;
    backgroundColor: uint32 = 0;
    borderColor: uint32 = 0
  ): KryonComponent =
  # Initialize ops if needed
  if kryon_input_ops.render == nil:
    kryon_input_ops.render = input_render
    kryon_input_ops.onEvent = input_onEvent
    kryon_input_ops.destroy = input_destroy
    # Use container layout logic
    kryon_input_ops.layout = kryon_container_ops.layout
  
  result = kryon_component_create(addr kryon_input_ops, nil)
  
  let state = KryonInputState(
    text: value,
    placeholder: placeholder,
    onTextChange: onTextChange,
    textColor: textColor,
    backgroundColor: backgroundColor,
    borderColor: borderColor,
    cursorPos: value.len,
    scrollOffset: 0.0,
    isFocused: false
  )
  
  inputRegistry[cast[uint](result)] = state
