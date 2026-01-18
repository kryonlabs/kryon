-- Kryon Web Reactive System
-- Provides reactive state management for browser environment
-- When state changes, notifies subscribers for DOM updates

local js = require("js")
local window = js.global
local document = window.document

local Reactive = {}

-- ============================================================================
-- JSON Serialization for JavaScript Bridge
-- ============================================================================

--- Serialize a Lua value to JSON string for JavaScript interop
--- @param val any Value to serialize
--- @return string JSON string
local function toJson(val)
    local t = type(val)
    if t == "nil" then
        return "null"
    elseif t == "boolean" then
        return val and "true" or "false"
    elseif t == "number" then
        return tostring(val)
    elseif t == "string" then
        -- Escape special characters
        local escaped = val:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
        return '"' .. escaped .. '"'
    elseif t == "table" then
        -- Check if it's a reactive proxy and unwrap it
        local mt = getmetatable(val)
        local data = (mt and mt.__data) and mt.__data or val

        -- Check if array or object
        local isArray = true
        local n = 0
        for k, _ in pairs(data) do
            n = n + 1
            if type(k) ~= "number" or k ~= n then
                isArray = false
                break
            end
        end
        if n == 0 then isArray = true end

        if isArray then
            local parts = {}
            for i, v in ipairs(data) do
                parts[i] = toJson(v)
            end
            return "[" .. table.concat(parts, ",") .. "]"
        else
            local parts = {}
            for k, v in pairs(data) do
                table.insert(parts, '"' .. tostring(k) .. '":' .. toJson(v))
            end
            return "{" .. table.concat(parts, ",") .. "}"
        end
    else
        return "null"
    end
end

Reactive.toJson = toJson

-- ============================================================================
-- Effect System - Automatic Dependency Tracking
-- ============================================================================

-- Global state for effect tracking
local activeEffect = nil
local effectStack = {}
local allEffects = {}  -- Track all registered effects

-- Push effect onto stack
local function pushEffect(effect)
    table.insert(effectStack, activeEffect)
    activeEffect = effect
end

-- Pop effect from stack
local function popEffect()
    activeEffect = table.remove(effectStack)
end

-- ============================================================================
-- Reactive Proxy Implementation
-- ============================================================================

--- Create a reactive proxy around a table
--- Changes to the proxy trigger subscriber notifications
--- @param initial table Initial data
--- @param parentPath string|nil Parent path for nested proxies (e.g., "displayedMonth")
--- @return table Reactive proxy
function Reactive.reactive(initial, parentPath)
    parentPath = parentPath or ""
    local data = {}
    local subscribers = {}
    local deps = {}  -- For effect dependency tracking

    -- Deep copy initial data
    if initial then
        for k, v in pairs(initial) do
            if type(v) == "table" then
                -- Recursively make nested tables reactive with full path
                local fullPath = parentPath == "" and tostring(k) or (parentPath .. "." .. tostring(k))
                data[k] = Reactive.reactive(v, fullPath)
            else
                data[k] = v
            end
        end
    end

    local proxy = {}
    local meta = {
        __index = function(t, k)
            -- Track this read as a dependency for the current effect
            if activeEffect then
                if not deps[k] then
                    deps[k] = {}
                end
                deps[k][activeEffect] = true
            end
            return data[k]
        end,

        __newindex = function(t, k, v)
            local oldValue = data[k]

            -- Compute the full path for this key
            local fullPath = parentPath == "" and tostring(k) or (parentPath .. "." .. tostring(k))

            -- Make nested tables reactive with full path
            if type(v) == "table" and not Reactive.isReactive(v) then
                v = Reactive.reactive(v, fullPath)
            end

            data[k] = v

            -- Trigger effects that depend on this key
            if deps[k] then
                for effect, _ in pairs(deps[k]) do
                    pcall(effect)
                end
            end

            -- Notify subscribers of change
            for _, subscriber in ipairs(subscribers) do
                pcall(subscriber, k, v, oldValue)
            end

            -- Notify JavaScript of state change for DOM updates
            -- Use eval pattern to avoid Fengari multi-argument issues
            if window and window.eval then
                pcall(function()
                    local jsonValue = toJson(v)
                    -- Use %q for proper string escaping, %s for JSON (already escaped)
                    local jsCode = string.format('kryonStateChanged(%q, %s)', fullPath, jsonValue)
                    window:eval(jsCode)
                end)
            end
        end,

        __pairs = function(t)
            return pairs(data)
        end,

        __ipairs = function(t)
            return ipairs(data)
        end,

        __len = function(t)
            return #data
        end,

        -- Mark as reactive
        __reactive = true,
        __data = data,
        __subscribers = subscribers
    }

    setmetatable(proxy, meta)
    return proxy
end

