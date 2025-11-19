# Kryon Development Guide

## Getting Started

### Prerequisites
*   Nim compiler >= 2.0.0
*   C compiler (GCC/Clang)
*   SDL3 (for desktop backend)

### Setup
1.  Clone the repository.
2.  Run `nimble install --depsOnly`.

## Building

### C Core
The C core can be built independently:
```bash
cd core
make static
```

### Nim Examples
Run examples using Nimble:
```bash
nimble examples
```

## Project Structure

*   **Core**: `core/` - Pure C implementation.
*   **Bindings**: `bindings/nim/` - Nim DSL and wrappers.
*   **Renderers**: `renderers/` - Backend implementations.

## Contributing

### Adding a New Component
1.  Define the component state struct in `core/include/kryon.h`.
2.  Implement component operations in `core/components.c`.
3.  Add Nim bindings in `bindings/nim/core_kryon.nim`.
4.  Create a DSL macro in `bindings/nim/kryon_dsl/components/`.

### Adding a New Backend
1.  Implement the renderer interface defined in `core/include/kryon.h`.
2.  Place your implementation in `renderers/<backend_name>/`.
3.  Update the build system to include your backend.
