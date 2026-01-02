-- Kryon Automatic Reactivity System
-- Vue 3 / SolidJS-inspired automatic dependency tracking
-- Clean, beautiful, no legacy code

local Reactive = {}

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
  return type(value) == "table" and reactiveMap[value] ~= nil
end

--- Create a deep reactive object from a table
--- @param target table The table to make reactive
--- @param parent table|nil Parent reactive object (for nested reactivity)
--- @param parentKey string|nil Key in parent (for nested reactivity)
--- @return table Reactive proxy
function Reactive.reactive(target, parent, parentKey)
  if type(target) ~= "table" then
    error("[Reactive] reactive() requires a table, got " .. type(target))
  end

  -- Return existing proxy if already reactive
  if reactiveMap[target] then
    return reactiveMap[target]
  end

  -- Create reactive proxy with metatable
  local proxy = setmetatable({}, {
    __index = function(t, key)
      -- Don't track internal keys
      if key == "_target" or key == "_deps" then
        return rawget(t, key)
      end

      -- Debug: trace reads
      if ReactiveContext.traceReads then
        local effectName = ReactiveContext.activeEffect and "inside effect" or "outside effect"
        print(string.format("[Reactive READ] %s %s", tostring(key), effectName))
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

      -- Recursively wrap nested tables (don't store back to avoid contaminating target)
      if type(value) == "table" and not isReactive(value) then
        local childProxy = Reactive.reactive(value, t, key)
        return childProxy
      end

      return value
    end,

    __newindex = function(t, key, newValue)
      -- Don't allow setting internal keys via metatable
      if key == "_target" or key == "_deps" then
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

  -- LuaJIT Fix: Pre-populate array indices for ipairs/len to work
  -- LuaJIT (Lua 5.1) doesn't call __ipairs or __len on empty tables
  if #target > 0 then
    for i = 1, #target do
      local value = target[i]
      if type(value) == "table" and not isReactive(value) then
        local wrapped = Reactive.reactive(value, proxy, i)
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
    if key ~= "_target" and key ~= "_deps" then
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
