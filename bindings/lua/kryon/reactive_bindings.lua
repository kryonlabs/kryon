-- Reactive Bindings for Kryon Web
-- Provides helpers for binding reactive state to DOM elements
-- Uses fine-grained reactivity (SolidJS-style) instead of virtual DOM

local Reactive = require("kryon.reactive")

local Bindings = {}

-- Cache document reference
local document = nil

local function getDocument()
    if document then return document end
    local js = require("js")
    document = js.global.document
    return document
end

-- Bind a function to an element's text content
-- The function is called inside an effect to track dependencies
function Bindings.text(elementId, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            element.textContent = tostring(value or "")
        end
    end)
end

-- Bind a function to an element's innerHTML
function Bindings.html(elementId, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            element.innerHTML = tostring(value or "")
        end
    end)
end

-- Bind a function to an element's style property
function Bindings.style(elementId, property, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            element.style[property] = tostring(value or "")
        end
    end)
end

-- Shortcut for background color (common case)
function Bindings.backgroundColor(elementId, fn)
    Bindings.style(elementId, "backgroundColor", fn)
end

-- Shortcut for color
function Bindings.color(elementId, fn)
    Bindings.style(elementId, "color", fn)
end

-- Conditional visibility
function Bindings.visible(elementId, fn)
    Reactive.effect(function()
        local show = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            element.style.display = show and "" or "none"
        end
    end)
end

-- Toggle CSS class based on condition
function Bindings.classList(elementId, className, fn)
    Reactive.effect(function()
        local add = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            if add then
                element.classList:add(className)
            else
                element.classList:remove(className)
            end
        end
    end)
end

-- Bind to an element's attribute
function Bindings.attribute(elementId, attrName, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            if value == nil or value == false then
                element:removeAttribute(attrName)
            else
                element:setAttribute(attrName, tostring(value))
            end
        end
    end)
end

-- Bind to input value (for controlled inputs)
function Bindings.inputValue(elementId, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            element.value = tostring(value or "")
        end
    end)
end

-- Bind to disabled state
function Bindings.disabled(elementId, fn)
    Reactive.effect(function()
        local isDisabled = fn()
        local doc = getDocument()
        local element = doc:getElementById(elementId)
        if element then
            element.disabled = isDisabled and true or false
        end
    end)
end

-- Query-based binding: Update all elements matching a selector
-- Useful for list items with data attributes
function Bindings.queryStyle(selector, property, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local elements = doc:querySelectorAll(selector)
        for i = 0, elements.length - 1 do
            local el = elements[i]
            el.style[property] = tostring(value or "")
        end
    end)
end

-- Query-based text binding
function Bindings.queryText(selector, fn)
    Reactive.effect(function()
        local value = fn()
        local doc = getDocument()
        local elements = doc:querySelectorAll(selector)
        for i = 0, elements.length - 1 do
            local el = elements[i]
            el.textContent = tostring(value or "")
        end
    end)
end

-- Custom effect that runs whenever dependencies change
-- For complex DOM updates that don't fit other patterns
function Bindings.custom(fn)
    Reactive.effect(fn)
end

-- Batch multiple DOM updates together
-- Useful for performance when updating many elements
function Bindings.batch(fn)
    Reactive.batch(fn)
end

return Bindings
