# Kryon Lua Bindings

Zero-overhead LuaJIT FFI bindings to the Kryon IR core.

## Overview

The Lua frontend provides idiomatic Lua syntax while using the **exact same C IR core** as the Nim bindings. Every component, style, and layout property maps directly to C function calls with **zero abstraction**.

```lua
local kryon = require("kryon")

local app = kryon.app {
  body = kryon.Container {
    width = "100%",
    backgroundColor = "#191970",

    kryon.Text {
      text = "Hello from Lua!",
      fontSize = 32,
    }
  }
}
```

## Installation

### Prerequisites

- **LuaJIT 2.0+** (required for FFI support)
- Kryon C IR core (`libkryon_ir.so`)

```bash
# Install LuaJIT
# Ubuntu/Debian
sudo apt install luajit

# macOS
brew install luajit

# Arch
sudo pacman -S luajit
```

### Setup

```bash
# Build the C IR core first
cd ../..
make build

# Test the Lua bindings
cd bindings/lua
luajit -l kryon.ffi -e "print('FFI loaded successfully!')"
```

## Architecture

```
Lua Application Code
        ↓
    Lua DSL Layer (kryon/dsl.lua)
        ↓
   LuaJIT FFI (kryon/ffi.lua)
        ↓  [zero overhead]
   C IR Core (libkryon_ir.so)
        ↓
  Backends (SDL3/Terminal/Web)
```

**Key Point**: The Lua layer is **pure FFI bindings**. All layout computation, rendering, and tree management happens in the shared C IR core.

## Module Structure

```
kryon/
├── init.lua         # Main entry point
├── ffi.lua          # LuaJIT FFI bindings to C
├── runtime.lua      # Event handlers, lifecycle
├── reactive.lua     # Reactive state system
└── dsl.lua          # Component builders
```

## API Reference

### Component Creation

```lua
-- Basic components
kryon.Container { ... }
kryon.Text { text = "Hello" }
kryon.Button { text = "Click me" }
kryon.Row { gap = 20 }
kryon.Column { gap = 20 }
kryon.Input {}
kryon.Checkbox {}
kryon.Markdown { content = "# Hello" }
```

### Properties

#### Dimensions
```lua
width = "100px"        -- Pixels
width = "50%"          -- Percentage
width = "auto"         -- Auto
width = "2fr"          -- Flex units
width = 100            -- Plain number = pixels
width = "10vw"         -- Viewport units
width = "2rem"         -- Root em
```

#### Colors
```lua
backgroundColor = "#FF0000"      -- Hex RGB
backgroundColor = "#FF0000AA"    -- Hex RGBA
backgroundColor = "transparent"
color = "#FFD700"
```

#### Spacing
```lua
padding = "20px"                 -- All sides
padding = "10px 20px"            -- Vertical Horizontal
padding = "10px 20px 30px 40px"  -- Top Right Bottom Left
margin = "20px"
gap = 20
```

#### Layout
```lua
alignItems = "center"        -- start, center, end, stretch
justifyContent = "center"    -- start, center, end, space-between, space-around
alignContent = "center"
```

#### Text
```lua
text = "Hello World"
fontSize = 24
fontWeight = 700          -- 100-900
textAlign = "center"      -- left, right, center, justify
lineHeight = 1.5
```

#### Visual Effects
```lua
opacity = 0.8
zIndex = 10
visible = true
borderRadius = 8
```

### Events

```lua
kryon.Button {
  text = "Click me",
  onClick = function()
    print("Clicked!")
  end
}
```

Available events:
- `onClick`
- `onHover`
- `onFocus`
- `onBlur`

### Reactive State

```lua
-- Create reactive state
local count = kryon.state(0)

-- Get value
local value = count:get()

-- Set value (notifies observers)
count:set(value + 1)

-- Update with function
count:update(function(old) return old + 1 end)

-- Observe changes
count:observe(function(newValue, oldValue)
  print("Changed from", oldValue, "to", newValue)
end)
```

#### Computed Values

```lua
local firstName = kryon.state("John")
local lastName = kryon.state("Doe")

local fullName = kryon.computed({firstName, lastName}, function()
  return firstName:get() .. " " .. lastName:get()
end)

print(fullName:get())  -- "John Doe"
firstName:set("Jane")
print(fullName:get())  -- "Jane Doe"
```

#### Reactive Arrays

```lua
local items = kryon.array({"Apple", "Banana"})

items:push("Cherry")
items:remove(2)
items:insert(1, "Apricot")
items:length()  -- Get count
items:getItems()  -- Get all items

-- Observe changes
items:observe(function(action, index, item)
  print(action, index, item)
end)
```

### Application Lifecycle

