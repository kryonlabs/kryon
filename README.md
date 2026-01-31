# Kryon

**Multi-language UI framework** - Write declarative UI once, deploy to Web, Desktop, Mobile, and more.

## Quick Start

```bash
# Build
make && make install

# Run example
kryon run --target=limbo examples/kry/hello_world.kry
kryon run --target=tcl+tk examples/kry/hello_world.kry
```

## Targets

| Target | Language | Toolkit | Description |
|--------|----------|---------|-------------|
| `limbo+draw` | Limbo | Draw | TaijiOS Inferno |
| `tcl+tk` | Tcl | Tk | Desktop GUI |
| `c+sdl3` | C | SDL3 | Native desktop |
| `web` | JavaScript | DOM | Browser apps |

```bash
# Explicit syntax (recommended)
kryon run --target=limbo+draw main.kry

# Short aliases
kryon run --target=limbo main.kry     # → limbo+draw
kryon run --target=tcltk main.kry     # → tcl+tk
```

## Directory Structure

```
kryon/
├── ir/              # Intermediate Representation
├── codegens/        # Code generators
│   ├── languages/   # Language emitters
│   └── toolkits/    # Toolkit profiles
├── cli/             # Command-line interface
├── runtime/         # Runtime libraries
└── examples/        # Examples
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed architecture.

## Building

```bash
make && make install
```

## Usage

```bash
# Build and run
kryon build --target=limbo+draw main.kry
kryon run --target=limbo+draw main.kry

# List all targets
kryon targets

# Dev server with hot reload
kryon dev main.kry
```

## License

MIT
