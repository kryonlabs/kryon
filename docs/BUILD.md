# Kryon Build Guide

Complete guide to building Kryon in all supported configurations.

## Table of Contents

- [Quick Start](#quick-start)
- [Build Modes Overview](#build-modes-overview)
- [Standard Linux Build](#standard-linux-build)
- [Inferno Build](#inferno-build)
- [TaijiOS Build](#taijios-build)
- [Development Environment](#development-environment)
- [Troubleshooting](#troubleshooting)
- [Build System Architecture](#build-system-architecture)

---

## Quick Start

```bash
# Clone repository
git clone https://github.com/kryonlabs/kryon.git
cd kryon

# Choose your build mode:

# Option 1: Standard Linux (no external dependencies)
make

# Option 2: Inferno (recommended for development with rc shell)
make -f Makefile.inferno

# Option 3: TaijiOS (native Plan 9 environment)
make -f Makefile.taijios
# or
mk
```

---

## Build Modes Overview

| Mode | Makefile | Dependencies | Output | Use Case |
|------|----------|--------------|--------|----------|
| **Standard Linux** | `Makefile` | SDL2, Raylib, OpenGL | `build/bin/kryon` | Standalone Linux app |
| **Inferno** | `Makefile.inferno` | Inferno, lib9 | `build/bin/kryon` | Linux + rc shell + Plan 9 integration |
| **TaijiOS** | `Makefile.taijios` / `mkfile` | TaijiOS, libinterp, lib9 | `kryon-taijios` | Native TaijiOS emu environment |

---

## Standard Linux Build

### Prerequisites

**System packages (Debian/Ubuntu):**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gcc \
    make \
    pkg-config \
    libsdl2-dev \
    libgl1-mesa-dev \
    libx11-dev \
    libxcursor-dev \
    libxi-dev \
    libxrandr-dev \
    libfreetype6-dev \
    libfontconfig1-dev
```

**System packages (Fedora/RHEL):**
```bash
sudo dnf install -y \
    gcc \
    make \
    pkg-config \
    SDL2-devel \
    mesa-libGL-devel \
    libX11-devel \
    libXcursor-devel \
    libXi-devel \
    libXrandr-devel \
    freetype-devel \
    fontconfig-devel
```

**System packages (Arch):**
```bash
sudo pacman -S \
    base-devel \
    sdl2 \
    mesa \
    libx11 \
    libxcursor \
    libxi \
    libxrandr \
    freetype2 \
    fontconfig
```

### Build Commands

```bash
# Standard build
make

# Clean build
make clean
make

# Verbose output
make VERBOSE=1

# Debug build
make DEBUG=1

# Install (optional)
sudo make install
```

### What Gets Built

- **Compiler:** Converts `.kry` files to `.krb` binary format
- **Runtime:** Executes Kryon applications
- **Renderers:** Raylib and SDL2 rendering backends
- **CLI:** Command-line interface for compilation and execution

### Features Included

- Element system (Button, Text, Container, etc.)
- Event handling
- Raylib and SDL2 rendering
- Native Kryon scripting

### Features NOT Included

- rc shell scripting
- Plan 9/Inferno integration
- Inferno namespace support

---

## Inferno Build

### Why Use Inferno Build?

The Inferno build adds:
- **rc shell scripting** - Write event handlers in Plan 9's rc shell
- **lib9 integration** - Plan 9 type system and utilities
- **Inferno services** - Access to Inferno's namespace and devices
- **kryonget/kryonset** - Bridge between rc shell and Kryon state

### Prerequisites

**1. Install System Dependencies:**

See [Standard Linux Build](#standard-linux-build) prerequisites.

**2. Install Inferno:**

```bash
# Clone Inferno
git clone https://github.com/inferno-os/inferno-os.git /opt/inferno
cd /opt/inferno

# Build Inferno
./makemk.sh
mk install

# Verify installation
/opt/inferno/Linux/amd64/bin/emu -h
```

**Alternative Inferno locations:**

The build system auto-detects Inferno in these locations:
- `/opt/inferno`
- `/usr/local/inferno`
- `~/inferno`
- Environment variable `$INFERNO`

### Build Commands

```bash
# Auto-detect Inferno and build
make -f Makefile.inferno

# Specify Inferno location explicitly
INFERNO=/path/to/inferno make -f Makefile.inferno

# Clean build
make -f Makefile.inferno clean
make -f Makefile.inferno

# Debug build
make -f Makefile.inferno DEBUG=1
```

### What's Different from Standard Build?

**Additional includes:**
- `$INFERNO/include` - lib9 headers (lib9.h, libc.h)

**Additional libraries:**
- `lib9` - Plan 9 standard library
- `libinterp` - Inferno interpreter (optional)

**Additional features:**
- Inferno plugin enabled
- rc shell function support
- kryonget/kryonset commands

### Verifying Inferno Build

```bash
# Run an rc shell example
./build/bin/kryon run examples/rc_shell_demo.kry --renderer raylib

# The example should execute rc shell functions
# Look for "Using rc shell" in debug output
```

### lib9.h Overview

lib9.h provides Plan 9/Inferno compatibility:

**Types:**
- `vlong`, `uvlong` - 64-bit integers
- `u32`, `u64` - Unsigned integers
- `Rune` - Unicode codepoint
- `uchar` - Unsigned char

**Macros:**
- `nil` - NULL pointer
- `nelem(arr)` - Array element count
- `USED(x)` - Mark variable as used

**Functions:**
- String manipulation (strcpy, strlen variants)
- Memory management (malloc, free wrappers)
- UTF-8/Rune conversion

Kryon uses lib9 extensively (78 files include lib9.h).

---

## TaijiOS Build

### Why Use TaijiOS Build?

The TaijiOS build provides:
- **Native Plan 9 environment** - Full Plan 9 namespace and devices
- **mk build system** - Plan 9's make replacement
- **Direct TaijiOS integration** - Access to TaijiOS-specific features
- **emu environment** - Runs inside TaijiOS emulator

### Prerequisites

**1. Install TaijiOS:**

```bash
# Clone TaijiOS
git clone https://github.com/Plan9-Archive/TaijiOS.git ~/Projects/TaijiOS
cd ~/Projects/TaijiOS

# Build TaijiOS (follow TaijiOS instructions)
# This varies by system, see TaijiOS documentation
```

**2. Verify TaijiOS Installation:**

```bash
# Check for mk tool
~/Projects/TaijiOS/Linux/amd64/bin/mk -h

# Check for lib9
ls ~/Projects/TaijiOS/Linux/amd64/lib/lib9.a
```

### Build Commands

**Using Makefile.taijios:**
```bash
# Standard build
make -f Makefile.taijios

# Clean build
make -f Makefile.taijios clean
make -f Makefile.taijios

# Specify TaijiOS location
TAIJIOS_ROOT=/path/to/TaijiOS make -f Makefile.taijios
```

**Using mk directly (recommended):**
```bash
# mk uses the mkfile
mk

# Clean build
mk clean
mk

# Install
mk install
```

### Running in TaijiOS

```bash
# Enter TaijiOS emulator
cd ~/Projects/TaijiOS
emu -r. /bin/sh

# Inside emu:
cd /path/to/kryon
./kryon-taijios compile examples/hello-world.kry -o examples/hello-world.krb
./kryon-taijios run examples/hello-world.krb
```

### TaijiOS-Specific Features

- **Full namespace support** - Mount, bind, etc.
- **Device files** - Access to TaijiOS devices
- **Native rc shell** - No emulation layer needed
- **Plan 9 ports** - All Plan 9 services available

---

## Development Environment

### Using Nix (Recommended)

```bash
# Enter nix-shell (auto-detects available build modes)
nix-shell

# The shell will display:
# - Available build modes
# - Whether TaijiOS/Inferno was detected
# - Environment variables set

# Build in nix-shell
make                        # Standard
make -f Makefile.inferno    # If Inferno detected
make -f Makefile.taijios    # If TaijiOS detected
```

**TaijiOS-only environment:**
```bash
nix-shell shell-taijios.nix
# Minimal environment for TaijiOS development
```

### Using direnv (Optional)

```bash
# Install direnv
# Add to ~/.bashrc or ~/.zshrc:
eval "$(direnv hook bash)"  # or zsh

# Enable for Kryon project
cd kryon
echo "use nix" > .envrc
direnv allow

# Now the environment auto-loads when entering the directory
```

### Build Scripts

```bash
# Use provided build scripts for convenience
./scripts/build.sh              # Standard build
./scripts/build.sh --debug      # Debug build
./scripts/build.sh --clean      # Clean build
```

### Development Tools

**Included in nix-shell:**
- gcc, clang
- make, pkg-config
- gdb, valgrind
- bear (for compile_commands.json)
- clang-tools (clangd, clang-tidy)
- doxygen (documentation)

---

## Troubleshooting

### "lib9.h: No such file or directory"

**Problem:** Build fails with `lib9.h: No such file or directory` errors.

**Root Cause:** Kryon depends on lib9 from either Inferno or TaijiOS.

**Solutions:**

1. **Use Inferno Build (Easiest):**
   ```bash
   # Install Inferno (see above)
   make -f Makefile.inferno
   ```

2. **Use TaijiOS Build:**
   ```bash
   # Install TaijiOS (see above)
   make -f Makefile.taijios
   ```

3. **Install Inferno System-Wide:**
   ```bash
   # If you installed Inferno elsewhere, set INFERNO:
   export INFERNO=/path/to/inferno
   make -f Makefile.inferno
   ```

4. **Remove lib9 Dependency (Not Recommended):**
   - Requires significant refactoring (78 files)
   - Loses Plan 9 integration features
   - Not currently supported

### "Cannot find -l9" or "Cannot find -linterp"

**Problem:** Linker can't find lib9.a or libinterp.a.

**Solution:**
```bash
# Verify library paths
ls $INFERNO/Linux/amd64/lib/lib9.a
ls $INFERNO/Linux/amd64/lib/libinterp.a

# Or for TaijiOS:
ls $TAIJIOS_ROOT/Linux/amd64/lib/lib9.a

# If missing, rebuild Inferno/TaijiOS:
cd $INFERNO
mk clean
mk install
```

### Makefile.inferno Can't Find Inferno

**Problem:** Auto-detection fails even though Inferno is installed.

**Solution:**
```bash
# Set INFERNO environment variable
export INFERNO=/path/to/inferno
make -f Makefile.inferno

# Or pass directly:
INFERNO=/path/to/inferno make -f Makefile.inferno
```

**Add to ~/.bashrc or ~/.zshrc:**
```bash
export INFERNO=/opt/inferno
```

### Build Succeeds but rc Shell Functions Don't Work

**Problem:** Built successfully but rc shell functions fail at runtime.

**Diagnostics:**
```bash
# Check if Inferno plugin is loaded
./build/bin/kryon run examples/rc_shell_demo.kry --renderer raylib --debug

# Look for:
# "Loaded plugin: inferno" - Should appear
# "rc shell function not available" - Indicates plugin not loaded
```

**Solution:**
- Ensure you used `Makefile.inferno` for the build
- Verify `$INFERNO` is set at runtime
- Check that Inferno emu is executable

### mk: command not found (TaijiOS Build)

**Problem:** `mk` command not available.

**Solution:**
```bash
# Add TaijiOS bin to PATH
export TAIJIOS_ROOT=~/Projects/TaijiOS
export PATH="$TAIJIOS_ROOT/Linux/amd64/bin:$PATH"

# Verify
mk -h

# Add to ~/.bashrc or ~/.zshrc:
export TAIJIOS_ROOT=~/Projects/TaijiOS
export PATH="$TAIJIOS_ROOT/Linux/amd64/bin:$PATH"
```

### Compilation Errors in Existing Code

**Problem:** Compilation fails with errors in Kryon source code.

**Diagnostics:**
```bash
# Try clean build
make clean
make

# Check compiler version
gcc --version

# Verify dependencies
pkg-config --modversion sdl2
pkg-config --modversion gl
```

**Common Issues:**
- **Outdated compiler:** Kryon requires C11 support (gcc 5.0+)
- **Missing headers:** Install development packages (see prerequisites)
- **Corrupted build:** Try `make clean && make`

### Runtime Errors

**Problem:** Build succeeds but runtime crashes or errors.

**Diagnostics:**
```bash
# Run with debug output
./build/bin/kryon run examples/hello-world.kry --renderer raylib --debug

# Use valgrind to check for memory issues
valgrind ./build/bin/kryon run examples/hello-world.kry --renderer raylib

# Use gdb for crashes
gdb --args ./build/bin/kryon run examples/hello-world.kry --renderer raylib
```

---

## Build System Architecture

### Makefile Structure

```
Makefile              - Standard Linux build
Makefile.inferno      - Inferno build (extends standard)
Makefile.taijios      - TaijiOS build (independent)
mkfile                - TaijiOS mk build (alternative)
```

### Compilation Flags

**Standard flags (all builds):**
- `-std=c11` - C11 standard
- `-Wall -Wextra` - Warnings enabled
- `-Iinclude` - Include directory

**Debug flags:**
- `-g` - Debug symbols
- `-DDEBUG` - Debug macros

**Release flags:**
- `-O2` - Optimization level 2
- `-DNDEBUG` - Disable asserts

**Inferno-specific:**
- `-I$INFERNO/include` - lib9 headers
- `-L$INFERNO/Linux/amd64/lib` - lib9 libraries
- `-l9 -linterp` - Link lib9 and libinterp

**TaijiOS-specific:**
- `-I$TAIJIOS_ROOT/include` - TaijiOS headers
- `-L$TAIJIOS_ROOT/Linux/amd64/lib` - TaijiOS libraries
- `-l9 -linterp` - Link TaijiOS lib9

### Build Targets

**Common targets (all Makefiles):**
```bash
make                  # Build everything
make clean            # Remove build artifacts
make install          # Install to system (may require sudo)
```

**Additional targets:**
```bash
make test             # Run test suite
make docs             # Generate documentation (Doxygen)
make format           # Format code (clang-format)
```

### Output Structure

```
build/
├── bin/
│   └── kryon         # Main executable
├── lib/              # Static libraries (if any)
├── obj/              # Object files (.o)
└── tmp/              # Temporary build artifacts

# TaijiOS build outputs to project root:
kryon-taijios         # TaijiOS executable
```

### Cross-Compilation

**For TaijiOS on different architectures:**
```bash
# Edit mkfile to change target architecture
# Default: Linux/amd64
# Available: Linux/amd64, Linux/386, etc.
```

---

## Summary

**Quick Decision Guide:**

| If you want... | Use this build |
|----------------|----------------|
| Simple Linux app | `make` |
| rc shell scripting | `make -f Makefile.inferno` |
| Full Plan 9 environment | `make -f Makefile.taijios` |
| Development environment | `nix-shell` then choose build |

**Most Common Workflow:**

```bash
# 1. Enter development environment
nix-shell

# 2. Build with Inferno support
make -f Makefile.inferno

# 3. Run examples
./scripts/run_example.sh rc_shell_demo raylib

# 4. Test changes
make -f Makefile.inferno test
```

For questions or issues, see:
- [README.md](../README.md) - Quick start guide
- [GitHub Issues](https://github.com/kryonlabs/kryon/issues) - Report problems
- [Inferno Documentation](http://www.vitanuova.com/inferno/) - Inferno details
- [Plan 9 Documentation](https://9p.io/plan9/) - Plan 9 details
