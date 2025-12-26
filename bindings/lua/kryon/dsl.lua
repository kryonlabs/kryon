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
local tabVisualStates = {}

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

--- Parse dimension string like "100px", "50%", "auto", "2fr"
--- @param value any Dimension value (number, string, or ReactiveState)
--- @return number dimensionType
--- @return number dimensionValue
local function parseDimension(value)
  -- Handle reactive state
  if type(value) == "table" and value.get then
    return parseDimension(value:get())
  end

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
--- @param color string|table Color value or ReactiveState
--- @return number r
--- @return number g
--- @return number b
--- @return number a
local function parseColor(color)
  -- Handle reactive state
  if type(color) == "table" and color.get then
    return parseColor(color:get())
  end

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

  -- Process each property
  for key, value in pairs(props) do
    -- Skip array indices (children)
    if type(key) == "number" then
      goto continue
    end

    -- ========== Dimensions ==========
    if key == "width" then
      local dimType, dimValue = parseDimension(value)
      C.ir_set_width(ensureStyle(), dimType, dimValue)

    elseif key == "height" then
      local dimType, dimValue = parseDimension(value)
      C.ir_set_height(ensureStyle(), dimType, dimValue)

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
    elseif key == "backgroundColor" or key == "bgColor" then
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
      local t, r, b, l = parseSpacing(value)
      C.ir_set_padding(ensureStyle(), t, r, b, l)

    elseif key == "margin" then
      local t, r, b, l = parseSpacing(value)
      C.ir_set_margin(ensureStyle(), t, r, b, l)

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

    -- ========== Border ==========
    elseif key == "borderWidth" then
      C.ir_set_border(ensureStyle(), tonumber(value) or 1, 0, 0, 0, 255, 0)

    elseif key == "borderColor" then
      local r, g, b, a = parseColor(value)
      C.ir_set_border(ensureStyle(), 1, r, g, b, a, 0)

    elseif key == "borderRadius" then
      C.ir_set_border(ensureStyle(), 1, 0, 0, 0, 255, tonumber(value) or 0)

    -- ========== Events ==========
    elseif key == "onClick" and type(value) == "function" then
      print("📝 Registering onClick handler for component")
      local handlerId = runtime.registerHandler(component, C.IR_EVENT_CLICK, value)
      print(string.format("   Handler ID: %d", handlerId))

    elseif key == "onHover" and type(value) == "function" then
      runtime.registerHandler(component, C.IR_EVENT_HOVER, value)

    elseif key == "onFocus" and type(value) == "function" then
      runtime.registerHandler(component, C.IR_EVENT_FOCUS, value)

    elseif key == "onTextChange" and type(value) == "function" then
      print("📝 Registering onTextChange handler for Input component")
      local handlerId = runtime.registerHandler(component, C.IR_EVENT_TEXT_CHANGE, value)
      print(string.format("   Handler ID: %d", handlerId))

    end

    ::continue::
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
            print("📝 Registered TabBar with context")
          elseif childType == C.IR_COMPONENT_TAB then
            table.insert(ctx.tabs, child)
            print(string.format("📝 Registered Tab #%d with context", #ctx.tabs))
          elseif childType == C.IR_COMPONENT_TAB_CONTENT then
            ctx.tabContent = child
            print("📝 Registered TabContent with context")
          elseif childType == C.IR_COMPONENT_TAB_PANEL then
            table.insert(ctx.panels, child)
            print(string.format("📝 Registered TabPanel #%d with context", #ctx.panels))
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
    print("📝 Pushing TabGroup context")
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
      print(string.format("📊 Found %d tabs to register", #tabs))

      for idx, tab in ipairs(tabs) do
        local tabType = tonumber(C.ir_get_component_type(tab))
        print(string.format("  Tab %d: type=%d", idx, tabType))

        -- Register tab with C core
        C.ir_tabgroup_register_tab(state, tab)

        -- Retrieve visual state from module table
        local key = tostring(tab)
        local visualState = tabVisualStates[key]

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
          print(string.format("  ✓ Registered visual state for tab %d (text: %s -> %s)",
                              idx, visualState.textColor, visualState.activeTextColor))
        else
          print(string.format("  ⚠️ No visual state found for tab %d", idx))
        end
      end

      -- Clean up visual states from module table after registration
      -- This prevents memory leaks if tabs are destroyed and recreated
      for idx, tab in ipairs(tabs) do
        local key = tostring(tab)
        tabVisualStates[key] = nil
      end

      -- Register all panels
      for _, panel in ipairs(panels) do
        C.ir_tabgroup_register_panel(state, panel)
      end

      -- Finalize (applies visuals, shows tabs)
      C.ir_tabgroup_finalize(state)

      print(string.format("✅ TabGroup initialized: %d tabs, %d panels, selected=%d",
                          #tabs, #panels, context.selectedIndex))
    else
      print("⚠️ TabGroup missing TabBar or TabContent!")
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
-- Component Constructors (Original API - Backward Compatible)
-- ============================================================================

function DSL.Container(props)
  return buildComponent(C.IR_COMPONENT_CONTAINER, props)
end

function DSL.Text(props)
  return buildComponent(C.IR_COMPONENT_TEXT, props)
end

function DSL.Button(props)
  return buildComponent(C.IR_COMPONENT_BUTTON, props)
end

function DSL.Input(props)
  return buildComponent(C.IR_COMPONENT_INPUT, props)
end

function DSL.Checkbox(props)
  return buildComponent(C.IR_COMPONENT_CHECKBOX, props)
end

function DSL.Row(props)
  return buildComponent(C.IR_COMPONENT_ROW, props)
end

function DSL.Column(props)
  return buildComponent(C.IR_COMPONENT_COLUMN, props)
end

function DSL.Center(props)
  return buildComponent(C.IR_COMPONENT_CENTER, props)
end

function DSL.Markdown(props)
  return buildComponent(C.IR_COMPONENT_MARKDOWN, props)
end

function DSL.Canvas(props)
  return buildComponent(C.IR_COMPONENT_CANVAS, props)
end

function DSL.Image(props)
  return buildComponent(C.IR_COMPONENT_IMAGE, props)
end

function DSL.Dropdown(props)
  return buildComponent(C.IR_COMPONENT_DROPDOWN, props)
end

-- ============================================================================
-- Tab Components
-- ============================================================================

function DSL.TabBar(props)
  local tabBarProps = props or {}

  -- TabBar should be horizontal (row) by default, matching JSON parser behavior
  -- (ir_json_v2.c:2646 sets flexDirection=1 for TabBar)
  if not tabBarProps.flexDirection then
    tabBarProps.flexDirection = "row"
  end

  -- TabBar should fill width of parent TabGroup
  if not tabBarProps.width then
    tabBarProps.width = "100%"
  end

  return buildComponent(C.IR_COMPONENT_TAB_BAR, tabBarProps)
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
  if not tabProps.minWidth then tabProps.minWidth = "16px" end
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
  -- Tab switching requires click events even if user doesn't provide onClick
  -- The desktop backend will detect this is a tab and call ir_tabgroup_handle_tab_click()
  if not tabProps.onClick then
    tabProps.onClick = function() end  -- Dummy handler to trigger event creation
  end

  -- Create a Button component instead of IR_COMPONENT_TAB
  -- The Nim DSL does this too (components.nim:1848)
  local button = buildComponent(C.IR_COMPONENT_BUTTON, tabProps)

  -- Store visual state in module-level table for later registration
  -- Use tostring() to convert cdata pointer to stable string key
  local key = tostring(button)
  tabVisualStates[key] = {
    backgroundColor = backgroundColor,
    activeBackgroundColor = activeBackgroundColor,
    textColor = textColor,
    activeTextColor = activeTextColor
  }

  return button
end

function DSL.TabContent(props)
  return buildComponent(C.IR_COMPONENT_TAB_CONTENT, props)
end

function DSL.TabPanel(props)
  return buildComponent(C.IR_COMPONENT_TAB_PANEL, props)
end

function DSL.TabGroup(props)
  return buildComponent(C.IR_COMPONENT_TAB_GROUP, props)
end

-- ============================================================================
-- Smart Component Wrappers (Nim-like DSL Syntax)
-- ============================================================================
-- These provide cleaner syntax where children are in array part of props table

DSL.Container_ = SmartComponent(C.IR_COMPONENT_CONTAINER)
DSL.Text_ = SmartComponent(C.IR_COMPONENT_TEXT)
DSL.Button_ = SmartComponent(C.IR_COMPONENT_BUTTON)
DSL.Input_ = SmartComponent(C.IR_COMPONENT_INPUT)
DSL.Checkbox_ = SmartComponent(C.IR_COMPONENT_CHECKBOX)
DSL.Row_ = SmartComponent(C.IR_COMPONENT_ROW)
DSL.Column_ = SmartComponent(C.IR_COMPONENT_COLUMN)
DSL.Center_ = SmartComponent(C.IR_COMPONENT_CENTER)
DSL.Markdown_ = SmartComponent(C.IR_COMPONENT_MARKDOWN)
DSL.Canvas_ = SmartComponent(C.IR_COMPONENT_CANVAS)
DSL.Image_ = SmartComponent(C.IR_COMPONENT_IMAGE)
DSL.Dropdown_ = SmartComponent(C.IR_COMPONENT_DROPDOWN)
DSL.TabBar_ = SmartComponent(C.IR_COMPONENT_TAB_BAR)
DSL.TabContent_ = SmartComponent(C.IR_COMPONENT_TAB_CONTENT)
DSL.TabPanel_ = SmartComponent(C.IR_COMPONENT_TAB_PANEL)
DSL.TabGroup_ = SmartComponent(C.IR_COMPONENT_TAB_GROUP)

-- Tab is special - it needs custom logic, so we'll keep using DSL.Tab directly
-- but users can alias it if needed

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
