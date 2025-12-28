-- Kryon Runtime Layer
-- Handles event dispatch, component lifecycle, and app management
-- All heavy lifting (layout, rendering, animation) is delegated to C IR core

local kryonFfi = require("kryon.ffi")
local C = kryonFfi.C
local Desktop = kryonFfi.Desktop
local ffi = require("ffi")  -- LuaJIT FFI for ffi.new(), ffi.cast(), etc.

-- ============================================================================
-- Helper Functions
-- ============================================================================

--- Parse color string to uint32 (RGBA format)
--- @param colorStr string Color in "#RRGGBB" or "#RRGGBBAA" format
--- @return number uint32 color value
local function parseColor(colorStr)
  if type(colorStr) ~= "string" then return 0 end

  -- Remove '#' prefix if present
  local hex = colorStr:gsub("^#", "")

  -- Parse hex color
  local r, g, b, a = 0, 0, 0, 255
  if #hex == 6 then
    r = tonumber(hex:sub(1, 2), 16)
    g = tonumber(hex:sub(3, 4), 16)
    b = tonumber(hex:sub(5, 6), 16)
  elseif #hex == 8 then
    r = tonumber(hex:sub(1, 2), 16)
    g = tonumber(hex:sub(3, 4), 16)
    b = tonumber(hex:sub(5, 6), 16)
    a = tonumber(hex:sub(7, 8), 16)
  end

  -- Pack into uint32 (RGBA format)
  return bit.lshift(r, 24) + bit.lshift(g, 16) + bit.lshift(b, 8) + a
end

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

