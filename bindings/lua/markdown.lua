-- Kryon Markdown Component - Lua Bindings
--
-- Provides Lua interface to the Kryon Markdown rendering system
-- with CommonMark compliance and theme support
--
-- Usage:
--   local markdown = require("markdown")
--   local md_component = markdown.create({
--     source = "# Hello **World**!",
--     theme = markdown.themeLight()
--   })

local M = {}

-- Import low-level bindings
local kryon = require("kryon")

-- ============================================================================
-- Theme Definitions
-- ============================================================================

-- Light theme (default)
function M.themeLight()
    return {
        name = "light",

        -- Document colors
        background_color = 0xFFFFFFFF,  -- White
        text_color = 0x2C3E50FF,        -- Dark blue-gray

        -- Heading colors
        h1_color = 0x2C3E50FF,          -- Dark blue-gray
        h2_color = 0x34495EFF,          -- Slightly lighter
        h3_color = 0x7F8C8DFF,          -- Medium gray

        -- Text formatting
        code_color = 0xE74C3CFF,         -- Red
        code_background = 0xF8F9FAFF,   -- Very light gray

        -- Block elements
        blockquote_color = 0x7F8C8DFF,   -- Medium gray
        blockquote_background = 0xF8F9FAFF,
        blockquote_border = 0xBDC3C7FF,  -- Light gray

        -- Links
        link_color = 0x3498DBFF,         -- Blue
        link_hover_color = 0x2980B9FF,   -- Darker blue

        -- Lists
        list_item_color = 0x2C3E50FF,    -- Same as text
        list_indent = 24,                -- Pixels

        -- Code blocks
        code_block_background = 0xF8F9FAFF,
        code_block_color = 0x2C3E50FF,

        -- Horizontal rules
        hr_color = 0xBDC3C7FF,           -- Light gray

        -- Tables
        table_border_color = 0xBDC3C7FF,
        table_header_background = 0xF8F9FAFF,
        table_alt_row_background = 0xF8F9FAFF,

        -- Fonts
        base_font_size = 16,
        h1_font_size = 28,
        h2_font_size = 24,
        h3_font_size = 20,
        code_font_size = 14
    }
end

-- Dark theme
function M.themeDark()
    return {
        name = "dark",

        -- Document colors
        background_color = 0x1E1E1EFF,  -- Dark gray
        text_color = 0xD4D4D4FF,        -- Light gray

        -- Heading colors
        h1_color = 0xFFFFFFFF,          -- White
        h2_color = 0xE0E0E0FF,          -- Light gray
        h3_color = 0xB0B0B0FF,          -- Medium gray

        -- Text formatting
        code_color = 0x4EC9B0FF,         -- Green
        code_background = 0x2D2D30FF,   -- Darker gray

        -- Block elements
        blockquote_color = 0xB0B0B0FF,   -- Medium gray
        blockquote_background = 0x2D2D30FF,
        blockquote_border = 0x555555FF,  -- Gray

        -- Links
        link_color = 0x569CD6FF,         -- Light blue
        link_hover_color = 0x9CDCFEFF,   -- Lighter blue

        -- Lists
        list_item_color = 0xD4D4D4FF,    -- Same as text
        list_indent = 24,                -- Pixels

        -- Code blocks
        code_block_background = 0x1E1E1EFF,
        code_block_color = 0xD4D4D4FF,

        -- Horizontal rules
        hr_color = 0x555555FF,           -- Gray

        -- Tables
        table_border_color = 0x555555FF,
        table_header_background = 0x2D2D30FF,
        table_alt_row_background = 0x252526FF,

        -- Fonts
        base_font_size = 16,
        h1_font_size = 28,
        h2_font_size = 24,
        h3_font_size = 20,
        code_font_size = 14
    }
end

