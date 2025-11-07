-- Hello World Terminal Example
-- Demonstrates basic terminal UI rendering using Kryon's terminal backend

-- Note: This would require the terminal-enabled Lua bindings
-- For now, this is a conceptual example showing how it would work

local terminal = require("../../../bindings/lua/init") -- Would need terminal support

-- Application state
local app_state = {
    click_count = 0,
    running = true
}

-- Terminal UI setup
local function setup_terminal()
    -- This would initialize the terminal renderer
    print("Initializing Kryon Terminal Application...")
    print("========================================")

    -- Show terminal capabilities
    print("Terminal Type: " .. (os.getenv("TERM") or "unknown"))
    print("Colors: " .. (os.getenv("COLORTERM") or "basic"))
    print("Size: " .. (terminal.get_size and terminal.get_size() or "80x24"))
    print("")
end

-- Draw UI to terminal
local function draw_ui()
    -- Clear screen
    io.write("\027[2J\027[H")
    io.flush()

    -- Draw title
    print("")
    print("Hello World from Kryon Terminal!")
    print("================================")
    print("")

    -- Draw decorative border using Unicode box characters
    print("┌─────────────────────────────────────────────────┐")
    print("│                                                 │")
    print("│         Hello from Lua + Kryon Terminal!        │")
    print("│                                                 │")
    print("└─────────────────────────────────────────────────┘")
    print("")

    -- Draw status information
    print("Status: Running")
    print("Language: Lua")
    print("Renderer: Terminal (libtickit)")
    print("Press Enter to continue...")
    print("")
end

-- Handle user input
local function handle_input()
    local input = io.read()
    if input then
        return true  -- Continue
    else
        return false  -- EOF / Quit
    end
end

-- Main application
local function main()
    -- Setup terminal
    setup_terminal()

    -- Create UI structure (conceptual)
    local app = terminal.app({
        width = 80,
        height = 24,
        title = "Hello World Terminal",

        body = terminal.container({
            background = terminal.colors.midnight_blue,
            children = {
                terminal.text({
                    text = "Hello World from Lua!",
                    color = terminal.colors.yellow,
                    x = 25,
                    y = 10
                })
            }
        })
    })

    -- Main loop
    local running = true

    while running do
        -- Draw UI
        draw_ui()

        -- Handle input
        running = handle_input()

        -- In a real implementation, this would be:
        -- app:render_frame()
        -- app:handle_events()
    end

    -- Cleanup
    print("")
    print("Application terminated.")
end

-- Run the application
main()