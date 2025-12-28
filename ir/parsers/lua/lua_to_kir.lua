--[[
  Kryon Lua → KIR Parser (Execution-Based)

  Parses Lua DSL by executing code with mocked UI/Reactive modules
  that capture component creation instead of building real components.
]]

local Parser = {}

-- Simple JSON encoder
local function encode_json(val, indent)
  indent = indent or 0
  local spaces = string.rep("  ", indent)

  if type(val) == "table" then
    if #val > 0 then
      -- Array
      local items = {}
      for i, v in ipairs(val) do
        table.insert(items, encode_json(v, indent + 1))
      end
      return "[\n" .. spaces .. "  " .. table.concat(items, ",\n" .. spaces .. "  ") .. "\n" .. spaces .. "]"
    else
      -- Object
      local items = {}
      for k, v in pairs(val) do
        if type(k) == "string" then
          table.insert(items, spaces .. "  \"" .. k .. "\": " .. encode_json(v, indent + 1))
        end
      end
      return "{\n" .. table.concat(items, ",\n") .. "\n" .. spaces .. "}"
    end
  elseif type(val) == "string" then
    return "\"" .. val:gsub('"', '\\"'):gsub("\n", "\\n") .. "\""
  elseif type(val) == "number" then
    return tostring(val)
  elseif type(val) == "boolean" then
    return tostring(val)
  else
    return "null"
  end
end

-- Extract function source code from debug info
local function extractFunctionSource(func)
  local info = debug.getinfo(func, "S")

  if not info or not info.source then
    return "function() end"
  end

  -- For inline functions, try to extract from source
  if info.source:match("^@") then
    -- Function defined in a file
    local filepath = info.source:sub(2)
    local file = io.open(filepath, "r")
    if not file then return "function() end" end

    local lines = {}
    for line in file:lines() do
      table.insert(lines, line)
    end
    file:close()

    if info.linedefined and info.lastlinedefined then
      local sourceLines = {}
      for i = info.linedefined, info.lastlinedefined do
        if lines[i] then
          table.insert(sourceLines, lines[i])
        end
      end
      return table.concat(sourceLines, "\n")
    end
  end

  -- Fallback for inline/string sources
  return info.source or "function() end"
end

-- Create mocked DSL that captures component creation
local function createMockDSL()
  local components = {}
  local nextId = 1
  local reactiveStates = {}
  local functions = {}
  local eventBindings = {}

  local MockUI = {}
  local MockReactive = {}

  -- Mock Reactive.state()
  function MockReactive.state(initialValue)
    local stateId = "state_" .. nextId
    nextId = nextId + 1

    -- Try to get variable name from caller scope
    local varName = "state" .. stateId
    local i = 1
    while true do
      local name, val = debug.getlocal(2, i)
      if not name then break end
      if val == nil then  -- This is the assignment target
        varName = name
        break
      end
      i = i + 1
    end

    table.insert(reactiveStates, {
      id = stateId,
      name = varName,
      type = type(initialValue),
      initial_value = tostring(initialValue)
    })

    -- Return mock state object
    return {
      _id = stateId,
      _value = initialValue,
      _type = "state",
      get = function(self) return self._value end,
      set = function(self, val) self._value = val end,
      update = function(self, fn) self._value = fn(self._value) end
    }
  end

  -- Mock Reactive.computed()
  function MockReactive.computed(dependencies, computeFn)
    local computedId = "computed_" .. nextId
    nextId = nextId + 1

    local depIds = {}
    for _, dep in ipairs(dependencies) do
      if type(dep) == "table" and dep._id then
        table.insert(depIds, dep._id)
      end
    end

    -- Extract function source
    local source = extractFunctionSource(computeFn)

    table.insert(reactiveStates, {
      id = computedId,
      type = "computed",
      dependencies = depIds,
      expression = source
    })

    return {
      _id = computedId,
      _type = "computed",
      get = function() return nil end
    }
  end

  -- Mock Reactive.array()
  function MockReactive.array(items)
    local arrayId = "array_" .. nextId
    nextId = nextId + 1

    table.insert(reactiveStates, {
      id = arrayId,
      type = "array",
      items = items or {}
    })

    return {
      _id = arrayId,
      _type = "array",
      _items = items or {},
      getItems = function(self) return self._items end,
      push = function(self, item) table.insert(self._items, item) end,
      remove = function(self, index) table.remove(self._items, index) end
    }
  end

  -- Mock component factory
  local function mockComponent(componentType)
    return function(props)
      local comp = {
        id = nextId,
        type = componentType,
        children = {}
      }
      nextId = nextId + 1

      if not props then
        table.insert(components, comp)
        return comp
      end

      -- Separate properties, events, and children
      for key, value in pairs(props) do
        if type(key) == "number" then
          -- Numeric key = child component
          table.insert(comp.children, value)
        elseif type(key) == "string" and key:match("^on[A-Z]") then
          -- Event handler (onClick, onHover, etc.)
          local handlerName = "handler_" .. #functions + 1 .. "_" .. key:sub(3):lower()

          -- Extract function source
          local source = extractFunctionSource(value)

          table.insert(functions, {
            name = handlerName,
            sources = {{
              language = "lua",
              source = source
            }}
          })

          table.insert(eventBindings, {
            component_id = comp.id,
            event_type = key:sub(3):lower(), -- onClick -> click
            handler_name = handlerName
          })
        else
          -- Regular property
          comp[key] = value
        end
      end

      table.insert(components, comp)
      return comp
    end
  end

  -- Create mock constructors for all component types
  MockUI.Container = mockComponent("Container")
  MockUI.Column = mockComponent("Column")
  MockUI.Row = mockComponent("Row")
  MockUI.Text = mockComponent("Text")
  MockUI.Button = mockComponent("Button")
  MockUI.Input = mockComponent("Input")
  MockUI.Checkbox = mockComponent("Checkbox")
  MockUI.Center = mockComponent("Center")
  MockUI.Canvas = mockComponent("Canvas")
  MockUI.Image = mockComponent("Image")
  MockUI.Markdown = mockComponent("Markdown")
  MockUI.Dropdown = mockComponent("Dropdown")

  -- Mock mapArray for dynamic children
  function MockUI.mapArray(items, mapFn)
    local result = {}
    for i, item in ipairs(items) do
      table.insert(result, mapFn(item, i))
    end
    return result
  end

  return {
    UI = MockUI,
    Reactive = MockReactive,
    components = components,
    reactiveStates = reactiveStates,
    functions = functions,
    eventBindings = eventBindings
  }
