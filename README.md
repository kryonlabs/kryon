# Flint UI

Flint is a lightweight C UI component library for embedded applications and
runtime environments. It provides core UI primitives and icon asset management
without external dependencies.

## Layout

- `include/` - public headers for Flint API
- `src/` - C implementation files
- `icons/` - icon bitmaps embedded by the Flint icon asset source
- `scripts/` - build helpers and asset embedding tools
- `examples/` - example programs demonstrating Flint usage

## Building

Flint can be built directly or integrated as part of a larger project:

```bash
# Standalone build
make

# Or via CMake (for Android/embedded platforms)
cmake -B build
cmake --build build
```

## Integration

To integrate Flint into your project:

1. Add `include/` to your header search paths
2. Compile the `src/` files and link with your application
3. Use the `scripts/` tools to embed custom icon assets if needed

## Documentation

- `docs/API.md` documents the public API.
- `docs/AGENTS.md` documents how agents and downstream apps should use Flint,
  including modal/input capture and submodule update rules.
