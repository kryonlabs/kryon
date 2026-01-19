-- Kryon Math Built-in Module
-- Wrapper around Lua's math library with capital M
-- This avoids conflict with Lua's built-in lowercase math

local Math = {}

-- Copy all functions from Lua's math library
for k, v in pairs(math) do
    Math[k] = v
end

-- Common math functions (delegated to Lua's math)
-- Math.abs, Math.ceil, Math.floor, Math.max, Math.min, Math.random, etc.

return Math
