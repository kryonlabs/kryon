## Project scaffolding and management
##
## Handles creating new Kryon projects with different language frontends

import os, strutils, json, times

# Forward declarations
proc createNimProject*(name: string)
proc createLuaProject*(name: string)
proc createCProject*(name: string)

proc createProject*(name: string, projectTemplate: string) =
  ## Create a new Kryon project with specified projectTemplate

  if dirExists(name):
    raise newException(IOError, "Directory already exists: " & name)

  echo "Setting up project structure..."

  # Create main project directory
  createDir(name)
  createDir(name / "src")
  createDir(name / "build")

  # Create project configuration
  let config = %*{
    "name": name,
    "version": "1.0.0",
    "projectTemplate": projectTemplate,
    "targets": ["linux"],
    "created": getTime().format("yyyy-MM-dd HH:mm:ss"),
    "kryon_version": "1.2.0"
  }

  writeFile(name / "kryon.json", pretty(config))

  case projectTemplate.toLowerAscii():
    of "nim":
      createNimProject(name)
    of "lua":
      createLuaProject(name)
    of "c":
      createCProject(name)
    else:
      raise newException(ValueError, "Unknown projectTemplate: " & projectTemplate)

proc createNimProject*(name: string) =
  ## Create Nim-based Kryon project

  # Create main.nim
  let mainNim = """
import kryon

# Application state
var counter = 0

# UI Declaration
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "$1"

  Container:
    x = 100
    y = 100
    width = 600
    height = 400
    backgroundColor = "#2d2d2d"

    Text:
      text = "Hello from Kryon!"
      color = "#ffffff"
      fontSize = 24

    Button:
      text = "Click me! (Count: " & $counter & ")"
      onClick = proc() =
        counter += 1
        echo "Button clicked! Count: " & $counter
      width = 200
      height = 50
      backgroundColor = "#007acc"
      textColor = "#ffffff"

    Spacer:
      height = 20

    Text:
      text = "Template: Nim"
      color = "#888888"
      fontSize = 16

# Run the application
app.run()
""" % [name]

  writeFile(name / "src" / "main.nim", mainNim)

  # Create build.nim
  let buildNim = """
# Kryon build configuration for Nim

when defined(stm32f4):
  # STM32F4 target configuration
  --cpu:arm
  --os:none
  --define:KRYON_PLATFORM_MCU
  --define:KRYON_NO_HEAP
  --define:KRYON_NO_FLOAT
  --passL:"-L../../build -lkryon_core -lkryon_fb"
  --passC:"-I../../core/include"
  --deadCodeElim:on
  --opt:size

elif defined(linux):
  # Linux desktop configuration
  --define:KRYON_PLATFORM_DESKTOP
  --passL:"-L../../build -lkryon_core -lkryon_fb -lkryon_common"
  --passC:"-I../../core/include"
  --threads:on
  --gc:arc
  --opt:speed

else:
  # Default configuration
  --define:KRYON_PLATFORM_DESKTOP
  --passL:"-L../../build -lkryon_core -lkryon_fb -lkryon_common"
  --passC:"-I../../core/include"
"""

  writeFile(name / "src" / "build.nim", buildNim)

  # Create README
  let readme = """
# $1

A Kryon application built with Nim.

## Build and Run

```bash
# Build for Linux desktop
kryon build --target=linux

# Run locally
kryon run --target=linux

# Build for STM32F4 (requires hardware)
kryon build --target=stm32f4
```

## Project Structure

- `src/main.nim` - Main application entry point
- `src/build.nim` - Target-specific build configuration
- `kryon.json` - Project metadata and configuration
- `build/` - Build output directory

## Architecture

This project uses the Kryon C99 ABI core with Nim language bindings.
The same source code can be compiled for multiple targets without modification.

For more information, see the [Kryon Framework](https://github.com/kryon/kryon) documentation.
""" % [name]

  writeFile(name / "README.md", readme)

