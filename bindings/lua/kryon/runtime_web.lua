-- Kryon Web Runtime
-- Handles event dispatch and DOM interaction for browser environment
-- Uses Fengari's js module instead of FFI

local js = require("js")
local window = js.global
local document = window.document

local Runtime = {}

-- ============================================================================
-- DOM Manipulation Helpers
-- ============================================================================

--- Get an element by ID
--- @param id string Element ID
--- @return userdata|nil Element or nil if not found
function Runtime.getElementById(id)
    return document:getElementById(id)
end

--- Update element text content
--- @param elementId string Element ID
--- @param text string New text content
function Runtime.updateText(elementId, text)
    local element = document:getElementById(elementId)
    if element then
        element.textContent = text
    end
end

--- Update element style
--- @param elementId string Element ID
--- @param property string CSS property name
--- @param value string CSS property value
function Runtime.updateStyle(elementId, property, value)
    local element = document:getElementById(elementId)
    if element then
        element.style[property] = value
    end
end

--- Add/remove CSS class
--- @param elementId string Element ID
--- @param className string Class name
--- @param add boolean True to add, false to remove
function Runtime.toggleClass(elementId, className, add)
    local element = document:getElementById(elementId)
    if element then
        if add then
            element.classList:add(className)
        else
            element.classList:remove(className)
        end
    end
end

--- Query all elements matching a CSS selector
--- @param selector string CSS selector
--- @return userdata NodeList of matching elements
function Runtime.querySelectorAll(selector)
    return document:querySelectorAll(selector)
end

--- Query a single element matching a CSS selector
--- @param selector string CSS selector
--- @return userdata|nil First matching element or nil
function Runtime.querySelector(selector)
    return document:querySelector(selector)
end

--- Set an element's attribute
--- @param elementId string Element ID
--- @param attr string Attribute name
--- @param value string Attribute value
function Runtime.setAttribute(elementId, attr, value)
    local element = document:getElementById(elementId)
    if element then
        element:setAttribute(attr, tostring(value))
    end
end

--- Get an element's attribute
--- @param elementId string Element ID
--- @param attr string Attribute name
--- @return string|nil Attribute value or nil
function Runtime.getAttribute(elementId, attr)
    local element = document:getElementById(elementId)
    if element then
        return element:getAttribute(attr)
    end
    return nil
end

--- Remove an element's attribute
--- @param elementId string Element ID
--- @param attr string Attribute name
function Runtime.removeAttribute(elementId, attr)
    local element = document:getElementById(elementId)
    if element then
        element:removeAttribute(attr)
    end
end

--- Set an input element's value
--- @param elementId string Input element ID
--- @param value string New value
function Runtime.setInputValue(elementId, value)
    local element = document:getElementById(elementId)
    if element then
        element.value = tostring(value or "")
    end
end

--- Get an input element's value
--- @param elementId string Input element ID
--- @return string|nil Input value or nil
function Runtime.getInputValue(elementId)
    local element = document:getElementById(elementId)
    if element then
        return element.value
    end
    return nil
end

--- Set element's disabled state
--- @param elementId string Element ID
--- @param disabled boolean True to disable
function Runtime.setDisabled(elementId, disabled)
    local element = document:getElementById(elementId)
    if element then
        element.disabled = disabled and true or false
    end
end

--- Update element's innerHTML (use with caution - XSS risk)
--- @param elementId string Element ID
--- @param html string HTML content
function Runtime.updateHTML(elementId, html)
    local element = document:getElementById(elementId)
    if element then
        element.innerHTML = html
    end
end

-- ============================================================================
-- Tab Switching Helpers
-- ============================================================================