--- Check if a value is a reactive proxy
--- @param value any Value to check
--- @return boolean
function Reactive.isReactive(value)
    if type(value) ~= "table" then
        return false
    end
    local mt = getmetatable(value)
    return mt and mt.__reactive == true
end

--- Get the raw underlying data from a reactive proxy
--- @param reactive table Reactive proxy
--- @return table Raw data (deep copy)
function Reactive.toRaw(reactive)
    if not Reactive.isReactive(reactive) then
        -- If not reactive, return as-is or deep copy if table
        if type(reactive) == "table" then
            local result = {}
            for k, v in pairs(reactive) do
                result[k] = Reactive.toRaw(v)
            end
            return result
        end
        return reactive
    end

    local mt = getmetatable(reactive)
    local data = mt.__data

    local result = {}
    for k, v in pairs(data) do
        result[k] = Reactive.toRaw(v)
    end
    return result
end

--- Subscribe to changes on a reactive proxy
--- @param reactive table Reactive proxy
--- @param callback function(key, newValue, oldValue)
function Reactive.subscribe(reactive, callback)
    if not Reactive.isReactive(reactive) then
        print("[Reactive Web] Warning: Cannot subscribe to non-reactive value")
        return
    end

    local mt = getmetatable(reactive)
    table.insert(mt.__subscribers, callback)
end

--- Unsubscribe from changes
--- @param reactive table Reactive proxy
--- @param callback function The callback to remove
function Reactive.unsubscribe(reactive, callback)
    if not Reactive.isReactive(reactive) then
        return
    end

    local mt = getmetatable(reactive)
    for i, sub in ipairs(mt.__subscribers) do
        if sub == callback then
            table.remove(mt.__subscribers, i)
            return
        end
    end
end

-- ============================================================================
-- JavaScript Binding Registration (Phase 1)
-- ============================================================================

--- Register a DOM binding with the JavaScript reactive system
--- This allows state changes to automatically update DOM elements
--- @param stateKey string State path (e.g., 'displayedMonth' or 'displayedMonth.month')
--- @param elementId string DOM element ID to update
--- @param updateType string Update type: 'text', 'style', 'attr', 'class', 'visibility', 'custom'
--- @param options table|nil Additional options based on updateType
function Reactive.registerBinding(stateKey, elementId, updateType, options)
    if window and window.eval then
        pcall(function()
            local jsOptions = options and toJson(options) or "{}"
            -- Use eval pattern to avoid Fengari multi-argument issues
            local jsCode = string.format('kryonRegisterBinding(%q, %q, %q, %s)',
                stateKey, elementId, updateType or "text", jsOptions)
            window:eval(jsCode)
        end)
    else
        print("[Reactive Web] Warning: window.eval not available yet")
    end
end

--- Batch register multiple bindings
--- @param bindings table Array of {stateKey, elementId, updateType, options}
function Reactive.registerBindings(bindings)
    for _, b in ipairs(bindings) do
        Reactive.registerBinding(b.stateKey or b[1], b.elementId or b[2], b.updateType or b[3], b.options or b[4])
    end
end

-- ============================================================================
-- DOM Binding Helpers
-- ============================================================================

--- Bind a reactive value to an element's text content
--- @param reactive table Reactive proxy
--- @param key string Key in the reactive object
--- @param elementId string Element ID to update
function Reactive.bindText(reactive, key, elementId)
    Reactive.subscribe(reactive, function(changedKey, newValue)
        if changedKey == key then
            local element = document:getElementById(elementId)
            if element then
                element.textContent = tostring(newValue)
            end
        end
    end)
end

--- Bind a reactive value to an element's style property
--- @param reactive table Reactive proxy
--- @param key string Key in the reactive object
--- @param elementId string Element ID to update
--- @param styleProperty string CSS property name
function Reactive.bindStyle(reactive, key, elementId, styleProperty)
    Reactive.subscribe(reactive, function(changedKey, newValue)
        if changedKey == key then
            local element = document:getElementById(elementId)
            if element then
                element.style[styleProperty] = tostring(newValue)
            end
        end
    end)
end

--- Bind a reactive boolean to element visibility
--- @param reactive table Reactive proxy
--- @param key string Key in the reactive object
--- @param elementId string Element ID to show/hide
function Reactive.bindVisibility(reactive, key, elementId)
    Reactive.subscribe(reactive, function(changedKey, newValue)
        if changedKey == key then
            local element = document:getElementById(elementId)
            if element then
                element.style.display = newValue and "" or "none"
            end
        end
    end)
end

--- Generic bind: call updateFn whenever reactive[key] changes
--- Also calls updateFn immediately with current value
--- @param reactive table Reactive proxy
--- @param key string Key in the reactive object
--- @param updateFn function(newValue) Function to call on change
function Reactive.bind(reactive, key, updateFn)
    -- Call immediately with current value
    local currentValue = reactive[key]
    if currentValue ~= nil then
        pcall(updateFn, currentValue)
    end

    -- Subscribe to future changes
    Reactive.subscribe(reactive, function(changedKey, newValue)
        if changedKey == key then
            pcall(updateFn, newValue)
        end
    end)
