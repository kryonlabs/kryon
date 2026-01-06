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
--- @param fn function The function to extract source from
--- @return table|nil {code=string, file=string, line=number} or nil
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
  -- Patterns like "toggleHabitCompletion(habitIndex, day.date)" indicate closure usage
  local usesHabitIndex = fullCode:match("habitIndex") or fullCode:match("%w+Index")
  local usesDayDate = fullCode:match("day%.date")
  local usesClosure = usesHabitIndex or usesDayDate

  if usesClosure then
    -- Add a wrapper that reads from component data attributes instead
    -- This replaces the closure variables with data attribute lookups
    -- Extract function body by stripping "function(...) ... end" wrapper

    -- First, remove "function" prefix
    local funcStart = fullCode:find("function")
    if funcStart then
      fullCode = fullCode:sub(funcStart + 8)  -- Skip "function"
    end

    -- Skip the opening paren and any params until we find the matching closing paren
    if fullCode:sub(1, 1) == "(" then
      local depth = 1
      local paramEnd = 1
      for i = 2, #fullCode do
        local c = fullCode:sub(i, i)
        if c == "(" then depth = depth + 1
        elseif c == ")" then depth = depth - 1 end
        if depth == 0 then
          paramEnd = i
          break
        end
      end
      fullCode = fullCode:sub(paramEnd + 1)
    end

    -- Now remove the trailing "end" statement
    -- It might be on its own line, or at the end of a line with content
    -- Pattern: "end" at end, possibly preceded by content, followed by optional whitespace/newline

    -- Trim trailing whitespace and newlines first
    fullCode = fullCode:gsub("%s*$", "")

    -- Now check if last 3 chars are "end"
    if fullCode:sub(-3) == "end" then
      fullCode = fullCode:sub(1, -4)
    end

    -- Also handle case where there's content before "end" on same line
    local endPos = fullCode:find("end%s*$")
    if endPos and endPos == #fullCode - 2 then
      fullCode = fullCode:sub(1, endPos - 1)
    end

    -- Trim trailing whitespace again
    fullCode = fullCode:gsub("%s*$", "")

    -- Substitute day.date with dateStr for handlers that use it
    fullCode = fullCode:gsub("day%.date", "dateStr")

    -- Add trailing newline to ensure proper separation from wrapper's 'end'
    fullCode = fullCode .. "\n"

    local wrapped = [[function()
      local js = require("js")
      local element = js.global.__kryon_get_event_element()
      local data = element and (element.data or (element.getData and element:getData()))
      local habitIndex = tonumber(data and data.habit)
      local dateStr = data and data.date
    ]] .. fullCode .. [[
    end]]
    fullCode = wrapped
  end

  return {
    code = fullCode,
    file = filename,
    line = info.linedefined
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
  -- Format: {"elementId": "my-id", "data": {"habit": "1"}}
  -- Note: Action names are no longer stored here - handler source is in event->handler_source
  local hasCustomAttrs = props.elementId or props.data
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
      end
      table.insert(parts, "}")
      local jsonStr = table.concat(parts)
      C.ir_set_custom_data(component, jsonStr)
    end
  end

  -- Add children from the children property
  if props.children and type(props.children) == "table" then
    for i, child in ipairs(props.children) do
      if type(child) == "cdata" then  -- It's an IRComponent*
        C.ir_add_child(component, child)

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
  end
end

--- Build a component from a property table
--- @param componentType number IR component type constant
--- @param props table Property table (can contain children as array elements)
--- @return cdata IRComponent* pointer
local function buildComponent(componentType, props)
  -- Step 1: Create IR component (C call)
  local component = C.ir_create_component(componentType)

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
      for _, panel in ipairs(panels) do
        C.ir_tabgroup_register_panel(state, panel)
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

return DSL
