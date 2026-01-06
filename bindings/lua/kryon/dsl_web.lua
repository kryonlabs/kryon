-- Kryon Web DSL
-- Simplified DSL for browser environment
-- Works with pre-rendered HTML - provides DOM manipulation helpers

local Runtime = require("kryon.runtime_web")
local js = require("js")
local document = js.global.document

local UI = {}

-- ============================================================================
-- Component Functions (Stubs for Web)
-- ============================================================================

-- In web mode, components are already rendered in HTML.
-- These functions are stubs that just return props.

--- Button component
--- @param props table Component properties
--- @return table props
function UI.Button(props)
    return props
end

--- Text component
--- @param props table Component properties
--- @return table props
function UI.Text(props)
    return props
end

--- Input component
--- @param props table Component properties
--- @return table props
function UI.Input(props)
    return props
end

--- Row component
--- @param props table Component properties
--- @return table props
function UI.Row(props)
    return props
end

--- Column component
--- @param props table Component properties
--- @return table props
function UI.Column(props)
    return props
end

--- Container component
--- @param props table Component properties
--- @return table props
function UI.Container(props)
    return props
end

--- Element component
--- @param props table Component properties
--- @return table props
function UI.Element(props)
    return props
end

--- Modal component
--- @param props table Component properties
--- @return table props
function UI.Modal(props)
    return props
end

--- TabGroup component
--- @param props table Component properties
--- @return table props
function UI.TabGroup(props)
    return props
end

--- TabBar component
--- @param props table Component properties
--- @return table props
function UI.TabBar(props)
    return props
end

--- TabContent component
--- @param props table Component properties
--- @return table props
function UI.TabContent(props)
    return props
end

--- TabPanel component
--- @param props table Component properties
--- @return table props
function UI.TabPanel(props)
    return props
end

--- Tab component
--- @param props table Component properties
--- @return table props
function UI.Tab(props)
    return props
end

-- ============================================================================
-- Utility Functions
-- ============================================================================

--- Map over an array and transform each element
--- @param arr table Array to map over
--- @param fn function(value, index) Transform function
--- @return table Transformed array
function UI.mapArray(arr, fn)
    local result = {}
    for i, v in ipairs(arr) do
        result[i] = fn(v, i)
    end
    return result
end

--- Lua 5.1/5.2 compatibility for unpacking arrays
UI.unpack = unpack or table.unpack

-- ============================================================================
-- DOM Update Helpers
-- ============================================================================

--- Update a text element's content
--- @param elementId string Element ID (e.g., "text-58")
--- @param text string New text content
function UI.updateText(elementId, text)
    Runtime.updateText(elementId, text)
end

--- Update an element's style
--- @param elementId string Element ID
--- @param styles table Key-value pairs of CSS properties
function UI.updateStyles(elementId, styles)
    for property, value in pairs(styles) do
        Runtime.updateStyle(elementId, property, value)
    end
end

--- Show an element
--- @param elementId string Element ID
function UI.show(elementId)
    Runtime.updateStyle(elementId, "display", "")
end

--- Hide an element
--- @param elementId string Element ID
function UI.hide(elementId)
    Runtime.updateStyle(elementId, "display", "none")
end

--- Open a modal
--- @param modalId string Modal ID
function UI.openModal(modalId)
    Runtime.openModal(modalId)
end

--- Close a modal
--- @param modalId string Modal ID
function UI.closeModal(modalId)
    Runtime.closeModal(modalId)
end

return UI
