# Kryon - Unified Cross-Platform UI Framework

This is the unified monorepo for Kryon, containing:

- **Compiler**: KRY language → KRB binary format compilation
- **Renderer**: Multi-backend rendering (Raylib, Ratatui, WGPU)
- **Shared**: Unified type definitions and utilities
- **Examples**: Comprehensive example applications
- **Tools**: Command-line utilities

## Structure

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

## Building

```bash
cargo build --workspace
```

## Running Examples

```bash
cargo run --bin kryc examples/counter.kry
cargo run --bin kryon-renderer-raylib examples/counter.krb
```