-- Kryon Canvas API - Love2D Style for Lua
--
-- Provides Love2D-inspired immediate mode drawing functions
-- that map to the enhanced C canvas implementation
--
-- Usage:
--   local canvas = require("canvas")
--   canvas.background(20, 20, 30)
--   canvas.fill(255, 100, 100)
--   canvas.rectangle("fill", 50, 50, 100, 80)

local M = {}

-- Import low-level canvas bindings
local kryon = require("kryon")

-- ============================================================================
-- Canvas State Management
-- ============================================================================

-- Current drawing state
local state = {
    fill_color = {255, 255, 255, 255},
    stroke_color = {255, 255, 255, 255},
    stroke_weight = 1,
    line_style = "solid",
    blend_mode = "alpha",
    font_size = 16,
    current_transform = {
        x = 0, y = 0,
        rotation = 0,
        scale_x = 1, scale_y = 1
    }
}

-- Transform stack for push/pop operations
local transform_stack = {}

-- ============================================================================
-- Color Management Functions
-- ============================================================================

function M.color(r, g, b, a)
    a = a or 255
    return kryon.color_rgba(r, g, b, a)
end

function M.fill(r, g, b, a)
    if type(r) == "table" then
        -- Support M.fill({r, g, b, a}) format
        state.fill_color = {r[1] or 255, r[2] or 255, r[3] or 255, r[4] or 255}
    else
        state.fill_color = {r or 255, g or 255, b or 255, a or 255}
    end
end

function M.stroke(r, g, b, a)
    if type(r) == "table" then
        -- Support M.stroke({r, g, b, a}) format
        state.stroke_color = {r[1] or 255, r[2] or 255, r[3] or 255, r[4] or 255}
    else
        state.stroke_color = {r or 255, g or 255, b or 255, a or 255}
    end
end

function M.strokeWeight(width)
    state.stroke_weight = width or 1
end

function M.background(r, g, b, a)
    -- Clear the entire canvas with background color
    local bg_color
    if type(r) == "table" then
        bg_color = M.color(r[1], r[2], r[3], r[4])
    else
        bg_color = M.color(r or 0, g or 0, b or 0, a or 255)
    end

    -- This would call the C canvas clear function
    -- TODO: Implement canvas clear in C bindings
    print("Setting background to:", string.format("0x%08X", bg_color))
end

function M.clear()
    -- Clear canvas to transparent
    M.background(0, 0, 0, 0)
end

-- ============================================================================
-- Color Constants (matching Nim version)
-- ============================================================================

M.COLOR_RED = M.color(255, 0, 0, 255)
M.COLOR_GREEN = M.color(0, 255, 0, 255)
M.COLOR_BLUE = M.color(0, 0, 255, 255)
M.COLOR_YELLOW = M.color(255, 255, 0, 255)
M.COLOR_CYAN = M.color(0, 255, 255, 255)
M.COLOR_MAGENTA = M.color(255, 0, 255, 255)
M.COLOR_ORANGE = M.color(255, 165, 0, 255)
M.COLOR_PURPLE = M.color(128, 0, 128, 255)
M.COLOR_WHITE = M.color(255, 255, 255, 255)
M.COLOR_BLACK = M.color(0, 0, 0, 255)
M.COLOR_TRANSPARENT = M.color(0, 0, 0, 0)

-- ============================================================================
-- Drawing Mode Constants
-- ============================================================================

M.FILL = "fill"
M.LINE = "line"
M.FILL_AND_LINE = "fill_and_line"

-- ============================================================================
-- Shape Drawing Functions
-- ============================================================================

function M.rectangle(mode, x, y, width, height)
    local color = mode == "fill" or mode == "fill_and_line" and state.fill_color or state.stroke_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas rectangle function
    print(string.format("Drawing rectangle: mode=%s, pos=(%d,%d), size=(%d,%d), color=0x%08X",
          mode, x, y, width, height, final_color))
end

function M.circle(mode, x, y, radius)
    local color = mode == "fill" or mode == "fill_and_line" and state.fill_color or state.stroke_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas circle function
    print(string.format("Drawing circle: mode=%s, center=(%d,%d), radius=%d, color=0x%08X",
          mode, x, y, radius, final_color))
end

function M.ellipse(mode, x, y, radius_x, radius_y)
    local color = mode == "fill" or mode == "fill_and_line" and state.fill_color or state.stroke_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas ellipse function
    print(string.format("Drawing ellipse: mode=%s, center=(%d,%d), radii=(%d,%d), color=0x%08X",
          mode, x, y, radius_x, radius_y, final_color))
end

function M.line(x1, y1, x2, y2)
    local color = state.stroke_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas line function
    print(string.format("Drawing line: from=(%d,%d) to=(%d,%d), width=%d, color=0x%08X",
          x1, y1, x2, y2, state.stroke_weight, final_color))
end

function M.line(points)
    if type(points) == "table" and #points >= 2 then
        for i = 1, #points - 1 do
            local p1 = points[i]
            local p2 = points[i + 1]
            M.line(p1.x or p1[1], p1.y or p1[2], p2.x or p2[1], p2.y or p2[2])
        end
    end
end

function M.point(x, y)
    local color = state.fill_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas point function
    print(string.format("Drawing point: (%d,%d), color=0x%08X", x, y, final_color))
end