--- Select a tab in a TabGroup by index
--- @param tabGroupId string TabGroup element ID (e.g., "tabgroup-336")
--- @param index number 0-based tab index
function Runtime.selectTab(tabGroupId, index)
    local tabGroup = document:getElementById(tabGroupId)
    if not tabGroup then
        return
    end

    -- Update data-selected attribute
    tabGroup:setAttribute("data-selected", tostring(index))

    -- Find tab bar and update tab buttons
    local tabBar = tabGroup:querySelector(".kryon-tab-bar")
    if tabBar then
        local tabs = tabBar.children
        for i = 0, tabs.length - 1 do
            local tab = tabs[i]
            local isSelected = (i == index)
            tab:setAttribute("aria-selected", isSelected and "true" or "false")
            -- Update visual styling
            if isSelected then
                tab.style.borderBottomColor = "#5c6bc0"
                tab.style.color = "#ffffff"
            else
                tab.style.borderBottomColor = "transparent"
                tab.style.color = "#888888"
            end
        end
    end

    -- Find tab content and show/hide panels
    local tabContent = tabGroup:querySelector(".kryon-tab-content")
    if tabContent then
        local panels = tabContent.children
        for i = 0, panels.length - 1 do
            local panel = panels[i]
            if i == index then
                panel:removeAttribute("hidden")
            else
                panel:setAttribute("hidden", "")
            end
        end
    end
end

--- Find the first TabGroup element on the page
--- @return string|nil TabGroup ID or nil if not found
function Runtime.findFirstTabGroup()
    local tabGroup = document:querySelector(".kryon-tabs")
    if tabGroup then
        return tabGroup.id
    end
    return nil
end

-- ============================================================================
-- Date Helpers (Fengari os.date/os.time don't work reliably)
-- ============================================================================

--- Get current date info using JavaScript Date
--- @return table { year, month, day }
function Runtime.getCurrentDate()
    local Date = js.global.Date
    local now = js.new(Date)
    return {
        year = math.floor(now:getFullYear()),
        month = math.floor(now:getMonth()) + 1,  -- JS months are 0-based
        day = math.floor(now:getDate())
    }
end

--- Get current date as string (YYYY-MM-DD)
--- @return string Date string
function Runtime.getCurrentDateString()
    local d = Runtime.getCurrentDate()
    return string.format("%04d-%02d-%02d", d.year, d.month, d.day)
end

--- Format a month/year as text (e.g., "January 2026")
--- @param year number Year
--- @param month number Month (1-12)
--- @return string Formatted month name
function Runtime.formatMonthYear(year, month)
    local monthNames = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    }
    return monthNames[math.floor(month)] .. " " .. string.format("%d", year)
end

-- ============================================================================
-- Page Refresh (temporary solution for state-dependent views)
-- ============================================================================

--- Force a full page refresh
--- Use this when navigating to a different view (e.g., month change)
--- Note: This reloads the page, state will be restored from localStorage
function Runtime.forceRefresh()
    window.location:reload()
end

--- Toggle a calendar cell's completion state visually
--- @param elementId string Button element ID (e.g., "btn-55")
--- @param isCompleted boolean Whether the day is completed
--- @param completedColor string Color when completed (e.g., "#9b59b6")
function Runtime.updateCalendarCell(elementId, isCompleted, completedColor)
    local element = document:getElementById(elementId)
    if element then
        if isCompleted then
            element.style.backgroundColor = completedColor
            element.style.color = "#ffffff"
        else
            element.style.backgroundColor = "transparent"
            element.style.color = "#888888"
        end
    end
end

-- ============================================================================
-- ForEach Component Support (Dynamic List Rendering)
-- ============================================================================

-- Registry of all ForEach containers and their render functions
local forEachContainers = {}

--- Register a ForEach container for runtime re-rendering
--- @param containerId string ForEach container element ID
--- @param config table Configuration table with:
---   - source: string - Data path (e.g., "state.calendarDays")
---   - itemName: string - Variable name for each item (e.g., "day")
---   - indexName: string - Variable name for index (e.g., "index")
---   - renderFn: function - Render function for each item
function Runtime.registerForEachContainer(containerId, config)
    forEachContainers[containerId] = {
        source = config.source,
        itemName = config.itemName or "item",
        indexName = config.indexName or "index",
        renderFn = config.renderFn,
        element = document:getElementById(containerId)
    }
    print("[Runtime] Registered ForEach container: " .. containerId .. " -> " .. config.source)