-- GitHub theme
function M.themeGitHub()
    return {
        name = "github",

        -- Document colors
        background_color = 0xFFFFFFFF,  -- White
        text_color = 0x24292EFF,        -- GitHub dark text

        -- Heading colors
        h1_color = 0x24292EFF,          -- Dark text
        h2_color = 0x24292EFF,          -- Dark text
        h3_color = 0x586069FF,          -- Gray

        -- Text formatting
        code_color = 0xD73A49FF,         -- GitHub red
        code_background = 0xF6F8FAFF,   -- GitHub light gray

        -- Block elements
        blockquote_color = 0x6A737DFF,   -- Gray
        blockquote_background = 0xF6F8FAFF,
        blockquote_border = 0xD0D7DEFF,  -- Light gray

        -- Links
        link_color = 0x0366D6FF,         -- GitHub blue
        link_hover_color = 0x0256CCFF,   -- Darker blue

        -- Lists
        list_item_color = 0x24292EFF,    -- Same as text
        list_indent = 24,                -- Pixels

        -- Code blocks
        code_block_background = 0xF6F8FAFF,
        code_block_color = 0x24292EFF,

        -- Horizontal rules
        hr_color = 0xE1E4E8FF,           -- Very light gray

        -- Tables
        table_border_color = 0xD0D7DEFF,
        table_header_background = 0xF6F8FAFF,
        table_alt_row_background = 0xF6F8FAFF,

        -- Fonts
        base_font_size = 16,
        h1_font_size = 32,
        h2_font_size = 24,
        h3_font_size = 20,
        code_font_size = 14
    }
end

-- ============================================================================
-- Markdown Component Creation
-- ============================================================================

function M.create(config)
    config = config or {}

    local theme = config.theme or M.themeLight()
    local source = config.source or ""
    local width = config.width or 400
    local height = config.height or 300
    local on_link_click = config.on_link_click

    -- Parse markdown content (simplified version)
    local parsed_content = M.parseMarkdown(source, theme)

    -- Create container component to hold markdown content
    local container = kryon.container()

    -- Set container properties
    container:set_bounds(
        config.x or 0,
        config.y or 0,
        width,
        height
    )

    container:set_background_color(theme.background_color)

    -- Add parsed content as text components
    for _, element in ipairs(parsed_content) do
        local text_component = kryon.text(element.text)
        text_component:set_bounds(
            element.x or 0,
            element.y or 0,
            element.width or width,
            element.height or 20
        )
        text_component:set_background_color(element.color or theme.text_color)

        container:add_child(text_component)
    end

    -- Store markdown metadata for later use
    container._markdown_data = {
        source = source,
        theme = theme,
        parsed_content = parsed_content,
        on_link_click = on_link_click
    }

    return container
end

-- ============================================================================
-- Markdown Parser (Simplified Implementation)
-- ============================================================================