--- Load KIR file and restore Lua callbacks by re-executing source
--- @param kir_filepath string Path to .kir file
--- @return table Application instance ready to run
function Runtime.loadKIR(kir_filepath)
  io.stderr:write("==============================================\n")
  io.stderr:write("[RUNTIME.LOADKIR] FUNCTION CALLED!\n")
  io.stderr:write("==============================================\n")
  io.stderr:flush()

  -- Load the KIR file (parses metadata into g_ir_context)
  local root = C.ir_read_json_file(kir_filepath)
  if root == nil then
    error("Failed to load KIR from " .. kir_filepath)
  end

  -- Get metadata from global context
  local ctx = C.ir_get_global_context()

  -- DEBUG: Check metadata access
  print(string.format("[runtime.loadKIR] ctx = %s", tostring(ctx)))
  if ctx ~= nil then
    print(string.format("[runtime.loadKIR] ctx.source_metadata = %s", tostring(ctx.source_metadata)))
    if ctx.source_metadata ~= nil then
      print("[runtime.loadKIR] source_metadata is NOT nil")
      if ctx.source_metadata.source_language ~= nil then
        local lang = ffi.string(ctx.source_metadata.source_language)
        print(string.format("[runtime.loadKIR] source_language = '%s'", lang))
      else
        print("[runtime.loadKIR] source_language IS nil")
      end
    else
      print("[runtime.loadKIR] source_metadata IS nil - THIS WAS THE BUG")
    end
  else
    print("[runtime.loadKIR] ctx IS nil")
  end

  local has_lua_events = false
  local source_file = nil

  if ctx ~= nil and ctx.source_metadata ~= nil then
    local meta = ctx.source_metadata

    if meta.source_language ~= nil then
      local lang = ffi.string(meta.source_language)
      if lang == "lua" then
        if meta.source_file ~= nil then
          source_file = ffi.string(meta.source_file)
          has_lua_events = true
        end
      end
    end
  end

  if has_lua_events and source_file then
    print(string.format("[runtime] KIR generated from Lua, re-executing: %s", source_file))

    -- CRITICAL: Reset handler registry before re-executing
    -- This ensures handler IDs match between compilation and runtime
    Runtime.handlers = {}
    Runtime.nextHandlerId = 1

    -- Re-execute the Lua file to rebuild handler registry
    local chunk, err = loadfile(source_file)
    if not chunk then
      print(string.format("[runtime] Warning: Cannot load source file: %s", err))
      print("[runtime] Running without callbacks (KIR tree only)")
      return {
        root = root,
        window = {width = 800, height = 600},
        running = false
      }
    end

    local app = chunk()  -- Execute, returns app table and registers handlers

    -- Count handlers (Runtime.handlers is a table, not an array)
    local handler_count = 0
    for _ in pairs(Runtime.handlers) do
      handler_count = handler_count + 1
    end
    print(string.format("[runtime] After re-execution, handler count = %d", handler_count))

    -- Use the KIR component tree (already has layout computed)
    -- But with handlers from fresh source execution
    return {
      root = root,  -- Use KIR tree
      window = app.window or {width = 800, height = 600, title = "Kryon App"},
      running = false
    }
  else
    -- No Lua source, return KIR-only app (no callbacks)
    print("[runtime] KIR has no Lua source metadata, running without callbacks")
    return {
      root = root,
      window = {width = 800, height = 600},
      running = false
    }
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

  -- Finalize TabGroups before starting renderer
  -- This registers tabs with TabGroupState so the C core can handle tab switching automatically
  print("[runtime] Finalizing TabGroups...")
  local dsl = require("kryon.dsl")
  tabVisualStates = dsl._tabVisualStates  -- Make it available to registration functions
  Runtime.finalizeTabGroups(app.root)

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

  -- Set up canvas draw callback
  local canvasDrawCallback = ffi.cast("LuaCanvasDrawCallback", function(component_id)
    print("[RUNTIME] Canvas draw callback invoked for component:", component_id)
    Runtime.invokeCanvasCallback(component_id)
  end)

  Desktop.desktop_ir_renderer_set_lua_canvas_draw_callback(renderer, canvasDrawCallback)
  print("[RUNTIME] Canvas draw callback registered successfully")
  Runtime._canvasDrawCallback = canvasDrawCallback

  -- Set up canvas update callback
  local canvasUpdateCallback = ffi.cast("LuaCanvasUpdateCallback", function(component_id, delta_time)
    local callback = Runtime.canvasUpdateCallbacks[component_id]
    if callback then
      callback(delta_time)
    end
  end)

  Desktop.desktop_ir_renderer_set_lua_canvas_update_callback(renderer, canvasUpdateCallback)
  print("[RUNTIME] Canvas update callback registered successfully")
  Runtime._canvasUpdateCallback = canvasUpdateCallback

  print("🎨 Rendering...")

  -- Run main loop (blocking - keeps Lua alive)
  -- Event handlers in Runtime.handlers will be called via dispatchEvent
  local success = Desktop.desktop_ir_renderer_run_main_loop(renderer, app.root)

  -- Cleanup
  Desktop.desktop_ir_renderer_destroy(renderer)
  Runtime._eventCallback = nil
  Runtime._canvasDrawCallback = nil
  Runtime._canvasUpdateCallback = nil
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

-- Canvas callbacks now use the standard event system (via registerHandler)
-- No canvas-specific callback registry needed

-- ============================================================================
-- Tab Group Registration (for automatic tab switching)
-- ============================================================================

--- Create TabGroupState and register with TabGroup component
--- @param group cdata IRComponent* TabGroup component
--- @param selectedIndex number Initial selected tab index (0-based)
--- @param reorderable boolean Whether tabs can be reordered
--- @return cdata TabGroupState* pointer
function Runtime.createTabGroupState(group, selectedIndex, reorderable)
  local state = C.ir_tabgroup_create_state(group, nil, nil, selectedIndex or 0, reorderable or false)
  return state
end

--- Register a tab button with TabGroupState
--- @param state cdata TabGroupState* pointer
--- @param tab cdata IRComponent* Button component
--- @param visual table Visual state {backgroundColor, activeBackgroundColor, textColor, activeTextColor}
function Runtime.registerTab(state, tab, visual)
  if not state or not tab then return end

  -- Register tab with C core
  C.ir_tabgroup_register_tab(state, tab)
  local tabIndex = C.ir_tabgroup_get_tab_count(state) - 1

  -- Set visual state in C core
  local cVisual = ffi.new("TabVisualState")
  cVisual.background_color = parseColor(visual.backgroundColor or "#292C30")
  cVisual.active_background_color = parseColor(visual.activeBackgroundColor or "#3C4047")
  cVisual.text_color = parseColor(visual.textColor or "#C7C9CC")
  cVisual.active_text_color = parseColor(visual.activeTextColor or "#FFFFFF")

  C.ir_tabgroup_set_tab_visual(state, tabIndex, cVisual)
