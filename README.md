# Kryon - Unified Cross-Platform UI Framework

This is the unified monorepo for Kryon, containing the complete framework for cross-platform UI development:

- **Compiler**: KRY language → KRB binary format compilation  
- **Renderer**: Multi-backend rendering (Raylib, Ratatui, WGPU)
- **Shared**: Unified type definitions and utilities
- **Examples**: Comprehensive example applications
- **Tools**: Command-line utilities

## Architecture

```
kryon/
├── crates/
│   ├── kryon-shared/     # Shared types and utilities
│   ├── kryon-compiler/   # KRY → KRB compilation
│   ├── kryon-renderer/   # Core rendering abstractions
│   ├── kryon-runtime/    # Application runtime
│   ├── kryon-layout/     # Layout engine
│   ├── kryon-render/     # Rendering commands
│   ├── kryon-raylib/     # Raylib backend
│   ├── kryon-ratatui/    # Terminal backend
│   └── kryon-wgpu/       # GPU backend
├── tools/                # CLI tools
├── examples/             # Example applications
└── tests/                # Integration tests
```

## Quick Start

```bash
# Build everything
cargo build --workspace

# Compile and run an example
cargo run --bin kryc examples/counter.kry
cargo run --bin kryon-renderer-raylib examples/counter.krb
```

## Features

- **Multi-Backend Rendering**: WGPU, Ratatui, Raylib support
- **KRY Language**: CSS-like styling with component system
- **Script Integration**: Lua, JavaScript, Python, Wren support
- **Layout Engine**: Flexbox and absolute positioning
- **SVG Support**: Native SVG rendering with caching
- **Transform System**: CSS-like transforms with matrix operations
- **Cross-Platform**: Windows, macOS, Linux, and web browsers

## Building

```bash
# Build entire monorepo
cargo build --workspace

# Build release version
cargo build --release --workspace
```

## Testing

```bash
# Run all tests
cargo test

# Run snapshot tests for terminal rendering
cargo test -p kryon-ratatui
cargo insta review  # Review visual changes

# Run specific test suites
cargo test --test debug_renderer_test
cargo test --test screenshot_test
```

## CLI Tools

The monorepo provides unified command-line tools:

```bash
# Compile KRY to KRB
cargo run --bin kryc input.kry output.krb

# Render with different backends
cargo run --bin kryon-renderer-raylib file.krb
cargo run --bin kryon-renderer-ratatui file.krb  
cargo run --bin kryon-renderer-wgpu file.krb

# Debug KRB files
cargo run --bin kryon-renderer-debug file.krb

# Bundle applications
cargo run --bin kryon-bundle file.krb output-executable
```

## Development

```bash
# Auto-compile and run examples
./run_examples.sh

# Build documentation
cd kryon-docs && mdbook serve

# Performance testing
cargo test -p kryon-ratatui
cargo bench
```
