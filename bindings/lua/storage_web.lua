-- Kryon Web Storage
-- Provides localStorage-based persistence for browser environment
-- Uses pcall wrappers to handle Fengari's JavaScript interop quirks

local js = require("js")
local window = js.global

local Storage = {}
Storage.prefix = ""

-- ============================================================================
-- Safe JavaScript API Wrappers
-- These handle Fengari's funcOff bug by using pcall and fallback patterns
-- ============================================================================

-- Safely get localStorage (might not exist in all contexts)
local function getLocalStorage()
    local ok, ls = pcall(function()
        return window.localStorage
    end)
    if ok and ls and ls ~= js.null then
        return ls
    end
    return nil
end

-- Safely call JSON.parse
local function safeJsonParse(jsonStr)
    if not jsonStr or jsonStr == "" or jsonStr == js.null then
        return nil
    end
    local ok, result = pcall(function()
        return window.JSON:parse(jsonStr)
    end)
    if ok and result ~= nil then
        return result
    end
    ok, result = pcall(function()
        return window.JSON.parse(window.JSON, jsonStr)
    end)
    if ok and result ~= nil then
        return result
    end
    return nil
end

-- Safely call JSON.stringify
local function safeJsonStringify(value)
    if value == nil then
        return "null"
    end
    local ok, result = pcall(function()
        return window.JSON:stringify(value)
    end)
    if ok and result then
        return result
    end
    ok, result = pcall(function()
        return window.JSON.stringify(window.JSON, value)
    end)
    if ok and result then
        return result
    end
    return "null"
end

-- Safely call localStorage.getItem
local function safeGetItem(key)
    local localStorage = getLocalStorage()
    if not localStorage then
        return nil
    end
    -- Try colon syntax first
    local ok, value = pcall(function()
        return localStorage:getItem(key)
    end)
    if ok then
        if value == js.null or value == nil then
            return nil
        end
        return value
    end
    -- Try dot syntax with explicit this
    ok, value = pcall(function()
        return localStorage.getItem(localStorage, key)
    end)
    if ok and value ~= js.null and value ~= nil then
        return value
    end
    return nil
end

-- Safely call localStorage.setItem
local function safeSetItem(key, value)
    local localStorage = getLocalStorage()
    if not localStorage then
        return false
    end
    -- Try colon syntax first
    local ok = pcall(function()
        localStorage:setItem(key, value)
    end)
    if ok then
        return true
    end
    -- Try dot syntax with explicit this
    ok = pcall(function()
        localStorage.setItem(localStorage, key, value)
    end)
    return ok
end

-- Safely call localStorage.removeItem
local function safeRemoveItem(key)
    local localStorage = getLocalStorage()
    if not localStorage then
        return false
    end
    local ok = pcall(function()
        localStorage:removeItem(key)
    end)
    if ok then
        return true
    end
    ok = pcall(function()
        localStorage.removeItem(localStorage, key)
    end)
    return ok
end

-- Safely call localStorage.key
local function safeGetKey(index)
    local localStorage = getLocalStorage()
    if not localStorage then
        return nil
    end
    local ok, key = pcall(function()
        return localStorage:key(index)
    end)
    if ok and key ~= js.null then
        return key
    end
    ok, key = pcall(function()
        return localStorage.key(localStorage, index)
    end)
    if ok and key ~= js.null then
        return key
    end
    return nil
end

-- Safely get localStorage.length
local function safeGetLength()
    local localStorage = getLocalStorage()
    if not localStorage then
        return 0
    end
    local ok, len = pcall(function()
        return localStorage.length
    end)
    if ok and len then
        return len
    end
    return 0
end

-- Safely check if value is array
local function safeIsArray(jsValue)
    local ok, result = pcall(function()
        return window.Array:isArray(jsValue)
    end)
    if ok then
        return result
    end
    ok, result = pcall(function()
        return window.Array.isArray(window.Array, jsValue)
    end)
    if ok then
        return result
    end
    return false
end

-- Safely get object keys
local function safeObjectKeys(jsValue)
    local ok, keys = pcall(function()
        return window.Object:keys(jsValue)
    end)
    if ok and keys then
        return keys
    end
    ok, keys = pcall(function()
        return window.Object.keys(window.Object, jsValue)
    end)
    if ok and keys then
        return keys
    end
    return nil
end

-- Safely create new JavaScript array
local function safeNewArray()
    local ok, arr = pcall(function()
        return js.new(window.Array)
    end)
    if ok and arr then
        return arr
    end
    return nil
end

-- Safely create new JavaScript object
local function safeNewObject()
    local ok, obj = pcall(function()
        return js.new(window.Object)
    end)
    if ok and obj then
        return obj
    end
    return nil
end

-- Safely push to array
local function safeArrayPush(arr, value)
    local ok = pcall(function()
        arr:push(value)
    end)
    if ok then
        return true
    end
    ok = pcall(function()
        arr.push(arr, value)
    end)
    return ok
end

-- ============================================================================
-- Initialization
-- ============================================================================

--- Initialize storage with app name prefix
--- @param appName string Application name used as storage prefix
function Storage.init(appName)
    Storage.prefix = appName .. "_"
end

