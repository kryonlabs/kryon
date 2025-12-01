#!/usr/bin/env luajit
-- Kryon Hello World Example (Lua)
-- Demonstrates basic component creation using the Lua DSL

-- Add parent directory to package path to find kryon module
package.path = "../../bindings/lua/?.lua;../../bindings/lua/?/init.lua;" .. package.path

local kryon = require("kryon")

-- Print version info
print("===== Kryon Hello World (Lua) =====")
kryon.version()
print()

-- Create the UI tree (all C IR calls under the hood!)
local app = kryon.app {
  window = {
    width = 800,
    height = 600,
    title = "Hello World - Kryon Lua"
  },

  body = kryon.Container {
    width = "100%",
    height = "100%",
    backgroundColor = "#191970",  -- Midnight blue
    padding = "40px",

    -- Column layout
    kryon.Column {
      gap = 20,
      alignItems = "center",
      justifyContent = "center",
      width = "100%",
      height = "100%",

      -- Title
      kryon.Text {
        text = "Hello from Lua!",
        fontSize = 48,
        color = "#FFD700",  -- Gold
        fontWeight = 700,
      },

      -- Subtitle
      kryon.Text {
        text = "Powered by the same C IR core as Nim",
        fontSize = 20,
        color = "#87CEEB",  -- Sky blue
        textAlign = "center",
      },

      -- Description
      kryon.Container {
        width = "600px",
        padding = "20px",
        backgroundColor = "#2C3E50",
        borderRadius = 8,
        margin = "20px 0",

        kryon.Text {
          text = "This UI was built using LuaJIT FFI bindings to the Kryon IR core. Every component you see was created with zero-overhead C function calls. The same layout engine, the same rendering pipeline, the same everything - just a different frontend language!",
          fontSize = 16,
          color = "#ECF0F1",
          lineHeight = 1.6,
          textAlign = "center",
        },
      },

      -- Info box
      kryon.Row {
        gap = 40,
        alignItems = "center",

        kryon.Container {
          backgroundColor = "#27AE60",
          padding = "15px 25px",
          borderRadius = 5,

          kryon.Text {
            text = "✓ Zero Abstraction",
            fontSize = 18,
            color = "white",
            fontWeight = 600,
          },
        },

        kryon.Container {
          backgroundColor = "#3498DB",
          padding = "15px 25px",
          borderRadius = 5,

          kryon.Text {
            text = "✓ Shared IR Core",
            fontSize = 18,
            color = "white",
            fontWeight = 600,
          },
        },

        kryon.Container {
          backgroundColor = "#E74C3C",
          padding = "15px 25px",
          borderRadius = 5,

          kryon.Text {
            text = "✓ Same Backends",
            fontSize = 18,
            color = "white",
            fontWeight = 600,
          },
        },
      },
    },
  }
}

-- Debug: Print the IR tree to console (if available)
print("\n===== IR Tree Structure =====")
local hasDebug, err = pcall(function()
  kryon.debugPrintTree(app.root)
end)
if not hasDebug then
  print("Debug functions not available in this build")
  print("(debug_print_tree needs to be linked in libkryon_ir.so)")
end

print("\n===== App Created Successfully =====")
print("Root component: ", app.root)
print("Window: ", app.window.width, "x", app.window.height)
print("\nNote: This example creates the IR tree but doesn't run a renderer.")
print("Use the Kryon CLI to run with a renderer backend (SDL3, Terminal, etc.)")
