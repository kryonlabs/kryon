# Kryon CLI Tool

The Kryon CLI provides a convenient way to run and build Kryon applications with support for multiple renderers.

---

## Installation

Build the CLI tool:

```bash
nimble build
```

This creates `./bin/kryon` executable.

---

## Commands

### `runKryon` - Run an Application

Compile and run a Kryon application:

```bash
nix-shell --run "./bin/kryon runKryon --filename=examples/hello_world.nim"
```

**Options**:
- `--filename=FILE` (required) - Path to .nim file to run
- `--renderer=RENDERER` (default: auto) - Renderer to use (auto, raylib, html, terminal)
- `--release` - Compile in release mode (optimized)
- `--verbose` - Show detailed compilation output

**Examples**:

```bash
# Run with autodetected renderer
nix-shell --run "./bin/kryon runKryon --filename=my_app.nim"

# Run with specific renderer
nix-shell --run "./bin/kryon runKryon --filename=my_app.nim --renderer=raylib"

# Run in release mode (faster, but slower to compile)
nix-shell --run "./bin/kryon runKryon --filename=my_app.nim --release"

# Run with verbose output
nix-shell --run "./bin/kryon runKryon --filename=my_app.nim --verbose"
```

### `build` - Build an Executable

Compile a Kryon application to an executable:

```bash
nix-shell --run "./bin/kryon build --filename=my_app.nim --output=my_app"
```

**Options**:
- `--filename=FILE` (required) - Path to .nim file to compile
- `--output=FILE` (optional) - Output executable path (default: same name as input)
- `--renderer=RENDERER` (default: auto) - Renderer to use (auto, raylib, html, terminal)
- `--release` (default: true) - Compile in release mode
- `--verbose` - Show detailed compilation output

**Examples**:

```bash
# Build with default options (release mode)
nix-shell --run "./bin/kryon build --filename=my_app.nim"

# Build with custom output path
nix-shell --run "./bin/kryon build --filename=my_app.nim --output=bin/myapp"

# Build in debug mode
nix-shell --run "./bin/kryon build --filename=my_app.nim --release=false"

# Build for specific renderer
nix-shell --run "./bin/kryon build --filename=my_app.nim --renderer=raylib"
```

### `info` - Show File Information

Display information about a Kryon file:

```bash
nix-shell --run "./bin/kryon info --filename=examples/counter.nim"
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
  Column: 1
  Row: 1
  Center: 1
  onClick handlers: 2
```

### `version` - Show Version

Display Kryon version and supported renderers:

```bash
./bin/kryon version
```

**Output**:
```
Kryon-Nim v0.2.0
A declarative UI framework for Nim

Supported renderers:
  - raylib (desktop, 60 FPS)
  - html (coming soon)
  - terminal (coming soon)
```

---

## Renderer Selection

The CLI supports multiple rendering backends:

### Auto Detection (Default)

By default (`--renderer=auto`), the CLI detects which renderer to use based on imports in your file:

```nim
import backends/raylib_backend  # → raylib renderer
import backends/html_backend    # → html renderer  (coming soon)
import backends/terminal_backend # → terminal renderer (coming soon)
```

### Manual Selection

Force a specific renderer:

```bash
# Force Raylib
nix-shell --run "./bin/kryon build --filename=my_app.nim --renderer=raylib"

# Force HTML (not yet implemented)
nix-shell --run "./bin/kryon build --filename=my_app.nim --renderer=html"
```

---

## Development Workflow

### Quick Iteration

Use `runKryon` for fast development:

```bash
# Edit my_app.nim
# Run it immediately
nix-shell --run "./bin/kryon runKryon --filename=my_app.nim"
```

### Production Builds

Use `build` with release mode for optimized executables:

```bash
# Build optimized executable
nix-shell --run "./bin/kryon build --filename=my_app.nim --output=dist/myapp"

# Run the standalone executable
./dist/myapp
```

---

## Troubleshooting

### "nim: command not found"

The CLI requires Nim to be in PATH. Always run inside `nix-shell`:

```bash
# ❌ Wrong
./bin/kryon build --filename=my_app.nim

# ✅ Correct
nix-shell --run "./bin/kryon build --filename=my_app.nim"
```

### "File not found"

Make sure to provide the full path or relative path:

```bash
# ❌ Wrong
nix-shell --run "./bin/kryon build --filename=my_app"

# ✅ Correct
nix-shell --run "./bin/kryon build --filename=my_app.nim"
nix-shell --run "./bin/kryon build --filename=./examples/counter.nim"
```

### Compilation Errors

Use `--verbose` to see detailed error messages:

```bash
nix-shell --run "./bin/kryon build --filename=my_app.nim --verbose"
```

---

## Advanced Usage

### Shell Alias

Add to your `~/.bashrc` or `~/.zshrc`:

```bash
alias kryon='nix-shell /path/to/kryon-nim --run "./bin/kryon"'
```

Then use it like:

```bash
kryon build --filename=my_app.nim
kryon runKryon --filename=my_app.nim
kryon version
```

### Makefile Integration

```makefile
.PHONY: build run clean

build:
	nix-shell --run "./bin/kryon build --filename=src/main.nim --output=dist/app"

run:
	nix-shell --run "./bin/kryon runKryon --filename=src/main.nim"

clean:
	rm -rf dist/
```

---

## Help

Get help for any command:

```bash
# General help
./bin/kryon --help

# Command-specific help
./bin/kryon runKryon --help
./bin/kryon build --help
./bin/kryon info --help
```

---

**Built with ❤️ using Nim and cligen**
