-- Interactive Button Demo
-- Shows button creation and event handling in Lua

local kryon = require("../../../bindings/lua/init")

-- Button click handler
local function handleButtonClick()
    print("🎯 Button clicked! Hello from Kryon-Lua!")
end

-- Define the UI with a centered button
local app = kryon.app({
    width = 600,
    height = 400,
    title = "Interactive Button Demo",

    body = kryon.container({
        background = kryon.rgb(25, 25, 25),  -- Dark background

        children = {
            kryon.center({
                width = 600,
                height = 400,

                children = {
                    kryon.button({
                        text = "Click Me!",
                        width = 150,
                        height = 50,
                        background = kryon.rgb(64, 64, 128),  -- Blue-gray button
                        color = kryon.rgb(255, 255, 255),     -- White text
                        on_click = handleButtonClick
                    })
                }
            })
        }
    })
})

-- TODO: Run the application when runtime is implemented
-- app:run()

print("Button Demo Lua example created successfully!")
print("Window: 600x400, Title: 'Interactive Button Demo'")
print("Features: Dark background with centered blue button")
print("Event: Click handler prints message when button is clicked")