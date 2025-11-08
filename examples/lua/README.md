# Kryon Lua Bindings

**Complete Lua interface for the Kryon UI framework** with Love2D-style canvas API and Markdown support.

## Overview

The Kryon Lua bindings provide:

- **🚀 Love2D-style Canvas API** - Immediate mode drawing functions
- **📝 Markdown Components** - CommonMark compliant rendering with themes
- **🎨 Component System** - Reusable UI components (Button, Text, Input, etc.)
- **🧩 Layout Helpers** - Row, column, and center layout functions
- **🔧 High-level DSL** - Clean, Lua-idiomatic interface

## Quick Start

### Installation

```bash
# The Lua bindings are part of the main Kryon framework
# Build the C library:
make build-lua
```

### Basic Example

```lua
local kryon = require("kryon")
local canvas = require("canvas")

local app = kryon.app({
    width = 800,
    height = 600,
    title = "Hello Lua",

    body = kryon.container({
        width = 800,
        height = 600,
        background = 0xf0f0f0,

        children = {
            kryon.text({
                text = "Hello, World!",
                color = 0x333333,
                x = 50,
                y = 50
            }),

            kryon.button({
                text = "Click me!",
                on_click = function()
                    print("Button clicked!")
                end,
                x = 50,
                y = 100
            })
        }
    })
})
```

## Canvas Drawing

The canvas system provides **Love2D-inspired immediate mode drawing**:

```lua
local canvas = require("canvas")

local function drawCanvas()
    canvas.background(25, 25, 35)
    canvas.fill(255, 100, 100, 200)
    canvas.rectangle(canvas.FILL, 50, 50, 100, 80)
    canvas.circle(canvas.FILL, 200, 90, 40)
end

local app = kryon.app({
    body = kryon.canvas_enhanced({
        width = 800,
        height = 600,
        on_draw = drawCanvas
    })
})
```

### Canvas API Reference

#### Color Management
```lua
canvas.fill(r, g, b, a)           -- Set fill color
canvas.stroke(r, g, b, a)         -- Set stroke color
canvas.strokeWeight(width)        -- Set line width
canvas.background(r, g, b, a)     -- Clear background
canvas.clear()                    -- Clear to transparent
```

#### Shape Drawing
```lua
canvas.rectangle(mode, x, y, w, h)  -- Draw rectangle
canvas.circle(mode, x, y, radius)    -- Draw circle
canvas.ellipse(mode, x, y, rx, ry)   -- Draw ellipse
canvas.line(x1, y1, x2, y2)          -- Draw line
canvas.polygon(mode, points)         -- Draw polygon
canvas.arc(mode, x, y, r, start, end) -- Draw arc
canvas.point(x, y)                    -- Draw point
```

#### Transforms
```lua
canvas.push()                -- Save transform state
canvas.pop()                 -- Restore transform state
canvas.translate(x, y)       -- Move origin
canvas.rotate(degrees)       -- Rotate
canvas.scale(x, y)           -- Scale
```

#### Convenience Functions
```lua
canvas.withColor(color, func)       -- Temporarily set color
canvas.withTransform(func)          -- Temporarily apply transform
canvas.print(text, x, y)            -- Draw text
canvas.printf(text, x, y, limit, align) -- Draw formatted text
```

#### Drawing Modes
```lua
canvas.FILL          -- Fill shapes
canvas.LINE          -- Outline shapes
canvas.FILL_AND_LINE -- Both fill and outline
```

## Markdown Rendering

Rich text formatting with **full CommonMark compliance**:

```lua
local markdown = require("markdown")

local app = kryon.app({
    body = kryon.markdown({
        source = """
        # Documentation

        This is **formatted text** with `code` and [links](https://example.com).

        ## Features
        - Easy to use
        - High performance
        - Cross-platform
        """,
        theme = markdown.themeDark()
    })
})
```

### Markdown Themes

```lua
-- Built-in themes
markdown.themeLight()   -- Light theme (default)
markdown.themeDark()    -- Dark theme
markdown.themeGitHub()  -- GitHub theme
```

## Component System

Built-in components for common UI patterns:

```lua
-- Container with layout
kryon.container({
    width = 400,
    height = 300,
    background = 0xf0f0f0,

    children = {
        kryon.row({
            gap = 10,

            children = {
                kryon.button({
                    text = "Click me!",
                    on_click = function() print("Clicked!") end
                }),

                kryon.text({
                    text = "Status: Ready",
                    color = 0x666666
                })
            }
        }),

        kryon.column({
            gap = 5,

            children = {
                kryon.input({
                    placeholder = "Enter text...",
                    on_change = function(text) print("Input:", text) end
                }),

                kryon.text({
                    text = "Enable notifications"
                }),

                kryon.checkbox({
                    checked = false,
                    on_change = function(checked) print("Notifications:", checked) end
                })
            }
        })
    }
})
```

### Component Properties

All components support these common properties:

