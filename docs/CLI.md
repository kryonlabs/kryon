# Kryon CLI Tool

The Kryon CLI provides commands for running and building Kryon applications.

## Installation

```bash
nimble build
```

Creates `./build/bin/kryon` executable.

## Commands

### run - Run an Application

Compile and run a Kryon application:

```bash
./build/bin/kryon run examples/button_demo.nim
```

**Options**:
- `--filename=FILE` (required) - Path to .nim file
- `--renderer=RENDERER` (default: auto) - Backend to use
- `--release` - Optimize for performance
- `--verbose` - Show compilation details

**Examples**:

```bash
# Auto-detect backend
./build/bin/kryon run examples/counter.nim

# Specify backend
./build/bin/kryon run examples/counter.nim --renderer=raylib
./build/bin/kryon run examples/counter.nim --renderer=sdl2
./build/bin/kryon run examples/counter.nim --renderer=skia

# Release mode
./build/bin/kryon run my_app.nim --release
```

### build - Build an Executable

Compile a Kryon application to a standalone executable:

```bash
./build/bin/kryon build --filename=my_app.nim --output=my_app
```

**Options**:
- `--filename=FILE` (required) - Path to .nim file
- `--output=FILE` (optional) - Output executable path
- `--renderer=RENDERER` (default: auto) - Backend to use
- `--release` (default: true) - Optimize for performance
- `--verbose` - Show compilation details

**Examples**:

```bash
# Build with default options
./build/bin/kryon build --filename=my_app.nim

# Custom output path
./build/bin/kryon build --filename=my_app.nim --output=bin/app

# Debug mode
./build/bin/kryon build --filename=my_app.nim --release=false

# Specific backend
./build/bin/kryon build --filename=my_app.nim --renderer=skia
```

### info - Show File Information

Display information about a Kryon file:

```bash
./build/bin/kryon info --filename=examples/counter.nim
```

**Output**:
```
Kryon File Info
────────────────────────────────────────────────────────────
File: examples/counter.nim
Size: 1098 bytes
Renderer: raylib

Elements used:
  Container: 1
  Text: 2
  Button: 2
  onClick handlers: 2
```

### version - Show Version

Display Kryon version:

```bash
./build/bin/kryon version
```

**Output**:
```
Kryon-Nim v0.2.0
A declarative UI framework for Nim

Supported renderers:
  - raylib (desktop, 60 FPS)
  - sdl2 (desktop, cross-platform)
  - skia (desktop, high-quality 2D graphics)
  - html (web, generates HTML/CSS/JS)
  - terminal (coming soon)
```

## Backend Selection

### Auto Detection (Default)

The CLI detects which backend to use based on imports:

```nim
import ../src/backends/integration/raylib      # → raylib
import ../src/backends/integration/sdl2        # → sdl2
import ../src/backends/integration/sdl2_skia   # → skia
import ../src/backends/integration/html        # → html
```

### Manual Selection

Force a specific backend:

```bash
./build/bin/kryon build --filename=my_app.nim --renderer=raylib
./build/bin/kryon build --filename=my_app.nim --renderer=sdl2
./build/bin/kryon build --filename=my_app.nim --renderer=skia
```

## Workflow

### Development

For rapid iteration during development:

```bash
# Edit my_app.nim
./build/bin/kryon run my_app.nim

# Make changes, run again
./build/bin/kryon run my_app.nim
```

### Production

Build optimized executable for distribution:

```bash
./build/bin/kryon build --filename=my_app.nim --output=dist/app
./dist/app
```

## Help

Get help for any command:

```bash
./build/bin/kryon --help
./build/bin/kryon run --help
./build/bin/kryon build --help
```
