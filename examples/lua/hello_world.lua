-- Hello World Example
-- Basic Kryon Lua application that displays "Hello World" text

local kryon = require("../../../bindings/lua/init")

-- Define the UI
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Hello World Example",

    body = kryon.container({
        background = kryon.rgb(25, 25, 112),  -- Midnight blue background

        children = {
            kryon.center({
                width = 800,
                height = 600,

                children = {
                    kryon.container({
                        x = 300,
                        y = 250,
                        width = 200,
                        height = 100,
                        background = kryon.rgb(25, 25, 112),  -- Same as parent
                        children = {
                            kryon.text({
                                text = "Hello World",
                                color = kryon.rgb(255, 255, 0)  -- Yellow text
                            })
                        }
                    })
                }
            })
        }
    })
})

-- TODO: Run the application when runtime is implemented
-- app:run()

print("Hello World Lua example created successfully!")
print("Window: 800x600, Title: 'Hello World Example'")
print("Background: Midnight blue with yellow 'Hello World' text")