end

--- Unregister a ForEach container
--- @param containerId string ForEach container element ID
function Runtime.unregisterForEachContainer(containerId)
    forEachContainers[containerId] = nil
end

--- Get a reactive value from a path string (e.g., "state.calendarDays")
--- @param path string Dot-notation path to the value
--- @return table|nil The value at the path
function Runtime.getReactiveValue(path)
    -- Start with global scope
    local value = _G
    local parts = {}
    for part in string.gmatch(path, "[^.]+") do
        table.insert(parts, part)
    end

    for _, part in ipairs(parts) do
        if type(value) == "table" then
            value = value[part]
        else
            return nil
        end
    end

    return value
end

--- Re-render a ForEach container with new data
--- @param containerId string ForEach container element ID
--- @param newData table New array of data to render
function Runtime.renderForEach(containerId, newData)
    local config = forEachContainers[containerId]
    if not config then
        print("[Runtime] ForEach container not found: " .. containerId)
        return
    end

    local element = config.element
    if not element then
        element = document:getElementById(containerId)
        if not element then
            print("[Runtime] ForEach element not found: " .. containerId)
            return
        end
        config.element = element
    end

    -- Clear existing children (except text nodes we want to preserve)
    while element.firstChild do
        element:removeChild(element.firstChild)
    end

    -- Render each item
    if type(newData) == "table" then
        for i, item in ipairs(newData) do
            -- Call the render function with item and index
            local success, result = pcall(config.renderFn, item, i)
            if success and result then
                -- The render function should return a table with component info
                -- For now, we'll create a simple element
                local child = document:createElement("div")
                child.textContent = tostring(item.text or tostring(item))
                element:appendChild(child)
            end
        end
    end
end

--- Update all ForEach containers watching a specific data path
--- @param path string Data path that changed (e.g., "state.habits[1].calendarDays")
function Runtime.updateForEachByPath(path)
    for containerId, config in pairs(forEachContainers) do
        -- Check if this ForEach is watching the changed path
        if config.source == path or string.match(path, "^" .. string.gsub(config.source, "%.", "%.")) then
            local newData = Runtime.getReactiveValue(config.source)
            Runtime.renderForEach(containerId, newData)
        end
    end
end

--- Get all ForEach containers (for debugging/testing)
--- @return table Map of container ID to config
function Runtime.getForEachContainers()
    return forEachContainers
end

-- ============================================================================
-- Modal Helpers
-- ============================================================================

--- Open a modal dialog
--- @param modalId string Modal ID
function Runtime.openModal(modalId)
    local dialog = document:getElementById(modalId)
    if dialog and not dialog.open then
        dialog:showModal()
    end
end

--- Close a modal dialog
--- @param modalId string Modal ID
function Runtime.closeModal(modalId)
    local dialog = document:getElementById(modalId)
    if dialog and dialog.open then
        dialog:close()
    end
end

-- ============================================================================
-- App Entry Point (Web Version)
-- ============================================================================

--- Create an app for web
--- In web mode, this just calls the root function to register handlers
--- The HTML is already pre-rendered by the codegen
--- @param config table { root = function, window = { width, height, title } }
--- @return table App instance
function Runtime.createApp(config)
    if config.root and type(config.root) == "function" then
        -- Call root function to register all event handlers
        local success, err = pcall(config.root)
        if not success then
            print("[Kryon] Init error: " .. tostring(err))
        end
    end

    -- Signal that Lua runtime is fully initialized
    window.kryon_lua_init_complete()

    return {
        run = function() end,
        stop = function() end
    }
end

return Runtime
