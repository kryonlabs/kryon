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

## Multi-Language Scripting

Kryon supports multiple scripting languages for event handlers and functions:

### Supported Languages

- **Native Kryon** (default) - Built-in scripting language for fast, direct state manipulation
- **rc shell** - Plan 9/Inferno rc shell for system integration and shell utilities

### Using rc Shell

Specify the language before the function name:

```kry
function "rc" handleClick() {
    echo Button clicked!
}
```

Access Kryon variables from rc shell:
```kry
var count = 0

function "rc" increment() {
    count=`{kryonget count}
    count=`{expr $count + 1}
    kryonset count $count
}
```

### Built-in Commands

When using rc shell functions, these commands interact with Kryon state:
- **`kryonget varname`** - Get the value of a Kryon variable
- **`kryonset varname value`** - Set a Kryon variable to a value

### Running rc Shell (TaijiOS)

Kryon uses the Inferno emulator to execute rc shell scripts:
```bash
emu -r. dis/sh.dis
```

This is configured automatically in the Kryon runtime. The rc shell provides access to system commands, file operations, and shell utilities within your Kryon applications.

### When to Use Each Language

**Use Native Kryon for:**
- Simple application logic
- Performance-critical operations
- Direct state manipulation
- UI state updates

**Use rc shell for:**
- System command integration
- File system operations
- Text processing (grep, sed, awk)
- Integration with TaijiOS/Inferno services
- Existing shell scripts

For more details, see:
- [KRY Language Spec - Multi-Language Functions](docs/KRY_LANGUAGE_SPEC.md#multi-language-function-support)
- [RC Shell Guide](docs/RC_SHELL_GUIDE.md)

### Example Applications

```bash
# rc shell demo - counter and file operations
./scripts/run_example.sh rc_shell_demo raylib

# Mixed languages - compare native vs rc shell
./scripts/run_example.sh mixed_languages raylib
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
