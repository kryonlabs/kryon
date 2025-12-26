-- Kryon Runtime Layer
-- Handles event dispatch, component lifecycle, and app management
-- All heavy lifting (layout, rendering, animation) is delegated to C IR core

local kryonFfi = require("kryon.ffi")
local C = kryonFfi.C
local Desktop = kryonFfi.Desktop
local ffi = require("ffi")  -- LuaJIT FFI for ffi.new(), ffi.cast(), etc.

local Runtime = {}

-- ============================================================================
-- Event Handler Registry
-- ============================================================================
-- Maps component_id -> { event_type -> callback_function }
-- This is Lua-side only - events are created in C, but dispatched here
Runtime.handlers = {}

-- Next component ID for tracking (IR generates its own IDs, but we track too)
Runtime.nextHandlerId = 1

-- ============================================================================
-- Event Handler Registration
-- ============================================================================

--- Register a Lua callback for a component event
--- @param component cdata IRComponent* pointer
--- @param eventType number Event type constant from ffi.EventType
--- @param callback function Lua function to call when event fires
function Runtime.registerHandler(component, eventType, callback)
  if not component or not callback then
    error("registerHandler requires component and callback")
  end

  -- Generate a unique ID for this handler
  local handlerId = Runtime.nextHandlerId
  print(string.format("🎯 registerHandler: handlerId=%d eventType=%d", handlerId, eventType))
  Runtime.nextHandlerId = Runtime.nextHandlerId + 1

  -- Store the callback
  Runtime.handlers[handlerId] = {
    component = component,
    eventType = eventType,
    callback = callback,
  }

  -- Create IR event with logic_id that identifies it as a Lua event
  -- Desktop renderer will check for "lua_event_" prefix
  local logicId = "lua_event_" .. handlerId
  print(string.format("   Creating IR event with logic_id='%s'", logicId))
  local event = C.ir_create_event(eventType, logicId, nil)
  C.ir_add_event(component, event)

  return handlerId
end

--- Cleanup handlers for a component
--- @param component cdata IRComponent* pointer
function Runtime.cleanupHandler(component)
  -- Remove all handlers associated with this component
  for id, handler in pairs(Runtime.handlers) do
    if handler.component == component then
      Runtime.handlers[id] = nil
    end
  end
end

--- Cleanup all handlers (for app shutdown)
function Runtime.cleanupAllHandlers()
  Runtime.handlers = {}
end

-- ============================================================================
-- Event Dispatch (called from C or event loop)
-- ============================================================================

--- Dispatch an event to the registered Lua callback
--- This is called from the desktop renderer when events fire
--- @param handlerId number Handler ID from logic_id
--- @param eventType number Event type (optional)
function Runtime.dispatchEvent(handlerId, eventType)
  local handler = Runtime.handlers[handlerId]
  if handler then
    print(string.format("✅ Found handler %d, executing callback", handlerId))

    -- For TEXT_CHANGE events, get the current input text and pass it to the callback
    if eventType == C.IR_EVENT_TEXT_CHANGE then
      local component = handler.component
      local text = component.text_content and ffi.string(component.text_content) or ""
      local success, err = pcall(handler.callback, text)
      if not success then
        print("❌ Error in Lua event handler: " .. tostring(err))
      end
    else
      -- For other events, call callback with no arguments
      local success, err = pcall(handler.callback)
      if not success then
        print("❌ Error in Lua event handler: " .. tostring(err))
      end
    end
  else
    print(string.format("⚠️ No handler found for ID: %d", handlerId))
  end
end

--- Dispatch event by component and event type (for compatibility)
--- @param componentId number Component ID
--- @param eventType number Event type
function Runtime.dispatchEventByComponent(componentId, eventType)
  -- Find the handler for this component and event type
  for id, handler in pairs(Runtime.handlers) do
    if handler.eventType == eventType then
      local handlerComponentId = tonumber(ffi.cast("uintptr_t", handler.component))
      if handlerComponentId == componentId then
        local success, err = pcall(handler.callback)
        if not success then
          print("❌ Error in Lua event handler: " .. tostring(err))
        end
        return
      end
    end
  end
end

-- ============================================================================
-- Application Lifecycle
-- ============================================================================

--- Create a Kryon application
--- @param config table Configuration with window and body
--- @return table Application object
function Runtime.createApp(config)
  if not config or not config.body then
    error("createApp requires config.body (root component)")
  end

  -- Create IR context (C call)
  local ctx = C.ir_create_context()
  C.ir_set_context(ctx)

  -- Set root component (C call)
  C.ir_set_root(config.body)

  return {
    context = ctx,
    root = config.body,
    window = config.window or {},
    running = false,
  }
end

--- Update loop - delegates to C IR animation system
--- @param app table Application object
--- @param deltaTime number Time since last frame in seconds
function Runtime.update(app, deltaTime)
  if not app or not app.root then return end

  -- TODO: Call IR animation system when available
  -- C.ir_animation_tree_update(app.root, deltaTime)
end