end

--- Bind reactive state to tab selection
--- Convenience function for TabGroup components
--- @param reactive table Reactive proxy
--- @param key string Key in the reactive object (e.g., "selectedHabit")
--- @param tabGroupId string|nil TabGroup element ID (auto-finds if nil)
function Reactive.bindTabGroup(reactive, key, tabGroupId)
    local Runtime = require("kryon.runtime_web")

    -- Auto-find TabGroup if not provided
    if not tabGroupId then
        tabGroupId = Runtime.findFirstTabGroup()
        if not tabGroupId then
            print("[Reactive Web] Warning: No TabGroup found to bind")
            return
        end
    end

    print("[Reactive Web] Binding", key, "to TabGroup:", tabGroupId)

    -- Bind state changes to tab selection
    -- Note: Lua uses 1-based indexing, DOM uses 0-based
    Reactive.bind(reactive, key, function(value)
        local index = tonumber(value)
        if index then
            Runtime.selectTab(tabGroupId, index - 1)  -- Convert to 0-based
        end
    end)
end

-- ============================================================================
-- ForEach Reactive Integration
-- ============================================================================

--- Watch a ForEach data source and auto-re-render on changes
--- @param reactive table Reactive proxy containing the data
--- @param sourcePath string Path to the array (e.g., "habits[1].calendarDays")
--- @param containerId string ForEach container element ID
function Reactive.watchForEach(reactive, sourcePath, containerId)
    local Runtime = require("kryon.runtime_web")

    -- Parse the path to get the top-level key
    -- e.g., "habits[1].calendarDays" -> "habits"
    local topLevelKey = string.match(sourcePath, "^([^%.[%]]+)")

    if not topLevelKey then
        print("[Reactive Web] Invalid source path for ForEach:", sourcePath)
        return
    end

    print("[Reactive Web] Watching ForEach:", containerId, "<-", sourcePath)

    -- Subscribe to changes on the top-level key
    Reactive.subscribe(reactive, function(changedKey, newValue, oldValue)
        -- Check if this change affects our ForEach source
        if changedKey == topLevelKey then
            -- Get the updated data
            local newData = Runtime.getReactiveValue(sourcePath)
            if newData then
                Runtime.renderForEach(containerId, newData)
            end
        end
    end)

    -- Initial render with current data
    local initialData = Runtime.getReactiveValue(sourcePath)
    if initialData then
        Runtime.renderForEach(containerId, initialData)
    end
end

--- Register a ForEach container with its render function and set up reactivity
--- @param config table Configuration:
---   - containerId: string - Element ID of ForEach container
---   - source: string - Reactive data path (e.g., "state.calendarDays")
---   - itemName: string - Variable name for each item
---   - indexName: string - Variable name for index
---   - renderFn: function - Render function(item, index) -> component
---   - state: table|nil - Optional reactive state to watch
function Reactive.registerForEach(config)
    local Runtime = require("kryon.runtime_web")

    -- Register the container with the runtime
    Runtime.registerForEachContainer(config.containerId, {
        source = config.source,
        itemName = config.itemName or "item",
        indexName = config.indexName or "index",
        renderFn = config.renderFn
    })

    -- If reactive state is provided, set up watching
    if config.state then
        Reactive.watchForEach(config.state, config.source, config.containerId)
    end
end

-- ============================================================================
-- Effect Function - Automatic Dependency Tracking
-- ============================================================================

--- Run a function that automatically tracks its reactive dependencies
--- and re-runs whenever those dependencies change.
--- @param effectFn function Function to run (will be re-run on dependency changes)
--- @return function Stop function to cancel the effect
function Reactive.effect(effectFn)
    if type(effectFn) ~= "function" then
        print("[Reactive Web] Error: effect() requires a function")
        return function() end
    end

    local stopped = false

    local function runEffect()
        if stopped then return end

        -- Push this effect as active (for dependency tracking)
        pushEffect(runEffect)

        -- Run the effect function
        local success, err = pcall(effectFn)
        if not success then
            print("[Reactive Web] Error in effect: " .. tostring(err))
        end

        -- Pop effect stack
        popEffect()
    end

    -- Track this effect
    table.insert(allEffects, runEffect)

    -- Run immediately to register dependencies
    runEffect()

    -- Return stop function
    return function()
        stopped = true
        -- Remove from allEffects
        for i, e in ipairs(allEffects) do
            if e == runEffect then
                table.remove(allEffects, i)
                break
            end
        end
    end
end

-- ============================================================================
-- Template Interpolation (Phase 4 DSL Enhancement)
-- ============================================================================

