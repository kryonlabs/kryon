# Kryon Architecture

## Overview

Kryon is a multi-platform UI framework designed with a clean separation between the core logic, rendering backends, and language frontends.

### Core Philosophy
*   **Minimal surfaces, maximal flexibility.**
*   **Hardware-aware abstractions.**
*   **Declarative UI in your application language.**

## Pipeline Diagram

```mermaid
flowchart TB
    subgraph Frontends["FRONTENDS (Language Bindings)"]
        direction LR
        Nim["Nim<br/>(DSL Macros)"]
        TS["TypeScript<br/>(JSX/Bun)"]
        Lua["Lua<br/>(DSL Tables)"]
        C["C<br/>(Direct API)"]
    end

    subgraph Core["IR CORE (C Library)"]
        direction TB
        Builder["ir_builder<br/>• Components<br/>• Tree mgmt<br/>• Styles"]
        Layout["ir_layout<br/>• Flexbox<br/>• Size calc<br/>• Alignment"]
        Events["ir_events<br/>• Click/Hover<br/>• Focus/Blur<br/>• Keyboard"]
        IRComp["IRComponent<br/>(Tree Structure)"]

        Builder --> IRComp
        Layout --> IRComp
        Events --> IRComp
    end

    subgraph Backends["BACKENDS (Platform Renderers)"]
        direction LR
        SDL3["SDL3<br/>• GPU accel<br/>• TTF fonts<br/>• Desktop"]
        Terminal["Terminal<br/>• libtickit<br/>• Unicode<br/>• SSH-able"]
        Framebuffer["Framebuffer<br/>• /dev/fb0<br/>• Embedded<br/>• No X11"]
        Web["Web<br/>• HTML gen<br/>• CSS flexbox<br/>• Browser"]
    end

    Nim --> Core
    TS --> Core
    Lua --> Core
    C --> Core

    Core --> SDL3
    Core --> Terminal
    Core --> Framebuffer
    Core --> Web

    SDL3 --> Desktop["Desktop Window<br/>(Linux/macOS/Windows)"]
    Terminal --> Term["Terminal Emulator<br/>(xterm/konsole/SSH)"]
    Framebuffer --> Embedded["Embedded Display<br/>(RPi/MCU)"]
    Web --> Browser["Web Browser<br/>(Chrome/Firefox/Safari)"]
```

## Data Flow

```mermaid
flowchart TB
    subgraph Define["1. DEFINE UI"]
        NimDSL["Nim DSL<br/>Container:<br/>  Button: text='Click'"]
        TSXDSL["TypeScript JSX<br/>&lt;column&gt;<br/>  &lt;button title='Click' /&gt;<br/>&lt;/column&gt;"]
    end

    subgraph IRTree["2. CREATE IR TREE"]
        Tree["IRComponent (Column)<br/>├─ style: width=100%<br/>├─ layout: gap=10<br/>└─ children:<br/>    └─ IRComponent (Button)<br/>        └─ events: onClick"]
    end

    subgraph Layout["3. COMPUTE LAYOUT"]
        Calc["ir_layout.c<br/>• Resolve dimensions<br/>• Position offsets<br/>• Flex distribution<br/>• Alignment"]
    end

    subgraph Render["4. RENDER TO TARGET"]
        direction LR
        R1["SDL3<br/>Draw rects<br/>TTF text"]
        R2["Terminal<br/>Box chars<br/>ANSI color"]
        R3["Framebuffer<br/>Direct pixels"]
        R4["Web<br/>Generate HTML<br/>CSS flexbox"]
    end

    NimDSL --> Tree
    TSXDSL --> Tree
    Tree --> Calc
    Calc --> R1
    Calc --> R2
    Calc --> R3
    Calc --> R4
```

## System Architecture

### Layered Onion Model

