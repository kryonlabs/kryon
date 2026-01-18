-- Kryon Automatic Reactivity System
-- Vue 3 / SolidJS-inspired automatic dependency tracking
-- Clean, beautiful, no legacy code

local Reactive = {}

-- ============================================================================
-- KIR Manifest Integration (Phase 3: Self-Contained KIR)
-- Register reactive variables with C manifest for serialization to KIR
-- ============================================================================

local ffi_available = false
local C = nil
local manifest = nil
local next_var_id = 1

-- Try to load FFI and create manifest (only works in compile mode, not in browser)
local function init_manifest()
  if manifest ~= nil then return manifest end  -- Already initialized

  local ok, ffi_module = pcall(function()
    return require("kryon.ffi")
  end)

  if ok and ffi_module and ffi_module.C then
    ffi_available = true
    C = ffi_module.C

    -- Create manifest for this compilation
    manifest = C.ir_reactive_manifest_create()
    if manifest == nil then
      print("[Reactive] Warning: Failed to create manifest")
      ffi_available = false
      return nil
    end
    print("[Reactive] KIR manifest created for reactive state transpilation")
    return manifest
  end

  return nil
end

-- Get reactive type enum from Lua value
local function get_reactive_type(value)
  local t = type(value)
  if t == "number" then
    if math.floor(value) == value then
      return 0  -- IR_REACTIVE_TYPE_INT
    else
      return 1  -- IR_REACTIVE_TYPE_FLOAT
    end
  elseif t == "string" then
    return 2  -- IR_REACTIVE_TYPE_STRING
  elseif t == "boolean" then
    return 3  -- IR_REACTIVE_TYPE_BOOL
  else
    return 4  -- IR_REACTIVE_TYPE_CUSTOM (tables, etc.)
  end
end

