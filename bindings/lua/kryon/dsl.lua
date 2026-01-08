-- Kryon DSL Layer
-- Transforms Lua tables into C IR function calls
-- Every property set here becomes a direct C call - zero abstraction

local ffi = require("kryon.ffi")
local runtime = require("kryon.runtime")
local C = ffi.C

local DSL = {}

-- Module-level table to track tab visual states
-- Keys are tostring(component), values are {backgroundColor, activeBackgroundColor, textColor, activeTextColor}
-- This is needed because we create tabs before TabGroup finalization, so we need to preserve color info
-- Exported as DSL._tabVisualStates for runtime access during TabGroup finalization
DSL._tabVisualStates = {}

-- ============================================================================
-- Module Tracking (for cross-file component references)
-- ============================================================================

-- Track the current module being compiled
-- Set by the wrapper script before loading user code
-- Format: module_id like "components/calendar" or "main"
DSL._currentModule = nil

-- Stack for tracking component origin during function calls
-- When a wrapped function from another module is called, it pushes its module
-- This allows buildComponent to know the origin module of components being created
local _originModuleStack = {}

-- Push an origin module onto the stack (for wrapped component functions)
local function pushOriginModule(module_id)
  table.insert(_originModuleStack, module_id)
end

-- Pop the origin module from the stack
local function popOriginModule()
  return table.remove(_originModuleStack)
end

