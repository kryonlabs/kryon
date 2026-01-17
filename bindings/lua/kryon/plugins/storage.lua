-- Kryon Unified Storage Plugin
-- Provides consistent storage API across web and native targets
-- Uses Plugin system for automatic target detection

local Plugin = require("kryon.plugin")

-- Create the unified storage facade
local Storage = {}

-- Internal: load the appropriate implementation
local impl = nil
local function getImpl()
    if impl then return impl end

    local target = Plugin.detectTarget()
    if target == "web" then
        -- Web: use storage_web module
        local success, mod = pcall(require, "storage_web")
        if success then
            impl = mod
            print("[Storage] Loaded web implementation")
        else
            -- Try fallback
            success, mod = pcall(require, "storage")
            if success then
                impl = mod
                print("[Storage] Loaded fallback implementation for web")
            end
        end
    else
        -- Native: use storage module with FFI
        local success, mod = pcall(require, "storage")
        if success then
            impl = mod
            print("[Storage] Loaded native implementation")
        end
    end

    if not impl then
        print("[Storage] Warning: No storage implementation available")
        -- Provide in-memory fallback
        impl = {
            _memory = {},
            init = function() end,
            get = function(key) return impl._memory[key] end,
            set = function(key, val) impl._memory[key] = val end,
            getItem = function(key) return impl._memory[key] end,
            setItem = function(key, val) impl._memory[key] = val end,
            removeItem = function(key) impl._memory[key] = nil end,
            Collections = {
                load = function(name, default)
                    return impl._memory[name] or default
                end,
                save = function(name, data)
                    impl._memory[name] = data
                end
            }
        }
    end

    return impl
end

-- ============================================================================
-- Unified API
-- ============================================================================

--- Initialize storage for the app
--- @param appName string Application name for storage namespace
function Storage.init(appName)
    local i = getImpl()
    if i.init then
        return i.init(appName)
    end
end

--- Get a value from storage
--- @param key string Storage key
--- @return any Value or nil
function Storage.get(key)
    local i = getImpl()
    -- Try different method names for compatibility
    if i.get then return i.get(key) end
    if i.getItem then return i.getItem(key) end
    return nil
end

--- Set a value in storage
--- @param key string Storage key
--- @param value any Value to store
function Storage.set(key, value)
    local i = getImpl()
    if i.set then return i.set(key, value) end
    if i.setItem then return i.setItem(key, value) end
end

--- Remove a value from storage
--- @param key string Storage key
function Storage.remove(key)
    local i = getImpl()
    if i.remove then return i.remove(key) end
    if i.removeItem then return i.removeItem(key) end
end

--- Clear all storage
function Storage.clear()
    local i = getImpl()
    if i.clear then return i.clear() end
end

-- ============================================================================
-- Collections API (for structured data)
-- ============================================================================

Storage.Collections = {}

--- Load a collection from storage
--- @param name string Collection name
--- @param default table Default value if not found
--- @return table Collection data
function Storage.Collections.load(name, default)
    local i = getImpl()
    if i.Collections and i.Collections.load then
        return i.Collections.load(name, default)
    end
    -- Fallback: try to load as JSON
    local data = Storage.get(name)
    if data then
        if type(data) == "string" then
            -- Try to parse JSON
            local json = require("cjson") or require("dkjson") or nil
            if json and json.decode then
                local success, result = pcall(json.decode, data)
                if success then return result end
            end
        elseif type(data) == "table" then
            return data
        end
    end
    return default
end

--- Save a collection to storage
--- @param name string Collection name
--- @param data table Collection data
function Storage.Collections.save(name, data)
    local i = getImpl()
    if i.Collections and i.Collections.save then
        return i.Collections.save(name, data)
    end
    -- Fallback: try to save as JSON
    local json = require("cjson") or require("dkjson") or nil
    if json and json.encode then
        local success, str = pcall(json.encode, data)
        if success then
            Storage.set(name, str)
        end
    else
        Storage.set(name, data)
    end
end

-- ============================================================================
-- Backward Compatibility
-- ============================================================================

-- Support direct method access for legacy code
Storage.getItem = Storage.get
Storage.setItem = Storage.set
Storage.removeItem = Storage.remove

return Storage