proc createLuaProject*(name: string) =
  ## Create Lua-based Kryon project

  # Create main.lua
  let mainLua = """
local kryon = require("kryon")

-- Application state
local counter = 0

-- UI Declaration
local app = kryon.app({
  header = {
    width = 800,
    height = 600,
    title = "$1"
  },

  body = kryon.container({
    x = 100,
    y = 100,
    width = 600,
    height = 400,
    backgroundColor = "#2d2d2d",

    children = {
      kryon.text({
        text = "Hello from Kryon!",
        color = "#ffffff",
        fontSize = 24
      }),

      kryon.button({
        text = string.format("Click me! (Count: %d)", counter),
        onClick = function()
          counter = counter + 1
          print("Button clicked! Count: " .. counter)
          -- Update button text
          this:setText("Click me! (Count: " .. counter .. ")")
        end,
        width = 200,
        height = 50,
        backgroundColor = "#007acc",
        textColor = "#ffffff"
      }),

      kryon.spacer({height = 20}),

      kryon.text({
        text = "Template: Lua",
        color = "#888888",
        fontSize = 16
      })
    }
  })
})

-- Run the application
kryon.run(app)
""" % [name]

  writeFile(name / "src" / "main.lua", mainLua)

  # Create build.lua
  let buildLua = """
-- Kryon build configuration for Lua

return {
  name = "$1",
  projectTemplate = "lua",
  targets = {"linux"},

  cflags = "-I../../core/include",
  libs = "-L../../build -lkryon_core -lkryon_fb -lkryon_common",

  lua_version = "5.4",
  lua_path = "lua5.4"
}
""" % [name]

  writeFile(name / "src" / "build.lua", buildLua)

proc createCProject*(name: string) =
  ## Create C-based Kryon project

  # Create main.c
  let mainC = """
#include <kryon.h>
#include <stdio.h>

// Application state
static int click_count = 0;

// Button click handler
void on_button_click(kryon_component_t* button, kryon_event_t* event) {
    click_count++;
    printf("Button clicked! Count: %d\\n", click_count);

    // Update button text (would need text component state update)
    // This is simplified - real implementation would update the text state
}

int main() {
    printf("$1 - Kryon C Application\\n");

    // Create root container
    kryon_component_t* root = kryon_component_create(&kryon_container_ops, NULL);
    if (!root) {
        printf("Failed to create root component\\n");
        return 1;
    }

    // Set container bounds
    kryon_component_set_bounds(root,
        kryon_fp_from_int(0), kryon_fp_from_int(0),
        kryon_fp_from_int(800), kryon_fp_from_int(600));

    // Create text component
    kryon_text_state_t text_state = {
        .text = "Hello from Kryon C Core!",
        .font_id = 0,
        .max_length = 32,
        .word_wrap = false
    };

    kryon_component_t* text = kryon_component_create(&kryon_text_ops, &text_state);
    if (text) {
        kryon_component_add_child(root, text);
    }

    // Create button component
    kryon_button_state_t button_state = {
        .text = "Click me! (Count: 0)",
        .font_id = 0,
        .pressed = false,
        .on_click = on_button_click
    };

    kryon_component_t* button = kryon_component_create(&kryon_button_ops, &button_state);
    if (button) {
        kryon_component_add_child(root, button);
    }

    // Perform layout
    kryon_layout_tree(root, kryon_fp_from_int(800), kryon_fp_from_int(600));

    // Create renderer (framebuffer for testing)
    kryon_renderer_t* renderer = kryon_renderer_create(&kryon_framebuffer_ops);
    if (!renderer) {
        printf("Failed to create renderer\\n");
        kryon_component_destroy(root);
        return 1;
    }

    // Simple rendering loop (would be proper event loop in real app)
    for (int i = 0; i < 100; i++) {
        kryon_render_frame(renderer, root);
        // In real app: handle events, sleep for frame timing, etc.
    }

    // Cleanup
    kryon_renderer_destroy(renderer);
    kryon_component_destroy(root);

    printf("Application completed successfully\\n");
    return 0;
}
""" % [name]

  writeFile(name / "src" / "main.c", mainC)

  # Create Makefile
  let makefile = """
# Kryon C Project Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../../core/include
LIBS = -L../../build -lkryon_core -lkryon_fb -lkryon_common -lm

TARGET = $1
SOURCE = src/main.c
BUILD_DIR = build

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET): $(SOURCE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCE) $(LIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)

run: $(BUILD_DIR)/$(TARGET)
	./$(BUILD_DIR)/$(TARGET)

.PHONY: all clean run
""" % [name]

  writeFile(name / "Makefile", makefile)