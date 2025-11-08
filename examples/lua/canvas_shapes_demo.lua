-- Canvas Shapes Demo - Love2D Style API
--
-- Demonstrates the new Love2D-inspired canvas API with all basic shapes
-- Shows immediate-mode drawing, transforms, colors, and advanced features

local math = require("math")
local kryon = require("../../../bindings/lua/init")
local canvas = require("../../../bindings/lua/canvas")

-- Animation state
local rotation = 0.0
local scale = 1.0
local time = 0.0
local random_points = {}
local polygon_sides = 6
local star_points = 5

-- Initialize random points for scatter plot
math.randomseed(os.time())
for i = 1, 50 do
    table.insert(random_points, {x = math.random(800), y = math.random(600)})
end

-- Drawing procedure using Love2D-style immediate mode API
local function drawCanvasDemo()
    -- Clear canvas with a nice background
    canvas.background(25, 25, 35)  -- Dark blue-gray background

    -- Update animation state
    time = time + 0.016  -- ~60fps timing
    rotation = time * 30.0  -- 30 degrees per second
    scale = 1.0 + math.sin(time) * 0.3  -- Breathing effect

    -- ============================================================================
    -- Basic Shapes Demo
    -- ============================================================================

    -- Title text
    canvas.fill(255, 255, 255)  -- White text
    canvas.print("Love2D-Style Canvas API Demo", 20, 30)
    canvas.print("Basic Shapes & Transforms", 20, 50)

    -- Rectangle section
    canvas.print("Rectangles:", 20, 90)

    -- Filled rectangle
    canvas.fill(255, 100, 100, 200)  -- Semi-transparent red
    canvas.rectangle(canvas.FILL, 50, 110, 80, 60)

    -- Outline rectangle
    canvas.stroke(100, 255, 100, 255)  -- Green outline
    canvas.strokeWeight(3)
    canvas.rectangle(canvas.LINE, 110, 110, 80, 60)

    -- Rounded rectangle effect (using multiple rectangles)
    canvas.fill(100, 100, 255, 200)  -- Semi-transparent blue
    canvas.rectangle(canvas.FILL, 200, 110, 80, 60)

    -- Circle section
    canvas.print("Circles:", 20, 190)

    -- Filled circle
    canvas.fill(255, 200, 0, 255)  -- Yellow
    canvas.circle(canvas.FILL, 60, 250, 30)

    -- Outline circle
    canvas.stroke(0, 255, 255, 255)  -- Cyan outline
    canvas.strokeWeight(2)
    canvas.circle(canvas.LINE, 130, 250, 30)

    -- Circle with thick stroke
    canvas.stroke(255, 0, 255, 255)  -- Magenta outline
    canvas.strokeWeight(4)
    canvas.circle(canvas.LINE, 200, 250, 25)

    -- Ellipse section
    canvas.print("Ellipses:", 20, 310)

    -- Filled ellipse
    canvas.fill(255, 150, 0, 200)  -- Orange
    canvas.ellipse(canvas.FILL, 60, 350, 40, 25)

    -- Outline ellipse
    canvas.stroke(150, 255, 0, 255)  -- Yellow-green outline
    canvas.strokeWeight(2)
    canvas.ellipse(canvas.LINE, 130, 350, 40, 25)

    -- Rotated ellipse
    canvas.withTransform(function()
        canvas.translate(200, 350)
        canvas.rotate(45)  -- 45 degrees
        canvas.ellipse(canvas.FILL, 0, 0, 30, 15)
    end)

    -- ============================================================================
    -- Advanced Shapes Demo
    -- ============================================================================

    -- Polygon section
    canvas.print("Polygons:", 350, 90)

    -- Hexagon (filled)
    local hexagon_vertices = {}
    for i = 0, polygon_sides - 1 do
        local angle = i * 360.0 / polygon_sides
        table.insert(hexagon_vertices, {
            x = math.cos(math.rad(angle)) * 40,
            y = math.sin(math.rad(angle)) * 40
        })
    end

    canvas.fill(255, 100, 200, 200)  -- Pink
    canvas.withTransform(function()
        canvas.translate(430, 140)
        canvas.polygon(canvas.FILL, hexagon_vertices)
    end)

    -- Pentagon (outline)
    local pentagon_vertices = {}
    for i = 0, 4 do
        local angle = i * 360.0 / 5.0 - 90  -- Start from top
        table.insert(pentagon_vertices, {
            x = math.cos(math.rad(angle)) * 35,
            y = math.sin(math.rad(angle)) * 35
        })
    end

    canvas.stroke(100, 200, 255, 255)  -- Light blue outline
    canvas.strokeWeight(2)
    canvas.withTransform(function()
        canvas.translate(520, 140)
        canvas.polygon(canvas.LINE, pentagon_vertices)
    end)

    -- Lines section
    canvas.print("Lines:", 350, 210)

    -- Simple line
    canvas.stroke(255, 255, 100, 255)  -- Light yellow
    canvas.strokeWeight(1)
    canvas.line(350, 230, 410, 250)

    -- Thick line
    canvas.strokeWeight(4)
    canvas.line(420, 230, 480, 250)

    -- Polyline (connected lines)
    canvas.strokeWeight(2)
    canvas.stroke(200, 255, 200, 255)  -- Light green
    local line_points = {
        {x = 350, y = 270},
        {x = 380, y = 290},
        {x = 420, y = 280},
        {x = 460, y = 300},
        {x = 490, y = 270}
    }
    canvas.line(line_points)

    -- Points section
    canvas.print("Points:", 350, 330)

    -- Random scatter plot
    canvas.fill(255, 255, 255, 255)  -- White points
    for i = 1, 20 do
        canvas.point(random_points[i].x, random_points[i].y)
    end

    -- Colored points
    for i = 1, 10 do
        local color = canvas.color(
            math.random(255),
            math.random(255),
            math.random(255)
        )
        canvas.withColor(color, function()
            canvas.point(random_points[i + 20].x, random_points[i + 20].y)
        end)
    end

    -- ============================================================================
    -- Transform Demo
    -- ============================================================================

    canvas.print("Transforms:", 600, 90)

    -- Rotating square
    canvas.withTransform(function()
        canvas.translate(680, 140)
        canvas.rotate(rotation)
        canvas.scale(scale)

        -- Draw rotating square
        canvas.fill(255, 100, 150, 200)  -- Semi-transparent pink
        canvas.rectangle(canvas.FILL, -30, -30, 60, 60)

        -- Draw outline
        canvas.stroke(255, 255, 255, 255)  -- White outline
        canvas.strokeWeight(2)
        canvas.rectangle(canvas.LINE, -30, -30, 60, 60)
    end)

    -- Transform stack demo
    canvas.withTransform(function()
        canvas.translate(680, 250)

        -- First transform
        canvas.push()
        canvas.rotate(rotation / 2)
        canvas.fill(100, 255, 100, 200)  -- Green
        canvas.rectangle(canvas.FILL, -20, -20, 40, 40)
        canvas.pop()

        -- Second transform (independent)
        canvas.push()
        canvas.translate(50, 0)
        canvas.rotate(-rotation / 2)
        canvas.fill(100, 100, 255, 200)  -- Blue
        canvas.circle(canvas.FILL, 0, 0, 20)
        canvas.pop()
    end)

    -- ============================================================================
    -- Advanced Features Demo
    -- ============================================================================

    canvas.print("Special:", 600, 320)

    -- Arc demonstration
    canvas.stroke(255, 200, 100, 255)  -- Orange outline
    canvas.strokeWeight(3)
    canvas.arc(canvas.LINE, 650, 350, 30, 0, math.pi * 1.5)  -- 3/4 circle

    -- Heart shape
    canvas.drawHeart(680, 450, 40)

    -- Star shape
    canvas.drawStar(750, 450, 25, star_points)

    -- ============================================================================
    -- Color and Style Demo
    -- ============================================================================

    canvas.print("Colors & Styles:", 20, 420)

    -- Color palette
    local colors = {
        canvas.COLOR_RED, canvas.COLOR_GREEN, canvas.COLOR_BLUE, canvas.COLOR_YELLOW,
        canvas.COLOR_CYAN, canvas.COLOR_MAGENTA, canvas.COLOR_ORANGE, canvas.COLOR_PURPLE
    }

    for i, color in ipairs(colors) do
        canvas.withColor(color, function()
            canvas.rectangle(canvas.FILL, 20 + (i-1) * 60, 440, 50, 30)
        end)
    end

    -- Line styles (if supported)
    canvas.print("Line Widths:", 20, 490)

    for i = 1, 5 do
        canvas.strokeWeight(i)
        canvas.stroke(255, 255, 255, 255)
        canvas.line(20 + i * 60, 510, 20 + i * 60, 530)
    end

    -- ============================================================================
    -- Performance Demo
    -- ============================================================================

    canvas.print("Performance:", 350, 420)

    -- Lots of small shapes to test performance
    canvas.fill(100, 255, 200, 150)  -- Semi-transparent green
    for i = 0, 19 do
        for j = 0, 9 do
            local x = 350 + i * 15
            local y = 440 + j * 10
            local size = 5 + math.sin(time + i * 0.5) * 3
            canvas.circle(canvas.FILL, x, y, size)
        end
    end
end

-- Main application setup
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Kryon Canvas - Love2D Style API Demo",

    body = kryon.container({
        width = 800,
        height = 600,
        background = 0x191919,  -- Dark background

        children = {
            kryon.canvas_enhanced({
                width = 800,
                height = 600,
                background = 0x0A0F1E,
                on_draw = drawCanvasDemo
            })
        }
    })
})

print("Love2D-Style Canvas Demo Started!")
print("Features demonstrated:")
print("- Basic shapes: rectangles, circles, ellipses")
print("- Advanced shapes: polygons, lines, points")
print("- Transform system: translate, rotate, scale")
print("- Color management and styles")
print("- Special shapes: hearts, stars")
print("- Performance with many shapes")
print("")
print("Animation running - shapes should rotate and scale smoothly!")