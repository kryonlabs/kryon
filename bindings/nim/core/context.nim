# 0BSD
## Kryon Context - Central State Management
## Replaces global state with context-based architecture
##
## Phase 1, Week 1: Foundation
## This module eliminates global state by providing a single KryonContext
## object that holds all framework state.

import ../ir_core
import font_manager, event_dispatcher, component_manager, reactive_engine
export font_manager, event_dispatcher, component_manager, reactive_engine

type
  # Application configuration
  KryonConfig* = object
    windowTitle*: string
    windowWidth*: int
    windowHeight*: int
    defaultFont*: string
    defaultFontSize*: float
    enableHotReload*: bool
    enableAnimations*: bool
    enablePlugins*: bool

  # Central context object - holds ALL framework state
  # This replaces all global variables (g_button_handlers, g_reactive_states, etc.)
  KryonContext* = ref object
    # IR tree context (C layer)
    irContext*: ptr IRContext

    # High-level subsystems (Nim layer)
    eventDispatcher*: EventDispatcher
    reactiveEngine*: ReactiveEngine
    componentManager*: ComponentManager
    fontManager*: FontManager

    # Application configuration
    config*: KryonConfig

    # Runtime state
    running*: bool
    frameCount*: uint64
    deltaTime*: float

# ============================================================================
# Context Creation and Lifecycle
# ============================================================================

proc defaultConfig*(): KryonConfig =
  ## Returns default configuration for Kryon applications
  result = KryonConfig(
    windowTitle: "Kryon Application",
    windowWidth: 800,
    windowHeight: 600,
    defaultFont: "Inter",
    defaultFontSize: 14.0,
    enableHotReload: true,
    enableAnimations: true,
    enablePlugins: true
  )

proc createKryonContext*(config: KryonConfig): KryonContext =
  ## Creates a new Kryon context with the given configuration
  ## This is the single entry point for creating framework state
  result = KryonContext(
    config: config,
    running: false,
    frameCount: 0,
    deltaTime: 0.0
  )

  # Create IR context (C layer)
  result.irContext = ir_create_context()
  ir_set_context(result.irContext)

  # Initialize subsystems
  result.eventDispatcher = createEventDispatcher()  # Fully implemented
  result.reactiveEngine = createReactiveEngine()  # Fully implemented - wraps IR manifest
  result.componentManager = createComponentManager(result.irContext, result.eventDispatcher)  # Fully implemented

  # Initialize font manager (fully implemented)
  result.fontManager = createFontManager()
  result.fontManager.discoverSystemFonts()

  # Preload common font sizes for default font
  result.fontManager.preloadFonts(@[config.defaultFont], @[config.defaultFontSize])

proc createKryonContext*(): KryonContext =
  ## Creates a new Kryon context with default configuration
  createKryonContext(defaultConfig())

proc destroyKryonContext*(ctx: KryonContext) =
  ## Cleans up all resources held by the context
  # Cleanup subsystems
  if not ctx.eventDispatcher.isNil:
    ctx.eventDispatcher.cleanupAll()

  if not ctx.fontManager.isNil:
    ctx.fontManager.cleanupFonts()

  if not ctx.reactiveEngine.isNil:
    ctx.reactiveEngine.destroyReactiveEngine()

  # Cleanup IR context
  if not ctx.irContext.isNil:
    ir_destroy_context(ctx.irContext)
    ctx.irContext = nil

  ctx.running = false

# ============================================================================
# Context Access Patterns
# ============================================================================

# Future: Thread-local storage for context (multi-threaded support)
# For Phase 1, we use a simple global variable that will be replaced
# with proper context passing in Phase 2-3

var g_currentContext {.threadvar.}: KryonContext

proc setCurrentContext*(ctx: KryonContext) =
  ## Sets the current context for this thread
  ## This is a transitional API - will be removed in Phase 3
  g_currentContext = ctx
  if not ctx.irContext.isNil:
    ir_set_context(ctx.irContext)

proc getCurrentContext*(): KryonContext =
  ## Gets the current context for this thread
  ## This is a transitional API - will be removed in Phase 3
  result = g_currentContext

# ============================================================================
# Utility Functions
# ============================================================================

proc isInitialized*(ctx: KryonContext): bool =
  ## Checks if the context is properly initialized
  result = not ctx.isNil and not ctx.irContext.isNil

proc getRoot*(ctx: KryonContext): ptr IRComponent =
  ## Gets the root component from the context
  if ctx.isInitialized:
    result = ir_get_root()
  else:
    result = nil

proc setRoot*(ctx: KryonContext, root: ptr IRComponent) =
  ## Sets the root component for the context
  if ctx.isInitialized:
    ir_set_root(root)
