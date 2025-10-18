# Backend System

## Available Backends

### Raylib
- **Windowing**: Raylib
- **Rendering**: Raylib native renderer
- **Usage**: `--renderer=raylib`
- **Best for**: Simple applications, rapid prototyping

### SDL2
- **Windowing**: SDL2
- **Rendering**: SDL2_Renderer
- **Usage**: `--renderer=sdl2`
- **Best for**: Cross-platform applications with hardware acceleration

### SDL2 + Skia
- **Windowing**: SDL2
- **Rendering**: Skia graphics library
- **Usage**: `--renderer=skia`
- **Best for**: High-quality graphics, anti-aliased rendering

### HTML
- **Windowing**: Browser window
- **Rendering**: DOM/Canvas API
- **Usage**: `--renderer=html`
- **Best for**: Web applications

## Renderer Interface

All backends implement these 7 primitives:

```nim
proc drawRectangle(backend, x, y, width, height: float, color: Color)
proc drawRectangleBorder(backend, x, y, width, height, borderWidth: float, color: Color)
proc drawText(backend, text: string, x, y: float, fontSize: int, color: Color)
proc drawLine(backend, x1, y1, x2, y2, thickness: float, color: Color)
proc beginClipping(backend, x, y, width, height: float)
proc endClipping(backend)
proc measureText(backend, text: string, fontSize: int): tuple[width, height: float]
```

## Installation

### Raylib Backend

```bash
# Ubuntu/Debian
sudo apt install libraylib-dev

# macOS
brew install raylib

# Or use Nix
nix-shell
```

### SDL2 Backend

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev

# macOS
brew install sdl2 sdl2_ttf

# Or use Nix
nix-shell
```

### Skia Backend

```bash
# Ubuntu/Debian (if available)
sudo apt install libskia-dev

# macOS
brew install skia

# Build from source
git clone https://skia.googlesource.com/skia.git
cd skia
python tools/git-sync-deps
bin/gn gen out/Shared --args='is_official_build=true'
ninja -C out/Shared
```

## Usage

### In Code

```nim
import ../src/kryon
import ../src/backends/integration/raylib  # or sdl2, sdl2_skia, html

let app = kryonApp:
  # ... your UI ...

when isMainModule:
  var backend = newRaylibBackendFromApp(app)  # or newSDL2BackendFromApp, etc.
  backend.run(app)
```

### With CLI

```bash
# Auto-detect from imports
./build/bin/kryon build --filename=app.nim

# Specify backend
./build/bin/kryon build --filename=app.nim --renderer=raylib
./build/bin/kryon build --filename=app.nim --renderer=sdl2
./build/bin/kryon build --filename=app.nim --renderer=skia
```

## Comparison

| Feature | Raylib | SDL2 | Skia | HTML |
|---------|--------|------|------|------|
| **Setup** | Easy | Medium | Hard | N/A |
| **Dependencies** | 1 | 2 | 2 | 0 |
| **Anti-aliasing** | Basic | Basic | Excellent | Good |
| **Performance** | Fast | Fast | Good | Varies |
| **Platform** | Desktop | Desktop | Desktop | Web |
| **Font Quality** | Good | Good | Excellent | Good |

## Creating Custom Backends

To create a new backend combination:

1. Create integration file: `src/backends/integration/my_backend.nim`
2. Import windowing and rendering FFI
3. Implement renderer interface (7 primitives)
4. Implement event handling
5. Implement `run()` procedure
6. Update CLI to recognize new backend

Example structure:

```nim
import ../../kryon/core
import ../windowing/glfw/glfw_ffi
import ../rendering/cairo/cairo_ffi

type
  MyBackend* = object
    window*: ptr GLFWWindow
    context*: ptr CairoContext
    # ... state fields

proc newMyBackendFromApp*(app: Element): MyBackend =
  # ... initialization

proc drawRectangle*(backend: var MyBackend, x, y, width, height: float, color: Color) =
  # ... implementation

# ... implement other 6 primitives

proc run*(backend: var MyBackend, root: Element) =
  # ... main loop
```
