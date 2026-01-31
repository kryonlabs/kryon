# Kryon

**Cross-platform UI framework** - Write once, deploy to Web, Desktop, Mobile, and more.

## Quick Start

```bash
# Install
make && make install

# Build for web
kryon build --target web examples/kry/hello_world.kry

# Build for desktop (Tcl/Tk)
kryon build --target tcl examples/kry/hello_world.kry

# Run with hot reload
kryon dev examples/kry/hello_world.kry
```

## Supported Languages

### Source Parsers

| Language | Status | Input | Output |
|----------|--------|-------|--------|
| KRY | ✅ | `.kry` | KIR |
| HTML | ✅ | `.html` | KIR |
| C | ✅ | `.c` | KIR |
| Tcl | ✅ | `.tcl` | KIR |
| Markdown | ✅ | `.md` | KIR |

### Code Generators

| Target | Status | Output | Use Case |
|--------|--------|--------|----------|
| Web | ✅ | HTML/CSS/JS | Browser apps |
| Tcl/Tk | ✅ | `.tcl` | Desktop GUI |
| C | ✅ | `.c` | Native apps |
| Limbo | ✅ | `.b` | TaijiOS Inferno |
| KRY | ✅ | `.kry` | Round-trip |
| Markdown | ✅ | `.md` | Documentation |
| DIS | ✅ | `.dis` | TaijiOS VM |

## Architecture

```
Source (.kry, .html, .c, .tcl)
    ↓
Parser (ir/parsers/)
    ↓
KIR (JSON Intermediate Representation)
    ↓
Code Generator (codegens/)
    ↓
Target (Web, Tcl, C, Limbo, etc.)
```

## Directory Structure

```
kryon/
├── ir/                     # Intermediate Representation
│   ├── src/                # Core IR implementation
│   ├── include/            # Public headers
│   └── parsers/            # Source language parsers
│       ├── kry/            # KRY DSL parser + expression transpiler
│       ├── html/           # HTML parser
│       ├── c/              # C parser
│       ├── tcl/            # Tcl parser
│       └── markdown/       # Markdown parser
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

- **Web target**: Modern web browser (no dependencies)
- **Tcl/Tk target**: `sudo apt install tcl tk` (Ubuntu/Debian)
- **Limbo target**: TaijiOS with Limbo compiler

## Building

```bash
make && make install
```

## Usage

```bash
# Build for different targets
kryon build --target web examples/kry/hello_world.kry
kryon build --target tcl examples/kry/hello_world.kry
kryon build --target limbo examples/kry/hello_world.kry

# Run with dev server (hot reload)
kryon dev examples/kry/hello_world.kry

# Run directly (for tcl target)
kryon run --target tcl examples/kry/hello_world.kry
```

## Configuration

Create `kryon.toml` in your project:

```toml
[project]
name = "myapp"
version = "0.1.0"

[build]
targets = ["web", "tcl"]
entry = "main.kry"

[dev]
hot_reload = true
port = 3000
```

## License

0BSD