function M.parseMarkdown(source, theme)
    local lines = {}
    for line in (source .. "\n"):gmatch("(.-)\n") do
        table.insert(lines, line)
    end

    local elements = {}
    local y = 20
    local base_color = theme.text_color

    for _, line in ipairs(lines) do
        local trimmed = line:match("^%s*(.-)%s*$") or ""

        if trimmed == "" then
            -- Empty line - add spacing
            y = y + 10
        elseif trimmed:match("^# ") then
            -- H1 Heading
            local text = trimmed:sub(3)
            table.insert(elements, {
                type = "h1",
                text = text,
                x = 20,
                y = y,
                width = 0,
                height = theme.h1_font_size + 4,
                color = theme.h1_color
            })
            y = y + theme.h1_font_size + 10

        elseif trimmed:match("^## ") then
            -- H2 Heading
            local text = trimmed:sub(4)
            table.insert(elements, {
                type = "h2",
                text = text,
                x = 20,
                y = y,
                width = 0,
                height = theme.h2_font_size + 4,
                color = theme.h2_color
            })
            y = y + theme.h2_font_size + 8

        elseif trimmed:match("^### ") then
            -- H3 Heading
            local text = trimmed:sub(5)
            table.insert(elements, {
                type = "h3",
                text = text,
                x = 20,
                y = y,
                width = 0,
                height = theme.h3_font_size + 4,
                color = theme.h3_color
            })
            y = y + theme.h3_font_size + 6

        elseif trimmed:match("^> ") then
            -- Blockquote
            local text = trimmed:sub(3)
            table.insert(elements, {
                type = "blockquote",
                text = "» " .. text,
                x = 40,
                y = y,
                width = 0,
                height = theme.base_font_size + 4,
                color = theme.blockquote_color
            })
            y = y + theme.base_font_size + 6

        elseif trimmed:match("^%- ") or trimmed:match("^\\* ") then
            -- List item
            local text = trimmed:sub(3)
            table.insert(elements, {
                type = "list_item",
                text = "• " .. text,
                x = 40,
                y = y,
                width = 0,
                height = theme.base_font_size + 4,
                color = theme.list_item_color
            })
            y = y + theme.base_font_size + 4

        elseif trimmed:match("^%d+%. ") then
            -- Ordered list item
            local text = trimmed:match("^%d+%. (.*)")
            local number = trimmed:match("^(%d+)%.")
            table.insert(elements, {
                type = "ordered_list_item",
                text = number .. ". " .. text,
                x = 40,
                y = y,
                width = 0,
                height = theme.base_font_size + 4,
                color = theme.list_item_color
            })
            y = y + theme.base_font_size + 4

        elseif trimmed:match("^```") then
            -- Code block (simplified)
            table.insert(elements, {
                type = "code_block",
                text = "Code block",
                x = 20,
                y = y,
                width = 0,
                height = theme.code_font_size + 4,
                color = theme.code_block_color
            })
            y = y + theme.code_font_size * 5 + 10  -- Multiple lines

        else
            -- Regular paragraph with basic formatting
            local formatted_text = M.formatInlineMarkdown(trimmed, theme)
            table.insert(elements, {
                type = "paragraph",
                text = formatted_text,
                x = 20,
                y = y,
                width = 0,
                height = theme.base_font_size + 4,
                color = base_color
            })
            y = y + theme.base_font_size + 8
        end
    end

    return elements
end

-- ============================================================================
-- Inline Markdown Formatting
-- ============================================================================

function M.formatInlineMarkdown(text, theme)
    local formatted = text

    -- Bold text (**text**)
    formatted = formatted:gsub("%*%*(.-)%*%*", function(match)
        -- Note: In a real implementation, this would use rich text formatting
        return "**" .. match .. "**"  -- Preserve markers for display
    end)

    -- Italic text (*text*)
    formatted = formatted:gsub("%*(.-)%*", function(match)
        -- Avoid double processing of bold text
        if not match:match("%*") then
            return "*" .. match .. "*"  -- Preserve markers
        end
        return match
    end)

    -- Inline code (`code`)
    formatted = formatted:gsub("`(.-)`", function(match)
        return "`" .. match .. "`"  -- Preserve markers
    end)

    -- Links [text](url)
    formatted = formatted:gsub("%[(.-)%]%((.-)%)", function(text, url)
        return text .. " → "  -- Simplified link display
    end)

    return formatted
end

-- ============================================================================
-- Utility Functions
-- ============================================================================

function M.updateMarkdown(component, new_source, new_theme)
    if not component._markdown_data then
        error("Component is not a markdown component")
    end

    -- Update stored data
    component._markdown_data.source = new_source
    component._markdown_data.theme = new_theme or component._markdown_data.theme

    -- Re-parse content
    local parsed_content = M.parseMarkdown(new_source, component._markdown_data.theme)
    component._markdown_data.parsed_content = parsed_content

    -- Clear existing children and re-add them
    -- TODO: Implement child removal in C bindings
    for _, element in ipairs(parsed_content) do
        local text_component = kryon.text(element.text)
        text_component:set_bounds(
            element.x or 0,
            element.y or 0,
            element.width or 400,
            element.height or 20
        )
        text_component:set_background_color(element.color or component._markdown_data.theme.text_color)

        component:add_child(text_component)
    end

    return component
end

function M.getMarkdownData(component)
    return component._markdown_data
end

function M.isMarkdownComponent(component)
    return component._markdown_data ~= nil
end

-- ============================================================================
-- Export Module
-- ============================================================================

return M