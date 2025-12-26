--- Kryon Lua DSL Utility Helpers
--- Functional programming helpers to reduce table.insert verbosity
local M = {}

--- Transform array with function
--- @param tbl table Input array
--- @param fn function Transform function(value, index) -> newValue
--- @return table Transformed array
function M.map(tbl, fn)
  local result = {}
  for i, v in ipairs(tbl) do
    result[i] = fn(v, i)
  end
  return result
end

--- Filter array with predicate
--- @param tbl table Input array
--- @param predicate function Test function(value) -> boolean
--- @return table Filtered array
function M.filter(tbl, predicate)
  local result = {}
  for _, v in ipairs(tbl) do
    if predicate(v) then
      result[#result + 1] = v
    end
  end
  return result
end

--- Generate numeric range
--- @param start number Start value (inclusive)
--- @param stop number Stop value (inclusive)
--- @param step number Step value (default 1)
--- @return table Array of numbers
function M.range(start, stop, step)
  step = step or 1
  local result = {}
  for i = start, stop, step do
    result[#result + 1] = i
  end
  return result
end

--- Flatten nested arrays one level
--- @param tbl table Array potentially containing nested arrays
--- @return table Flattened array
function M.flatten(tbl)
  local result = {}
  for _, v in ipairs(tbl) do
    if type(v) == "table" and #v > 0 then
      for _, nested in ipairs(v) do
        result[#result + 1] = nested
      end
    else
      result[#result + 1] = v
    end
  end
  return result
end

--- Build array from items using builder function
--- @param items table Source array
--- @param builder function Builder function(item, index) -> component
--- @return table Array of components
function M.build(items, builder)
  return M.map(items, builder)
end

--- Conditional component (returns component if true, nil if false)
--- @param condition boolean Condition to test
--- @param component any Component to return if true
--- @return any Component or nil
function M.when(condition, component)
  return condition and component or nil
end

return M
