-- Kryon DSL Layer
-- Transforms Lua tables into C IR function calls
-- Every property set here becomes a direct C call - zero abstraction

local ffi = require("kryon.ffi")
local runtime = require("kryon.runtime")
local C = ffi.C

local DSL = {}

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

    -- ========== Text ==========
    elseif key == "text" or key == "content" then
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
      runtime.registerHandler(component, C.IR_EVENT_CLICK, value)

    elseif key == "onHover" and type(value) == "function" then
      runtime.registerHandler(component, C.IR_EVENT_HOVER, value)

    elseif key == "onFocus" and type(value) == "function" then
      runtime.registerHandler(component, C.IR_EVENT_FOCUS, value)

    end

    ::continue::
  end

  -- Add children from the children property
  if props.children and type(props.children) == "table" then
    for i, child in ipairs(props.children) do
      if type(child) == "cdata" then  -- It's an IRComponent*
        C.ir_add_child(component, child)
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

  -- Step 2: Apply all properties (each becomes a C call)
  if props then
    applyProperties(component, props)
  end

  return component
end

-- ============================================================================
-- Component Constructors
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

return DSL
