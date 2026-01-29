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

## Build Modes

Kryon supports three build configurations depending on your needs and available dependencies:

### 1. Standard Linux Build (Default)

The standard build uses native Linux libraries for graphics and UI.

**Dependencies:**
- SDL2, Raylib, OpenGL
- Standard C development tools (gcc, make, pkg-config)
- X11 libraries

**Command:**
```bash
make
```

**Output:** `build/bin/kryon`

**Use when:** You want a standalone Linux application without Plan 9/Inferno features.

### 2. Inferno Build (Recommended for Development)

Links against Inferno's lib9 and enables the Inferno plugin for sh (Inferno shell) support.

**Dependencies:**
- Inferno installation (auto-detected in common locations)
- lib9 (from Inferno)
- All Standard Linux dependencies

**Command:**
```bash
make -f Makefile.inferno
```

**Output:** `build/bin/kryon` (with Inferno services)

**Use when:** You want sh (Inferno shell) scripting and Plan 9 integration on standard Linux.

**Installing Inferno:**
```bash
# Download and build Inferno
git clone https://github.com/inferno-os/inferno-os.git /opt/inferno
cd /opt/inferno
./makemk.sh
mk install
```

### 3. Native TaijiOS Build

Builds for the TaijiOS environment using the mk build system.

**Dependencies:**
- TaijiOS installation at `/home/wao/Projects/TaijiOS`
- libinterp, lib9 (from TaijiOS)
- mk build tool

**Commands:**
```bash
make -f Makefile.taijios
# Or using mk directly:
mk
```

**Output:** `kryon-taijios`

**Use when:** You're running in the TaijiOS emu environment and want full Plan 9 namespace/device file access.

**Installing TaijiOS:**
```bash
git clone https://github.com/Plan9-Archive/TaijiOS.git ~/Projects/TaijiOS
cd ~/Projects/TaijiOS
# Follow TaijiOS build instructions
```

### Troubleshooting

**Error: "lib9.h: No such file or directory"**

This means the build system can't find lib9. Solutions:

1. Use the Inferno build: `make -f Makefile.inferno`
2. Install Inferno (see above)
3. Or install TaijiOS and use: `make -f Makefile.taijios`

**Which build mode should I use?**

- **New to Kryon?** Start with Standard Linux build (`make`)
- **Want sh (Inferno shell) features?** Use Inferno build (`make -f Makefile.inferno`)
- **Working with TaijiOS?** Use TaijiOS build (`make -f Makefile.taijios`)

For detailed build instructions, see [docs/BUILD.md](docs/BUILD.md).

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
- **sh (Inferno shell)** - Plan 9/Inferno sh (Inferno shell) for system integration and shell utilities

### Using rc Shell

Specify the language before the function name:

```kry
function "sh" handleClick() {
    echo Button clicked!
}
```

Access Kryon variables from sh (Inferno shell):
```kry
var count = 0

function "sh" increment() {
    count=`{kryonget count}
    count=`{expr $count + 1}
    kryonset count $count
}
```

### Built-in Commands

When using sh (Inferno shell) functions, these commands interact with Kryon state:
- **`kryonget varname`** - Get the value of a Kryon variable
- **`kryonset varname value`** - Set a Kryon variable to a value

### Running rc Shell (TaijiOS)

Kryon uses the Inferno emulator to execute sh (Inferno shell) scripts:
```bash
emu -r. dis/sh.dis
```

This is configured automatically in the Kryon runtime. The sh (Inferno shell) provides access to system commands, file operations, and shell utilities within your Kryon applications.

### When to Use Each Language

**Use Native Kryon for:**
- Simple application logic
- Performance-critical operations
- Direct state manipulation
- UI state updates

**Use sh (Inferno shell) for:**
- System command integration
- File system operations
- Text processing (grep, sed, awk)
- Integration with TaijiOS/Inferno services
- Existing shell scripts

For more details, see:
- [KRY Language Spec - Multi-Language Functions](docs/KRY_LANGUAGE_SPEC.md#multi-language-function-support)
- [Inferno Shell (sh) Guide](docs/SH_LANGUAGE_GUIDE.md)

### Example Applications

```bash
# sh (Inferno shell) demo - counter and file operations
./scripts/run_example.sh rc_shell_demo raylib

# Mixed languages - compare native vs sh (Inferno shell)
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

**Quick Start:**
- [docs/KRYON_OVERVIEW.md](docs/KRYON_OVERVIEW.md) - System architecture
- [docs/KRY_LANGUAGE_SPEC.md](docs/KRY_LANGUAGE_SPEC.md) - KRY language guide
- [docs/KRL_QUICKSTART.md](docs/KRL_QUICKSTART.md) - KRL (Lisp) alternative syntax

**Full Documentation Index:** [docs/README.md](docs/README.md)

**Key Topics:**
- Language specs (KRY, KRL, Inferno Shell (sh))
- Compilation pipeline (KIR, KRB formats)
- Plugin development

## License

0BSD License - see [LICENSE](LICENSE) file for details.
