--[[
  NativeCanvas Component for Lua/LuaJIT

  Provides a canvas element that allows direct access to the active rendering backend.
  Users can call backend-specific rendering functions (raylib, SDL3, etc.) within
  the onRender callback.

  Example usage with raylib:
  ```lua
  local NativeCanvas = require("native_canvas")
  local rl = require("raylib")  -- Use external raylib bindings

  local canvas = NativeCanvas {
    width = 800,
    height = 600,
    onRender = function()
      -- Call raylib functions directly
      rl.BeginMode3D(camera)
      rl.DrawCube(rl.Vector3(0, 0, 0), 1.0, 1.0, 1.0, rl.RED)
      rl.EndMode3D()
    end
  }
  ```
--]]

local ffi = require("ffi")

-- ============================================================================
-- C FFI Declarations
-- ============================================================================

ffi.cdef[[
  // Forward declarations
  typedef struct IRComponent IRComponent;
  typedef struct IRStyle IRStyle;

  // NativeCanvas data structure
  typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t background_color;
    void (*render_callback)(uint32_t component_id);
    void* user_data;
  } IRNativeCanvasData;

  // NativeCanvas C API
  IRComponent* ir_create_native_canvas(uint16_t width, uint16_t height);
  void ir_native_canvas_set_callback(uint32_t component_id, void (*callback)(uint32_t));
  void ir_native_canvas_set_user_data(uint32_t component_id, void* user_data);
  void* ir_native_canvas_get_user_data(uint32_t component_id);
  bool ir_native_canvas_invoke_callback(uint32_t component_id);
  void ir_native_canvas_set_background_color(uint32_t component_id, uint32_t color);
  IRNativeCanvasData* ir_native_canvas_get_data(IRComponent* component);

  // Component structure (minimal definition)
  struct IRComponent {
    uint32_t id;
    // ... other fields omitted
  };
]]

-- Load the IR library
local ir_lib = ffi.C  -- Assumes symbols are already linked

-- ============================================================================
-- Callback Registry
-- ============================================================================

-- Global table mapping component IDs to Lua callbacks
local g_callbacks = {}

-- C callback bridge (called by C code)
local function native_canvas_callback_bridge_impl(component_id)
  local callback = g_callbacks[tonumber(component_id)]
  if callback then
    -- Call the Lua callback
    local success, err = pcall(callback)
    if not success then
      print("[NativeCanvas] Error in callback:", err)
    end
  end
end

-- Create a C callback that can be passed to C
local c_callback_bridge = ffi.cast("void (*)(uint32_t)", native_canvas_callback_bridge_impl)

-- ============================================================================
-- Nim API
-- ============================================================================

local NativeCanvas = {}

--- Register a Lua callback for a NativeCanvas component
--- @param component_id number The component ID
--- @param callback function The callback function to register
function NativeCanvas.registerCallback(component_id, callback)
  g_callbacks[tonumber(component_id)] = callback
end

--- Create a NativeCanvas component
--- @param opts table Options table with width, height, backgroundColor, onRender
--- @return cdata The created IR component
function NativeCanvas.create(opts)
  opts = opts or {}

  local width = opts.width or 800
  local height = opts.height or 600
  local backgroundColor = opts.backgroundColor or 0x000000FF
  local onRender = opts.onRender

  -- Create the native canvas component
  local comp = ir_lib.ir_create_native_canvas(width, height)

  if comp == nil then
    error("[NativeCanvas] Failed to create component")
  end

  -- Set background color if specified
  if backgroundColor ~= 0x000000FF then
    ir_lib.ir_native_canvas_set_background_color(comp.id, backgroundColor)
  end

  -- Register callback if provided
  if onRender ~= nil then
    NativeCanvas.registerCallback(comp.id, onRender)
    ir_lib.ir_native_canvas_set_callback(comp.id, c_callback_bridge)
  end

  return comp
end

--- Set user data for a NativeCanvas component
--- @param component cdata The component
--- @param user_data any User data to store
function NativeCanvas.setUserData(component, user_data)
  if component ~= nil then
    -- Store in Lua-side registry
    g_callbacks["userdata_" .. tonumber(component.id)] = user_data

    -- For C-side, we can't easily pass Lua data, so we just mark it
    -- Users should access via getUserData()
  end
end

--- Get user data from a NativeCanvas component
--- @param component cdata The component
--- @return any The stored user data
function NativeCanvas.getUserData(component)
  if component ~= nil then
    return g_callbacks["userdata_" .. tonumber(component.id)]
  end
  return nil
end

--- Get the NativeCanvas data structure
--- @param component cdata The component
--- @return cdata The IRNativeCanvasData structure
function NativeCanvas.getData(component)
  return ir_lib.ir_native_canvas_get_data(component)
end

-- ============================================================================
-- Color Utilities
-- ============================================================================

--- Create RGBA color value (0xRRGGBBAA format)
--- @param r number Red component (0-255)
--- @param g number Green component (0-255)
--- @param b number Blue component (0-255)
--- @param a number Alpha component (0-255, default 255)
--- @return number RGBA color value
function NativeCanvas.rgba(r, g, b, a)
  a = a or 255
  return bit.bor(
    bit.lshift(r, 24),
    bit.lshift(g, 16),
    bit.lshift(b, 8),
    a
  )
end

--- Create RGB color value with full opacity
--- @param r number Red component (0-255)
--- @param g number Green component (0-255)
--- @param b number Blue component (0-255)
--- @return number RGB color value
function NativeCanvas.rgb(r, g, b)
  return NativeCanvas.rgba(r, g, b, 255)
end

-- ============================================================================
-- Metatable for DSL-style usage
-- ============================================================================

setmetatable(NativeCanvas, {
  __call = function(_, opts)
    return NativeCanvas.create(opts)
  end
})

return NativeCanvas
