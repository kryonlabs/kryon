-- Kryon Runtime Layer
-- Handles event dispatch, component lifecycle, and app management
-- All heavy lifting (layout, rendering, animation) is delegated to C IR core

local ffi = require("kryon.ffi")
local C = ffi.C

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
  Runtime.nextHandlerId = Runtime.nextHandlerId + 1

  -- Store the callback
  Runtime.handlers[handlerId] = {
    component = component,
    eventType = eventType,
    callback = callback,
  }

  -- Create IR event in C (the C layer will handle event routing to component)
  local event = C.ir_create_event(eventType, nil, nil)
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
--- This would be called from the C event system or renderer backend
--- @param componentId number Component ID
--- @param eventType number Event type
function Runtime.dispatchEvent(componentId, eventType)
  -- Find the handler for this component and event type
  for id, handler in pairs(Runtime.handlers) do
    if handler.eventType == eventType then
      -- TODO: Check if handler.component matches componentId
      -- For now, just call all matching event types
      local success, err = pcall(handler.callback)
      if not success then
        print("Error in event handler: " .. tostring(err))
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
