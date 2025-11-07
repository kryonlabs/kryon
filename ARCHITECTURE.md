```markdown
# Kryon Framework Architecture v1.2

**License:** 0BSD  
**Core Philosophy:** *Minimal surfaces, maximal flexibility.*  
**Goal:** A single coherent model for UI development that scales from 8-bit microcontrollers to desktop applications — with zero syntax fragmentation and zero hidden costs.

***

## 🔷 Vision Statement

> "Your UI declaration should be platform-agnostic poetry. The button on your ESP32 should share DNA with the button in your desktop app — not through pixel-perfect illusions, but through identical interaction primitives."

Kryon rejects the false dichotomy between "full framework" and "bare metal." We deliver:
- **Hardware-aware abstractions** (not hardware-agnostic lies)
- **Declarative UI in your application language** (no embedded DSLs)
- **Renderer independence** through primitive command buffers
- **Memory sovereignty** — you control every allocation

***

## 🧩 System Architecture

### Layered Onion Model
***
[ Application Layer ]   ← Your business logic (Nim/Rust/C/JS/Lua)
        ↓
[ Interface Layer   ]   ← Language bindings & macros
        ↓
[     Core Layer    ]   ← Component model, state, layout (C ABI)
        ↓
[ Renderer Abstraction] ← draw_rect/draw_text primitives (C ABI)
        ↓
[ Platform Backends ]   ← Target-specific implementations (C)
***

**Critical Boundary:**  
The **Core Layer** and **Renderer Abstraction** expose a pure C ABI. Everything above/below is replaceable. This enables:
- Microcontrollers to link directly to the C core
- Desktop apps to use Nim/Rust/Lua bindings
- Web apps to compile WASM modules from the same core

***

## 📂 Repository Structure

***
kryon/
├── LICENSE
├── README.md
├── ARCHITECTURE.md       # This document
│
├── core/                 # Platform-agnostic engine (<8KB ROM)
│   ├── component.c       # Component tree lifecycle
│   ├── event.c           # Unified event system
│   ├── layout.c          # Flexbox-inspired layout engine
│   ├── style.c           # Style resolver
│   ├── storage.c         # Key-value persistence API
│   └── include/
│       └── kryon.h       # SINGLE PUBLIC HEADER (C99)
│
├── renderers/            # Backend implementations
│   ├── common/           # Shared utilities
│   │   └── command_buf.c # Ring buffer implementation
│   │
│   ├── framebuffer/      # MCU/SBC/Low-level native
│   │   ├── fb_backend.c  # Direct memory framebuffer access
│   │   ├── fonts/        # 1-bit monochrome font atlas
│   │   │   ├── font_6x8.c
│   │   │   └── font_generator.py
│   │   └── platform/
│   │       ├── stm32f4_spi.c
│   │       ├── rp2040_pio.c
│   │       └── linux_fb.c
│   │
│   ├── sdl3/             # Desktop/mobile (Windows/macOS/Linux/iOS/Android)
│   │   ├── sdl3_backend.c
│   │   ├── sdl3_input.c
│   │   └── sdl3_fonts.c  # TTF font loading wrapper
│   │
│   └── terminal/         # Terminal/TUI backend (Linux/macOS/Windows)
│       ├── terminal_backend.c    # Main terminal renderer implementation
│       ├── terminal_backend.h    # Public API header
│       ├── terminal_events.c     # Event handling system
│       ├── terminal_utils.c      # Utility functions
│       ├── test_suite.c          # Comprehensive unit tests
│       ├── regression_tests.c    # Regression test suite
│       ├── event_system_tests.c  # Event system edge case tests
│       ├── terminal_compatibility_matrix.c  # Terminal compatibility testing
│       ├── stress_test.c         # High-load stress testing
│       ├── run_tests.sh          # Automated test runner
│       └── Makefile              # Complete build system with testing
│
├── bindings/             # Language frontends
│   ├── nim/              # Primary interface
│   │   ├── kryon_dsl.nim # @component macro system
│   │   ├── runtime.nim   # Nim<->C ABI glue
│   │   └── canvas.nim    # Drawing context implementation
│   │
│   └── lua/              # Lua 5.4 binding
│       ├── kryon_lua.c   # C binding implementation
│       ├── init.lua      # Lua module initialization
│       └── canvas.lua    # Drawing context wrapper
│
├── platforms/            # Platform integration kits
│   ├── stm32/            # HAL examples for STM32F4
│   ├── rp2040/           # Pico SDK integration
│   ├── linux/            # SDL3 + Linux setup
│   ├── windows/          # SDL3 + Win32 setup
│   └── web_template/     # HTML boilerplate
│
├── cli/                  # Kryon CLI implementation
│   ├── main.nim          # CLI entry point
│   ├── project.nim       # Project scaffolding
│   ├── build.nim         # Build orchestrator
│   ├── device.nim        # Physical device management
│   └── deps/             # Bundled toolchains
│       ├── arm_gcc/      # ARM toolchain (compressed)
│       └── sdl3_bins/    # Prebuilt SDL3 binaries
│
└── tools/
    ├── kryon_inspect.nim # Component tree debugger
    └── test_runner.nim   # Validation test harness
***

***

## 🛠️ Kryon CLI: The Unified Orchestrator

### Philosophy
The Kryon CLI is the **single interface** for all development workflows — from creating a new project to deploying to a $2 microcontroller. It abstracts away toolchain complexity while exposing full power when needed. No GUI builders, no IDE lock-in — just a fast, scriptable terminal experience.

### Core Responsibilities
| Command               | Functionality                                  | Target Support               |
|-----------------------|-----------------------------------------------|------------------------------|
| `kryon new`           | Project scaffolding with target templates     | All platforms                |
| `kryon build`         | Compile for specified target(s)               | STM32, RP2040, Linux, etc.   |
| `kryon run`           | Build + deploy + live debug session           | Physical devices & emulators |
| `kryon inspect`       | Component tree visualization at runtime       | All platforms (via UART/USB) |
| `kryon add-backend`   | Integrate new renderer backend                 | Developer use only           |
| `kryon doctor`        | Diagnose toolchain/environment issues         | All platforms                |

### Critical Non-Negotiables
1. **No required dependencies** — CLI runs as single static binary
2. **Offline-first** — Works without internet (pre-bundled toolchains)
3. **Deterministic builds** — Identical input → identical output everywhere
4. **Physical device awareness** — Auto-detects connected MCUs/debug probes
5. **Zero configuration defaults** — Sensible defaults for 80% of use cases

### Workflow Example: Hello World
***
# Create Nim version for all targets
kryon new hello_world --template=nim
cd hello_world

# Build for STM32F4 + Linux simultaneously
kryon build --targets=stm32f4,linux

# Deploy Nim version to connected STM32 board
kryon run --target=stm32f4 --device=/dev/ttyUSB0

# Create Lua version for desktop
cd ..
kryon new hello_world_lua --template=lua
cd hello_world_lua
kryon build --target=linux
kryon run --target=linux
***

### Under the Hood: How CLI Integrates Architecture
***
┌─────────────────────┐
│   kryon CLI (Nim)   │ ← User commands
└─────────┬───────────┘
          ↓
┌───────────────────────────────────────┐
│        Build Orchestrator             │
├───────────────────┬───────────────────┤
│  Platform Kits    │  Dependency Mgmt  │
│ (stm32/rp2040/...)│ (SDL3, toolchains)│
└─────────┬─────────┴─────────┬─────────┘
          ↓                   ↓
┌─────────────────────┐ ┌─────────────────────┐
│ Core C Layer        │ │ Renderer Backends   │
│ (kryon.h ABI)       │ │ (framebuffer/sdl3)  │
└─────────────────────┘ └─────────────────────┘
***

**Key Integration Points:**
- **Platform Kits** contain all target-specific knowledge:
  - STM32: ARM GCC toolchain paths, flash scripts, debugger configs
  - Linux: SDL3 static linking flags, package dependencies
  - Web: WASM toolchain, HTML template injectors
- **Dependency Manager** handles:
  - SDL3 prebuilt binaries for all desktop platforms
  - ARM toolchains for MCUs (bundled as .zip in CLI binary)
  - Font assets for different renderers
- **Build Orchestrator** ensures:
  - Core C layer compiled with correct flags per target
  - Frontend-specific compilation (Nim macros/Lua parser)
  - Final binaries stripped/minimized per platform constraints

### CLI Binary Constraints
| Platform    | Max Size | Dependencies | Distribution Method      |
|-------------|----------|--------------|--------------------------|
| Linux x64   | 15MB     | None         | Single static binary     |
| Windows     | 18MB     | None         | .exe + DLLs (self-extracting) |
| macOS       | 16MB     | None         | Universal binary         |
| STM32F4     | 0KB      | N/A          | Not applicable           |

**Achieving Small Size:**
- SDL3 stripped to minimal required symbols
- ARM toolchains compressed with zstd (decompressed on first use)
- No embedded Python/JS runtime — pure Nim implementation
- Font assets only for default theme (6x8 bitmap font)

### Validation Tests for CLI
***
test "CLI creates valid Nim project":
  run("kryon new test_app --template=nim")
  check fileExists("test_app/src/main.nim")
  check fileExists("test_app/kryon.yml")  # Project config

test "CLI creates valid Lua project":
  run("kryon new test_app --template=lua")
  check fileExists("test_app/src/main.lua")
  check fileExists("test_app/kryon.yml")

test "build command respects memory constraints":
  run("kryon build --target=stm32f4")
  let binarySize = getFileSize("build/stm32f4/app.bin")
  check binarySize <= 16384  # 16KB max

test "storage module works across targets":
  run("kryon run --target=stm32f4 --test=storage")
  check storageContains("hello_key")
***

***

## ⚙️ Core Layer Specification (C99 ABI)

### Memory Constraints (NON-NEGOTIABLE)
| Platform    | Max ROM | Max RAM | Heap Usage | Floating Point |
|-------------|---------|---------|------------|----------------|
| STM32F4     | 32KB    | 4KB     | FORBIDDEN  | FORBIDDEN      |
| RP2040      | 64KB    | 8KB     | Optional   | Fixed-point only |
| Linux       | ∞       | ∞       | Allowed    | Allowed        |

### Critical Data Structures (`kryon.h`)
***
// Unified event system
typedef enum { EVT_CLICK, EVT_TOUCH, EVT_KEY, EVT_TIMER } event_type_t;
typedef struct {
  event_type_t type;
  int16_t x, y;          // Coordinates (fixed-point 8.4)
  uint32_t param;        // Key code, timer ID, etc.
} kryon_event_t;

// Component interface (vtable pattern)
typedef struct component_ops {
  void (*render)(void* self, kryon_cmd_buf_t* buf);
  void (*on_event)(void* self, kryon_event_t* evt);
  void (*destroy)(void* self);
} component_ops_t;

typedef struct component {
  component_ops_t* ops;
  void* state;           // Language-specific state pointer
  struct component* parent;
  struct component** children;
  uint8_t child_count;
} component_t;

// Storage API
typedef bool (*storage_set_fn)(const char* key, const void* data, size_t size);
typedef bool (*storage_get_fn)(const char* key, void* buffer, size_t* size);
typedef struct {
  storage_set_fn set;
  storage_get_fn get;
  bool (*has)(const char* key);
} kryon_storage_api_t;

// Command buffer (ring buffer, stack-allocated)
#define CMD_BUF_SIZE 256
typedef struct {
  uint8_t cmds[CMD_BUF_SIZE];
  uint16_t head;
  uint16_t tail;
} kryon_cmd_buf_t;
***

### Core Guarantees (TESTABLE REQUIREMENTS)
1. **No global state** — Pass all context via function parameters
2. **Deterministic layout** — Identical input produces identical output on all platforms
3. **No hidden allocations** — Core functions never call `malloc`/`free`
4. **Fixed-point math** — Layout engine uses 16.16 fixed-point arithmetic (no floats on MCUs)
5. **Event propagation** — Events bubble from leaf to root components
6. **Storage persistence** — Data survives device reboot on capable platforms

***

## 🖼️ Renderer Abstraction

### Primitive Command Set (ALL BACKENDS MUST IMPLEMENT)
***
// Required commands (minimal set)
void kryon_draw_rect(kryon_cmd_buf_t* buf, int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color);
void kryon_draw_text(kryon_cmd_buf_t* buf, const char* text, int16_t x, int16_t y, uint16_t font_id, uint32_t color);
void kryon_draw_line(kryon_cmd_buf_t* buf, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);

// Optional commands (MCUs may stub these)
void kryon_draw_arc(kryon_cmd_buf_t* buf, int16_t cx, int16_t cy, uint16_t radius, 
                   int16_t start_angle, int16_t end_angle, uint32_t color);
void kryon_draw_texture(kryon_cmd_buf_t* buf, uint16_t texture_id, int16_t x, int16_t y);

// Buffer management
void kryon_swap_buffers(kryon_renderer_t* renderer);
***

### Backend Implementation Criteria

#### Framebuffer Backend (MCUs)
**Target Platforms:** STM32F4, RP2040, ESP32  
**Memory Budget:** <8KB ROM, <1KB RAM for renderer  
**Required Features:**
- Direct memory framebuffer access (no GPU)
- 1-bit monochrome font rendering (6x8 pixels minimum)
- Bit-banged SPI/I2C for display communication
- Stack-allocated command buffers only
- Emulated storage using flash memory sectors

**Success Metrics:**
- Draws 100 rectangles at 30fps on STM32F4 @ 80MHz
- Total renderer ROM usage <4KB
- Zero heap allocations during rendering
- Storage survives 10,000 write cycles

#### SDL3 Backend (Desktop/Mobile)
**Target Platforms:** Windows, macOS, Linux, iOS, Android
**Memory Budget:** No hard limits (optimize for performance)
**Required Features:**
- Hardware-accelerated rendering (OpenGL/Vulkan/Metal)
- TTF font loading and Unicode text rendering
- Unified input handling (mouse/touch/keyboard/gamepad)
- Window management (resizing, DPI scaling)
- Filesystem-based storage implementation

**Success Metrics:**
- 60fps rendering with 1000+ UI elements
- Full Unicode text support (including emoji)
- Touch and mouse input working identically
- <50ms input-to-pixel latency
- Storage writes complete in <10ms

#### Terminal Backend (TUI)
**Target Platforms:** Linux, macOS, Windows (WSL/Cygwin)
**Memory Budget:** <1MB typical usage
**Required Features:**
- libtickit-based terminal abstraction with fallback support
- 16/256/24-bit color support with capability detection
- Unicode and UTF-8 character encoding
- Mouse interaction (click, drag, wheel events)
- Terminal control (cursor visibility, title setting, clear screen)
- Dynamic terminal resize handling

**Success Metrics:**
- 1000 rendering operations at 30fps
- Compatible with 14+ terminal emulators (Alacritty, Kitty, GNOME Terminal, etc.)
- Full feature support on modern terminals, graceful degradation on limited terminals
- <200ms startup time to first UI display
- <50ms input-to-display latency
- Zero memory leaks confirmed via comprehensive testing

***

## ✨ Interface Layer: Multi-Language Frontends

### Declarative UI Rules (All Frontends)
1. **No strings for UI structure** — UI declarations must be type-safe in host language
2. **Direct state access** — Application variables accessible in UI callbacks
3. **Compile-time tree construction** — All UI structure known before execution
4. **Zero runtime parser** — No JSON/XML parsing at runtime

### Hello World Example: Nim Frontend
***
import kryon
import std/strutils  # For string utilities

# Application state
var clickCount = 0

# Load count from storage if available
when defined(hasStorage):
  if storageHas("clickCount"):
    clickCount = storageGet("clickCount").parseInt

# UI declaration
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Hello World (Nim)"
  
  Container:
    x = 200
    y = 100
    width = 400
    height = 200
    backgroundColor = "#191970"
    borderColor = "#0099FF"
    borderWidth = 2
    contentAlignment = "center"
    
    Text:
      text = "Hello World from Nim!"
      color = "yellow"
      fontSize = 24
    
    Button:
      text = "Click me! (Count: " & $clickCount & ")"
      onClick = proc() =
        clickCount += 1
        when defined(hasStorage):
          storageSet("clickCount", $clickCount)
      width = 200
      height = 40
      backgroundColor = "#4a90e2"
      textColor = "white"
    
    Canvas:
      width = 300
      height = 100
      onDraw = proc(ctx: DrawingContext) =
        # Draw a simple animated circle
        let time = getTime().toUnix()
        let radius = 40 + sin(time.float * 2.0) * 10.0
        ctx.fillStyle = rgba(255, 100, 100, 200)
        ctx.beginPath()
        ctx.arc(150, 50, radius, 0, 2*PI)
        ctx.fill()
***

### Hello World Example: Lua Frontend
***
local kryon = require("kryon")

-- Application state
local clickCount = 0

-- Load count from storage if available
if kryon.storage.has("clickCount") then
  clickCount = tonumber(kryon.storage.get("clickCount")) or 0
end

-- UI declaration
local app = kryon.app({
  header = {
    width = 800,
    height = 600,
    title = "Hello World (Lua)"
  },
  body = kryon.container({
    x = 200,
    y = 100,
    width = 400,
    height = 200,
    backgroundColor = "#191970",
    borderColor = "#0099FF",
    borderWidth = 2,
    contentAlignment = "center",
    children = {
      kryon.text({
        text = "Hello World from Lua!",
        color = "yellow",
        fontSize = 24
      }),
      kryon.button({
        text = string.format("Click me! (Count: %d)", clickCount),
        onClick = function()
          clickCount = clickCount + 1
          if kryon.storage then
            kryon.storage.set("clickCount", tostring(clickCount))
          end
        end,
        width = 200,
        height = 40,
        backgroundColor = "#4a90e2",
        textColor = "white"
      }),
      kryon.canvas({
        width = 300,
        height = 100,
        onDraw = function(ctx)
          -- Draw a simple animated circle
          local time = os.time()
          local radius = 40 + math.sin(time * 2) * 10
          ctx:setFillColor(255, 100, 100, 200)
          ctx:beginPath()
          ctx:arc(150, 50, radius, 0, math.pi * 2)
          ctx:fill()
        end
      })
    }
  })
})

kryon.run(app)
***

### Implementation Criteria for Language Bindings
- **Nim Binding:**
  - Macro expansion time <100ms for 1000-line UI file
  - Zero overhead calls to core C functions
  - Type safety enforced at compile time
  - Storage module accessible via `storageSet`/`storageGet`

- **Lua Binding:**
  - C module compiles as Lua 5.4 rock
  - <50KB RAM overhead for Lua VM on desktop
  - Storage API exposed as `kryon.storage` table
  - Canvas API mirrors HTML5 Canvas for familiarity
  - No global pollution (all functions under `kryon` namespace)

***

## 🧪 Validation Tests (MUST PASS BEFORE MERGE)

### Core Layer Tests
***
test "component tree lifecycle":
  let root = newContainer()
  let btn = newButton("Click me")
  root.addChild(btn)
  
  check root.childCount == 1
  check btn.parent == root
  
  destroyComponent(root)
  # No memory leaks detected

test "storage persistence":
  storageSet("test_key", "test_value")
  var buffer: array[20, char]
  var size = 20
  check storageGet("test_key", addr(buffer), addr(size))
  check $buffer == "test_value"
***

### Renderer Tests
***
test "framebuffer draws rectangle":
  var fb: Framebuffer = createTestFramebuffer(320, 240)
  drawRect(fb, 10, 20, 100, 50, 0xFF0000)
  
  check fb.pixel(10, 20) == 0xFF0000
  check fb.pixel(109, 69) == 0xFF0000
  check fb.pixel(110, 70) == 0x000000  # Outside rect

test "canvas draws circle identically":
  let nimCanvas = createCanvas(100, 100)
  drawCircle(nimCanvas, 50, 50, 30, 0x00FF00)
  
  local luaCanvas = kryon.createCanvas(100, 100)
  luaCanvas:arc(50, 50, 30, 0, 2*math.pi)
  luaCanvas:setFillColor(0, 255, 0)
  luaCanvas:fill()
  
  check compareCanvases(nimCanvas, luaCanvas) == true
***

### Language Binding Tests
***
test "Nim storage works after reboot":
  storageSet("hello", "nim")
  simulateReboot()
  check storageGet("hello") == "nim"

test "Lua UI structure matches Nim":
  let nimTree = kryonApp: Container: Button: text="Test"
  local luaTree = kryon.app({body = kryon.container({children = {kryon.button({text="Test"})}})})
  
  # Compare serialized component trees
  check serialize(nimTree) == serializeLua(luaTree)
***

***

## ⚡ Build System Requirements

### Unified Build Command
***
# Build Nim version for STM32
kryon build --app=examples/hello_world/nim --target=stm32f4

# Build Lua version for Linux
kryon build --app=examples/hello_world/lua --target=linux
***

### Output Specifications
| Target      | Output Type | Max Size | Required Files             |
|-------------|-------------|----------|----------------------------|
| STM32F4     | .bin        | 16KB     | firmware.bin               |
| Linux (Nim) | .elf        | 200KB    | hello_world                |
| Linux (Lua) | .elf + .lua | 350KB    | hello_world, main.lua      |
| Terminal    | .elf        | 500KB    | hello_world, libtickit.so  |

### Dependency Management Rules
- **MCU targets:** Zero external dependencies (all code in repo)
- **Desktop Nim targets:** SDL3 statically linked
- **Desktop Lua targets:** Lua 5.4 runtime + SDL3 dynamically loaded
- **Terminal targets:** libtickit, ncurses, terminfo (system packages)
- **All targets:** No external JS frameworks

***

## 🚀 Implementation Roadmap (PHASED APPROACH)

### Phase 1: Foundation (Week 1-2)
**Goal:** Hello World on STM32F4 and Linux framebuffer  
**Acceptance Criteria:**
- [ ] Core C API compiles with -nostdlib for STM32
- [ ] `framebuffer` backend draws container + text on ILI9341 LCD
- [ ] Identical C code draws UI in Linux /dev/fb0
- [ ] Total ROM usage <8KB on STM32
- [ ] Zero heap allocations in rendering path

### Phase 2: CLI & Multi-Language (Week 3-4)
**Goal:** Kryon CLI with Nim/Lua support  
**Acceptance Criteria:**
- [ ] `kryon new` creates both Nim and Lua project templates
- [ ] CLI builds Nim version for STM32
- [ ] CLI builds Lua version for Linux desktop
- [ ] Storage module works identically in both frontends
- [ ] CLI binary size <15MB

### Phase 3: Canvas & Advanced Features (Week 5-6)
**Goal:** Full canvas support in both frontends  
**Acceptance Criteria:**
- [ ] Canvas drawing context implemented for framebuffer backend
- [ ] Nim and Lua canvas APIs produce identical output
- [ ] Storage persistence survives reboot on STM32
- [ ] Animation works at 30fps on STM32

### Phase 4: Mobile & Web (Week 7-8)
**Goal:** Hello World on iOS/Android and Web  
**Acceptance Criteria:**
- [ ] SDL3 backend renders UI on Android/iOS
- [ ] Web target generates static HTML + WASM module
- [ ] Touch events work on mobile
- [ ] Total Web bundle <50KB

***

## 📏 Success Metrics (QUANTIFIABLE)

| Metric                          | Target Value       | Measurement Method               |
|---------------------------------|--------------------|----------------------------------|
| STM32 ROM usage (hello world)   | <8KB               | arm-none-eabi-size               |
| Linux binary size (Nim static)  | <200KB             | strip + stat                     |
| Lua VM overhead (desktop)       | <50KB RAM          | valgrind --tool=massif           |
| Terminal binary size            | <500KB             | strip + stat                     |
| Frame time (STM32, 5 elements)  | <33ms (30fps)      | Logic analyzer on GPIO pin       |
| Frame time (Terminal, 20 elems) | <33ms (30fps)      | time command + renderer profiling |
| Storage write time (STM32)      | <5ms               | Timer peripheral measurement     |
| Storage write time (Terminal)   | <10ms              | time command + filesystem timing |
| CLI startup time                | <50ms              | time kryon --version             |
| Terminal compatibility          | 14+ emulators      | automated compatibility matrix   |

***

## 🛑 Hard Constraints (NON-NEGOTIABLE)

1. **No heap in core** — Core layer must function without malloc/free
2. **No global state** — All context passed explicitly via pointers
3. **No floating point in MCU build** — Use fixed-point macros:
   ***
   #define FP_16_16(value) ((int32_t)((value) * 65536.0f))
   #define FP_TO_INT(value) ((value) >> 16)
   ***
4. **No external dependencies in core** — `kryon.h` must compile standalone
5. **ABI stability** — Core C interface frozen after v1.0
6. **Storage API consistency** — Same keys/values across all platforms

## 🚀 Recent Improvements (v0.2.1+)

### Window Configuration
- **Fixed SDL3 backend** to properly use DSL-configured window properties
- **Window dimensions** (`windowWidth`, `windowHeight`) now correctly set window size
- **Window title** (`windowTitle`) now properly displayed in application title bar
- **No more unwanted fallbacks** - removed automatic fallback to framebuffer renderer

### Layout System Enhancements
- **Fixed Center component** to properly center on both X and Y axes
- **Added width flex support** in column layout for proper container expansion
- **Body component** now uses `flexGrow = 1` by default to fill available space
- **Responsive layouts** work correctly across different window sizes

### Button Component Improvements
- **Default text centering** - Button text is now centered both horizontally and vertically
- **Default borders** - All buttons now have a 1px light gray border by default
- **Better visual hierarchy** - Improved contrast between button background and border
- **Professional appearance** - Buttons now look polished out of the box

### Development Experience
- **VSCode syntax highlighting** - Full support for `kryon_dsl` module in VSCode
- **IntelliSense support** - Proper module resolution and code completion
- **Development tasks** - Added `nimble dev_setup`, `nimble dev_install`, `nimble check_syntax`
- **IDE configuration** - VSCode settings automatically configure Nim language server
- **Documentation** - Added `DEVELOPMENT.md` with setup instructions

### Build System
- **Better organization** - Improved build artifact management
- **Cleaner project structure** - Enhanced module organization and path resolution
- **Streamlined development** - Simplified setup process for new contributors

### Technical Details
- **Module resolution**: Fixed `import kryon_dsl` to work in IDE and CLI
- **Path configuration**: Added `--path:bindings/nim` to compiler settings
- **Nimble integration**: Enhanced package configuration for development
- **Error handling**: Improved build error messages and troubleshooting guidance

***

## 📜 License (0BSD)

***
Copyright (C) 2023 Kryon Framework Authors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
***

***

**First Action Item:**  
Implement `renderers/framebuffer/fb_backend.c` with ONLY:
- `kryon_fb_init(framebuffer_ptr, width, height)`
- `kryon_draw_rect(x, y, w, h, color)`
- `kryon_swap_buffers()`

**Validation Test:**  
Run on STM32F4 Discovery board with ILI9341 LCD. Must draw blue rectangle at (50,50,200,100) using <2KB ROM.

**Deadline:** 3 days from project start

> "Measure twice, cut once. The framework's true test isn't how much it can do on a desktop — it's how little it needs to run on a $0.50 microcontroller."
```