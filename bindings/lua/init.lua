-- Kryon Lua Frontend
-- High-level Lua API for Kryon UI framework
-- This module provides a clean, Lua-idiomatic interface to the Kryon C core

local kryon = require("kryon")

-- Module table
local M = {}

-- Import low-level functions
local container = kryon.container
local text = kryon.text
local button = kryon.button
local input = kryon.input
local canvas = kryon.canvas
local markdown = kryon.markdown
local color_rgba = kryon.color_rgba
local color_rgb = kryon.color_rgb

-- ============================================================================
-- Utility Functions
-- ============================================================================

-- Color helpers
function M.rgba(r, g, b, a)
    a = a or 255
    return color_rgba(r, g, b, a)
end

function M.rgb(r, g, b)
    return color_rgb(r, g, b)
end

-- Common color constants
M.colors = {
    white = M.rgb(255, 255, 255),
    black = M.rgb(0, 0, 0),
    red = M.rgb(255, 0, 0),
    green = M.rgb(0, 255, 0),
    blue = M.rgb(0, 0, 255),
    yellow = M.rgb(255, 255, 0),
    cyan = M.rgb(0, 255, 255),
    magenta = M.rgb(255, 0, 255),
    gray = M.rgb(128, 128, 128),
    light_gray = M.rgb(192, 192, 192),
    dark_gray = M.rgb(64, 64, 64),
    transparent = M.rgba(0, 0, 0, 0)
}

-- ============================================================================
-- Component Builder Functions
-- ============================================================================

-- Container builder
function M.container(config)
    config = config or {}
    local comp = container()

    -- Apply common properties
    if config.x or config.y or config.width or config.height then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width or 100,
            config.height or 100
        )
    end

    if config.background then
        comp:set_background_color(config.background)
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- Text builder
function M.text(config)
    config = config or {}
    local comp = text(config.text or "", config.font_id or 0)

    -- Apply common properties
    if config.x or config.y or config.width or config.height then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width or 100,
            config.height or 30
        )
    end

    if config.color then
        comp:set_background_color(config.color)
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- Button builder
function M.button(config)
    config = config or {}
    local comp = button(config.text or "Button", config.font_id or 0)

    -- Apply common properties
    if config.x or config.y or config.width or config.height then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width or 100,
            config.height or 40
        )
    end

    if config.background then
        comp:set_background_color(config.background)
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Set click handler if provided
    if config.on_click then
        -- TODO: Implement event handler registration
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- Input builder
function M.input(config)
    config = config or {}
    local comp = input(config.placeholder or "", config.value or "", config.font_id or 0, config.password or false)

    -- Apply common properties
    if config.x or config.y or config.width or config.height then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width or 200,
            config.height or 40
        )
    end

    if config.background then
        comp:set_background_color(config.background)
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Set event handlers if provided
    if config.on_change then
        -- TODO: Implement event handler registration
    end

    if config.on_submit then
        -- TODO: Implement event handler registration
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- Canvas builder
function M.canvas(config)
    config = config or {}
    local comp = canvas(config.width or 400, config.height or 300, config.background or M.colors.black)

    -- Apply common properties
    if config.x or config.y then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width,
            config.height
        )
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Set draw handler if provided
    if config.on_draw then
        -- TODO: Implement canvas draw handler registration
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- Markdown builder
function M.markdown(config)
    config = config or {}
    local comp = markdown(config.source or "", config.width or 400, config.height or 300)

    -- Apply common properties
    if config.x or config.y then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width,
            config.height
        )
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Set theme if provided
    if config.theme then
        -- TODO: Set markdown theme in C bindings
        print("Setting markdown theme:", config.theme.name or "custom")
    end

    -- Set link click handler if provided
    if config.on_link_click then
        -- TODO: Implement link click handler in C bindings
        print("Setting markdown link click handler")
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- Enhanced Canvas builder with draw handler support
function M.canvas_enhanced(config)
    config = config or {}
    local comp = canvas(config.width or 400, config.height or 300, config.background or M.colors.black)

    -- Apply common properties
    if config.x or config.y then
        comp:set_bounds(
            config.x or 0,
            config.y or 0,
            config.width,
            config.height
        )
    end

    if config.visible ~= nil then
        comp:set_visible(config.visible)
    end

    -- Set draw handler if provided
    if config.on_draw then
        -- Store draw handler for later execution
        comp._draw_handler = config.on_draw
        print("Setting canvas draw handler")
    end

    -- Add children if provided
    if config.children then
        for _, child in ipairs(config.children) do
            comp:add_child(child)
        end
    end

    return comp
end

-- ============================================================================
-- Layout Helper Functions
-- ============================================================================

-- Create a vertical layout (column)
function M.column(config)
    config = config or {}

    -- Calculate child positions
    local y = config.y or 0
    local total_height = 0

    if config.children then
        for i, child in ipairs(config.children) do
            local child_y = y
            if i > 1 then
                child_y = child_y + (config.gap or 10)
            end

            -- Set child position
            local child_x = config.x or 0
            child:set_bounds(child_x, child_y,
                           config.child_width or 100,
                           config.child_height or 30)

            y = child_y + (config.child_height or 30)
            total_height = y - (config.y or 0)
        end
    end

    return M.container({
        x = config.x,
        y = config.y,
        width = config.width or 200,
        height = config.height or total_height,
        background = config.background,
        children = config.children
    })
end

-- Create a horizontal layout (row)
function M.row(config)
    config = config or {}

    -- Calculate child positions
    local x = config.x or 0
    local total_width = 0

    if config.children then
        for i, child in ipairs(config.children) do
            local child_x = x
            if i > 1 then
                child_x = child_x + (config.gap or 10)
            end

            -- Set child position
            local child_y = config.y or 0
            child:set_bounds(child_x, child_y,
                           config.child_width or 100,
                           config.child_height or 30)

            x = child_x + (config.child_width or 100)
            total_width = x - (config.x or 0)
        end
    end

    return M.container({
        x = config.x,
        y = config.y,
        width = config.width or total_width,
        height = config.height or 50,
        background = config.background,
        children = config.children
    })
end

-- Center content
function M.center(config)
    config = config or {}

    if config.children then
        for _, child in ipairs(config.children) do
            -- Center the child within the container
            local container_width = config.width or 200
            local container_height = config.height or 200
            local child_width = config.child_width or 100
            local child_height = config.child_height or 30

            local centered_x = (container_width - child_width) / 2
            local centered_y = (container_height - child_height) / 2

            child:set_bounds(
                (config.x or 0) + centered_x,
                (config.y or 0) + centered_y,
                child_width,
                child_height
            )
        end
    end

    return M.container(config)
end

-- ============================================================================
-- Application Builder
-- ============================================================================

function M.app(config)
    config = config or {}

    local app_instance = kryon.app({
        width = config.width or 800,
        height = config.height or 600,
        title = config.title or "Kryon App"
    })

    -- Build the UI tree if body is provided
    if config.body then
        local root = config.body
        if type(root) == "function" then
            root = root()
        end

        -- Set root component bounds to fill the window
        root:set_bounds(0, 0, config.width or 800, config.height or 600)

        -- TODO: Set root component on app instance
    end

    return app_instance
end

-- ============================================================================
-- Module Export
-- ============================================================================

return M