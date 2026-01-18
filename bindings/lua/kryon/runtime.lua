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
--- @param sourceInfo table|nil Optional {code=string, file=string, line=number} for KIR embedding
function Runtime.registerHandler(component, eventType, callback, sourceInfo)
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

  -- Create IR event with logic_id that identifies it as a Lua event
  -- Desktop renderer will check for "lua_event_" prefix
  local logicId = "lua_event_" .. handlerId
  local event = C.ir_create_event(eventType, logicId, nil)

  -- Set handler source if provided (for KIR embedding in web output)
  if sourceInfo and sourceInfo.code then
    local handlerSource = C.ir_create_handler_source(
      "lua",
      sourceInfo.code,
      sourceInfo.file or "",
      sourceInfo.line or 0
    )
    if handlerSource ~= nil then
      -- Set closure metadata if present (for target-agnostic KIR)
      if sourceInfo.closure_vars and #sourceInfo.closure_vars > 0 then
        -- Create a cdata array of strings for FFI
        local varsArray = ffi.new("const char*[?]", #sourceInfo.closure_vars)
        for i, var in ipairs(sourceInfo.closure_vars) do
          varsArray[i - 1] = var  -- FFI arrays are 0-indexed
        end
        C.ir_handler_source_set_closures(handlerSource, varsArray, #sourceInfo.closure_vars)
      end
      C.ir_event_set_handler_source(event, handlerSource)
    end
  end

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

--- Recursively cleanup handlers for a component tree
--- @param component cdata IRComponent* pointer
function Runtime.cleanupComponentTreeHandlers(component)
  if not component then return end

  -- Cleanup this component's handlers
  Runtime.cleanupHandler(component)

  -- Recursively cleanup all children
  local childCount = C.ir_get_child_count(component)
  for i = 0, childCount - 1 do
    local child = C.ir_get_child_at(component, i)
    if child then
      Runtime.cleanupComponentTreeHandlers(child)
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
--- @param eventType number Event type
--- @param textData string|nil Text data for TEXT_CHANGE events, or custom_data JSON for CLICK events
function Runtime.dispatchEvent(handlerId, eventType, textData)
  local handler = Runtime.handlers[handlerId]
  if handler then
    -- For TEXT_CHANGE events, use the text passed from C layer
    if eventType == C.IR_EVENT_TEXT_CHANGE then
      local text = textData or ""
      local success, err = pcall(handler.callback, text)
      if not success then
        print("❌ Error in Lua event handler: " .. tostring(err))
      end
    elseif eventType == C.IR_EVENT_CLICK and textData then
      -- For CLICK events with custom_data, parse it and pass event data to callback
      local eventData = nil

      -- Try to load cjson if available
      local ok, json = pcall(require, "cjson")
      if ok and json then
        local parseOk, decoded = pcall(json.decode, textData)
        if parseOk and decoded and decoded.data then
          eventData = decoded.data
        end
      end

      -- Fallback: simple pattern extraction for known formats
      if not eventData then
        local date = textData:match('"date":"([^"]+)"')
        local isCompleted = textData:match('"isCompleted":(%w+)')
        if date then
          eventData = {
            date = date,
            isCompleted = isCompleted == "true"
          }
        end
      end

      -- Call callback with event data (handler can check for this)
      local success, err = pcall(handler.callback, eventData)
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

--- Auto-wrap all global tables with reactive proxies
--- This makes state variables reactive without requiring explicit Reactive.reactive() calls
local function autoWrapGlobals()
  local Reactive = require("kryon.reactive")

  -- Standard library modules to exclude from auto-wrapping
  local excludeModules = {
    ["_G"] = true,
    ["package"] = true,
    ["string"] = true,
    ["table"] = true,
    ["math"] = true,
    ["io"] = true,
    ["os"] = true,
    ["debug"] = true,
    ["coroutine"] = true,
    ["utf8"] = true,
    ["bit"] = true,
    ["jit"] = true,
  }

  -- Also exclude anything starting with "kryon." or "Reactive"
  local function shouldWrap(name, value)
    if excludeModules[name] then return false end
    if type(value) ~= "table" then return false end
    if name:match("^kryon") or name:match("^Reactive") then return false end
    if Reactive.isReactive(value) then return false end
    return true
  end

  -- Auto-wrap all qualifying global tables
  for name, value in pairs(_G) do
    if shouldWrap(name, value) then
      _G[name] = Reactive.reactive(value)
    end
  end
end

--- Create a Kryon application with automatic reactivity
--- @param config table Configuration with window and body (component) or root (function)
--- @return table Application object
function Runtime.createApp(config)
  local Reactive = require("kryon.reactive")

  -- Auto-wrap all global tables with reactive proxies
  autoWrapGlobals()

  -- Create IR context
  local ctx = C.ir_create_context()
  C.ir_set_context(ctx)

  -- Create animation context for smooth animations
  local anim_ctx = C.ir_animation_context_create()

  -- Support both pre-built components and root functions
  local rootFn = config.root or config.body
  local isReactive = type(rootFn) == "function"

  if isReactive then
    -- Create reactive app with effect-based re-rendering
    local app = {
      context = ctx,
      animation_context = anim_ctx,
      root = nil,
      window = config.window or {},
      running = false,
      _rootFn = rootFn,
      _stopEffect = nil,
    }

    local isBuilding = false

    local function buildAndUpdateRoot()
      if isBuilding then return end
      isBuilding = true

      if app.root then
        Runtime.cleanupComponentTreeHandlers(app.root)
      end

      local newRoot = rootFn()

      if app.root then
        C.ir_set_root(newRoot)
        app.root = newRoot
        if Runtime._globalRenderer then
          Desktop.desktop_ir_renderer_update_root(Runtime._globalRenderer, newRoot)
        end
      else
        C.ir_set_root(newRoot)
        app.root = newRoot
      end

      isBuilding = false
    end

    app._stopEffect = Reactive.effect(buildAndUpdateRoot)
    return app
  else
    -- Static app with pre-built component
    C.ir_set_root(rootFn)
    return {
      context = ctx,
      animation_context = anim_ctx,
      root = rootFn,
      window = config.window or {},
      running = false,
    }
  end
end

--- Update loop - delegates to C IR animation system
--- @param app table Application object
--- @param deltaTime number Time since last frame in seconds
function Runtime.update(app, deltaTime)
  if not app or not app.root then return end

  -- Update animations if animation context exists
  if app.animation_context then
    C.ir_animation_update(app.animation_context, deltaTime)
  end
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

  -- Clean up animation context
  if app.animation_context then
    C.ir_animation_context_destroy(app.animation_context)
    app.animation_context = nil
  end

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
  -- Load the KIR file (parses metadata into g_ir_context)
  local root = C.ir_read_json_file(kir_filepath)
  if root == nil then
    error("Failed to load KIR from " .. kir_filepath)
  end

  -- NOTE: Don't expand ForEach here yet - we'll expand after Lua source re-execution
  -- The KIR root is expanded, but it gets discarded when we re-execute the Lua source

  -- Get metadata from global context
  local ctx = C.ir_get_global_context()

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
    -- CRITICAL: Reset handler registry before re-executing
    -- This ensures handler IDs match between compilation and runtime
    Runtime.handlers = {}
    Runtime.nextHandlerId = 1

    -- Set runtime mode flag - tells DSL to use actual data for ForEach
    -- (instead of marker for KIR serialization)
    Runtime._isRuntimeMode = true

    -- Re-execute the Lua file to rebuild handler registry
    local chunk, err = loadfile(source_file)
    if not chunk then
      return {
        root = root,
        window = {width = 800, height = 600},
        running = false
      }
    end

    local app = chunk()  -- Execute, returns app table and registers handlers

    -- CRITICAL FIX: Use the app returned from source execution!
    -- This preserves reactive effects created by createApp()
    if app and app.root then
      -- IMPORTANT: Expand ForEach on the NEW app.root (not the old KIR root)
      -- The KIR root was expanded earlier, but that root is discarded when we
      -- re-execute the Lua source. We need to expand the new app.root instead.
      C.ir_expand_foreach(app.root)
      return app  -- Return the full reactive app with effects intact
    else
      return {
        root = root,  -- Fallback to KIR tree if source didn't return app
        window = {width = 800, height = 600, title = "Kryon App"},
        running = false
      }
    end
  else
    -- No Lua source, expand ForEach on the KIR root
    C.ir_expand_foreach(root)
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
  -- text_data is non-NULL for TEXT_CHANGE events
  local eventCallback = ffi.cast("LuaEventCallback", function(handler_id, event_type, text_data)
    -- Convert C string to Lua string if present
    local textStr = nil
    if text_data ~= nil then
      textStr = ffi.string(text_data)
    end
    Runtime.dispatchEvent(handler_id, event_type, textStr)
  end)

  Desktop.desktop_ir_renderer_set_lua_event_callback(renderer, eventCallback)

  -- Store callback to prevent garbage collection
  Runtime._eventCallback = eventCallback

  -- Set up canvas draw callback
  local canvasDrawCallback = ffi.cast("LuaCanvasDrawCallback", function(component_id)
    Runtime.invokeCanvasCallback(component_id)
  end)

  Desktop.desktop_ir_renderer_set_lua_canvas_draw_callback(renderer, canvasDrawCallback)
  Runtime._canvasDrawCallback = canvasDrawCallback

  -- Set up canvas update callback
  local canvasUpdateCallback = ffi.cast("LuaCanvasUpdateCallback", function(component_id, delta_time)
    local callback = Runtime.canvasUpdateCallbacks[component_id]
    if callback then
      callback(delta_time)
    end
  end)

  Desktop.desktop_ir_renderer_set_lua_canvas_update_callback(renderer, canvasUpdateCallback)
  Runtime._canvasUpdateCallback = canvasUpdateCallback

  -- Expand ForEach components with actual data before rendering
  -- This is critical for standalone binaries where ForEach was serialized with
  -- "__runtime__" markers during build. Without this expansion, only the first
  -- ForEach item would render initially.
  C.ir_expand_foreach(app.root)

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
      -- Create TabGroupState
      local state = Runtime.createTabGroupState(component, 0, false)

      -- Register TabBar
      Runtime.registerTabBar(state, tabBar)

      -- Register tabs (children of TabBar that are buttons)
      for i = 0, tabBar.child_count - 1 do
        local child = tabBar.children[i]
        if child.type == C.IR_COMPONENT_BUTTON then
          -- Get visual state from component's custom_data (stored by DSL during Tab creation)
          local visual = {}
          local customData = C.ir_get_custom_data(child)

          if customData ~= nil then
            local json = require("cjson")
            if json then
              local success, decoded = pcall(json.decode, ffi.string(customData))
              if success and decoded then
                visual = decoded
              end
            end
          end

          Runtime.registerTab(state, child, visual)
        end
      end

      -- Register TabContent
      Runtime.registerTabContent(state, tabContent)

      -- Register panels (children of TabContent that are TabPanel)
      for i = 0, tabContent.child_count - 1 do
        local child = tabContent.children[i]
        if child.type == C.IR_COMPONENT_TAB_PANEL then
          Runtime.registerPanel(state, child)
        end
      end

      -- Finalize (apply visuals, set initial selected tab)
      Runtime.finalizeTabGroup(state)

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