```lua
kryon.component({
    -- Position and size
    x = 50,              -- X position
    y = 100,             -- Y position
    width = 200,         -- Width
    height = 50,         -- Height

    -- Appearance
    background = 0xffffff,    -- Background color
    visible = true,           -- Visibility

    -- Content
    children = {...}          -- Child components
})
```

### Specialized Components

#### Button
```lua
kryon.button({
    text = "Click me",
    on_click = function() end,  -- Click handler
    background = 0x3498db
})
```

#### Text
```lua
kryon.text({
    text = "Hello World",
    color = 0x333333,
    font_size = 16
})
```

#### Input
```lua
kryon.input({
    placeholder = "Enter text...",
    value = "",
    password = false,
    on_change = function(text) end,
    on_submit = function(text) end
})
```

#### Canvas
```lua
kryon.canvas_enhanced({
    width = 400,
    height = 300,
    background = 0x000000,
    on_draw = function() end  -- Drawing handler
})
```

#### Markdown
```lua
kryon.markdown({
    source = "# Title\n\nContent...",
    theme = markdown.themeLight(),
    on_link_click = function(url) end
})
```

## Layout Helpers

### Row Layout
```lua
kryon.row({
    x = 0, y = 0,
    gap = 10,           -- Space between items
    child_width = 100,  -- Child component width
    child_height = 30,  -- Child component height

    children = {
        kryon.button({text = "Button 1"}),
        kryon.button({text = "Button 2"}),
        kryon.button({text = "Button 3"})
    }
})
```

### Column Layout
```lua
kryon.column({
    x = 0, y = 0,
    gap = 5,            -- Space between items
    child_width = 200,  -- Child component width
    child_height = 30,  -- Child component height

    children = {
        kryon.text({text = "First item"}),
        kryon.text({text = "Second item"}),
        kryon.text({text = "Third item"})
    }
})
```

### Center Layout
```lua
kryon.center({
    width = 200,
    height = 200,

    children = {
        kryon.button({
            text = "Centered",
            child_width = 100,
            child_height = 30
        })
    }
})
```

## Color Management

### RGB/RGBA Helpers
```lua
local color = kryon.rgba(255, 0, 0, 128)  -- Semi-transparent red
local color2 = kryon.rgb(0, 255, 0)        -- Green (opaque)
```

### Color Constants
```lua
kryon.colors.white
kryon.colors.black
kryon.colors.red
kryon.colors.green
kryon.colors.blue
kryon.colors.yellow
kryon.colors.cyan
kryon.colors.magenta
kryon.colors.gray
kryon.colors.transparent
```

## Canvas Examples

### Advanced Drawing
```lua
local function drawAdvanced()
    -- Background
    canvas.background(20, 20, 30)

    -- Transformed shapes
    canvas.withTransform(function()
        canvas.translate(200, 200)
        canvas.rotate(45)
        canvas.scale(2)

        canvas.fill(255, 100, 100)
        canvas.rectangle(canvas.FILL, -50, -50, 100, 100)
    end)

    -- Complex polygon
    local star_points = {}
    for i = 0, 9 do
        local angle = (i * 36 - 90) * math.pi / 180
        local radius = i % 2 == 0 and 50 or 25
        table.insert(star_points, {
            x = math.cos(angle) * radius,
            y = math.sin(angle) * radius
        })
    end

    canvas.fill(255, 255, 0)
    canvas.polygon(canvas.FILL, star_points)
end
```

### Animation
```lua
local rotation = 0

local function drawAnimated()
    canvas.background(40, 40, 50)

    rotation = rotation + 2  -- Rotate 2 degrees per frame

    canvas.withTransform(function()
        canvas.translate(300, 200)
        canvas.rotate(rotation)

        canvas.fill(100, 200, 255)
        canvas.rectangle(canvas.FILL, -40, -40, 80, 80)
    end)
end
```

## Examples

See the `examples/lua/` directory for complete examples:

- `canvas_shapes_demo.lua` - Comprehensive canvas API demo
- `canvas_basic_test.lua` - Simple canvas test
- `markdown_demo.lua` - Interactive markdown demo
- `hello_world.lua` - Basic application example
- `button_demo.lua` - Button interaction example
- `todo.lua` - Todo list application

## Performance

- **Memory efficient** - Minimal allocations in core rendering path
- **Cross-platform** - Works from MCUs to desktop
- **Optimized parsing** - Fast markdown rendering
- **Immediate mode** - Direct drawing without overhead

## Architecture

The Lua bindings use a layered architecture:

```
Lua Application Layer
    ↓
High-level Lua API (init.lua, canvas.lua, markdown.lua)
    ↓
Low-level C Bindings (kryon_lua.c)
    ↓
C99 Core Engine (UI, Layout, Events, Canvas, Markdown)
    ↓
Platform Backends (SDL3, Framebuffer, Terminal)
```

## Building

```bash
# Build with Lua support
make build-lua

# Run Lua example
./build/lua-kryon examples/lua/hello_world.lua
```

## License

**0BSD License** - Permissive, business-friendly open source license.

---

*"Measure twice, cut once. The framework's true test isn't how much it can do on a desktop — it's how little it needs to run on a $2 microcontroller."*