-- ============================================================================
-- Low-level Storage Operations
-- ============================================================================

--- Get a value from localStorage
--- @param key string Storage key
--- @return string|nil Value or nil if not found
function Storage.get(key)
    local fullKey = Storage.prefix .. key
    return safeGetItem(fullKey)
end

--- Set a value in localStorage
--- @param key string Storage key
--- @param value string Value to store
function Storage.set(key, value)
    local fullKey = Storage.prefix .. key
    return safeSetItem(fullKey, value)
end

--- Remove a value from localStorage
--- @param key string Storage key
function Storage.remove(key)
    local fullKey = Storage.prefix .. key
    return safeRemoveItem(fullKey)
end

--- Clear all storage with this app's prefix
function Storage.clear()
    local length = safeGetLength()
    for i = length - 1, 0, -1 do
        local key = safeGetKey(i)
        if key and key:sub(1, #Storage.prefix) == Storage.prefix then
            safeRemoveItem(key)
        end
    end
end

-- ============================================================================
-- Collections API (compatible with desktop storage plugin)
-- ============================================================================

Storage.Collections = {}

--- Load a collection from storage
--- @param name string Collection name
--- @param default any Default value if not found
--- @return table Collection data
function Storage.Collections.load(name, default)
    local key = "collection_" .. name
    local jsonStr = Storage.get(key)

    if jsonStr then
        local jsValue = safeJsonParse(jsonStr)
        if jsValue then
            return Storage._jsToLua(jsValue)
        end
    end

    return default or {}
end

--- Save a collection to storage
--- @param name string Collection name
--- @param data table Collection data
function Storage.Collections.save(name, data)
    local key = "collection_" .. name
    local jsValue = Storage._luaToJs(data)
    local jsonStr = safeJsonStringify(jsValue)
    Storage.set(key, jsonStr)
end

-- ============================================================================
-- JS <-> Lua Conversion Helpers
-- ============================================================================

--- Convert a JavaScript value to Lua
--- Avoids js.typeof which can fail in Fengari
--- @param jsValue any JavaScript value
--- @return any Lua value
function Storage._jsToLua(jsValue)
    if jsValue == nil then
        return nil
    end

    -- Check for js.null
    local isNull = pcall(function()
        return jsValue == js.null
    end)
    if isNull and jsValue == js.null then
        return nil
    end

    local luaType = type(jsValue)

    if luaType == "string" or luaType == "number" or luaType == "boolean" then
        return jsValue
    elseif luaType == "table" or luaType == "userdata" then
        local result = {}

        -- Check if array by .length property
        local len = 0
        local lenOk, lenVal = pcall(function() return jsValue.length end)
        if lenOk and type(lenVal) == "number" then
            len = lenVal
        end

        local isArr = safeIsArray(jsValue)

        if isArr or len > 0 then
            for i = 0, len - 1 do
                local valOk, val = pcall(function() return jsValue[i] end)
                if valOk then
                    result[i + 1] = Storage._jsToLua(val)
                end
            end
        else
            local keys = safeObjectKeys(jsValue)
            if keys then
                local keyLen = 0
                local keyLenOk, keyLenVal = pcall(function() return keys.length end)
                if keyLenOk and type(keyLenVal) == "number" then
                    keyLen = keyLenVal
                end

                for i = 0, keyLen - 1 do
                    local keyOk, key = pcall(function() return keys[i] end)
                    if keyOk and key then
                        local valOk, val = pcall(function() return jsValue[key] end)
                        if valOk then
                            result[tostring(key)] = Storage._jsToLua(val)
                        end
                    end
                end
            elseif luaType == "table" then
                for k, v in pairs(jsValue) do
                    result[tostring(k)] = Storage._jsToLua(v)
                end
            end
        end

        return result
    end

    return jsValue
end

--- Convert a Lua value to JavaScript
--- @param luaValue any Lua value
--- @return any JavaScript value
function Storage._luaToJs(luaValue)
    if luaValue == nil then
        return js.null
    end

    local luaType = type(luaValue)

    if luaType == "string" or luaType == "number" or luaType == "boolean" then
        return luaValue
    end

    if luaType == "table" then
        -- Check if it's an array (sequential integer keys starting at 1)
        local isArray = true
        local maxIndex = 0
        for k, _ in pairs(luaValue) do
            if type(k) ~= "number" or k < 1 or k ~= math.floor(k) then
                isArray = false
                break
            end
            if k > maxIndex then
                maxIndex = k
            end
        end

        -- Also check for gaps
        if isArray and maxIndex > 0 then
            for i = 1, maxIndex do
                if luaValue[i] == nil then
                    isArray = false
                    break
                end
            end
        end

        if isArray and maxIndex > 0 then
            local jsArray = safeNewArray()
            if jsArray then
                for i = 1, maxIndex do
                    safeArrayPush(jsArray, Storage._luaToJs(luaValue[i]))
                end
                return jsArray
            end
        else
            local jsObject = safeNewObject()
            if jsObject then
                for k, v in pairs(luaValue) do
                    local keyStr = tostring(k)
                    pcall(function()
                        jsObject[keyStr] = Storage._luaToJs(v)
                    end)
                end
                return jsObject
            end
        end
    end

    return js.null
end

return Storage