```lua
-- Create app
local app = kryon.app {
  window = {
    width = 800,
    height = 600,
    title = "My App"
  },
  body = kryon.Container { ... }
}

-- Update (call in your render loop)
kryon.update(app, deltaTime)

-- Render (computes layout)
kryon.render(app)

-- Cleanup
kryon.destroyApp(app)
```

### Debug Utilities

```lua
-- Print IR tree to console
kryon.debugPrintTree(app.root)

-- Print IR tree to file
kryon.debugPrintTreeToFile(app.root, "/tmp/tree.txt")
```

## Examples

### Hello World

```lua
local kryon = require("kryon")

local app = kryon.app {
  body = kryon.Container {
    width = "100%",
    height = "100%",
    backgroundColor = "#191970",

    kryon.Text {
      text = "Hello from Lua!",
      fontSize = 48,
      color = "yellow",
    }
  }
}
```

### Counter with Reactive State

```lua
local kryon = require("kryon")

local count = kryon.state(0)

local textComponent = kryon.Text {
  text = tostring(count:get()),
  fontSize = 32,
}

-- Update text when state changes
count:observe(function(newValue)
  kryon.C.ir_set_text_content(textComponent, tostring(newValue))
end)

local app = kryon.app {
  body = kryon.Row {
    gap = 20,

    kryon.Button {
      text = "-",
      onClick = function()
        count:set(count:get() - 1)
      end
    },

    textComponent,

    kryon.Button {
      text = "+",
      onClick = function()
        count:set(count:get() + 1)
      end
    },
  }
}
```

### Reusable Components

```lua
local function Card(props)
  return kryon.Container {
    width = props.width or "300px",
    padding = "20px",
    backgroundColor = "#2C3E50",
    borderRadius = 8,

    kryon.Column {
      gap = 10,

      kryon.Text {
        text = props.title,
        fontSize = 24,
        fontWeight = 700,
        color = "white",
      },

      kryon.Text {
        text = props.description,
        fontSize = 16,
        color = "#BDC3C7",
      },
    }
  }
end

-- Usage
local app = kryon.app {
  body = kryon.Column {
    gap = 20,

    Card {
      title = "Hello",
      description = "This is a card"
    },

    Card {
      title = "World",
      description = "This is another card"
    },
  }
}
```

## IR Pipeline Verification

The Lua bindings use the **exact same IR core** as Nim:

```lua
-- Lua code
local component = kryon.Container {
  width = "100px",
  backgroundColor = "#FF0000",
}

-- This becomes:
local component = C.ir_create_component(C.IR_COMPONENT_CONTAINER)
local style = C.ir_create_style()
C.ir_set_width(style, C.IR_DIMENSION_PX, 100)
C.ir_set_background_color(style, 255, 0, 0, 255)
C.ir_set_style(component, style)
```

**Same IR tree**, **same layout**, **same rendering** - just different frontend syntax!

## Performance

LuaJIT FFI calls are **JIT-compiled** and have nearly zero overhead:

```lua
-- Creating 10,000 components
local start = os.clock()
for i = 1, 10000 do
  local component = kryon.C.ir_create_component(kryon.C.IR_COMPONENT_TEXT)
  kryon.C.ir_set_text_content(component, "Hello")
end
print("Time:", os.clock() - start, "seconds")
-- Result: ~0.001s (FFI calls are inlined!)
```

## Comparison with Nim

**Nim:**
```nim
Container:
  width = 100.px
  backgroundColor = "#FF0000"
  Text:
    content = "Hello"
```

**Lua:**
```lua
kryon.Container {
  width = "100px",
  backgroundColor = "#FF0000",
  kryon.Text {
    text = "Hello"
  }
}
```

**Both produce identical IR trees in C memory!**

## Running Examples

```bash
# Navigate to examples
cd ../../examples/lua

# Run hello world
luajit hello_world.lua

# Run counter demo
luajit counter_demo.lua
```

## Integration with Game Engines

### LÖVE (Love2D)

```lua
local kryon = require("kryon")

local app

function love.load()
  app = kryon.app {
    body = kryon.Container { ... }
  }
end

function love.update(dt)
  kryon.update(app, dt)
end

function love.draw()
  kryon.render(app)
  -- Custom rendering integration here
end

function love.mousepressed(x, y, button)
  kryon.runtime.dispatchEvent(component_id, kryon.EventType.CLICK)
end
```

## Contributing

The Lua frontend is designed to be a **thin FFI layer** over the C IR core. When adding features:

1. Check if the C IR function exists
2. Add FFI binding in `kryon/ffi.lua`
3. Add DSL helper in `kryon/dsl.lua`
4. Keep abstraction at zero!

## License

Same as Kryon core project.
