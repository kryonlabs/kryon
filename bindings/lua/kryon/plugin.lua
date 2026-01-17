-- Kryon Plugin System
-- Provides target-aware plugin loading with automatic platform detection
-- This enables unified APIs across web and native targets

local Plugin = {}

-- Cache the detected target
local _detectedTarget = nil

-- ============================================================================
-- Target Detection
-- ============================================================================

--- Detect the current runtime target
--- @return string "web" or "native"
function Plugin.detectTarget()
    if _detectedTarget then
        return _detectedTarget
    end

    -- Try web detection first (Fengari environment)
    local success, js = pcall(require, "js")
    if success and js and js.global then
        _detectedTarget = "web"
        return "web"
    end

    -- Try native FFI detection
    local ffiSuccess, ffi = pcall(require, "ffi")
    if ffiSuccess and ffi then
        _detectedTarget = "native"
        return "native"
    end

    -- Fallback to native
    _detectedTarget = "native"
    return "native"
end

--- Check if running in web environment
--- @return boolean
function Plugin.isWeb()
    return Plugin.detectTarget() == "web"
end

--- Check if running in native environment
--- @return boolean
function Plugin.isNative()
    return Plugin.detectTarget() == "native"
end

-- ============================================================================
-- Plugin Loading
-- ============================================================================

-- Plugin registry - maps plugin names to their loaded implementations
local _plugins = {}

--- Load a plugin with automatic target detection
--- Tries <name>_web for web targets, <name> for native
--- @param name string Plugin base name (e.g., "storage")
--- @param options table|nil Options for loading
--- @return table Plugin module or nil
function Plugin.load(name, options)
    options = options or {}

    -- Check cache first
    if _plugins[name] and not options.force then
        return _plugins[name]
    end

    local target = Plugin.detectTarget()
    local impl = nil
    local loadedFrom = nil

    if target == "web" then
        -- Try web-specific module first
        local webName = name .. "_web"
        local webSuccess, webMod = pcall(require, webName)
        if webSuccess then
            impl = webMod
            loadedFrom = webName
        else
            -- Fall back to standard module
            local stdSuccess, stdMod = pcall(require, name)
            if stdSuccess then
                impl = stdMod
                loadedFrom = name
            end
        end
    else
        -- Native: try standard module
        local success, mod = pcall(require, name)
        if success then
            impl = mod
            loadedFrom = name
        end
    end

    if impl then
        _plugins[name] = impl
        if not options.silent then
            print(string.format("[Plugin] Loaded %s from %s (%s target)",
                name, loadedFrom, target))
        end
    else
        if not options.silent then
            print(string.format("[Plugin] Warning: Could not load plugin %s for %s target",
                name, target))
        end
    end

    return impl
end

--- Require a plugin, throwing error if not found
--- @param name string Plugin name
--- @return table Plugin module
function Plugin.require(name)
    local impl = Plugin.load(name)
    if not impl then
        error(string.format("Plugin '%s' not found for %s target",
            name, Plugin.detectTarget()))
    end
    return impl
end

--- Register a plugin implementation directly
--- @param name string Plugin name
--- @param impl table Plugin implementation
function Plugin.register(name, impl)
    _plugins[name] = impl
    print(string.format("[Plugin] Registered %s", name))
end

--- Check if a plugin is loaded
--- @param name string Plugin name
--- @return boolean
function Plugin.isLoaded(name)
    return _plugins[name] ~= nil
end

--- Unload a plugin
--- @param name string Plugin name
function Plugin.unload(name)
    _plugins[name] = nil
end

-- ============================================================================
-- Plugin Facade Creation
-- ============================================================================

--- Create a unified plugin facade with target-specific implementations
--- @param config table Configuration:
---   - name: string - Plugin name
---   - api: table - API definition { methodName = true, ... }
---   - web: table|string - Web implementation or module name
---   - native: table|string - Native implementation or module name
--- @return table Unified plugin facade
function Plugin.createFacade(config)
    local facade = {}
    local target = Plugin.detectTarget()

    -- Determine which implementation to use
    local implSource = target == "web" and config.web or config.native
    local impl = nil

    if type(implSource) == "string" then
        -- Load module by name
        local success, mod = pcall(require, implSource)
        if success then
            impl = mod
        end
    elseif type(implSource) == "table" then
        impl = implSource
    end

    if not impl then
        print(string.format("[Plugin] Warning: No implementation for %s on %s",
            config.name or "unknown", target))
        -- Return stub that warns on access
        return setmetatable({}, {
            __index = function(_, k)
                return function()
                    print(string.format("[Plugin] %s.%s not available on %s",
                        config.name or "plugin", k, target))
                end
            end
        })
    end

    -- Create facade by copying implementation
    for k, v in pairs(impl) do
        facade[k] = v
    end

    -- Add metadata
    facade._name = config.name
    facade._target = target
    facade._impl = impl

    return facade
end

-- ============================================================================
-- Common Plugin Interface Definitions
-- ============================================================================

--- Standard storage plugin interface
Plugin.StorageInterface = {
    init = true,       -- init(appName)
    get = true,        -- get(key) -> value
    set = true,        -- set(key, value)
    remove = true,     -- remove(key)
    clear = true,      -- clear()
    Collections = {
        load = true,   -- load(name, default) -> table
        save = true,   -- save(name, data)
    }
}

--- Standard datetime plugin interface
Plugin.DateTimeInterface = {
    now = true,        -- now() -> {year, month, day, hour, minute, second}
    format = true,     -- format(date, pattern) -> string
    parse = true,      -- parse(string, pattern) -> date
    diff = true,       -- diff(date1, date2) -> seconds
}

return Plugin