end

-- Parse Lua code to KIR
function Parser.parse(luaCode, filename)
  filename = filename or "stdin"

  local mockDSL = createMockDSL()

  -- Create sandboxed environment with mocked modules
  local env = {
    -- Mocked require
    require = function(module)
      if module == "kryon.dsl" or module == "kryon.ui" then
        return mockDSL.UI
      end
      if module == "kryon.reactive" then
        return mockDSL.Reactive
      end
      -- Allow standard library
      return require(module)
    end,

    -- Suppress output
    print = function() end,
    io = { write = function() end },

    -- Standard library
    pairs = pairs,
    ipairs = ipairs,
    next = next,
    type = type,
    tostring = tostring,
    tonumber = tonumber,
    table = table,
    string = string,
    math = math,
    os = { date = os.date, time = os.time },

    -- Allow basic globals
    _VERSION = _VERSION,
    assert = assert,
    error = error,
    pcall = pcall,
    xpcall = xpcall,
    select = select,
    unpack = unpack or table.unpack
  }

  -- Set environment metatable for globals
  setmetatable(env, { __index = _G })

  -- Load and execute Lua code in mocked environment
  local chunk, loadErr = load(luaCode, filename, "t", env)
  if not chunk then
    error("Failed to load Lua code: " .. tostring(loadErr))
  end

  local success, result = pcall(chunk)
  if not success then
    error("Failed to execute Lua code: " .. tostring(result))
  end

  -- Find root component
  local root = nil
  if type(result) == "table" and result.root then
    root = result.root
  elseif type(result) == "table" and result.id then
    root = result
  elseif #mockDSL.components > 0 then
    root = mockDSL.components[#mockDSL.components]
  end

  -- Build KIR JSON
  local kir = {
    format = "kir",
    metadata = {
      source_language = "lua",
      source_file = filename,
      compiler_version = "kryon-lua-parser-1.0.0"
    }
  }

  -- Add logic_block if we have functions or event bindings
  if #mockDSL.functions > 0 or #mockDSL.eventBindings > 0 then
    kir.logic_block = {
      functions = mockDSL.functions,
      event_bindings = mockDSL.eventBindings
    }
  end

  -- Add reactive manifest if we have state
  if #mockDSL.reactiveStates > 0 then
    kir.reactive_manifest = {
      states = mockDSL.reactiveStates
    }
  end

  -- Add root component
  if root then
    kir.root = root
  end

  return encode_json(kir, 0)
end

return Parser
