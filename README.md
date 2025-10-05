# Kryon-C

A UI system implemented in C with the Kryon language for declarative UI development.

## Overview

Kryon-C provides:
- **KRY Language Compiler** - Compile .kry source files to .krb binary format
- **Runtime System** - Execute Kryon applications with element trees
- **CLI Tools** - Command line interface for compilation and execution  
- **UI Elements** - Text, Button, Container, Column, Row, Dropdown, etc.
- **Build Scripts** - Convenient development workflow

## Project Structure

```
kryon-c/
├── src/                    # Source code
│   ├── cli/                # Command line interface
│   ├── compiler/           # KRY → KRB compiler
│   ├── runtime/            # Core runtime and elements
│   ├── core/               # Memory, containers, utilities
│   ├── events/             # Event system
│   ├── renderers/          # Raylib, SDL2, HTML renderers
│   └── shared/             # KRB format utilities
├── examples/               # KRY example files
├── scripts/                # Build and run scripts
├── tests/                  # Test suite
└── docs/                   # Documentation
```

## Building

### Prerequisites
- CMake 3.15+
- GCC or Clang
- Git (for submodules)

### Build Steps
```bash
# Clone and build
git clone https://github.com/kryonlabs/kryon.git
cd kryon
git submodule update --init --recursive

# Build (recommended)
./scripts/build.sh

# Or manually
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Running Examples

```bash
# List all examples
./scripts/run_examples.sh list

# Run a specific example
./scripts/run_example.sh hello-world raylib
./scripts/run_example.sh button raylib

# Run with debug output
./scripts/run_example.sh --debug button raylib

# Run all examples
./scripts/run_examples.sh raylib
```

## Manual Usage

```bash
# Compile a .kry file to .krb binary
./build/bin/kryon compile examples/hello-world.kry -o examples/hello-world.krb

# Run the compiled binary
./build/bin/kryon run examples/hello-world.krb --renderer raylib

# Or compile and run in one step
./build/bin/kryon run examples/hello-world.kry --renderer raylib
```

## Examples

See the [examples/](examples/) directory for KRY application samples. Use the scripts to run them:

```bash
./scripts/run_examples.sh list    # List all examples
./scripts/run_example.sh hello-world raylib
```

## Development

### Running Tests
```bash
cd build
ctest --verbose
```

### Build Scripts
```bash
./scripts/build.sh              # Release build
./scripts/build.sh --debug      # Debug build  
./scripts/build.sh --clean      # Clean build
```

## Documentation

See the `docs/` directory for:
- KRY Language Specification  
- KRB Binary Format
- Architecture Overview

## License

0BSD License - see [LICENSE](LICENSE) file for details.