```mermaid
flowchart TB
    subgraph L1["Application Layer"]
        App["Your Business Logic<br/>(Nim/Rust/C/JS/Lua)"]
    end

    subgraph L2["Interface Layer"]
        Bindings["Language Bindings & Macros<br/>(Nim DSL, JSX Runtime)"]
    end

    subgraph L3["Core Layer"]
        Core2["Component Model, State, Layout<br/>(C ABI)"]
    end

    subgraph L4["Renderer Abstraction"]
        Primitives["Primitive Drawing Commands<br/>(C ABI)"]
    end

    subgraph L5["Platform Backends"]
        Platforms["SDL3 | Terminal | Framebuffer | Web"]
    end

    L1 --> L2 --> L3 --> L4 --> L5
```

### Critical Boundary

The **Core Layer** and **Renderer Abstraction** expose a pure C ABI. This allows:
*   Microcontrollers to link directly to the C core.
*   Desktop apps to use high-level bindings.
*   Web apps to compile to WASM.

## Component Types

```mermaid
flowchart TB
    subgraph Layout["LAYOUT COMPONENTS"]
        Container["Container (div)"]
        Container --> Row["Row (←→)"]
        Container --> Column["Column (↑↓)"]
        Container --> Center["Center (⊕)"]
    end

    subgraph Content["CONTENT COMPONENTS"]
        Text["Text"]
        Button["Button"]
        Input["Input"]
        Checkbox["Checkbox"]
        Dropdown["Dropdown"]
        Image["Image"]
    end

    subgraph Special["SPECIAL COMPONENTS"]
        TabGroup["TabGroup"]
        Canvas["Canvas"]
        Markdown["Markdown"]
    end
```

## Directory Structure

```
kryon/
├── ir/                          # IR Core (C)
│   ├── ir_core.h               # Component types, structs
│   ├── ir_builder.c            # Tree construction
│   ├── ir_layout.c             # Flexbox layout
│   └── ir_events.c             # Event handling
│
├── backends/
│   ├── desktop/                # Desktop backends
│   │   ├── sdl_backend.c       # SDL3 rendering
│   │   └── ir_desktop_renderer.c
│   └── web/                    # Web backend
│       ├── html_generator.c    # HTML output
│       ├── css_generator.c     # CSS output
│       └── ir_web_renderer.c
│
├── bindings/
│   ├── nim/                    # Nim frontend
│   │   ├── kryon_dsl.nim       # DSL macros
│   │   ├── runtime.nim         # Event handlers
│   │   └── reactive_system.nim # State management
│   └── typescript/             # TypeScript frontend
│       └── src/
│           ├── jsx-runtime.ts  # JSX transform
│           ├── renderer.ts     # IRNode → C IR
│           ├── ffi.ts          # Bun FFI bindings
│           └── app.ts          # Entry point
│
├── examples/
│   ├── nim/                    # Nim examples
│   │   ├── hello_world.nim
│   │   ├── button_demo.nim
│   │   └── ...
│   └── typescript/             # TypeScript examples
│       ├── hello_world.tsx
│       ├── button_demo.tsx
│       └── ...
│
└── build/                      # Compiled libraries
    ├── libkryon_ir.a
    ├── libkryon_desktop.so
    └── libkryon_web.so
```

## Running Examples

```bash
# Nim + SDL3 (default)
./run_example.sh hello_world
./run_example.sh hello_world nim sdl3

# Nim + Terminal
./run_example.sh hello_world nim terminal

# TypeScript + SDL3
./run_example.sh hello_world ts
./run_example.sh hello_world typescript sdl3

# TypeScript + Web (generates HTML, starts server)
./run_example.sh hello_world ts web

# By number (from example list)
./run_example.sh 22   # hello_world typescript
```

## Core Layer (C99 ABI)

The core layer handles:
*   Component tree lifecycle.
*   Unified event system.
*   Flexbox-inspired layout engine.
*   Style resolution.
*   Storage API.

It guarantees:
*   **No global state.**
*   **Deterministic layout.**
*   **No hidden allocations.**
*   **Fixed-point math** for MCU compatibility.

## Renderer Abstraction

All backends must implement a minimal set of primitive commands:
*   `draw_rect`
*   `draw_text`
*   `draw_line`
*   `swap_buffers`

## Frontend × Backend Matrix

