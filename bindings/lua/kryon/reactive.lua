-- Kryon Reactive State System
-- Lua-side observable state that triggers IR tree updates via C calls
-- Similar to Nim's ReactiveState[T]

local Reactive = {}

-- ============================================================================
-- ReactiveState
-- ============================================================================

local ReactiveState = {}
ReactiveState.__index = ReactiveState

--- Create a new reactive state value
--- @param initialValue any Initial value
--- @return table ReactiveState object
function Reactive.state(initialValue)
  local self = setmetatable({
    _value = initialValue,
    _observers = {},
  }, ReactiveState)
  return self
end

--- Get the current value
--- @return any Current value
function ReactiveState:get()
  return self._value
end

--- Set a new value and notify observers
--- @param newValue any New value
function ReactiveState:set(newValue)
  if self._value ~= newValue then
    local oldValue = self._value
    self._value = newValue
    self:_notify(newValue, oldValue)
  end
end

--- Update value using a function
--- @param updateFn function Function that receives old value and returns new value
function ReactiveState:update(updateFn)
  local newValue = updateFn(self._value)
  self:set(newValue)
end

--- Notify all observers of value change
--- @param newValue any New value
--- @param oldValue any Old value
function ReactiveState:_notify(newValue, oldValue)
  for _, observer in ipairs(self._observers) do
    local success, err = pcall(observer, newValue, oldValue)
    if not success then
      print("Error in reactive observer: " .. tostring(err))
    end
  end
end

--- Register an observer callback
--- @param callback function Function called when value changes
--- @return function Unsubscribe function
function ReactiveState:observe(callback)
  if type(callback) ~= "function" then
    error("observe requires a function callback")
  end

  table.insert(self._observers, callback)

  -- Return unsubscribe function
  return function()
    for i, obs in ipairs(self._observers) do
      if obs == callback then
        table.remove(self._observers, i)
        break
      end
    end
  end
end

--- Get the number of observers
--- @return number Number of observers
function ReactiveState:observerCount()
  return #self._observers
end

-- ============================================================================
-- Computed Values (Derived State)
-- ============================================================================

--- Create a computed value that depends on other reactive values
--- @param dependencies table Array of ReactiveState objects
--- @param computeFn function Function that computes the derived value
--- @return table ReactiveState object
function Reactive.computed(dependencies, computeFn)
  if type(dependencies) ~= "table" then
    error("computed requires a table of dependencies")
  end
  if type(computeFn) ~= "function" then
    error("computed requires a compute function")
  end

  -- Create a reactive state for the computed result
  local result = Reactive.state(computeFn())

  -- Subscribe to all dependencies
  for _, dep in ipairs(dependencies) do
    dep:observe(function()
      result:set(computeFn())
    end)
  end

  return result
end

-- ============================================================================
-- Reactive Collections
-- ============================================================================

local ReactiveArray = {}
ReactiveArray.__index = ReactiveArray

--- Create a reactive array
--- @param initialItems table Optional initial items
--- @return table ReactiveArray object
function Reactive.array(initialItems)
  local self = setmetatable({
    _items = initialItems or {},
    _observers = {},
  }, ReactiveArray)
  return self
end

--- Get all items
--- @return table Array of items
function ReactiveArray:getItems()
  return self._items
end

--- Get item at index
--- @param index number Index (1-based)
--- @return any Item at index
function ReactiveArray:get(index)
  return self._items[index]
end

--- Push an item to the end
--- @param item any Item to add
function ReactiveArray:push(item)
  table.insert(self._items, item)
  self:_notify("push", #self._items, item)
end

--- Remove item at index
--- @param index number Index to remove (1-based)
--- @return any Removed item
function ReactiveArray:remove(index)
  local item = table.remove(self._items, index)
  self:_notify("remove", index, item)
  return item
end

--- Insert item at index
--- @param index number Index to insert at (1-based)
--- @param item any Item to insert
function ReactiveArray:insert(index, item)
  table.insert(self._items, index, item)
  self:_notify("insert", index, item)
end

--- Clear all items
function ReactiveArray:clear()
  self._items = {}
  self:_notify("clear")
end

--- Get array length
--- @return number Length
function ReactiveArray:length()
  return #self._items
end

--- Notify observers of array change
--- @param action string Action type (push, remove, insert, clear)
--- @param ... any Additional arguments
function ReactiveArray:_notify(action, ...)
  for _, observer in ipairs(self._observers) do
    local success, err = pcall(observer, action, ...)
    if not success then
      print("Error in reactive array observer: " .. tostring(err))
    end
  end
end

--- Register an observer callback
--- @param callback function Function called when array changes
--- @return function Unsubscribe function
function ReactiveArray:observe(callback)
  if type(callback) ~= "function" then
    error("observe requires a function callback")
  end

  table.insert(self._observers, callback)

  -- Return unsubscribe function
  return function()
    for i, obs in ipairs(self._observers) do
      if obs == callback then
        table.remove(self._observers, i)
        break
      end
    end
  end
end

-- ============================================================================
-- Reactive Effects
-- ============================================================================

--- Run an effect when dependencies change
--- @param dependencies table Array of ReactiveState objects
--- @param effectFn function Function to run
--- @return function Cleanup function
function Reactive.effect(dependencies, effectFn)
  if type(dependencies) ~= "table" then
    error("effect requires a table of dependencies")
  end
  if type(effectFn) ~= "function" then
    error("effect requires an effect function")
  end

  -- Run the effect immediately
  effectFn()

  -- Subscribe to all dependencies
  local unsubscribers = {}
  for _, dep in ipairs(dependencies) do
    local unsub = dep:observe(function()
      effectFn()
    end)
    table.insert(unsubscribers, unsub)
  end

  -- Return cleanup function
  return function()
    for _, unsub in ipairs(unsubscribers) do
      unsub()
    end
  end
end

-- ============================================================================
-- Batch Updates
-- ============================================================================

Reactive._batchDepth = 0
Reactive._pendingUpdates = {}

--- Begin a batch update (defer notifications)
function Reactive.batch(batchFn)
  Reactive._batchDepth = Reactive._batchDepth + 1

  local success, err = pcall(batchFn)

  Reactive._batchDepth = Reactive._batchDepth - 1

  if Reactive._batchDepth == 0 then
    -- Execute all pending updates
    for _, update in ipairs(Reactive._pendingUpdates) do
      update()
    end
    Reactive._pendingUpdates = {}
  end

  if not success then
    error("Error in batch function: " .. tostring(err))
  end
end

return Reactive