--- Interpolate template strings with state values
--- Supports ${key} and ${key.subkey} syntax
--- @param template string Template string (e.g., "Month: ${month}, Year: ${year}")
--- @param context table Context object to look up values from
--- @return string Interpolated string
function Reactive.interpolate(template, context)
    if type(template) ~= "string" then return tostring(template) end
    if type(context) ~= "table" then return template end

    return template:gsub("%${([^}]+)}", function(path)
        local value = context
        for part in path:gmatch("[^.]+") do
            if type(value) == "table" then
                value = value[part]
            else
                return "${" .. path .. "}"
            end
        end
        return value ~= nil and tostring(value) or ""
    end)
end

--- Create a computed value that updates reactively
--- @param computeFn function Function that returns the computed value
--- @return function Getter function
function Reactive.computed(computeFn)
    local cachedValue = nil
    local isDirty = true

    local getter = function()
        if isDirty then
            cachedValue = computeFn()
            isDirty = false
        end
        return cachedValue
    end

    -- Mark dirty when any effect runs (simple approach)
    Reactive.effect(function()
        isDirty = true
        cachedValue = computeFn()
    end)

    return getter
end

--- Batch multiple state updates to avoid multiple renders
--- @param updateFn function Function containing multiple state updates
function Reactive.batch(updateFn)
    -- For now, just run the function - the JS side handles debouncing
    pcall(updateFn)
end

-- ============================================================================
-- Dynamic Binding System: Runtime Lua Expression Evaluation with Tracking
-- This is called by JavaScript's kryonEvalBinding() to evaluate Lua expressions
-- and discover their reactive dependencies at runtime.
-- ============================================================================

-- Global tracking state for dependency discovery
local isTracking = false
local accessedPaths = {}

--- Create a tracking proxy that records all property accesses
--- @param target table The table to wrap
--- @param path string The current path (e.g., "state.displayedMonth")
--- @return table Tracking proxy
local function createTrackingProxy(target, path)
    path = path or ""

    return setmetatable({}, {
        __index = function(_, k)
            local fullPath = path == "" and tostring(k) or (path .. "." .. tostring(k))

            -- Record the access if we're tracking
            if isTracking then
                accessedPaths[fullPath] = true
            end

            local value = target[k]
            if type(value) == "table" then
                -- Recursively wrap nested tables
                return createTrackingProxy(value, fullPath)
            end
            return value
        end,

        __newindex = function(_, k, v)
            target[k] = v
        end,

        __pairs = function()
            return pairs(target)
        end,

        __ipairs = function()
            return ipairs(target)
        end,

        __len = function()
            return #target
        end
    })
end

--- Evaluate a Lua expression with dependency tracking enabled
--- Returns both the result and the discovered dependencies
--- @param luaExpr string The Lua expression to evaluate
--- @param context table Context variables (state, DateTime, etc.)
--- @return any Result of evaluation
--- @return table Array of dependency paths
function Reactive.evalWithTracking(luaExpr, context)
    -- Reset tracking state
    accessedPaths = {}
    isTracking = true

    -- Wrap context variables in tracking proxies
    local trackedContext = {}
    for k, v in pairs(context or {}) do
        if type(v) == "table" then
            trackedContext[k] = createTrackingProxy(v, k)
        else
            trackedContext[k] = v
        end
    end

    -- Add globals to context (with tracking for tables)
    if _G.state and not trackedContext.state then
        trackedContext.state = createTrackingProxy(_G.state, "state")
    end
    if _G.DateTime and not trackedContext.DateTime then
        trackedContext.DateTime = _G.DateTime  -- DateTime is a module, not state
    end

    -- Compile and run the expression
    local result = nil
    local fn, err = load("return " .. luaExpr, "binding", "t", trackedContext)
    if fn then
        local ok, val = pcall(fn)
        if ok then
            result = val
        else
            print("[Reactive Web] Error evaluating expression:", val)
        end
    else
        print("[Reactive Web] Error compiling expression:", err)
    end

    isTracking = false

    -- Collect dependencies
    local deps = {}
    for path, _ in pairs(accessedPaths) do
        table.insert(deps, path)
    end

    return result, deps
end

-- Global function for JavaScript to call
-- This is the bridge that JavaScript uses to evaluate Lua expressions
_G.kryonEvalWithTracking = function(luaExpr)
    local result, deps = Reactive.evalWithTracking(luaExpr, {})

    -- Return as a table that JavaScript can access
    -- Fengari will convert this to a JS object
    return {
        result = result,
        deps = deps
    }
end

-- Expose to window for JavaScript access
if window and window.eval then
    pcall(function()
        -- Create the global function that JavaScript can call
        window.kryonEvalWithTracking = _G.kryonEvalWithTracking
    end)
end

print("[Reactive Web] Reactive system loaded")

return Reactive
