# Kryon

UI framework with two-target architecture: Web (HTML/CSS/JS) and DIS VM (TaijiOS bytecode).

## Architecture

Kryon is a simplified cross-platform framework that compiles KRY source code to either:
- **Web**: HTML/CSS/JavaScript for browser deployment
- **DIS VM**: TaijiOS DIS bytecode for native execution

```
Source (.kry)
    ↓
Parser (ir/parsers/kry/)
    ↓
KIR (JSON Intermediate Representation)
    ↓
Code Generator (codegens/dis/ or codegens/web/)
    ↓
Target (DIS bytecode or HTML/CSS/JS)
```

## Features

### Code Generators

| Target | Status | Location | Notes |
|--------|--------|----------|-------|
| DIS (TaijiOS) | ✅ | `codegens/dis/` | Primary target |
| HTML/CSS/JS | ✅ | `codegens/web/` | Full support |
| C | ✅ | `codegens/c/` | For reference |
| Limbo | ✅ | `codegens/limbo/` | TaijiOS Inferno |
| Tcl/Tk | ✅ | `codegens/tcltk/` | Desktop GUI |
| KRY | ✅ | `codegens/kry/` | Round-trip |
| Markdown | ✅ | `codegens/markdown/` | Documentation |

### Expression Transpiler

Converts KRY expressions to target language syntax.

| Target | Status | Notes |
|--------|--------|-------|
| JavaScript | ✅ | ES6 syntax |
| C | ✅ | Arrow functions via registry |

### Runtime Bindings

FFI bindings for using Kryon from other languages.

| Language | Status | Location |
|----------|--------|----------|
| C | ✅ | `bindings/c/` |
| JavaScript | ✅ | `bindings/javascript/` |

## Directory Structure

```
kryon/
├── ir/                     # Intermediate Representation
│   ├── src/                # Core IR implementation
│   ├── include/            # Public headers
│   └── parsers/            # Source language parsers
│       ├── kry/            # KRY DSL parser + expression transpiler
│       ├── html/           # HTML parser
│       └── c/              # C parser
├── codegens/               # Code generators
│   ├── dis/                # DIS bytecode (TaijiOS)
│   ├── web/                # HTML/CSS/JS
│   ├── limbo/              # Limbo source (Inferno)
│   ├── tcltk/              # Tcl/Tk scripts
│   ├── c/                  # C source
│   ├── kry/                # KRY round-trip
│   └── markdown/           # Markdown documentation
├── bindings/               # Language bindings
│   ├── c/                  # C FFI
│   └── javascript/         # JavaScript bindings
├── cli/                    # Command-line tools
├── examples/               # Example applications
└── build/                  # Build output
```

## Requirements

### For Web Target
- No additional requirements
- Modern web browser

### For DIS Target
- **TaijiOS** installation at `/home/wao/Projects/TaijiOS` (or custom path)
- TaijiOS must be built with emu (DIS VM) available

#### Installing TaijiOS

```bash
# Clone TaijiOS repository
git clone https://github.com/taiji-os/TaijiOS.git /home/wao/Projects/TaijiOS
cd /home/wao/Projects/TaijiOS

# Build TaijiOS (follow TaijiOS build instructions)
# Ensure emu binary is available at: Linux/amd64/bin/emu
```

## Building

```bash
# Build everything
make

# Build specific components
make cli       # Build CLI tool only
make ir        # Build IR library only
make codegens  # Build all code generators

# Clean build artifacts
make clean
```

## Usage

### Web Target

```bash
# Build for web
kryon build web

# Run with dev server
kryon dev
```

### DIS Target

```bash
# Build DIS bytecode
kryon build dis

# Run with TaijiOS emu
kryon run dis

# Or run manually with TaijiOS
/home/wao/Projects/TaijiOS/Linux/amd64/bin/emu -r /home/wao/Projects/TaijiOS app.dis

# Using run-app.sh wrapper
/home/wao/Projects/TaijiOS/run-app.sh app.dis
```

### Tcl/Tk Target

```bash
# Build Tcl/Tk script (uses "tcl" alias for convenience)
kryon build --target=tcl examples/kry/hello_world.kry

# Run with wish
wish app.tcl

# Or use the run command
kryon run --target=tcl examples/kry/hello_world.kry
```

**Note:** Tcl/Tk must be installed on your system:
```bash
# Ubuntu/Debian
sudo apt-get install tcl tk

# macOS
brew install tcl-tk
```

## Configuration

Create `kryon.toml` in your project:

```toml
[project]
name = "myapp"
version = "0.1.0"
author = "Your Name"
description = "A Kryon application"

[build]
targets = ["dis", "web"]  # Build both targets
output_dir = "dist"
entry = "main.kry"
frontend = "kry"

[taiji]
path = "/home/wao/Projects/TaijiOS"  # TaijiOS installation path

[dev]
hot_reload = true
port = 3000
auto_open = true
```

## TaijiOS Integration

### DIS Execution

Kryon generates `.dis` files compatible with TaijiOS DIS VM:

- **File Format**: Standard TaijiOS .dis format (header, code, types, data, links)
- **Execution**: Via TaijiOS `emu` or `run-app.sh` wrapper
- **Windowing**: Automatic X11 window creation for UI apps
- **Graphics**: Uses TaijiOS libdraw for rendering

### TaijiOS Path Configuration

Set TaijiOS path in `kryon.toml`:

```toml
[taiji]
path = "/home/wao/Projects/TaijiOS"
```

Or use environment variable:

```bash
export TAIJI_PATH=/home/wao/Projects/TaijiOS
```

## Migration from Old Targets

If you were using desktop, android, or terminal targets:

1. Update `kryon.toml` to use `targets = ["dis"]`
2. Install TaijiOS (see requirements above)
3. Run `kryon build dis` to generate DIS bytecode
4. Run `kryon run dis` to execute with TaijiOS

## License

0BSD
