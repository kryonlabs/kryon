#!/usr/bin/env lu9
-- counter-demo.lua - External controller using 9P
-- Updated for lu9 (native Plan 9 filesystem API)

local p9 = require("p9")

-- Configuration
local MOUNT_POINT = "/mnt/term"  -- Where marrow mounts the 9P filesystem
local POLL_INTERVAL = 0.05

-- Find first window
local function find_window()
    local path = MOUNT_POINT .. "/mnt/wm/win"
    local fd, err = p9.open(path, "r")
    if not fd then
        io.stderr:write("Failed to open " .. path .. ": " .. (err or "unknown") .. "\n")
        return nil
    end

    local data = fd:read(4096)
    fd:close()

    local win_id = tonumber(data:match("%d+"))
    return win_id
end

local function read_state(win_id, key)
    local path = MOUNT_POINT .. "/mnt/wm/win/" .. win_id .. "/state/" .. key
    local fd, err = p9.open(path, "r")
    if not fd then
        return nil
    end

    local data = fd:read(4096)
    fd:close()

    if data and #data > 0 then
        return tonumber(data) or 0
    end
    return nil
end

local function write_state(win_id, key, value)
    local path = MOUNT_POINT .. "/mnt/wm/win/" .. win_id .. "/state/" .. key
    local fd, err = p9.open(path, "w")
    if not fd then
        return false, err
    end

    fd:write(tostring(value))
    fd:close()
    return true
end

local function set_widget_text(win_id, widget_name, text)
    local path = MOUNT_POINT .. "/mnt/wm/win/" .. win_id .. "/widgets/" .. widget_name .. "/text"
    local fd, err = p9.open(path, "w")
    if not fd then
        return false, err
    end

    fd:write(text)
    fd:close()
    return true
end

local function poll_event(win_id, widget_name)
    local path = MOUNT_POINT .. "/mnt/wm/win/" .. win_id .. "/events/" .. widget_name
    local fd, err = p9.open(path, "r")
    if not fd then
        return nil
    end

    local data = fd:read(4096)
    fd:close()

    if data and #data > 0 then
        -- Clear event
        fd = p9.open(path, "w")
        if fd then
            fd:write("")
            fd:close()
        end
        return true
    end
    return nil
end

-- Main
local win_id = find_window()
if not win_id then
    io.stderr:write("No window found. Make sure marrow is running and a counter-demo window is open.\n")
    os.exit(1)
end

local count = read_state(win_id, "count") or 0
local step = read_state(win_id, "step") or 1

-- Update display
set_widget_text(win_id, "counter_display", "Count: " .. count)

print("Connected to window " .. win_id .. ", count=" .. count)

-- Event loop
local widgets = {"increment", "decrement", "increment_large", "decrement_large", "reset"}
while true do
    for _, widget in ipairs(widgets) do
        if poll_event(win_id, widget) then
            -- Handle click
            if widget == "increment" then
                count = count + step
            elseif widget == "decrement" then
                count = count - step
            elseif widget == "increment_large" then
                count = count + (step * 10)
            elseif widget == "decrement_large" then
                count = count - (step * 10)
            elseif widget == "reset" then
                count = 0
            end

            -- Update state
            write_state(win_id, "count", count)

            -- Update display
            set_widget_text(win_id, "counter_display", "Count: " .. count)

            print(widget .. ": count = " .. count)
        end
    end

    p9.sleep(POLL_INTERVAL * 1000)  -- p9.sleep takes milliseconds
end