-- Serialize value to JSON for KIR
local function serialize_to_json(value)
  local t = type(value)
  if t == "nil" then
    return "null"
  elseif t == "boolean" then
    return value and "true" or "false"
  elseif t == "number" then
    return tostring(value)
  elseif t == "string" then
    -- Escape special characters
    local escaped = value:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
    return '"' .. escaped .. '"'
  elseif t == "table" then
    -- Check if array
    local is_array = (#value > 0 and next(value, #value) == nil)
    if is_array then
      local parts = {}
      for i, v in ipairs(value) do
        table.insert(parts, serialize_to_json(v))
      end
      return "[" .. table.concat(parts, ",") .. "]"
    else
      local parts = {}
      for k, v in pairs(value) do
        if type(k) == "string" then
          table.insert(parts, '"' .. k .. '":' .. serialize_to_json(v))
        end
      end
      return "{" .. table.concat(parts, ",") .. "}"
    end
  else
    return "null"
  end
end

-- Register a reactive variable with the KIR manifest
local function register_var_with_manifest(name, value)
  if not ffi_available or not manifest then
    init_manifest()
  end

  if not ffi_available or not manifest then
    return nil  -- Can't register without FFI
  end

  local var_type = get_reactive_type(value)
  local ffi = require("ffi")

  -- Create value union
  local ir_value = ffi.new("IRReactiveValue")
  if var_type == 0 then  -- INT
    ir_value.as_int = tonumber(value) or 0
  elseif var_type == 1 then  -- FLOAT
    ir_value.as_float = tonumber(value) or 0.0
  elseif var_type == 2 then  -- STRING
    ir_value.as_string = ffi.cast("char*", value or "")
  elseif var_type == 3 then  -- BOOL
    ir_value.as_bool = value and true or false
  end

  -- Add variable to manifest
  local var_id = C.ir_reactive_manifest_add_var(manifest, name, var_type, ir_value)

  -- Set metadata with JSON serialization of initial value
  local initial_json = serialize_to_json(value)
  local type_string = type(value)
  C.ir_reactive_manifest_set_var_metadata(manifest, var_id, type_string, initial_json, "state")

  return var_id
end

-- Export manifest for writing to KIR
function Reactive.getManifest()
  return manifest
end

-- Export registration function for DSL integration
Reactive._registerVarWithManifest = register_var_with_manifest

-- ============================================================================
-- Reactive Context - Automatic Dependency Tracking
-- ============================================================================

local ReactiveContext = {
  activeEffect = nil,      -- Currently executing effect/computed
  effectStack = {},        -- Stack for nested effects
  shouldTrack = true,      -- Flag to pause tracking
  debugMode = false,       -- Enable debug logging
  traceReads = false,      -- Trace reactive reads
  traceWrites = false,     -- Trace reactive writes
}

-- Track currently executing effects to prevent cycles
local executingEffects = {}

local function pushEffect(effect)
  table.insert(ReactiveContext.effectStack, ReactiveContext.activeEffect)
  ReactiveContext.activeEffect = effect
end

local function popEffect()
  ReactiveContext.activeEffect = table.remove(ReactiveContext.effectStack)
end

-- Track a dependency (called during reactive reads)
local function trackDependency(target, key, dep)
  if not ReactiveContext.activeEffect or not ReactiveContext.shouldTrack then
    return
  end

  if not target or type(target) ~= "table" then
    return
  end

  dep = dep or ReactiveContext.activeEffect

  -- Initialize _deps if needed (use rawget/rawset to avoid triggering metatables)
  if not rawget(target, "_deps") then
    rawset(target, "_deps", {})
  end

  local deps = rawget(target, "_deps")
  if not deps[key] then
    deps[key] = {}
  end

  -- Avoid duplicate subscriptions
  for _, existingDep in ipairs(deps[key]) do
    if existingDep == dep then
      return
    end
  end

  table.insert(deps[key], dep)

  if ReactiveContext.debugMode then
    print(string.format("[trackDependency] Added dep, now %d deps for key %s", #deps[key], tostring(key)))
  end
end

-- ============================================================================
-- Compile-Time Dependency Tracking (for computed value bindings)
-- Used by DSL to capture dependencies when evaluating text expressions
-- ============================================================================

local compileTrackingStack = {}

-- Start tracking dependencies for compile-time binding extraction
-- Returns a tracker object that will collect accessed reactive paths
function Reactive.startTracking()
  local tracker = { deps = {} }
  table.insert(compileTrackingStack, tracker)
  return tracker
end

-- Stop tracking and return collected dependencies
-- @param tracker The tracker returned by startTracking()
-- @return table Array of dependency paths (e.g., {"displayedMonth.year", "displayedMonth.month"})
function Reactive.stopTracking(tracker)
  local removed = table.remove(compileTrackingStack)
  if removed ~= tracker then
    -- Stack mismatch - this shouldn't happen but handle gracefully
    print("[Reactive] Warning: Tracking stack mismatch")
  end
  return tracker and tracker.deps or {}
end

-- Record a dependency path during compile-time tracking
-- Called internally when reactive proxies are accessed
local function recordCompileDependency(path)
  if #compileTrackingStack > 0 and path then
    local tracker = compileTrackingStack[#compileTrackingStack]
    -- Avoid duplicates
    for _, existing in ipairs(tracker.deps) do
      if existing == path then
        return
      end
    end
    table.insert(tracker.deps, path)
    if ReactiveContext.debugMode then
      print(string.format("[Reactive] Recorded compile dependency: %s", path))
    end
  end
end

-- Expose for use in reactive proxy __index
Reactive._recordCompileDependency = recordCompileDependency

-- Trigger all effects for a dependency (called during reactive writes)
local function triggerEffects(target, key)
  if not target or type(target) ~= "table" then
    return
  end

  if not target._deps or not target._deps[key] then
    return
  end

  for _, effect in ipairs(target._deps[key]) do
    if type(effect) == "function" then
      -- Cycle detection: skip if effect is already executing
      if executingEffects[effect] then
        if ReactiveContext.debugMode then
          print(string.format("[Reactive] ⚠️  Cycle detected: effect tried to trigger itself"))
        end
        goto continue
      end

      -- Mark effect as executing
      executingEffects[effect] = true

      local success, err = pcall(effect)
      if not success then
        print("[Reactive] Error in effect: " .. tostring(err))
      end

      -- Unmark effect
      executingEffects[effect] = nil

      ::continue::
    end
  end
end

-- ============================================================================
-- Reactive Primitives - For Simple Values
-- ============================================================================

--- Create a reactive signal for primitive values (numbers, strings, booleans)
--- Returns getter and setter functions
--- @param initialValue any Initial value
--- @return function, function getter, setter
function Reactive.signal(initialValue)
  local value = initialValue
  local deps = {}

  local function read()
    -- Track dependency if inside reactive context
    if ReactiveContext.activeEffect and ReactiveContext.shouldTrack then
      -- Avoid duplicate subscriptions
      local isDuplicate = false
      for _, dep in ipairs(deps) do
        if dep == ReactiveContext.activeEffect then
          isDuplicate = true
          break
        end
      end
      if not isDuplicate then
        table.insert(deps, ReactiveContext.activeEffect)
      end
    end
    return value
  end

  local function write(newValue)
    if value ~= newValue then
      value = newValue
      -- Trigger all subscribed effects
      for _, effect in ipairs(deps) do
        if type(effect) == "function" then
          local success, err = pcall(effect)
          if not success then
            print("[Reactive] Error in signal effect: " .. tostring(err))
          end
        end
      end
    end
  end

  return read, write
end

-- ============================================================================
-- Reactive Objects - Deep Reactivity for Tables
-- ============================================================================

-- Track which tables are already reactive to avoid double-wrapping
local reactiveMap = {}
setmetatable(reactiveMap, {__mode = "k"})  -- Weak keys for GC

-- Check if a value is a reactive proxy
local function isReactive(value)
  return type(value) == "table" and rawget(value, "_target") ~= nil
end

--- Create a deep reactive object from a table
--- @param target table The table to make reactive
--- @param parent table|nil Parent reactive object (for nested reactivity)
--- @param parentKey string|nil Key in parent (for nested reactivity)
--- @param basePath string|nil Base path for tracking (e.g., "displayedMonth")
--- @return table Reactive proxy
function Reactive.reactive(target, parent, parentKey, basePath)
  if type(target) ~= "table" then
    error("[Reactive] reactive() requires a table, got " .. type(target))
  end

  -- Return existing proxy if already reactive
  if reactiveMap[target] then
    return reactiveMap[target]
  end

  -- Build the path for this proxy
  -- If we have a parent key, extend the base path with it
  local proxyPath = basePath
  if parentKey and type(parentKey) == "string" then
    if basePath then
      proxyPath = basePath .. "." .. parentKey
    else
      proxyPath = parentKey
    end
  elseif parentKey and type(parentKey) == "number" then
    if basePath then
      proxyPath = basePath .. "[" .. parentKey .. "]"
    end
  end

  -- Phase 3: Register variables with KIR manifest (only for top-level state)
  -- If no parent, this is the root state object - register all keys
  if parent == nil then
    for key, value in pairs(target) do
      if type(key) == "string" then
        register_var_with_manifest(key, value)
      end
    end
  end

  -- Create reactive proxy with metatable
  local proxy = setmetatable({}, {
    __index = function(t, key)
      -- Don't track internal keys
      if key == "_target" or key == "_deps" or key == "_path" then
        return rawget(t, key)
      end

      -- Debug: trace reads
      if ReactiveContext.traceReads then
        local effectName = ReactiveContext.activeEffect and "inside effect" or "outside effect"
        print(string.format("[Reactive READ] %s %s", tostring(key), effectName))
      end

      -- Compile-time dependency tracking: record path for binding extraction
      -- This is used by DSL when evaluating computed expressions like DateTime.format(state.year)
      if proxyPath and type(key) == "string" then
        local fullPath = proxyPath .. "." .. key
        recordCompileDependency(fullPath)
      elseif type(key) == "string" then
        recordCompileDependency(key)
      end

      -- Check for pre-populated array indices (LuaJIT compatibility)
      local directValue = rawget(t, key)
      if directValue ~= nil then
        -- Track this access even for direct values
        trackDependency(t, key)
        return directValue
      end

      -- Track this access
      trackDependency(t, key)

      local value = target[key]

      -- Recursively wrap nested tables with proxy caching for consistent identity
      if type(value) == "table" and not isReactive(value) then
        -- Check if we already have a cached proxy for this nested table
        local proxyCache = rawget(target, "_proxyCache")
        if not proxyCache then
          proxyCache = {}
          rawset(target, "_proxyCache", proxyCache)
        end

        -- Return cached proxy if it exists
        local cachedProxy = proxyCache[key]
        if cachedProxy then
          return cachedProxy
        end

        -- Create new proxy and cache it
        -- Pass proxyPath (parent's path) as basePath - Reactive.reactive() will compute the child's path
        local childProxy = Reactive.reactive(value, t, key, proxyPath)
        proxyCache[key] = childProxy
        return childProxy
      end

      return value
    end,

    __newindex = function(t, key, newValue)
      -- Don't allow setting internal keys via metatable
      if key == "_target" or key == "_deps" or key == "_path" then
        rawset(t, key, newValue)
        return
      end

      -- Debug: trace writes
      if ReactiveContext.traceWrites then
        print(string.format("[Reactive WRITE] %s = %s", tostring(key), tostring(newValue)))
      end

      local oldValue = target[key]

      -- Wrap new tables automatically (but don't recurse!)
      if type(newValue) == "table" and not isReactive(newValue) then
        newValue = Reactive.reactive(newValue, t, key)
      end

      if oldValue ~= newValue then
        -- Set on raw target (no recursion)
        rawset(target, key, newValue)

        -- Also update proxy directly for array indices (LuaJIT compatibility)
        if type(key) == "number" then
          rawset(t, key, newValue)
        end

        -- Trigger effects for this key
        triggerEffects(t, key)

        -- Bubble up to parent if exists
        if parent and parentKey then
          triggerEffects(parent, parentKey)
        end
      end
    end,

    -- Preserve table operations
    __len = function(t)
      return #target
    end,

    __pairs = function(t)
      return pairs(target)
    end,

    __ipairs = function(t)
      return ipairs(target)
    end,

    -- Mark as reactive
    __tostring = function(t)
      return "ReactiveProxy(" .. tostring(target) .. ")"
    end,
  })

  -- Store in reactive map
  reactiveMap[target] = proxy
  proxy._target = target
  proxy._deps = {}
  proxy._path = proxyPath  -- Store path for automatic binding detection

  -- LuaJIT Fix: Pre-populate array indices for ipairs/len to work
  -- LuaJIT (Lua 5.1) doesn't call __ipairs or __len on empty tables
  if #target > 0 then
    for i = 1, #target do
      local value = target[i]
      if type(value) == "table" and not isReactive(value) then
        -- Build path for array element
        local elementPath = proxyPath and (proxyPath .. "[" .. i .. "]") or tostring(i)
        local wrapped = Reactive.reactive(value, proxy, i, elementPath)
        -- MUST pre-populate proxy with wrapped value for ipairs/# to work
        rawset(proxy, i, wrapped)
      else
        rawset(proxy, i, value)
      end
    end
  end

  return proxy
end

--- Get the raw underlying object from a reactive proxy
--- Recursively unwraps all nested reactive proxies
--- @param proxy table Reactive proxy
--- @param table table|nil Internal: visited set for cycle detection
--- @return table Raw object without _target or _deps
function Reactive.toRaw(proxy, visited)
  -- Handle primitives: numbers, strings, booleans, nil
  if type(proxy) ~= "table" then
    return proxy
  end

  -- Cycle detection for circular references
  visited = visited or {}
  if visited[proxy] then
    return nil  -- Already processed this table
  end
  visited[proxy] = true

  -- Extract raw data from reactive proxy
  if proxy._target then
    local target = proxy._target

    -- DEBUG: Count properties in _target
    local propCount = 0
    for _ in pairs(target) do
      propCount = propCount + 1
    end

    if propCount == 0 then
      -- WARNING: Empty _target!
      print(string.format("[toRaw] WARNING: _target is empty! keys in proxy:"))
      for k, v in pairs(proxy) do
        if k ~= "_target" and k ~= "_deps" then
          print(string.format("  proxy[%s] = %s", tostring(k), tostring(v)))
        end
      end
    else
      if ReactiveContext.debugMode then
        print(string.format("[toRaw] Extracting _target with %d properties", propCount))
      end
    end

    proxy = target
  end

  -- Recursively clean all nested tables
  local cleaned = {}
  for key, value in pairs(proxy) do
    -- Skip internal reactive metadata
    if key ~= "_target" and key ~= "_deps" and key ~= "_proxyCache" then
      cleaned[key] = Reactive.toRaw(value, visited)
    end
  end

  return cleaned
end

-- ============================================================================
-- Computed Values - Auto-Tracking Derived State
-- ============================================================================

--- Create a computed value that auto-tracks its dependencies
--- @param computeFn function Function that computes the value
--- @return function Getter function for computed value
function Reactive.computed(computeFn)
  if type(computeFn) ~= "function" then
    error("[Reactive] computed() requires a function")
  end

  local value = nil
  local dirty = true
  local deps = {}

  local function recompute()
    -- Clear old dependencies
    deps = {}

    -- Push as active effect to track dependencies
    pushEffect(recompute)

    -- Run computation (auto-tracks dependencies)
    local success, result = pcall(computeFn)
    if not success then
      print("[Reactive] Error in computed: " .. tostring(result))
      value = nil
    else
      value = result
    end
    dirty = false

    -- Pop effect stack
    popEffect()

    -- Trigger our own subscribers
    triggerEffects({_deps = {value = deps}}, "value")
  end

  -- Initial computation
  recompute()

  -- Return getter function
  return function()
    -- Track this computed value as dependency
    if ReactiveContext.activeEffect and ReactiveContext.shouldTrack then
      table.insert(deps, ReactiveContext.activeEffect)
    end

    if dirty then
      recompute()
    end
    return value
  end
end

-- ============================================================================
-- Effects - Side Effects with Auto-Tracking
-- ============================================================================

--- Run an effect that auto-tracks its dependencies
--- @param effectFn function Function to run
--- @return function Cleanup/stop function
function Reactive.effect(effectFn)
  if type(effectFn) ~= "function" then
    error("[Reactive] effect() requires a function")
  end

  local cleanup = nil
  local stopped = false

  local function runEffect()
    if stopped then return end

    -- Run cleanup from previous execution
    if cleanup then
      if type(cleanup) == "function" then
        pcall(cleanup)
      end
      cleanup = nil
    end

    -- Push as active effect to track dependencies
    pushEffect(runEffect)

    -- Run effect (auto-tracks dependencies)
    local success, result = pcall(effectFn)
    if not success then
      print("[Reactive] Error in effect: " .. tostring(result))
    else
      cleanup = result  -- Effect can return cleanup function
    end

    -- Pop effect stack
    popEffect()
  end

  -- Run immediately
  runEffect()

  -- Return stop function
  return function()
    stopped = true
    if cleanup and type(cleanup) == "function" then
      pcall(cleanup)
    end
  end
end

-- ============================================================================
-- Batch Updates - Performance Optimization
-- ============================================================================

local batchDepth = 0
local pendingEffects = {}

--- Batch multiple reactive updates to minimize re-renders
--- @param batchFn function Function containing reactive updates
function Reactive.batch(batchFn)
  batchDepth = batchDepth + 1
  ReactiveContext.shouldTrack = false

  local success, err = pcall(batchFn)

  batchDepth = batchDepth - 1

  -- Flush all pending effects when batch completes
  if batchDepth == 0 then
    ReactiveContext.shouldTrack = true
    for _, effect in ipairs(pendingEffects) do
      if type(effect) == "function" then
        pcall(effect)
      end
    end
    pendingEffects = {}
  end

  if not success then
    ReactiveContext.shouldTrack = true
    error("[Reactive] Error in batch: " .. tostring(err))
  end
end

-- ============================================================================
-- Utilities
-- ============================================================================

--- Check if a value is reactive
--- @param value any Value to check
--- @return boolean True if reactive
function Reactive.isReactive(value)
  return isReactive(value)
end

--- Unwrap a reactive value (get raw value)
--- @param value any Potentially reactive value
--- @return any Raw value
function Reactive.unref(value)
  -- If it's a function (signal getter or computed), call it
  if type(value) == "function" then
    return value()
  end
  -- If it's a reactive object, return raw
  if isReactive(value) then
    return Reactive.toRaw(value)
  end
  -- Otherwise return as-is
  return value
end

-- ============================================================================
-- Debugging Utilities
-- ============================================================================

--- Enable debug mode with optional trace flags
function Reactive.enableDebug(options)
  options = options or {}
  ReactiveContext.debugMode = true
  ReactiveContext.traceReads = options.traceReads or false
  ReactiveContext.traceWrites = options.traceWrites or false
  print("[Reactive] Debug mode enabled")
end

--- Disable debug mode
function Reactive.disableDebug()
  ReactiveContext.debugMode = false
  ReactiveContext.traceReads = false
  ReactiveContext.traceWrites = false
end

--- Get current effect stack for debugging
function Reactive.getEffectStack()
  return ReactiveContext.effectStack
end

--- Get active effect for debugging
function Reactive.getActiveEffect()
  return ReactiveContext.activeEffect
end

-- ============================================================================
-- Export
-- ============================================================================

return Reactive