-- Get the current origin module (top of stack, or nil if empty)
local function getCurrentOriginModule()
  return _originModuleStack[#_originModuleStack]
end

-- Wrap a function to set its origin module when called
-- Used by the wrapper to tag exported component functions
-- @param fn The function to wrap
-- @param module_id The module that created this function (e.g., "components/tabs")
-- @param fn_name The name of the function (for tracking component trees)
function DSL.wrapWithModuleOrigin(fn, module_id, fn_name)
  return function(...)
    pushOriginModule(module_id)
    local results = {fn(...)}
    popOriginModule()

    -- Capture returned components for serialization to component_definitions
    -- Check if any of the returned values are IR components (cdata with type field)
    if fn_name then
      local captured = {}
      for _, result in ipairs(results) do
        -- Check if result is a cdata IRComponent
        local type_ok, comp_type = pcall(function()
          return result ~= nil and tonumber(C.ir_get_component_type(result)) or nil
        end)
        if type_ok and comp_type then
          table.insert(captured, result)
        end
        -- Also check if it's a table/array of components
        if type(result) == "table" then
          for i, v in ipairs(result) do
            local ok, t = pcall(function()
              return v ~= nil and tonumber(C.ir_get_component_type(v)) or nil
            end)
            if ok and t then
              table.insert(captured, v)
            end
          end
        end
      end

      -- Store captured components for this module/function
      if #captured > 0 then
        DSL._capturedComponentTrees[module_id] = DSL._capturedComponentTrees[module_id] or {}
        DSL._capturedComponentTrees[module_id][fn_name] = captured
      end
    end

    return unpack(results)
  end
end

-- Table mapping module_id to {exports = {name = true}, source_file = path}
DSL._moduleRegistry = {}

-- Table to track captured component trees by module
-- When a wrapped function returns components, we store them here
-- Structure: {module_id = {function_name = {component1, component2, ...}}}
DSL._capturedComponentTrees = {}

-- Set the current module context (called from wrapper)
function DSL.setCurrentModule(module_id)
  DSL._currentModule = module_id
end

-- Register a module's exports for reference tracking
function DSL.registerModule(module_id, exports)
  DSL._moduleRegistry[module_id] = DSL._moduleRegistry[module_id] or {}
  DSL._moduleRegistry[module_id].exports = DSL._moduleRegistry[module_id].exports or {}
  for _, exp in ipairs(exports) do
    DSL._moduleRegistry[module_id].exports[exp.name] = true
  end
end

-- Check if a component name is exported from a different module
function DSL.getComponentModule(componentName)
  local currentModule = DSL._currentModule
  if not currentModule then return nil end

  -- Search all registered modules except current
  for module_id, info in pairs(DSL._moduleRegistry) do
    if module_id ~= currentModule and info.exports and info.exports[componentName] then
      return module_id
    end
  end
  return nil
end

-- Get current module from environment variable (for backward compatibility)
local function getCurrentModuleFromEnv()
  local source_file = os.getenv("KRYON_SOURCE_FILE")
  if not source_file then return nil end

  -- Convert file path to module_id
  -- e.g., "/path/to/components/calendar.lua" -> "components/calendar"
  local module_id = source_file:gsub("^.*/", ""):gsub("%.lua$", "")

  -- If in a subdirectory, include it
  local subdir = source_file:match("([^/]+/[^/]+)%.lua$")
  if subdir then
    module_id = subdir:gsub("%.lua$", "")
  end

  return module_id
end

-- ============================================================================
-- Reactive Value Handling
-- ============================================================================

--- Unwrap a potentially reactive value to get its current raw value
--- @param value any Potentially reactive value
--- @return any Raw value
local function unrefValue(value)
  -- If it's a function (signal getter or computed), call it to get value
  if type(value) == "function" then
    local success, result = pcall(value)
    if success then
      return result
    end
    return value
  end

  -- If it's a reactive proxy, access it (triggers dependency tracking)
  -- The proxy's __index metamethod will handle this automatically
  -- We don't need to do anything special - just return it
  return value
end

-- ============================================================================
-- TabGroup Context Stack (for automatic initialization)
-- ============================================================================

local tabGroupContextStack = {}

local function pushTabGroupContext(context)
  table.insert(tabGroupContextStack, context)
end

local function popTabGroupContext()
  return table.remove(tabGroupContextStack)
end

local function getCurrentTabGroupContext()
  return tabGroupContextStack[#tabGroupContextStack]
end

-- ============================================================================
-- Utility Functions
-- ============================================================================

--- Extract function source code using debug.getinfo + file read
--- This reads the actual function text from the source file
--- Returns clean source code WITHOUT target-specific wrappers
--- @param fn function The function to extract source from
--- @return table|nil {code=string, file=string, line=number, closure_vars=table|nil} or nil
local function extractFunctionSource(fn)
  if type(fn) ~= "function" then return nil end

  local info = debug.getinfo(fn, "S")
  if not info or not info.source or info.source:sub(1,1) ~= "@" then
    return nil
  end

  local filepath = info.source:sub(2)  -- Remove @ prefix
  local file = io.open(filepath, "r")
  if not file then return nil end

  local lines = {}
  local lineNum = 0
  for line in file:lines() do
    lineNum = lineNum + 1
    if lineNum >= info.linedefined and lineNum <= info.lastlinedefined then
      table.insert(lines, line)
    end
  end
  file:close()

  if #lines == 0 then return nil end

  -- Join lines and extract just the function part
  local fullCode = table.concat(lines, "\n")

  -- Find the start of "function" keyword (handles "onClick = function" -> "function")
  local funcStart = fullCode:find("function")
  if funcStart then
    fullCode = fullCode:sub(funcStart)
  end

  -- Convert named functions to anonymous: "function name(" -> "function("
  -- This is needed because we assign handlers as expressions
  fullCode = fullCode:gsub("^function%s+[%w_]+%s*%(", "function(")

  -- Remove trailing comma if present (from table literal syntax)
  fullCode = fullCode:gsub(",%s*$", "")

  -- Get just the filename for the file field
  local filename = filepath:gsub(".*/", "")

  -- Check if handler uses closure variables that need special handling
  -- Store these as metadata for target-specific codegen to handle
  local closureVars = {}

  -- Detect common closure variable patterns
  if fullCode:match("habitIndex") or fullCode:match("%w+Index") then
    table.insert(closureVars, "habitIndex")
  end
  if fullCode:match("day%.date") then
    table.insert(closureVars, "day.date")
  end

  -- Store closure metadata for codegen (empty table if no closures)
  local closureMetadata = #closureVars > 0 and closureVars or nil

  -- Return clean source code WITHOUT any wrapper
  -- Target-specific codegen will add appropriate wrappers based on closure_metadata
  return {
    code = fullCode,
    file = filename,
    line = info.linedefined,
    closure_vars = closureMetadata
  }
end

--- Parse dimension string like "100px", "50%", "auto", "2fr"
--- @param value any Dimension value (number, string, or reactive)
--- @return number dimensionType
--- @return number dimensionValue
local function parseDimension(value)
  -- Unwrap reactive values
  value = unrefValue(value)

  -- Plain number = pixels
  if type(value) == "number" then
    return C.IR_DIMENSION_PX, value
  end

  -- String parsing
  if type(value) == "string" then
    value = value:lower():gsub("%s+", "")  -- Remove whitespace

    if value == "auto" then
      return C.IR_DIMENSION_AUTO, 0
    elseif value:match("%%$") then
      local num = tonumber(value:match("^(.-)%%"))
      return C.IR_DIMENSION_PERCENT, num or 0
    elseif value:match("px$") then
      local num = tonumber(value:match("^(.-)px"))
      return C.IR_DIMENSION_PX, num or 0
    elseif value:match("fr$") then
      local num = tonumber(value:match("^(.-)fr"))
      return C.IR_DIMENSION_FLEX, num or 1
    elseif value:match("vw$") then
      local num = tonumber(value:match("^(.-)vw"))
      return C.IR_DIMENSION_VW, num or 0
    elseif value:match("vh$") then
      local num = tonumber(value:match("^(.-)vh"))
      return C.IR_DIMENSION_VH, num or 0
    elseif value:match("rem$") then
      local num = tonumber(value:match("^(.-)rem"))
      return C.IR_DIMENSION_REM, num or 0
    elseif value:match("em$") then
      local num = tonumber(value:match("^(.-)em"))
      return C.IR_DIMENSION_EM, num or 0
    else
      -- Try parsing as plain number string
      local num = tonumber(value)
      if num then
        return C.IR_DIMENSION_PX, num
      end
    end
  end

  return C.IR_DIMENSION_AUTO, 0
end

--- Parse color string like "#RRGGBB" or "#RRGGBBAA"
--- @param color string|any Color value or reactive
--- @return number r
--- @return number g
--- @return number b
--- @return number a
local function parseColor(color)
  -- Unwrap reactive values
  color = unrefValue(color)

  if type(color) == "string" then
    -- Named colors
    if color:lower() == "transparent" then
      return 0, 0, 0, 0
    end

    -- Hex colors
    if color:sub(1, 1) == "#" then
      local hex = color:sub(2)
      if #hex == 6 then
        local r = tonumber(hex:sub(1, 2), 16) or 0
        local g = tonumber(hex:sub(3, 4), 16) or 0
        local b = tonumber(hex:sub(5, 6), 16) or 0
        return r, g, b, 255
      elseif #hex == 8 then
        local r = tonumber(hex:sub(1, 2), 16) or 0
        local g = tonumber(hex:sub(3, 4), 16) or 0
        local b = tonumber(hex:sub(5, 6), 16) or 0
        local a = tonumber(hex:sub(7, 8), 16) or 255
        return r, g, b, a
      elseif #hex == 3 then
        -- Short form: #RGB -> #RRGGBB
        local r = tonumber(hex:sub(1, 1):rep(2), 16) or 0
        local g = tonumber(hex:sub(2, 2):rep(2), 16) or 0
        local b = tonumber(hex:sub(3, 3):rep(2), 16) or 0
        return r, g, b, 255
      end
    end

    -- Try as named color via C function
    local c = C.ir_color_named(color)
    return c.data.r, c.data.g, c.data.b, c.data.a
  end

  return 0, 0, 0, 255
end

--- Parse spacing (margin/padding) value
--- Can be: number, "10px", "10px 20px", "10px 20px 30px 40px"
--- @param value any Spacing value
--- @return number top
--- @return number right
--- @return number bottom
--- @return number left
local function parseSpacing(value)
  if type(value) == "number" then
    return value, value, value, value
  end

  if type(value) == "string" then
    local parts = {}
    for part in value:gmatch("%S+") do
      local _, v = parseDimension(part)
      table.insert(parts, v)
    end

    if #parts == 1 then
      return parts[1], parts[1], parts[1], parts[1]
    elseif #parts == 2 then
      return parts[1], parts[2], parts[1], parts[2]
    elseif #parts == 3 then
      return parts[1], parts[2], parts[3], parts[2]
    elseif #parts >= 4 then
      return parts[1], parts[2], parts[3], parts[4]
    end
  end

  return 0, 0, 0, 0
end

--- Parse alignment string to IR constant
--- @param align string Alignment value
--- @return number Alignment constant
local function parseAlignment(align)
  if not align then return C.IR_ALIGNMENT_START end

  local a = tostring(align):lower()
  if a == "start" or a == "flex-start" then
    return C.IR_ALIGNMENT_START
  elseif a == "center" then
    return C.IR_ALIGNMENT_CENTER
  elseif a == "end" or a == "flex-end" then
    return C.IR_ALIGNMENT_END
  elseif a == "stretch" then
    return C.IR_ALIGNMENT_STRETCH
  elseif a == "space-between" then
    return C.IR_ALIGNMENT_SPACE_BETWEEN
  elseif a == "space-around" then
    return C.IR_ALIGNMENT_SPACE_AROUND
  elseif a == "space-evenly" then
    return C.IR_ALIGNMENT_SPACE_EVENLY
  end

  return C.IR_ALIGNMENT_START
end

--- Parse text alignment
--- @param align string Text alignment value
--- @return number Text alignment constant
local function parseTextAlign(align)
  if not align then return C.IR_TEXT_ALIGN_LEFT end

  local a = tostring(align):lower()
  if a == "left" then
    return C.IR_TEXT_ALIGN_LEFT
  elseif a == "right" then
    return C.IR_TEXT_ALIGN_RIGHT
  elseif a == "center" then
    return C.IR_TEXT_ALIGN_CENTER
  elseif a == "justify" then
    return C.IR_TEXT_ALIGN_JUSTIFY
  end

  return C.IR_TEXT_ALIGN_LEFT
end

--- Convert hex color string to packed RGBA uint32 (0xRRGGBBAA format)
--- Matches Nim rgba() function logic for C core compatibility
--- @param color string Hex color like "#FFFFFF" or "#FFFFFFFF"
--- @return number Packed RGBA as uint32
local function colorToRGBA(color)
  local r, g, b, a = parseColor(color)
  -- Pack as 0xRRGGBBAA: (R << 24) | (G << 16) | (B << 8) | A
  -- Use LuaJIT bit library for bitwise operations
  return bit.bor(
    bit.lshift(r, 24),
    bit.lshift(g, 16),
    bit.lshift(b, 8),
    a
  )
end

-- ============================================================================
-- Component Builder
-- ============================================================================

--- Apply properties to a component
--- Each property becomes a C IR function call
--- @param component cdata IRComponent* pointer
--- @param props table Property table
local function applyProperties(component, props)
  if not props then return end

  local style = nil
  local layout = nil

  -- Helper: Ensure style exists
  local function ensureStyle()
    if not style then
      style = C.ir_get_style(component)
      if not style or style == nil then
        style = C.ir_create_style()
        C.ir_set_style(component, style)
      end
    end
    return style
  end

  -- Helper: Ensure layout exists
  local function ensureLayout()
    if not layout then
      layout = C.ir_get_layout(component)
      if not layout or layout == nil then
        layout = C.ir_create_layout()
        C.ir_set_layout(component, layout)
      end
    end
    return layout
  end

  -- IMPORTANT: Process event handlers in FIXED ORDER first to ensure deterministic handler IDs
  -- This prevents handler ID mismatches between compilation and Runtime.loadKIR re-execution
  -- We extract function source for embedding in KIR (used by web codegen)
  local event_handler_keys = {"onClick", "onTextChange", "onDraw", "onUpdate", "onHover", "onFocus"}
  for _, key in ipairs(event_handler_keys) do
    local value = props[key]
    if value ~= nil and type(value) == "function" then
      -- Extract function source for KIR embedding
      local sourceInfo = extractFunctionSource(value)

      if key == "onClick" then
        runtime.registerHandler(component, C.IR_EVENT_CLICK, value, sourceInfo)

      elseif key == "onTextChange" then
        runtime.registerHandler(component, C.IR_EVENT_TEXT_CHANGE, value, sourceInfo)

      elseif key == "onDraw" then
        -- Get canvas_draw event type from plugin registry
        local event_type_id = C.ir_plugin_get_event_type_id("canvas_draw")
        if event_type_id == 0 then
          error("canvas_draw event type not registered - is canvas plugin loaded?")
        end
        runtime.registerHandler(component, event_type_id, value, sourceInfo)

      elseif key == "onUpdate" then
        -- Get canvas_update event type from plugin registry
        local event_type_id = C.ir_plugin_get_event_type_id("canvas_update")
        if event_type_id == 0 then
          error("canvas_update event type not registered - is canvas plugin loaded?")
        end
        runtime.registerHandler(component, event_type_id, value, sourceInfo)

      elseif key == "onHover" then
        runtime.registerHandler(component, C.IR_EVENT_HOVER, value, sourceInfo)

      elseif key == "onFocus" then
        runtime.registerHandler(component, C.IR_EVENT_FOCUS, value, sourceInfo)
      end
    end
  end

  -- Process each property
  for key, value in pairs(props) do
    -- Skip array indices (children)
    if type(key) == "number" then
      goto continue
    end

    -- Skip event handlers (already processed above)
    if key == "onClick" or key == "onTextChange" or key == "onDraw" or key == "onUpdate" or key == "onHover" or key == "onFocus" then
      goto continue
    end

    -- ========== Dimensions ==========
    if key == "width" then
      ensureStyle()
      local dimType, dimValue = parseDimension(value)
      C.ir_set_width(component, dimType, dimValue)

    elseif key == "height" then
      ensureStyle()
      local dimType, dimValue = parseDimension(value)
      C.ir_set_height(component, dimType, dimValue)

    elseif key == "minWidth" then
      local dimType, dimValue = parseDimension(value)
      C.ir_set_min_width(ensureLayout(), dimType, dimValue)

    elseif key == "minHeight" then
      local dimType, dimValue = parseDimension(value)
      C.ir_set_min_height(ensureLayout(), dimType, dimValue)

    elseif key == "maxWidth" then
      local dimType, dimValue = parseDimension(value)
      C.ir_set_max_width(ensureLayout(), dimType, dimValue)

    elseif key == "maxHeight" then
      local dimType, dimValue = parseDimension(value)
      C.ir_set_max_height(ensureLayout(), dimType, dimValue)

    -- ========== Colors ==========
    elseif key == "backgroundColor" or key == "bgColor" or key == "background" then
      local r, g, b, a = parseColor(value)
      C.ir_set_background_color(ensureStyle(), r, g, b, a)

    elseif key == "color" or key == "textColor" then
      local r, g, b, a = parseColor(value)
      -- Check if fontSize is also specified in props to avoid overriding
      local size = 16  -- Default size
      if props.fontSize then
        local _, s = parseDimension(props.fontSize)
        size = s
      end
      -- Text color is set via font
      C.ir_set_font(ensureStyle(), size, nil, r, g, b, a, false, false)

    -- ========== Spacing ==========
    elseif key == "padding" then
      ensureStyle()
      local t, r, b, l = parseSpacing(value)
      C.ir_set_padding(component, t, r, b, l)

    elseif key == "margin" then
      ensureStyle()
      local t, r, b, l = parseSpacing(value)
      C.ir_set_margin(component, t, r, b, l)

    elseif key == "gap" then
      local _, gapValue = parseDimension(value)
      C.ir_set_flexbox(ensureLayout(), false, gapValue, C.IR_ALIGNMENT_START, C.IR_ALIGNMENT_START)

    -- ========== Layout ==========
    elseif key == "alignItems" then
      C.ir_set_align_items(ensureLayout(), parseAlignment(value))

    elseif key == "justifyContent" then
      C.ir_set_justify_content(ensureLayout(), parseAlignment(value))

    elseif key == "alignContent" then
      C.ir_set_align_content(ensureLayout(), parseAlignment(value))

    elseif key == "flexDirection" then
      local direction = 0  -- Default: column (vertical)
      local dirStr = tostring(value):lower()
      if dirStr == "row" or dirStr == "horizontal" then
        direction = 1
      elseif dirStr == "column" or dirStr == "vertical" then
        direction = 0
      end
      -- Set flex properties with direction (grow=0, shrink=0 by default)
      C.ir_set_flex_properties(ensureLayout(), 0, 0, direction)

    -- ========== Text ==========
    elseif key == "text" or key == "content" then
      C.ir_set_text_content(component, tostring(value))

    elseif key == "value" then
      -- For Input components, set the initial text value
      C.ir_set_text_content(component, tostring(value))

    elseif key == "placeholder" then
      -- For Input components, set the placeholder hint text
      C.ir_set_custom_data(component, tostring(value))

    elseif key == "title" then
      -- Tab title is set as text content
      C.ir_set_text_content(component, tostring(value))

    elseif key == "fontSize" then
      local _, size = parseDimension(value)
      -- Check if color is also specified in props to avoid overriding
      local r, g, b, a = 255, 255, 255, 255  -- Default to white
      if props.color or props.textColor then
        r, g, b, a = parseColor(props.color or props.textColor)
      end
      C.ir_set_font(ensureStyle(), size, nil, r, g, b, a, false, false)

    elseif key == "fontWeight" then
      C.ir_set_font_weight(ensureStyle(), tonumber(value) or 400)

    elseif key == "textAlign" then
      C.ir_set_text_align(ensureStyle(), parseTextAlign(value))

    elseif key == "lineHeight" then
      C.ir_set_line_height(ensureStyle(), tonumber(value) or 1.2)

    -- ========== Visual Effects ==========
    elseif key == "opacity" then
      C.ir_set_opacity(ensureStyle(), tonumber(value) or 1.0)

    elseif key == "zIndex" then
      C.ir_set_z_index(ensureStyle(), tonumber(value) or 0)

    elseif key == "visible" then
      C.ir_set_visible(ensureStyle(), value == true)

    elseif key == "disabled" then
      C.ir_set_disabled(component, value == true)

    -- ========== Border ==========
    elseif key == "borderWidth" then
      C.ir_set_border(ensureStyle(), tonumber(value) or 1, 0, 0, 0, 255, 0)

    elseif key == "borderColor" then
      local r, g, b, a = parseColor(value)
      C.ir_set_border(ensureStyle(), 1, r, g, b, a, 0)

    elseif key == "borderRadius" then
      C.ir_set_border(ensureStyle(), 1, 0, 0, 0, 255, tonumber(value) or 0)

    -- ========== Events (already processed in fixed-order loop above) ==========
    -- Event handlers are now processed before this loop to ensure deterministic handler IDs

    end

    ::continue::
  end

  -- ========== Custom Element ID and Data Attributes ==========
  -- These are stored as JSON in custom_data for the HTML generator to parse
  -- Format: {"elementId": "my-id", "data": {"habit": "1"}, "selectedIndex": 0}
  -- Note: Action names are no longer stored here - handler source is in event->handler_source
  local hasCustomAttrs = props.elementId or props.data or props.selectedIndex ~= nil
  if hasCustomAttrs then
    -- Build JSON object
    local json = require("kryon.ffi").cjson
    if json then
      local obj = {}
      if props.elementId then
        obj.elementId = tostring(props.elementId)
      end
      if props.data and type(props.data) == "table" then
        obj.data = {}
        for k, v in pairs(props.data) do
          obj.data[tostring(k)] = tostring(v)
        end
      end
      if props.selectedIndex ~= nil then
        obj.selectedIndex = tonumber(props.selectedIndex) or 0
      end
      -- Encode to JSON string
      local encoded = json.encode(obj)
      if encoded then
        C.ir_set_custom_data(component, encoded)
      end
    else
      -- Fallback: Simple string concatenation if cJSON not available
      local parts = {}
      local needsComma = false
      table.insert(parts, "{")
      if props.elementId then
        table.insert(parts, string.format("\"elementId\":\"%s\"", tostring(props.elementId):gsub('"', '\\"')))
        needsComma = true
      end
      if props.data and type(props.data) == "table" then
        if needsComma then
          table.insert(parts, ",")
        end
        table.insert(parts, "\"data\":{")
        local first = true
        for k, v in pairs(props.data) do
          if not first then table.insert(parts, ",") end
          table.insert(parts, string.format("\"%s\":\"%s\"",
            tostring(k):gsub('"', '\\"'),
            tostring(v):gsub('"', '\\"')))
          first = false
        end
        table.insert(parts, "}")
        needsComma = true
      end
      if props.selectedIndex ~= nil then
        if needsComma then
          table.insert(parts, ",")
        end
        table.insert(parts, string.format("\"selectedIndex\":%d", tonumber(props.selectedIndex) or 0))
      end
      table.insert(parts, "}")
      local jsonStr = table.concat(parts)
      C.ir_set_custom_data(component, jsonStr)
    end
  end

  -- Add children from the children property
  if props.children and type(props.children) == "table" then
    local childCount = 0
    for i, child in ipairs(props.children) do
      if type(child) == "cdata" then  -- It's an IRComponent*
        C.ir_add_child(component, child)
        childCount = childCount + 1

        -- Auto-register with TabGroup context if applicable
        local ctx = getCurrentTabGroupContext()
        if ctx then
          local childType = C.ir_get_component_type(child)

          if childType == C.IR_COMPONENT_TAB_BAR then
            ctx.tabBar = child
          elseif childType == C.IR_COMPONENT_TAB then
            table.insert(ctx.tabs, child)
          elseif childType == C.IR_COMPONENT_TAB_CONTENT then
            ctx.tabContent = child
          elseif childType == C.IR_COMPONENT_TAB_PANEL then
            table.insert(ctx.panels, child)
          end
        end
      end
    end
    local componentType = tonumber(C.ir_get_component_type(component))
    io.stderr:write("[applyProperties] Added " .. childCount .. " children to component type " .. tostring(componentType) .. "\n")
  end
end

--- Build a component from a property table
--- @param componentType number IR component type constant
--- @param props table Property table (can contain children as array elements)
--- @return cdata IRComponent* pointer
local function buildComponent(componentType, props)
  -- Step 1: Create IR component (C call)
  local component = C.ir_create_component(componentType)

  -- Step 1.5: Set module_ref if this component is from an external module
  -- Check if we're inside a wrapped function from another module
  local originModule = getCurrentOriginModule()
  if originModule then
    -- This component was created by a function from originModule
    -- Set module_ref to create a cross-file reference instead of inline expansion
    local currentModule = DSL._currentModule
    if currentModule and currentModule ~= originModule then
      -- For Container components, set module_ref to use $module: syntax
      -- The export_name is inferred from the function that created this
      C.ir_set_component_module_ref(component, originModule, nil)
    end
  end

  -- Step 2: Handle TabGroup initialization
  if componentType == C.IR_COMPONENT_TAB_GROUP then
    -- Create context for this TabGroup
    local context = {
      group = component,
      tabBar = nil,
      tabContent = nil,
      tabs = {},
      panels = {},
      selectedIndex = (props and props.selectedIndex) or 0,
      reorderable = (props and props.reorderable) or false
    }
    pushTabGroupContext(context)
  end

  -- Step 3: Apply all properties (adds children, which auto-register)
  if props then
    applyProperties(component, props)
  end

  -- Step 4: Finalize TabGroup after all children are added
  if componentType == C.IR_COMPONENT_TAB_GROUP then
    local context = popTabGroupContext()

    if context.tabBar and context.tabContent then
      -- Recursively find and register Tab components from TabBar's children
      local function findChildrenOfType(parent, targetType)
        local result = {}
        if not parent or parent == nil then return result end

        local childCount = C.ir_get_child_count(parent)
        for i = 0, childCount - 1 do
          local child = C.ir_get_child_at(parent, i)
          if child and child ~= nil then
            local childType = C.ir_get_component_type(child)
            if childType == targetType then
              table.insert(result, child)
            end
            -- Recursively search child's children too
            local grandchildren = findChildrenOfType(child, targetType)
            for _, gc in ipairs(grandchildren) do
              table.insert(result, gc)
            end
          end
        end
        return result
      end

      -- Find all Button components under TabBar (tabs are buttons in the DSL)
      local tabs = findChildrenOfType(context.tabBar, C.IR_COMPONENT_BUTTON)

      -- Find all TabPanel components under TabContent
      local panels = findChildrenOfType(context.tabContent, C.IR_COMPONENT_TAB_PANEL)

      -- Create TabGroupState
      local state = C.ir_tabgroup_create_state(
        context.group,
        context.tabBar,
        context.tabContent,
        context.selectedIndex,
        context.reorderable
      )

      -- Register TabBar and TabContent with state (critical for rendering!)
      -- This sets tabBar->custom_data = state which the renderer needs
      C.ir_tabgroup_register_bar(state, context.tabBar)
      C.ir_tabgroup_register_content(state, context.tabContent)

      -- Register all tabs AND their visual states
      io.stderr:write(string.format("📊 Found %d tabs to register", #tabs) .. "\n")

      for idx, tab in ipairs(tabs) do
        local tabType = tonumber(C.ir_get_component_type(tab))
        io.stderr:write(string.format("  Tab %d: type=%d", idx, tabType) .. "\n")

        -- Register tab with C core
        C.ir_tabgroup_register_tab(state, tab)

        -- Retrieve visual state from module table
        local key = tostring(tab)
        local visualState = DSL._tabVisualStates[key]

        if visualState then
          -- Convert hex colors to packed RGBA format
          local rawffi = require("ffi")
          local visual = rawffi.new("TabVisualState", {
            background_color = colorToRGBA(visualState.backgroundColor),
            active_background_color = colorToRGBA(visualState.activeBackgroundColor),
            text_color = colorToRGBA(visualState.textColor),
            active_text_color = colorToRGBA(visualState.activeTextColor)
          })

          -- Register visual state with C core (0-indexed!)
          C.ir_tabgroup_set_tab_visual(state, idx - 1, visual)
          io.stderr:write(string.format("  ✓ Registered visual state for tab %d (text: %s -> %s)",
                              idx, visualState.textColor, visualState.activeTextColor))
        else
          io.stderr:write(string.format("  ⚠️ No visual state found for tab %d", idx) .. "\n")
        end
      end

      -- Clean up visual states from module table after registration
      -- This prevents memory leaks if tabs are destroyed and recreated
      for idx, tab in ipairs(tabs) do
        local key = tostring(tab)
        DSL._tabVisualStates[key] = nil
      end

      -- Register all panels
      io.stderr:write("[TabGroup] Registering " .. #panels .. " panels\n")
      for idx, panel in ipairs(panels) do
        local panelChildCount = C.ir_get_child_count(panel)
        io.stderr:write("[TabGroup] Panel " .. idx .. " has " .. panelChildCount .. " children BEFORE registration\n")

        -- Check child types before registration
        for i = 0, panelChildCount - 1 do
          local child = C.ir_get_child_at(panel, i)
          local childType = tonumber(C.ir_get_component_type(child))
          io.stderr:write("[TabGroup]   Child " .. i .. " type=" .. childType .. "\n")
        end

        C.ir_tabgroup_register_panel(state, panel)

        local panelChildCountAfter = C.ir_get_child_count(panel)
        io.stderr:write("[TabGroup] Panel " .. idx .. " has " .. panelChildCountAfter .. " children AFTER registration\n")
      end

      -- Finalize (applies visuals, shows tabs)
      C.ir_tabgroup_finalize(state)

      io.stderr:write(string.format("✅ TabGroup initialized: %d tabs, %d panels, selected=%d",
                          #tabs, #panels, context.selectedIndex))
    else
      io.stderr:write("⚠️ TabGroup missing TabBar or TabContent!\n")
    end
  end

  return component
end

-- ============================================================================
-- Smart Component Factory (Metaprogramming DSL Enhancement)
-- ============================================================================
-- Enables Nim-like syntax by detecting children from array part of table
-- Example:
--   Column {
--     backgroundColor = "#1a1a1a",  -- property (string key)
--     Row { Text {text = "Hello"} } -- child (array index)
--   }

local function SmartComponent(componentType)
  return function(props)
    props = props or {}
    local finalProps = {}
    local children = {}

    -- Separate children (array part) from properties (hash part)
    for k, v in pairs(props) do
      if type(k) == "number" then
        -- Numeric key = child component
        children[k] = v
      else
        -- String key = property
        finalProps[k] = v
      end
    end

    -- If we found children in array part, set them
    if #children > 0 then
      finalProps.children = children
    end

    return buildComponent(componentType, finalProps)
  end
end

-- ============================================================================
-- Component Constructors (Smart DSL)
-- ============================================================================

DSL.Container = SmartComponent(C.IR_COMPONENT_CONTAINER)
DSL.Text = SmartComponent(C.IR_COMPONENT_TEXT)
DSL.Input = SmartComponent(C.IR_COMPONENT_INPUT)
DSL.Checkbox = SmartComponent(C.IR_COMPONENT_CHECKBOX)
DSL.Center = SmartComponent(C.IR_COMPONENT_CENTER)
DSL.Markdown = SmartComponent(C.IR_COMPONENT_MARKDOWN)
DSL.Canvas = SmartComponent(C.IR_COMPONENT_CANVAS)
DSL.Image = SmartComponent(C.IR_COMPONENT_IMAGE)
DSL.Dropdown = SmartComponent(C.IR_COMPONENT_DROPDOWN)

-- Row with default gap for "comfy" spacing
DSL.Row = function(props)
  props = props or {}
  if not props.gap then props.gap = 8 end  -- Default 8px gap
  return SmartComponent(C.IR_COMPONENT_ROW)(props)
end

-- Column with default gap for "comfy" spacing
DSL.Column = function(props)
  props = props or {}
  if not props.gap then props.gap = 8 end  -- Default 8px gap
  return SmartComponent(C.IR_COMPONENT_COLUMN)(props)
end

-- Button with default padding for "comfy" spacing
DSL.Button = function(props)
  props = props or {}
  if not props.padding then props.padding = "8px 16px" end  -- Default padding
  return SmartComponent(C.IR_COMPONENT_BUTTON)(props)
end

-- ============================================================================
-- Modal Component
-- ============================================================================

-- Modal with overlay behavior and centered positioning
-- Usage:
--   UI.Modal {
--     isOpen = state.showModal,
--     onClose = function() state.showModal = false end,
--     title = "Optional Title",
--     width = "400px",
--     height = "300px",
--     children = { ... }
--   }
DSL.Modal = function(props)
  props = props or {}

  -- Default modal styling
  if not props.backgroundColor then props.backgroundColor = "#1a1a1a" end
  if not props.padding then props.padding = 24 end
  if not props.borderRadius then props.borderRadius = 8 end

  -- Extract modal-specific props for custom_data
  local isOpen = props.isOpen
  local onClose = props.onClose
  local title = props.title
  local backdropColor = props.backdropColor

  -- Remove modal-specific props from standard props
  props.isOpen = nil
  props.onClose = nil
  props.title = nil
  props.backdropColor = nil

  -- Create the component
  local comp = SmartComponent(C.IR_COMPONENT_MODAL)(props)

  -- Set up modal state using custom_data string (survives JSON serialization)
  if comp then
    -- Build modal state string: "open|title" or "closed|title"
    local stateStr = (isOpen ~= false) and "open" or "closed"
    if title then
      stateStr = stateStr .. "|" .. title
    end

    -- Use ir_set_custom_data to store the state string
    C.ir_set_custom_data(comp, stateStr)

    -- Register onClose handler as IR_EVENT_CLICK (same as buttons)
    -- This allows C code to properly invoke the handler on backdrop click or ESC
    if onClose then
      runtime.registerHandler(comp, C.IR_EVENT_CLICK, onClose)
    end
  end

  return comp
end

-- ============================================================================
-- Tab Components
-- ============================================================================

-- TabBar with smart DSL and default properties
DSL.TabBar = function(props)
  props = props or {}

  -- TabBar should be horizontal (row) by default, matching JSON parser behavior
  -- (ir_json_v2.c:2646 sets flexDirection=1 for TabBar)
  if not props.flexDirection then
    props.flexDirection = "row"
  end

  -- TabBar should fill width of parent TabGroup
  if not props.width then
    props.width = "100%"
  end

  return SmartComponent(C.IR_COMPONENT_TAB_BAR)(props)
end

function DSL.Tab(props)
  -- Tabs in the Nim DSL are Button components with specific styling
  -- This matches the Nim DSL implementation (components.nim:1745-1912)
  local tabProps = props or {}

  -- Extract tab-specific props
  local title = tabProps.title or "Tab"
  local backgroundColor = tabProps.backgroundColor or "#292C30"
  local activeBackgroundColor = tabProps.activeBackgroundColor or "#3C4047"
  local textColor = tabProps.textColor or "#C7C9CC"
  local activeTextColor = tabProps.activeTextColor or "#FFFFFF"

  -- Set defaults matching Nim DSL tab layout (from components.nim:1816-1824)
  if not tabProps.height then tabProps.height = "32px" end
  if not tabProps.minHeight then tabProps.minHeight = "28px" end
  if not tabProps.minWidth then tabProps.minWidth = "60px" end
  if not tabProps.maxWidth then tabProps.maxWidth = "180px" end
  if not tabProps.alignItems then tabProps.alignItems = "center" end
  if not tabProps.justifyContent then tabProps.justifyContent = "center" end
  if not tabProps.padding then tabProps.padding = "6px 10px" end
  if not tabProps.margin then tabProps.margin = "2px 1px 0px 1px" end
  if not tabProps.borderColor then tabProps.borderColor = "#4C5057" end
  if not tabProps.borderWidth then tabProps.borderWidth = 1 end

  -- Set text from title
  tabProps.text = title

  -- Use backgroundColor for the button (active state colors handled by TabGroup)
  if not tabProps.backgroundColor then
    tabProps.backgroundColor = backgroundColor
  end
  if not tabProps.color then
    tabProps.color = textColor
  end

  -- CRITICAL: Always add onClick to ensure IR_EVENT_CLICK is created
  -- The C core detects tab clicks via try_handle_as_tab_click() which requires
  -- an IR_EVENT_CLICK event to trigger the input handler check
  -- Tab switching happens in C after event detection
  if not tabProps.onClick then
    tabProps.onClick = function() end  -- Dummy handler creates IR_EVENT_CLICK
  end

  -- Create a Button component instead of IR_COMPONENT_TAB
  -- The Nim DSL does this too (components.nim:1848)
  local button = buildComponent(C.IR_COMPONENT_BUTTON, tabProps)

  -- Store visual state in module-level table for later registration
  -- Use tostring() to convert cdata pointer to stable string key
  local key = tostring(button)
  DSL._tabVisualStates[key] = {
    backgroundColor = backgroundColor,
    activeBackgroundColor = activeBackgroundColor,
    textColor = textColor,
    activeTextColor = activeTextColor
  }

  return button
end

DSL.TabContent = SmartComponent(C.IR_COMPONENT_TAB_CONTENT)
DSL.TabGroup = SmartComponent(C.IR_COMPONENT_TAB_GROUP)

-- TabPanel with default padding for "comfy" spacing
DSL.TabPanel = function(props)
  props = props or {}
  if not props.padding then props.padding = 16 end  -- Default 16px padding
  return SmartComponent(C.IR_COMPONENT_TAB_PANEL)(props)
end

-- ============================================================================
-- Helper Functions for Smart DSL
-- ============================================================================

--- Array mapping utility (like JavaScript's Array.map)
--- Useful for generating dynamic children lists
--- @param arr table Array to map
--- @param fn function Mapper function(value, index) -> result
--- @return table Mapped array
function DSL.mapArray(arr, fn)
  local result = {}
  for i, v in ipairs(arr) do
    result[i] = fn(v, i)
  end
  return result
end

--- Lua 5.1/5.2 compatibility for unpacking arrays
--- Use this to spread mapArray results into children
--- Example: Row { unpack(mapArray(days, function(d) return Button{text=d} end)) }
DSL.unpack = unpack or table.unpack

--- ============================================================================
--- ForEach Component (Dynamic List Rendering)
--- ============================================================================

-- Helper: Recursively serialize a Lua value to JSON string
-- Handles nested tables, arrays, strings, numbers, booleans, and nil
local function serialize_json_value(v)
  local t = type(v)
  if t == "string" then
    return '"' .. v:gsub('"', '\\"'):gsub('\\', '\\\\') .. '"'
  elseif t == "number" then
    return tostring(v)
  elseif t == "boolean" then
    return tostring(v)
  elseif t == "nil" then
    return "null"
  elseif t == "table" then
    -- Check if it's an array (consecutive integer keys starting at 1)
    local is_array = #v > 0
    local max_key = 0
    for k in pairs(v) do
      if type(k) ~= "number" or k <= 0 or k % 1 ~= 0 then
        is_array = false
        break
      end
      if k > max_key then max_key = k end
    end
    if is_array and max_key == #v then
      -- Array: serialize as [val1, val2, ...]
      local parts = {}
      for i = 1, #v do
        table.insert(parts, serialize_json_value(v[i]))
      end
      return "[" .. table.concat(parts, ",") .. "]"
    else
      -- Object: serialize as {"key":val, ...}
      local parts = {}
      for k, val in pairs(v) do
        if type(k) == "string" then
          table.insert(parts, serialize_json_value(k) .. ":" .. serialize_json_value(val))
        end
      end
      return "{" .. table.concat(parts, ",") .. "}"
    end
  else
    -- Unknown type, serialize as null
    return "null"
  end
end

--- ForEach: dynamic list rendering with build-time evaluation and runtime reactivity
--- Usage:
---   UI.ForEach {
---     each = state.calendarDays,  -- Source array to iterate
---     as = "day",                  -- Variable name for each item
---     index = "i",                 -- Variable name for index (optional)
---     render = function(day, i)   -- Render function for each item
---       return Button { text = day.dayNumber }
---     end
---   }
--- @param props table ForEach properties
--- @return cdata IRComponent* pointer
DSL.ForEach = function(props)
  if not props then
    error("ForEach requires props table")
  end

  local each_source = props.each or props.source
  local item_name = props.as or props.itemName or "item"
  local index_name = props.index or props.indexName or "index"
  local render_fn = props.render
  -- New: 'expression' property provides a template reference instead of serializing data
  local source_expr = props.expression or props.sourceExpr or props.var

  if not each_source then
    error("ForEach requires 'each' property (source array)")
  end
  if not render_fn then
    error("ForEach requires 'render' function")
  end
  if type(render_fn) ~= "function" then
    error("ForEach 'render' must be a function")
  end

  -- ForEach is ALWAYS runtime/reactive - serialize the source for runtime expansion
  -- This allows re-rendering when the source data changes (e.g., month navigation)

  -- Build template child (render function called with first item for structure)
  local template_child
  if type(each_source) == "table" and #each_source > 0 then
    template_child = render_fn(each_source[1], 1)
  elseif type(each_source) == "function" then
    local success, result = pcall(each_source)
    if success and type(result) == "table" and #result > 0 then
      template_child = render_fn(result[1], 1)
    else
      template_child = render_fn({}, 1)
    end
  else
    template_child = render_fn({}, 1)
  end

  -- ForEach has exactly one child: the template to be expanded at runtime
  local children = {template_child}

  -- Create ForEach component with template child
  local forEachProps = {
    children = children
  }

  -- Create the ForEach component using buildComponent
  local component = buildComponent(C.IR_COMPONENT_FOR_EACH, forEachProps)

  -- Set ForEach-specific fields directly on the C struct
  -- Now that the FFI struct has correct field offsets, we can set these directly
  -- Use ir_set_custom_data for now since we need proper memory allocation
  local encoded = string.format('{"forEach":true,"each_item_name":"%s","each_index_name":"%s"',
                                 item_name:gsub('"', '\\"'),
                                 index_name:gsub('"', '\\"'))

  -- Serialize each_source for runtime expansion
  -- Priority: 1) expression (template reference), 2) string (variable name), 3) table (data)
  if source_expr and type(source_expr) == "string" then
    -- Store expression reference for template KIR (not the evaluated data)
    encoded = encoded .. string.format(',"each_source":"%s","each_source_is_expr":true', source_expr:gsub('"', '\\"'))
  elseif type(each_source) == "string" then
    -- Variable reference (e.g., "state.items")
    encoded = encoded .. string.format(',"each_source":"%s"', each_source:gsub('"', '\\"'))
  elseif type(each_source) == "table" then
    -- Check if we're in multi-file/template mode
    -- In template mode, don't serialize table data - use a reference marker instead
    local template_mode = os.getenv("KRYON_TEMPLATE_MODE")
    if template_mode == "1" then
      -- Template mode: store a reference marker for runtime re-evaluation
      encoded = encoded .. string.format(',"each_source":"__dynamic__","each_source_is_expr":true')
    else
      -- Desktop mode: ALWAYS serialize the actual table data
      -- This ensures ForEach components are expanded at compile-time for desktop rendering
      -- The data (like calendar rows) is already computed at build-time
      encoded = encoded .. ',"each_source":' .. serialize_json_value(each_source)
    end
  elseif type(each_source) == "function" then
    -- Function source - store marker for runtime evaluation
    encoded = encoded .. string.format(',"each_source":"__function"')
  end
  encoded = encoded .. "}"
  C.ir_set_custom_data(component, encoded)

  return component
end

return DSL
