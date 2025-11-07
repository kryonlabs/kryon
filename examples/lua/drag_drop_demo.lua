-- Drag and Drop Demo
-- Demonstrates draggable and drop target behaviors

local kryon = require("../../../bindings/lua/init")

-- Component state
local sourceItems = {"Item 1", "Item 2", "Item 3"}
local droppedItems = {}

-- Event handlers
local function handleDrop(data)
    print("[DROP] Dropped: " .. data)
    -- Move the item from sourceItems to droppedItems
    for i, item in ipairs(sourceItems) do
        if item == data then
            table.remove(sourceItems, i)
            table.insert(droppedItems, item)
            break
        end
    end
    -- TODO: Trigger UI update when reactive system is implemented
end

local function handleReset()
    print("[RESET] Reset drop zone")
    -- Return all items back to sourceItems
    for _, item in ipairs(droppedItems) do
        table.insert(sourceItems, item)
    end
    droppedItems = {}
    -- TODO: Trigger UI update when reactive system is implemented
end

-- Build draggable source items
local function buildSourceItems()
    local items = {}

    for i, item in ipairs(sourceItems) do
        local container = kryon.container({
            width = 150,
            height = 100,
            background = kryon.rgb(42, 42, 42),
            border = { color = kryon.rgb(74, 74, 74), width = 2 },
            padding = 10,

            children = {
                kryon.center({
                    children = {
                        kryon.column({
                            gap = 5,

                            children = {
                                kryon.text({
                                    text = item,
                                    color = kryon.rgb(255, 255, 255),
                                    font_size = 18
                                }),

                                kryon.text({
                                    text = "Drag me",
                                    color = kryon.rgb(136, 136, 136),
                                    font_size = 12
                                })
                            }
                        })
                    }
                })
            }
        })

        -- TODO: Make draggable when behavior system is implemented
        -- with Draggable:
        --     itemType = "demo-item"
        --     data = sourceItems[i]

        table.insert(items, container)
    end

    return items
end

-- Build drop zone content
local function buildDropZoneContent()
    if #droppedItems > 0 then
        local items = {}

        -- Dropped items row
        local droppedItemContainers = {}
        for _, item in ipairs(droppedItems) do
            table.insert(droppedItemContainers, kryon.container({
                width = 120,
                height = 60,
                background = kryon.rgb(42, 74, 42),
                border = { color = kryon.rgb(82, 196, 26), width = 1 },
                padding = 8,

                children = {
                    kryon.center({
                        children = {
                            kryon.text({
                                text = item,
                                color = kryon.rgb(82, 196, 26),
                                font_size = 14
                            })
                        }
                    })
                }
            }))
        end

        table.insert(items, kryon.row({
            gap = 10,
            children = droppedItemContainers
        }))

        -- Reset button
        table.insert(items, kryon.button({
            text = "Reset All",
            background = kryon.rgb(255, 77, 79),
            color = kryon.rgb(255, 255, 255),
            width = 120,
            height = 40,
            on_click = handleReset
        }))

        return kryon.column({
            gap = 15,
            children = items
        })
    else
        return kryon.center({
            children = {
                kryon.column({
                    gap = 5,

                    children = {
                        kryon.text({
                            text = "Drop containers here",
                            color = kryon.rgb(102, 102, 102),
                            font_size = 20
                        }),

                        kryon.text({
                            text = "v",
                            color = kryon.rgb(68, 68, 68),
                            font_size = 32
                        })
                    }
                })
            }
        })
    end
end

-- Define the UI
local app = kryon.app({
    width = 900,
    height = 700,
    title = "Drag & Drop Demo - Kryon Lua",

    body = kryon.container({
        background = kryon.rgb(26, 26, 26),

        children = {
            kryon.column({
                gap = 30,
                padding = 40,

                children = {
                    -- Title
                    kryon.text({
                        text = "Drag & Drop Demo",
                        font_size = 36,
                        color = kryon.rgb(255, 255, 255)
                    }),

                    -- Instructions
                    kryon.text({
                        text = "Click and drag containers from the source area to the drop zone below",
                        font_size = 16,
                        color = kryon.rgb(170, 170, 170)
                    }),

                    -- Source area with draggable items
                    kryon.column({
                        gap = 15,

                        children = {
                            kryon.text({
                                text = "Draggable Containers:",
                                font_size = 20,
                                color = kryon.rgb(74, 144, 226)
                            }),

                            kryon.row({
                                gap = 20,
                                children = buildSourceItems()
                            })
                        }
                    }),

                    -- Drop zone area
                    kryon.column({
                        gap = 15,

                        children = {
                            kryon.text({
                                text = "Drop Zone:",
                                font_size = 20,
                                color = kryon.rgb(82, 196, 26)
                            }),

                            kryon.container({
                                width = 500,
                                height = 200,
                                background = kryon.rgb(30, 30, 30),
                                border = { color = kryon.rgb(82, 196, 26), width = 3 },
                                padding = 20,

                                children = {
                                    buildDropZoneContent()
                                }

                                -- TODO: Make drop target when behavior system is implemented
                                -- with DropTarget:
                                --     itemType = "demo-item"
                                --     onDrop = handleDrop
                            })
                        }
                    }),

                    -- Legend
                    kryon.column({
                        gap = 10,

                        children = {
                            kryon.text({
                                text = "How it works:",
                                font_size = 16,
                                color = kryon.rgb(136, 136, 136)
                            }),

                            kryon.text({
                                text = "• Containers use 'with Draggable' to become draggable",
                                font_size = 14,
                                color = kryon.rgb(102, 102, 102)
                            }),

                            kryon.text({
                                text = "• Drop zone uses 'with DropTarget' to accept drops",
                                font_size = 14,
                                color = kryon.rgb(102, 102, 102)
                            }),

                            kryon.text({
                                text = "• UI elements actually move from source to drop zone",
                                font_size = 14,
                                color = kryon.rgb(102, 102, 102)
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

print("Drag & Drop Demo Lua example created successfully!")
print("Window: 900x700, Title: 'Drag & Drop Demo - Kryon Lua'")
print("Features: Draggable containers and drop zone")
print("State: Source items =", table.concat(sourceItems, ", "))
print("State: Dropped items =", table.concat(droppedItems, ", "))
print("Behaviors: Draggable, DropTarget (when implemented)")