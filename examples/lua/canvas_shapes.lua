-- Canvas Shapes Example
-- Demonstrates the Love2D-style Canvas API with various drawing operations

local kryon = require("../../../bindings/lua/init")
local canvas = require("../../../bindings/lua/canvas")

-- Drawing procedure that creates various shapes using Love2D-style API
local function drawShapes()
    -- Clear canvas with light blue background
    canvas.background(135, 206, 235)  -- Sky blue

    -- Row 1: Basic filled shapes
    canvas.fill(255, 0, 0, 255)  -- Red
    canvas.rectangle(canvas.FILL, 50, 50, 80, 60)

    canvas.fill(0, 0, 255, 255)  -- Blue
    canvas.circle(canvas.FILL, 200, 80, 35)

    canvas.fill(0, 255, 0, 255)  -- Green
    -- Triangle as polygon
    local triangle_vertices = {
        {x = 300, y = 120},
        {x = 260, y = 40},
        {x = 340, y = 40}
    }
    canvas.polygon(canvas.FILL, triangle_vertices)

    -- Row 2: Arc demos (half circle and quarter circle)
    canvas.fill(255, 255, 0, 255)  -- Yellow
    canvas.arc(canvas.FILL, 450, 80, 35, 0, math.pi)  -- Half circle

    canvas.stroke(0, 255, 255, 255)  -- Cyan
    canvas.strokeWeight(3)
    canvas.arc(canvas.LINE, 80, 200, 35, 0, math.pi / 2)  -- Quarter circle

    -- Row 3: Sine wave
    canvas.stroke(148, 0, 211, 255)  -- Purple
    canvas.strokeWeight(2)
    local wave_points = {}
    for i = 0, 100 do
        local x = 50 + i * 5
        local y = 320 + math.sin(0.08 * i) * 20
        table.insert(wave_points, {x = x, y = y})
    end
    canvas.line(wave_points)
end

-- Define the UI with a canvas
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Canvas Shapes Example",

    body = kryon.container({
        width = 800,
        height = 600,
        background = 0x2C3E50,  -- Dark blue-gray

        children = {
            -- Title
            kryon.text({
                text = "Canvas Drawing Examples",
                x = 20,
                y = 20,
                color = 0xECF0F1  -- Light gray
            }),

            -- Canvas with shapes
            kryon.container({
                width = 550,
                height = 400,
                background = 0x34495E,  -- Darker blue-gray
                x = 100,
                y = 100,

                children = {
                    kryon.canvas_enhanced({
                        width = 550,
                        height = 400,
                        background = 0x3498DB,
                        on_draw = drawShapes  -- No ctx parameter needed!
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