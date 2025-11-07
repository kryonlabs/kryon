-- Counters Demo
-- Demonstrates component state management with multiple counters

local kryon = require("../../../bindings/lua/init")

-- Component state
local counters = {
    { value = 0, label = "Counter A" },
    { value = 0, label = "Counter B" },
    { value = 0, label = "Counter C" }
}

local total = 0

-- Event handlers
local function incrementCounter(index)
    counters[index].value = counters[index].value + 1
    updateTotal()
    print(counters[index].label .. " incremented to: " .. counters[index].value)
    -- TODO: Trigger UI update when reactive system is implemented
end

local function decrementCounter(index)
    counters[index].value = counters[index].value - 1
    updateTotal()
    print(counters[index].label .. " decremented to: " .. counters[index].value)
    -- TODO: Trigger UI update when reactive system is implemented
end

local function resetCounter(index)
    counters[index].value = 0
    updateTotal()
    print(counters[index].label .. " reset to: 0")
    -- TODO: Trigger UI update when reactive system is implemented
end

local function resetAllCounters()
    for i, counter in ipairs(counters) do
        counter.value = 0
    end
    updateTotal()
    print("All counters reset")
    -- TODO: Trigger UI update when reactive system is implemented
end

local function updateTotal()
    total = 0
    for _, counter in ipairs(counters) do
        total = total + counter.value
    end
end

-- Build counter component
local function buildCounter(index)
    local counter = counters[index]

    return kryon.container({
        background = kryon.rgb(45, 45, 45),
        border = { color = kryon.rgb(74, 144, 226), width = 2 },
        padding = 20,
        width = 200,
        height = 150,

        children = {
            kryon.column({
                gap = 10,

                children = {
                    -- Counter label
                    kryon.text({
                        text = counter.label,
                        color = kryon.rgb(74, 144, 226),
                        font_size = 18
                    }),

                    -- Counter value
                    kryon.text({
                        text = tostring(counter.value),
                        color = kryon.rgb(255, 255, 255),
                        font_size = 24
                    }),

                    -- Control buttons
                    kryon.row({
                        gap = 5,

                        children = {
                            kryon.button({
                                text = "-",
                                width = 40,
                                height = 30,
                                background = kryon.rgb(220, 53, 69),
                                color = kryon.rgb(255, 255, 255),
                                on_click = function() decrementCounter(index) end
                            }),

                            kryon.button({
                                text = "+",
                                width = 40,
                                height = 30,
                                background = kryon.rgb(40, 167, 69),
                                color = kryon.rgb(255, 255, 255),
                                on_click = function() incrementCounter(index) end
                            }),

                            kryon.button({
                                text = "Reset",
                                width = 50,
                                height = 30,
                                background = kryon.rgb(108, 117, 125),
                                color = kryon.rgb(255, 255, 255),
                                on_click = function() resetCounter(index) end
                            })
                        }
                    })
                }
            })
        }
    })
end

-- Define the UI
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Counters Demo - Component State",

    body = kryon.container({
        background = kryon.rgb(25, 25, 25),

        children = {
            kryon.column({
                gap = 30,
                padding = 40,

                children = {
                    -- Title
                    kryon.text({
                        text = "Component State Management",
                        font_size = 32,
                        color = kryon.rgb(255, 255, 255)
                    }),

                    -- Description
                    kryon.text({
                        text = "Each counter maintains its own independent state with increment/decrement controls",
                        font_size = 16,
                        color = kryon.rgb(170, 170, 170)
                    }),

                    -- Counters row
                    kryon.row({
                        gap = 30,

                        children = {
                            buildCounter(1),
                            buildCounter(2),
                            buildCounter(3)
                        }
                    }),

                    -- Total display and reset all
                    kryon.center({
                        children = {
                            kryon.column({
                                gap = 15,

                                children = {
                                    kryon.text({
                                        text = "Total Count: " .. tostring(total),
                                        font_size = 24,
                                        color = kryon.rgb(255, 193, 7)
                                    }),

                                    kryon.button({
                                        text = "Reset All Counters",
                                        width = 180,
                                        height = 40,
                                        background = kryon.rgb(220, 53, 69),
                                        color = kryon.rgb(255, 255, 255),
                                        on_click = resetAllCounters
                                    })
                                }
                            })
                        }
                    }),

                    -- State info
                    kryon.text({
                        text = "Current State: " ..
                               counters[1].label .. "=" .. counters[1].value .. ", " ..
                               counters[2].label .. "=" .. counters[2].value .. ", " ..
                               counters[3].label .. "=" .. counters[3].value .. " (Total: " .. total .. ")",
                        font_size = 14,
                        color = kryon.rgb(136, 136, 136)
                    })
                }
            })
        }
    })
})

-- TODO: Run the application when runtime is implemented
-- app:run()

print("Counters Demo Lua example created successfully!")
print("Window: 800x600, Title: 'Counters Demo - Component State'")
print("Features: Three independent counters with individual controls")
print("State: Each counter maintains separate value")
print("Total: " .. total)
print("Controls: Increment, decrement, reset individual, reset all")