-- Counter Controller
-- Demonstrates event handling and state management

local count = 0
local lbl = kryon.widget("lbl_count")
local btn_inc = kryon.widget("btn_inc")
local btn_dec = kryon.widget("btn_dec")

-- Increment button handler
btn_inc.on_click(function()
    count = count + 1
    lbl.text = "Count: " .. count
end)

-- Decrement button handler
btn_dec.on_click(function()
    count = count - 1
    lbl.text = "Count: " .. count
end)

-- Start event loop
kryon.run()