--- Render loop - delegates to C layout computation
--- @param app table Application object
function Runtime.render(app)
  if not app or not app.root then return end

  -- Compute layout (C call)
  local width = app.window.width or 800
  local height = app.window.height or 600
  C.ir_layout_compute(app.root, width, height)

  -- NOTE: Actual rendering happens in the backend (SDL3, Terminal, Web)
  -- Lua doesn't touch rendering - it's all in C!
end

--- Cleanup application resources
--- @param app table Application object
function Runtime.destroyApp(app)
  if not app then return end

  Runtime.cleanupAllHandlers()

  if app.root then
    C.ir_destroy_component(app.root)
    app.root = nil
  end

  if app.context then
    C.ir_destroy_context(app.context)
    app.context = nil
  end
end

-- ============================================================================
-- Debug Utilities
-- ============================================================================

--- Print the IR tree to console (C debug function)
--- @param component cdata IRComponent* pointer
function Runtime.debugPrintTree(component)
  if not component then
    print("debugPrintTree: component is nil")
    return
  end
  C.debug_print_tree(component)
end

--- Print the IR tree to a file (C debug function)
--- @param component cdata IRComponent* pointer
--- @param filename string Output file path
function Runtime.debugPrintTreeToFile(component, filename)
  if not component or not filename then
    error("debugPrintTreeToFile requires component and filename")
  end
  C.debug_print_tree_to_file(component, filename)
end

-- ============================================================================
-- IR Serialization
-- ============================================================================

--- Save IR tree to .kir file (binary format)
--- @param app table Application object with root component
--- @param filepath string Path to save the .kir file
--- @return boolean Success status
function Runtime.saveIR(app, filepath)
  if not app or not app.root then
    error("saveIR requires app with root component")
  end

  local success = C.ir_write_binary_file(app.root, filepath)
  if not success then
    error("Failed to save IR to " .. filepath)
  end

  print("Saved IR to: " .. filepath)
  return true
end

--- Global renderer reference (for dynamic UI updates)
Runtime._globalRenderer = nil
Runtime._globalApp = nil

--- Update the UI root component in the renderer
--- @param newRoot cdata New IRComponent* root
function Runtime.updateRoot(newRoot)
  if Runtime._globalRenderer and newRoot then
    Desktop.desktop_ir_renderer_update_root(Runtime._globalRenderer, newRoot)
  end
end

--- Run the desktop renderer with the app (keeps Lua process alive)
--- This is the preferred way to run Lua apps with event handlers
--- @param app table Application object with root, window config
--- @return boolean Success status
function Runtime.runDesktop(app)
  if not app or not app.root then
    error("runDesktop requires app with root component")
  end

  -- Check if desktop library is available
  if not Desktop then
    error("Desktop renderer library not available. Make sure libkryon_desktop.so is built and in the library path.")
  end

  -- Store app globally for updates
  Runtime._globalApp = app

  -- Create renderer config
  local config = ffi.new("DesktopRendererConfig")
  config.backend_type = 0  -- SDL3
  config.window_width = app.window.width or 800
  config.window_height = app.window.height or 600
  config.window_title = app.window.title or "Kryon App"
  config.resizable = true
  config.fullscreen = false
  config.vsync_enabled = true
  config.target_fps = 60

  -- Create and initialize renderer
  local renderer = Desktop.desktop_ir_renderer_create(config)
  if not renderer or renderer == nil then
    error("Failed to create desktop renderer")
  end

  local initSuccess = Desktop.desktop_ir_renderer_initialize(renderer)
  if not initSuccess then
    Desktop.desktop_ir_renderer_destroy(renderer)
    error("Failed to initialize desktop renderer")
  end

  -- Store renderer globally for dynamic updates
  Runtime._globalRenderer = renderer

  -- Set up event callback so C can call back to Lua
  -- This callback will be invoked when Lua events fire
  local eventCallback = ffi.cast("LuaEventCallback", function(handler_id, event_type)
    -- handler_id is the ID we stored in Runtime.handlers (will be passed by C after we update it)
    print(string.format("🔔 Lua callback invoked: handler_id=%d event_type=%d", handler_id, event_type))
    Runtime.dispatchEvent(handler_id, event_type)
  end)

  Desktop.desktop_ir_renderer_set_lua_event_callback(renderer, eventCallback)

  -- Store callback to prevent garbage collection
  Runtime._eventCallback = eventCallback

  print("🎨 Rendering...")

  -- Run main loop (blocking - keeps Lua alive)
  -- Event handlers in Runtime.handlers will be called via dispatchEvent
  local success = Desktop.desktop_ir_renderer_run_main_loop(renderer, app.root)

  -- Cleanup
  Desktop.desktop_ir_renderer_destroy(renderer)
  Runtime._eventCallback = nil
  Runtime._globalRenderer = nil
  Runtime._globalApp = nil

  return success
end

--- Load IR tree from .kir file (binary format)
--- @param filepath string Path to the .kir file
--- @return cdata IRComponent* root component
function Runtime.loadIR(filepath)
  local root = C.ir_read_binary_file(filepath)
  if root == nil then
    error("Failed to load IR from " .. filepath)
  end

  print("Loaded IR from: " .. filepath)
  return root
end

return Runtime
