#!/usr/bin/env lua
-- counter-demo.lua - External controller for counter demo
-- Demonstrates event notification system and state file access

local WM_ROOT = "/mnt/wm"
local POLL_INTERVAL = 0.05

local state = {
    window_id = nil,
    widgets = {},
}

-- Utilities
local function file_read(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local data = f:read("*a")
    f:close()
    return data:gsub("%s*$", "")
end

local function file_write(path, content)
    local f = io.open(path, "w")
    if not f then return false end
    f:write(tostring(content))
    f:close()
    return true
end

local function log(msg)
    io.stderr:write("[counter] " .. msg .. "\n")
end

-- State helpers
local function state_path(var)
    return string.format("%s/win/%d/state/%s", WM_ROOT, state.window_id, var)
end

local function state_read(var)
    return file_read(state_path(var))
end

local function state_write(var, value)
    return file_write(state_path(var), value)
end

-- Widget helpers
local function widget_path(name)
    return string.format("%s/win/%d/widgets/%s", WM_ROOT, state.window_id, name)
end

local function widget_write(name, prop, value)
    return file_write(widget_path(name) .. "/" .. prop, value)
end

-- Discover widgets by name
local function discover_widgets()
    local widgets_dir = WM_ROOT .. "/win/" .. state.window_id .. "/widgets"
    local f = io.popen("ls " .. widgets_dir)
    if f then
        for line in f:lines() do
            local widget_file = widgets_dir .. "/" .. line .. "/name"
            local widget_name = file_read(widget_file)
            if widget_name then
                state.widgets[widget_name:gsub("%s*$", "")] = line
                log("Found widget: " .. widget_name:gsub("%s*$", "") .. " -> " .. line)
            end
        end
        f:close()
    end
end

-- Event handlers
local function handle_click(widget_name)
    local count = tonumber(state_read("count") or "0")
    local step = tonumber(state_read("step") or "1")

    if widget_name == "increment" then
        count = count + step
    elseif widget_name == "decrement" then
        count = count - step
    elseif widget_name == "increment_large" then
        count = count + (step * 10)
    elseif widget_name == "decrement_large" then
        count = count - (step * 10)
    elseif widget_name == "reset" then
        count = 0
    end

    -- Update state
    state_write("count", tostring(count))

    -- Update UI
    widget_write("counter_display", "text", "Count: " .. count)

    log(string.format("%s: count = %d", widget_name, count))
end

-- Discover window
local function discover_window()
    local windows_dir = WM_ROOT .. "/win"
    local f = io.popen("ls " .. windows_dir)
    if f then
        local first = f:read("*a"):match("%d+")
        f:close()
        if first then
            state.window_id = tonumber(first)
            log("Found window: " .. state.window_id)
            return true
        end
    end
    return false
end

-- Event notification watcher
local function watch_events()
    log("Watching events...")

    while true do
        -- Check each widget's event file
        local widgets = {"increment", "decrement", "increment_large", "decrement_large", "reset"}

        for _, widget_name in ipairs(widgets) do
            local event_path = string.format("%s/win/%d/events/%s", WM_ROOT, state.window_id, widget_name)
            local event_json = file_read(event_path)

            if event_json and #event_json > 0 then
                -- Parse JSON (simple)
                local event_type = event_json:match('"event":"([^"]+)"')

                if event_type == "click" then
                    handle_click(widget_name)
                end

                -- Clear event file
                file_write(event_path, "")
            end
        end

        os.execute("sleep " .. POLL_INTERVAL)
    end
end

-- Main
local function main()
    if not discover_window() then
        log("Error: No window found")
        return
    end

    -- Discover widgets
    discover_widgets()

    -- Initialize display
    local initial_count = state_read("count")
    widget_write("counter_display", "text", "Count: " .. (initial_count or "0"))

    -- Start event loop
    watch_events()
end

main()