```mermaid
%%{init: {'theme': 'base', 'themeVariables': { 'fontSize': '14px'}}}%%
flowchart LR
    subgraph Matrix["Compatibility Matrix"]
        direction TB

        subgraph Headers[" "]
            direction LR
            H1["SDL3"]
            H2["Terminal"]
            H3["Framebuffer"]
            H4["Web"]
        end

        subgraph NimRow["Nim"]
            N1["✓"]
            N2["✓"]
            N3["✓"]
            N4["✓"]
        end

        subgraph TSRow["TypeScript"]
            T1["✓"]
            T2["✓"]
            T3["~"]
            T4["✓"]
        end

        subgraph LuaRow["Lua"]
            L1["-"]
            L2["-"]
            L3["-"]
            L4["-"]
        end

        subgraph CRow["C"]
            C1["✓"]
            C2["✓"]
            C3["✓"]
            C4["✓"]
        end
    end
```

| Frontend   | SDL3 | Terminal | Framebuffer | Web |
|------------|:----:|:--------:|:-----------:|:---:|
| Nim        |  ✓   |    ✓     |      ✓      |  ✓  |
| TypeScript |  ✓   |    ✓     |      ~      |  ✓  |
| Lua        |  -   |    -     |      -      |  -  |
| C          |  ✓   |    ✓     |      ✓      |  ✓  |

**Legend:** ✓ = Supported, ~ = Partial, - = Planned

## Backend Comparison

```mermaid
graph TB
    subgraph SDL3["SDL3 Backend"]
        SDL3_use["Use: Desktop apps, Games"]
        SDL3_plat["Platform: Linux/macOS/Windows"]
        SDL3_feat["Features: GPU accel, TTF fonts, Mouse/KB"]
    end

    subgraph Terminal["Terminal Backend"]
        Term_use["Use: CLI tools, Remote/SSH"]
        Term_plat["Platform: Any terminal"]
        Term_feat["Features: TUI boxes, Unicode, ANSI color"]
    end

    subgraph Framebuffer["Framebuffer Backend"]
        FB_use["Use: Embedded, Kiosk"]
        FB_plat["Platform: Linux /dev/fb0, RPi"]
        FB_feat["Features: Direct pixels, No X11, Minimal deps"]
    end

    subgraph Web["Web Backend"]
        Web_use["Use: Browser apps, Dashboards"]
        Web_plat["Platform: Any browser"]
        Web_feat["Features: HTML gen, CSS flexbox, JS events"]
    end
```

| Backend     | Use Case                | Platform              | Key Features                        |
|-------------|-------------------------|-----------------------|-------------------------------------|
| SDL3        | Desktop apps, Games     | Linux/macOS/Windows   | GPU accel, TTF fonts, Mouse/KB      |
| Terminal    | CLI tools, Remote/SSH   | Any terminal          | TUI boxes, Unicode, ANSI color      |
| Framebuffer | Embedded, Kiosk         | Linux /dev/fb0, RPi   | Direct pixels, No X11, Minimal deps |
| Web         | Browser apps, Dashboards| Any browser           | HTML gen, CSS flexbox, JS events    |

## Frontend Overview

| Frontend   | Syntax                | Runtime         | Best For                      |
|------------|----------------------|-----------------|-------------------------------|
| Nim        | DSL macros           | Compiled native | Performance, Desktop, Embedded|
| TypeScript | JSX syntax           | Bun + FFI       | Web devs, Rapid prototyping   |
| Lua        | Table-based (planned)| Lua VM          | Scripting, Hot-reload, Modding|
| C          | Direct API calls     | Compiled native | Max control, Library integration |

## Quick Reference

```bash
# List all examples
./run_example.sh

# Frontend options: nim, ts (typescript), lua, c
# Backend options: sdl3, terminal, framebuffer, web

# Examples:
./run_example.sh hello_world              # nim + sdl3 (default)
./run_example.sh hello_world nim sdl3     # nim + sdl3 (explicit)
./run_example.sh hello_world nim terminal # nim + terminal
./run_example.sh hello_world ts           # typescript + sdl3
./run_example.sh hello_world ts web       # typescript + web (serves HTML)
./run_example.sh hello_world ts terminal  # typescript + terminal
```