function M.polygon(mode, points)
    local color = mode == "fill" or mode == "fill_and_line" and state.fill_color or state.stroke_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- Convert points to expected format
    local point_list = {}
    for _, point in ipairs(points) do
        table.insert(point_list, {x = point.x or point[1], y = point.y or point[2]})
    end

    -- TODO: Call C canvas polygon function
    print(string.format("Drawing polygon: mode=%s, points=%d, color=0x%08X",
          mode, #point_list, final_color))
end

function M.arc(mode, x, y, radius, start_angle, end_angle)
    local color = mode == "fill" or mode == "fill_and_line" and state.fill_color or state.stroke_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas arc function
    print(string.format("Drawing arc: mode=%s, center=(%d,%d), radius=%d, angles=(%.2f,%.2f), color=0x%08X",
          mode, x, y, radius, start_angle, end_angle, final_color))
end

-- ============================================================================
-- Transform Functions
-- ============================================================================

function M.push()
    -- Save current transform state
    table.insert(transform_stack, {
        x = state.current_transform.x,
        y = state.current_transform.y,
        rotation = state.current_transform.rotation,
        scale_x = state.current_transform.scale_x,
        scale_y = state.current_transform.scale_y
    })
end

function M.pop()
    -- Restore previous transform state
    if #transform_stack > 0 then
        local saved_transform = table.remove(transform_stack)
        state.current_transform = saved_transform

        -- TODO: Apply transform to canvas in C bindings
        print("Restored transform")
    end
end

function M.translate(x, y)
    state.current_transform.x = state.current_transform.x + x
    state.current_transform.y = state.current_transform.y + y

    -- TODO: Apply translation to canvas in C bindings
    print(string.format("Translate: (%d, %d)", x, y))
end

function M.rotate(angle)
    state.current_transform.rotation = state.current_transform.rotation + angle

    -- TODO: Apply rotation to canvas in C bindings
    print(string.format("Rotate: %.2f degrees", angle))
end

function M.scale(x, y)
    y = y or x  -- Default to uniform scaling
    state.current_transform.scale_x = state.current_transform.scale_x * x
    state.current_transform.scale_y = state.current_transform.scale_y * y

    -- TODO: Apply scale to canvas in C bindings
    print(string.format("Scale: (%.2f, %.2f)", x, y))
end

-- ============================================================================
-- Text Drawing Functions
-- ============================================================================

function M.print(text, x, y)
    local color = state.fill_color
    local final_color = M.color(color[1], color[2], color[3], color[4])

    -- TODO: Call C canvas text function
    print(string.format("Drawing text: '%s' at (%d,%d), size=%d, color=0x%08X",
          text, x, y, state.font_size, final_color))
end

function M.printf(text, x, y, limit, align)
    align = align or "left"

    -- TODO: Implement formatted text with alignment in C bindings
    print(string.format("Drawing formatted text: '%s' at (%d,%d), limit=%d, align=%s",
          text, x, y, limit, align))
end

function M.setFont(font)
    -- TODO: Set font in C bindings
    print("Setting font:", font)
end

function M.setFontSize(size)
    state.font_size = size
    print("Setting font size:", size)
end

-- ============================================================================
-- Convenience Functions
-- ============================================================================

function M.withColor(color, func)
    local old_fill = {table.unpack(state.fill_color)}
    local old_stroke = {table.unpack(state.stroke_color)}

    M.fill(color)
    M.stroke(color)

    local result = func()

    state.fill_color = old_fill
    state.stroke_color = old_stroke

    return result
end

function M.withTransform(func)
    M.push()
    local result = func()
    M.pop()
    return result
end

-- ============================================================================
-- Special Shape Functions
-- ============================================================================

function M.drawHeart(x, y, size)
    M.withTransform(function()
        M.translate(x, y)
        M.scale(size / 40, size / 40)

        -- Draw heart shape using two circles and a polygon
        M.fill(255, 100, 150)  -- Pink
        M.circle("fill", -15, -15, 15)  -- Left circle
        M.circle("fill", 15, -15, 15)   -- Right circle

        -- Bottom triangle
        local heart_points = {
            {x = -30, y = 0},
            {x = 30, y = 0},
            {x = 0, y = 35}
        }
        M.polygon("fill", heart_points)
    end)
end

function M.drawStar(x, y, size, points)
    points = points or 5

    M.withTransform(function()
        M.translate(x, y)
        M.scale(size / 30, size / 30)

        local star_points = {}
        for i = 0, points * 2 - 1 do
            local angle = (i * math.pi) / points
            local radius
            if i % 2 == 0 then
                radius = 30  -- Outer radius
            else
                radius = 12  -- Inner radius
            end

            table.insert(star_points, {
                x = math.cos(angle - math.pi / 2) * radius,
                y = math.sin(angle - math.pi / 2) * radius
            })
        end

        M.fill(255, 255, 100)  -- Yellow
        M.polygon("fill", star_points)
    end)
end

-- ============================================================================
-- Canvas State Reset
-- ============================================================================

function M.reset()
    state.fill_color = {255, 255, 255, 255}
    state.stroke_color = {255, 255, 255, 255}
    state.stroke_weight = 1
    state.line_style = "solid"
    state.blend_mode = "alpha"
    state.font_size = 16
    state.current_transform = {
        x = 0, y = 0,
        rotation = 0,
        scale_x = 1, scale_y = 1
    }
    transform_stack = {}
end

return M