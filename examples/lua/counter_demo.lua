#!/usr/bin/env luajit
-- Kryon Counter Demo (Lua)
-- Demonstrates reactive state management with multiple counters

package.path = "../../bindings/lua/?.lua;../../bindings/lua/?/init.lua;" .. package.path

local kryon = require("kryon")

print("===== Kryon Counter Demo (Lua) =====")
print("Demonstrating reactive state system\n")

-- ============================================================================
-- Reusable Counter Component
-- ============================================================================

local function Counter(props)
  props = props or {}
  local initialValue = props.initialValue or 0
  local label = props.label or "Counter"
  local color = props.color or "#3498DB"

  -- Create reactive state (Lua-side observable)
  local count = kryon.state(initialValue)

  -- Build UI components
  local textComponent = kryon.Text {
    text = tostring(count:get()),
    fontSize = 32,
    color = "white",
    fontWeight = 700,
    minWidth = "80px",
    textAlign = "center",
  }

  -- Observer: Update text when state changes
  count:observe(function(newValue)
    -- This triggers a C IR call to update the text!
    kryon.C.ir_set_text_content(textComponent, tostring(newValue))
    print(string.format("[%s] Count changed to: %d", label, newValue))
  end)

  -- Return the counter UI
  return kryon.Container {
    backgroundColor = "#2C3E50",
    padding = "20px",
    borderRadius = 8,

    kryon.Column {
      gap = 15,
      alignItems = "center",

      -- Label
      kryon.Text {
        text = label,
        fontSize = 18,
        color = "#BDC3C7",
        fontWeight = 600,
      },

      -- Counter controls
      kryon.Row {
        gap = 20,
        alignItems = "center",

        -- Decrement button
        kryon.Button {
          text = "-",
          width = "60px",
          height = "50px",
          backgroundColor = "#E74C3C",
          fontSize = 28,
          fontWeight = 700,
          borderRadius = 5,
          onClick = function()
            count:set(count:get() - 1)
          end
        },

        -- Count display (reactive!)
        textComponent,

        -- Increment button
        kryon.Button {
          text = "+",
          width = "60px",
          height = "50px",
          backgroundColor = "#2ECC71",
          fontSize = 28,
          fontWeight = 700,
          borderRadius = 5,
          onClick = function()
            count:set(count:get() + 1)
          end
        },
      },

      -- Additional controls row
      kryon.Row {
        gap = 10,

        kryon.Button {
          text = "Reset",
          width = "80px",
          height = "35px",
          backgroundColor = color,
          fontSize = 14,
          borderRadius = 3,
          onClick = function()
            count:set(initialValue)
          end
        },

        kryon.Button {
          text = "×2",
          width = "60px",
          height = "35px",
          backgroundColor = "#9B59B6",
          fontSize = 14,
          borderRadius = 3,
          onClick = function()
            count:set(count:get() * 2)
          end
        },

        kryon.Button {
          text = "÷2",
          width = "60px",
          height = "35px",
          backgroundColor = "#F39C12",
          fontSize = 14,
          borderRadius = 3,
          onClick = function()
            count:set(math.floor(count:get() / 2))
          end
        },
      },
    },
  }
end

-- ============================================================================
-- Main Application
-- ============================================================================

local app = kryon.app {
  window = {
    width = 1000,
    height = 700,
    title = "Counter Demo - Kryon Lua"
  },

  body = kryon.Container {
    width = "100%",
    height = "100%",
    backgroundColor = "#1E1E1E",
    padding = "40px",

    kryon.Column {
      gap = 30,
      width = "100%",
      height = "100%",

      -- Header
      kryon.Container {
        width = "100%",
        padding = "0 0 20px 0",

        kryon.Column {
          gap = 10,
          alignItems = "center",

          kryon.Text {
            text = "Reactive Counters",
            fontSize = 36,
            color = "#ECF0F1",
            fontWeight = 700,
          },

          kryon.Text {
            text = "Each counter has independent reactive state",
            fontSize = 16,
            color = "#95A5A6",
          },
        },
      },

      -- Counters grid
      kryon.Row {
        gap = 30,
        alignItems = "center",
        justifyContent = "center",

        Counter {
          label = "Counter A",
          initialValue = 0,
          color = "#3498DB",
        },

        Counter {
          label = "Counter B",
          initialValue = 10,
          color = "#E67E22",
        },

        Counter {
          label = "Counter C",
          initialValue = -5,
          color = "#1ABC9C",
        },
      },

      -- Info box
      kryon.Container {
        width = "100%",
        backgroundColor = "#34495E",
        padding = "20px",
        borderRadius = 8,
        margin = "auto 0 0 0",

        kryon.Column {
          gap = 10,

          kryon.Text {
            text = "💡 How it works:",
            fontSize = 18,
            color = "#F39C12",
            fontWeight = 700,
          },

          kryon.Text {
            text = "• Each counter uses kryon.state() for reactive state management\n• When you click +/-, the state updates and observers are notified\n• Observers call C IR functions to update the component tree\n• Layout and rendering happen entirely in the C IR core\n• Same pipeline as Nim - zero abstraction over IR!",
            fontSize = 14,
            color = "#BDC3C7",
            lineHeight = 1.8,
          },
        },
      },
    },
  }
}

-- Debug output
print("\n===== IR Tree Structure =====")
kryon.debugPrintTree(app.root)

print("\n===== App Created Successfully =====")
print("Try interacting with the counters!")
print("(This demo creates the IR tree with event handlers attached)")
