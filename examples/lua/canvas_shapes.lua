-- Canvas Shapes Example
-- Demonstrates the Canvas API with various drawing operations

local kryon = require("../../../bindings/lua/init")
local canvas = require("../../../bindings/lua/canvas")

-- Drawing procedure that creates various shapes using high-level drawing functions
local function drawShapes(ctx, width, height)
    -- Clear canvas with light blue background
    ctx:clear_background(kryon.rgb(135, 206, 235))  -- Sky blue

    -- Row 1: Basic filled shapes
    ctx:fill_style(kryon.rgb(255, 0, 0))  -- Red
    ctx:draw_rectangle(50, 50, 80, 60)

    ctx:fill_style(kryon.rgb(0, 0, 255))  -- Blue
    ctx:draw_circle(200, 80, 35)

    ctx:fill_style(kryon.rgb(0, 255, 0))  -- Green
    ctx:draw_triangle(300, 120, 260, 40, 340, 40)

    -- Row 2: Arc demos (half circle and quarter circle)
    ctx:fill_style(kryon.rgb(255, 255, 0))  -- Yellow
    ctx:begin_path()
    ctx:arc(450, 80, 35, 0, math.pi)  -- Half circle
    ctx:fill()

    ctx:stroke_style(kryon.rgb(0, 255, 255))  -- Cyan
    ctx:line_width(3.0)
    ctx:begin_path()
    ctx:arc(80, 200, 35, 0, math.pi / 2)  -- Quarter circle
    ctx:stroke()

    -- Row 3: Sine wave
    ctx:stroke_style(kryon.rgb(148, 0, 211))  -- Purple
    ctx:line_width(2.0)
    ctx:draw_sine_wave(50, 320, 500, 20, 0.08)
end

-- Define the UI with a canvas
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Canvas Shapes Example",

    body = kryon.container({
        background = kryon.rgb(44, 62, 80),  -- Dark blue-gray

        children = {
            -- Title
            kryon.text({
                text = "Canvas Drawing Examples",
                font_size = 32,
                color = kryon.rgb(236, 240, 241),  -- Light gray
                x = 200,
                y = 20
            }),

            -- Canvas with shapes
            kryon.center({
                children = {
                    kryon.canvas({
                        width = 550,
                        height = 400,
                        background = kryon.rgb(52, 73, 94),  -- Darker blue-gray
                        on_draw = drawShapes
                    })
                }
            })
        }
    })
})

-- TODO: Run the application when runtime is implemented
-- app:run()

print("Canvas Shapes Lua example created successfully!")
print("Window: 800x600, Title: 'Canvas Shapes Example'")
print("Features: Canvas with various drawing operations")
print("Shapes: Rectangle, circle, triangle, arcs, sine wave")
print("Colors: Red, blue, green, yellow, cyan, purple")