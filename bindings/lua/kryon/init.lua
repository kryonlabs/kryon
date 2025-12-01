-- Kryon Lua Frontend
-- Main entry point that exports the public API
--
-- Usage:
--   local kryon = require("kryon")
--   local app = kryon.app { ... }

local ffi = require("kryon.ffi")
local runtime = require("kryon.runtime")
local reactive = require("kryon.reactive")
local dsl = require("kryon.dsl")

local Kryon = {}

-- ============================================================================
-- Core API
-- ============================================================================

--- Create a Kryon application
--- @param config table Configuration with window and body
--- @return table Application object
function Kryon.app(config)
  return runtime.createApp(config)
end

--- Initialize Kryon context (usually not needed - app() does this)
--- @return cdata IRContext* pointer
function Kryon.init()
  local ctx = ffi.C.ir_create_context()
  ffi.C.ir_set_context(ctx)
  return ctx
end

-- ============================================================================
-- Component Constructors (from DSL)
-- ============================================================================

Kryon.Container = dsl.Container
Kryon.Text = dsl.Text
Kryon.Button = dsl.Button
Kryon.Input = dsl.Input
Kryon.Checkbox = dsl.Checkbox
Kryon.Row = dsl.Row
Kryon.Column = dsl.Column
Kryon.Center = dsl.Center
Kryon.Markdown = dsl.Markdown
Kryon.Canvas = dsl.Canvas
Kryon.Image = dsl.Image
Kryon.Dropdown = dsl.Dropdown

-- ============================================================================
-- Reactive State (from reactive module)
-- ============================================================================

Kryon.state = reactive.state
Kryon.computed = reactive.computed
Kryon.array = reactive.array
Kryon.effect = reactive.effect
Kryon.batch = reactive.batch

-- ============================================================================
-- Runtime Functions
-- ============================================================================

Kryon.update = runtime.update
Kryon.render = runtime.render
Kryon.destroyApp = runtime.destroyApp

-- IR Serialization
Kryon.saveIR = runtime.saveIR
Kryon.loadIR = runtime.loadIR

-- ============================================================================
-- Debug Utilities
-- ============================================================================

Kryon.debugPrintTree = runtime.debugPrintTree
Kryon.debugPrintTreeToFile = runtime.debugPrintTreeToFile

-- ============================================================================
-- Low-level Access (for advanced usage)
-- ============================================================================

Kryon.ffi = ffi
Kryon.runtime = runtime
Kryon.reactive = reactive
Kryon.dsl = dsl

-- Direct access to C functions
Kryon.C = ffi.C

-- Constants
Kryon.ComponentType = ffi.ComponentType
Kryon.DimensionType = ffi.DimensionType
Kryon.Alignment = ffi.Alignment
Kryon.TextAlign = ffi.TextAlign
Kryon.EventType = ffi.EventType

-- ============================================================================
-- Version Information
-- ============================================================================

Kryon.VERSION = "0.1.0"
Kryon.LUA_BINDING_VERSION = "0.1.0"

function Kryon.version()
  print("Kryon Lua Bindings v" .. Kryon.LUA_BINDING_VERSION)
  print("IR Core Library: " .. ffi.lib_name)
  print("LuaJIT version: " .. jit.version)
end

return Kryon
