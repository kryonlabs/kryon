-- Todo App Example
-- Demonstrates state management and user input in Lua

local kryon = require("../../../bindings/lua/init")

-- Application state
local todos = {"Test todo", "Another item"}
local newTodo = ""

-- Event handlers
local function addTodoHandler()
    if newTodo and newTodo ~= "" then
        table.insert(todos, newTodo)
        newTodo = ""
        print("Added todo: " .. todos[#todos])
        -- TODO: Trigger UI update when reactive system is implemented
    end
end

local function inputChangeHandler(value)
    newTodo = value
    print("Input changed: " .. value)
end

local function inputSubmitHandler(value)
    if value and value ~= "" then
        addTodoHandler()
    end
end

-- Build todo list items
local function buildTodoItems()
    local items = {}

    -- Add title
    table.insert(items, kryon.text({
        text = "Todo List:",
        color = kryon.rgb(51, 51, 51),
        font_size = 18
    }))

    -- Add each todo item
    for i, todo in ipairs(todos) do
        table.insert(items, kryon.text({
            text = "- " .. todo .. " (len=" .. #todos .. ")",
            color = kryon.rgb(51, 51, 51),
            font_size = 16
        }))
    end

    return items
end

-- Define the UI
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Simple Todo App",

    body = kryon.container({
        background = kryon.rgb(240, 240, 240),  -- Light gray background

        children = {
            kryon.column({
                x = 0,
                y = 0,
                width = 800,
                height = 600,
                gap = 20,
                padding = 30,

                children = {
                    -- Title
                    kryon.text({
                        text = "Simple Todo App",
                        font_size = 28,
                        color = kryon.rgb(51, 51, 51)
                    }),

                    -- Input row
                    kryon.row({
                        gap = 10,

                        children = {
                            kryon.input({
                                placeholder = "Enter a new todo...",
                                width = 400,
                                height = 40,
                                background = kryon.rgb(255, 255, 255),
                                border = { color = kryon.rgb(204, 204, 204), width = 1 },
                                padding = 10,
                                on_change = inputChangeHandler,
                                on_submit = inputSubmitHandler
                            }),

                            kryon.button({
                                text = "Add",
                                width = 80,
                                height = 40,
                                background = kryon.rgb(0, 123, 255),
                                color = kryon.rgb(255, 255, 255),
                                on_click = addTodoHandler
                            })
                        }
                    }),

                    -- Todo list
                    kryon.column({
                        gap = 5,

                        children = buildTodoItems()
                    })
                }
            })
        }
    })
})

-- TODO: Run the application when runtime is implemented
-- app:run()

print("Todo App Lua example created successfully!")
print("Window: 800x600, Title: 'Simple Todo App'")
print("Features: Input field with Add button, dynamic todo list")
print("State: Current todos =", table.concat(todos, ", "))
print("Event handlers: addTodo, inputChange, inputSubmit")