end

--- Register TabBar with TabGroupState
--- @param state cdata TabGroupState* pointer
--- @param tabBar cdata IRComponent* TabBar component
function Runtime.registerTabBar(state, tabBar)
  if not state or not tabBar then return end
  C.ir_tabgroup_register_bar(state, tabBar)
end

--- Register TabContent with TabGroupState
--- @param state cdata TabGroupState* pointer
--- @param tabContent cdata IRComponent* TabContent component
function Runtime.registerTabContent(state, tabContent)
  if not state or not tabContent then return end
  C.ir_tabgroup_register_content(state, tabContent)
end

--- Register panel with TabGroupState
--- @param state cdata TabGroupState* pointer
--- @param panel cdata IRComponent* TabPanel component
function Runtime.registerPanel(state, panel)
  if not state or not panel then return end
  C.ir_tabgroup_register_panel(state, panel)
end

--- Finalize TabGroup (apply visuals, set initial state)
--- @param state cdata TabGroupState* pointer
function Runtime.finalizeTabGroup(state)
  if not state then return end
  C.ir_tabgroup_finalize(state)
end

--- Walk component tree and finalize all TabGroups
--- @param root cdata IRComponent* root component
function Runtime.finalizeTabGroups(root)
  -- Recursively find and finalize TabGroups
  Runtime._walkAndFinalizeTabGroups(root, {})
end

--- Internal helper to walk tree and finalize TabGroups
--- @param component cdata IRComponent* current component
--- @param processedGroups table Set of already processed group pointers
function Runtime._walkAndFinalizeTabGroups(component, processedGroups)
  if not component then return end

  -- Check if this is a TabGroup
  if component.type == C.IR_COMPONENT_TAB_GROUP then
    -- Avoid processing same group twice
    local key = tonumber(ffi.cast("uintptr_t", component))
    if processedGroups[key] then return end
    processedGroups[key] = true

    -- Find TabBar and TabContent children
    local tabBar = nil
    local tabContent = nil

    for i = 0, component.child_count - 1 do
      local child = component.children[i]
      if child.type == C.IR_COMPONENT_TAB_BAR then
        tabBar = child
      elseif child.type == C.IR_COMPONENT_TAB_CONTENT then
        tabContent = child
      end
    end

    if tabBar and tabContent then
      print(string.format("[runtime] Finalizing TabGroup id=%d", component.id))

      -- Create TabGroupState
      local state = Runtime.createTabGroupState(component, 0, false)

      -- Register TabBar
      Runtime.registerTabBar(state, tabBar)

      -- Register tabs (children of TabBar that are buttons)
      for i = 0, tabBar.child_count - 1 do
        local child = tabBar.children[i]
        if child.type == C.IR_COMPONENT_BUTTON then
          -- Get visual state from tabVisualStates table
          local visualKey = tostring(child)
          local visual = tabVisualStates[visualKey] or {}
          Runtime.registerTab(state, child, visual)
          print(string.format("[runtime]   Registered tab button id=%d", child.id))
        end
      end

      -- Register TabContent
      Runtime.registerTabContent(state, tabContent)

      -- Register panels (children of TabContent that are TabPanel)
      for i = 0, tabContent.child_count - 1 do
        local child = tabContent.children[i]
        if child.type == C.IR_COMPONENT_TAB_PANEL then
          Runtime.registerPanel(state, child)
          print(string.format("[runtime]   Registered panel id=%d", child.id))
        end
      end

      -- Finalize (apply visuals, set initial selected tab)
      Runtime.finalizeTabGroup(state)
      print(string.format("[runtime] TabGroup finalized with %d tabs", C.ir_tabgroup_get_tab_count(state)))

      -- Store state in component custom_data for runtime access
      component.custom_data = ffi.cast("char*", state)
    end
  end

  -- Recurse into children
  for i = 0, component.child_count - 1 do
    Runtime._walkAndFinalizeTabGroups(component.children[i], processedGroups)
  end
end

return Runtime
