-- Kryon Reactive Protocol
-- Shared reactive API that works across compile-time and browser runtime
-- This module provides a unified interface for reactive state management

local Protocol = {}

-- ============================================================================
-- Protocol Definition
-- ============================================================================

-- These functions will be implemented by the target-specific reactive module
-- (reactive.lua for native/compile-time, reactive_web.lua for browser)

--- Create a reactive proxy around a table
--- @param initial table Initial data
--- @return table Reactive proxy
Protocol.reactive = nil

--- Check if a value is reactive
--- @param value any Value to check
--- @return boolean
Protocol.isReactive = nil

--- Get raw data from a reactive proxy
--- @param reactive table Reactive proxy
--- @return table Raw data
Protocol.toRaw = nil

--- Subscribe to changes on a reactive proxy
--- @param reactive table Reactive proxy
--- @param callback function(key, newValue, oldValue)
Protocol.subscribe = nil

--- Unsubscribe from changes
--- @param reactive table Reactive proxy
--- @param callback function The callback to remove
Protocol.unsubscribe = nil

--- Run an effect function that tracks dependencies
--- @param effectFn function Function to run
--- @return function Stop function
Protocol.effect = nil

--- Create a computed value
--- @param computeFn function Function to compute value
--- @return table Reactive computed
Protocol.computed = nil

--- Batch multiple updates
--- @param updateFn function Function containing updates
Protocol.batch = nil

-- ============================================================================
-- Binding Protocol (Phase 1 addition)
-- ============================================================================

--- Register a DOM binding (web-only, no-op on native)
--- @param stateKey string State path
--- @param elementId string DOM element ID
--- @param updateType string Update type
--- @param options table|nil Options
Protocol.registerBinding = nil

--- Serialize value to JSON
--- @param value any Value to serialize
--- @return string JSON string
Protocol.toJson = nil

-- ============================================================================
-- Target Detection and Auto-Configuration
-- ============================================================================

local function detectTarget()
    -- Try to detect if we're in a web (Fengari) environment
    local success, js = pcall(require, "js")
    if success and js and js.global then
        return "web"
    end

    -- Try to detect if we have FFI (native LuaJIT or Lua with FFI)
    local ffiSuccess, ffi = pcall(require, "ffi")
    if ffiSuccess and ffi then
        return "native"
    end

    -- Fallback - assume native
    return "native"
end

--- Get the current target
--- @return string "web" or "native"
function Protocol.getTarget()
    if not Protocol._target then
        Protocol._target = detectTarget()
    end
    return Protocol._target
end

--- Initialize the protocol with the appropriate implementation
--- This should be called once at startup
function Protocol.init()
    local target = Protocol.getTarget()
    local impl

    if target == "web" then
        local success, mod = pcall(require, "kryon.reactive_web")
        if success then
            impl = mod
            print("[Reactive Protocol] Using web implementation")
        else
            print("[Reactive Protocol] Warning: Failed to load reactive_web, falling back")
            local fallback, nativeMod = pcall(require, "kryon.reactive")
            if fallback then impl = nativeMod end
        end
    else
        local success, mod = pcall(require, "kryon.reactive")
        if success then
            impl = mod
            print("[Reactive Protocol] Using native implementation")
        end
    end

    if impl then
        -- Copy implementation functions to protocol
        Protocol.reactive = impl.reactive
        Protocol.isReactive = impl.isReactive
        Protocol.toRaw = impl.toRaw
        Protocol.subscribe = impl.subscribe
        Protocol.unsubscribe = impl.unsubscribe
        Protocol.effect = impl.effect
        Protocol.computed = impl.computed
        Protocol.batch = impl.batch
        Protocol.registerBinding = impl.registerBinding
        Protocol.toJson = impl.toJson
    else
        error("[Reactive Protocol] No reactive implementation available")
    end

    return Protocol
end

-- ============================================================================
-- Convenience Functions
-- ============================================================================

--- Create reactive state with automatic target detection
--- @param initial table Initial data
--- @return table Reactive proxy
function Protocol.createState(initial)
    if not Protocol.reactive then
        Protocol.init()
    end
    return Protocol.reactive(initial)
end

--- Register a text binding (convenience wrapper)
--- @param stateKey string State path
--- @param elementId string DOM element ID
function Protocol.bindText(stateKey, elementId)
    if Protocol.registerBinding then
        Protocol.registerBinding(stateKey, elementId, "text")
    end
end

--- Register a visibility binding (convenience wrapper)
--- @param stateKey string State path
--- @param elementId string DOM element ID
function Protocol.bindVisibility(stateKey, elementId)
    if Protocol.registerBinding then
        Protocol.registerBinding(stateKey, elementId, "visibility")
    end
end

return Protocol
