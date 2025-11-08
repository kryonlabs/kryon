-- Basic Canvas API Test
--
-- Simple test to verify the Love2D-style canvas API works correctly

local kryon = require("../../../bindings/lua/init")
local canvas = require("../../../bindings/lua/canvas")

-- Simple drawing test
local function drawBasicTest()
    -- Clear with dark background
    canvas.background(20, 20, 30)

    -- Test basic shapes
    canvas.print("Canvas API Test", 10, 30)

    -- Rectangle
    canvas.fill(255, 100, 100, 255)  -- Red
    canvas.rectangle(canvas.FILL, 50, 50, 100, 80)

    -- Circle
    canvas.fill(100, 255, 100, 255)  -- Green
    canvas.circle(canvas.FILL, 250, 90, 40)

    -- Ellipse
    canvas.fill(100, 100, 255, 255)  -- Blue
    canvas.ellipse(canvas.FILL, 400, 90, 60, 40)

    -- Triangle (polygon)
    local triangle = {
        {x = 550, y = 130},
        {x = 500, y = 50},
        {x = 600, y = 50}
    }
    canvas.fill(255, 255, 100, 255)  -- Yellow
    canvas.polygon(canvas.FILL, triangle)

    -- Line
    canvas.stroke(255, 255, 255, 255)  -- White
    canvas.strokeWeight(2)
    canvas.line(50, 200, 600, 200)

    -- Points
    canvas.fill(255, 150, 255, 255)  -- Magenta
    for i = 0, 9 do
        canvas.point(100 + i * 50, 250)
    end

    -- Transformed rectangle
    canvas.withTransform(function()
        canvas.translate(350, 350)
        canvas.rotate(45)  -- 45 degrees
        canvas.fill(0, 255, 255, 200)  -- Cyan
        canvas.rectangle(canvas.FILL, -50, -50, 100, 100)
    end)

    canvas.print("If you see shapes above, canvas API works!", 10, 450)
end

local app = kryon.app({
    width = 700,
    height = 500,
    title = "Canvas Basic API Test",

    body = kryon.container({
        width = 700,
        height = 500,
        background = 0x000000,

        children = {
            kryon.canvas_enhanced({
                width = 700,
                height = 500,
                background = 0x141414,
                on_draw = drawBasicTest  -- No ctx parameter needed!
            })
        }
    })
})

print("Basic Canvas Test Running")