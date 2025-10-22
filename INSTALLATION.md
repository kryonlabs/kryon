# Kryon Installation Guide

This guide covers how to install and use Kryon as a system-wide CLI tool and library, eliminating the need for relative paths and manual configuration.

## Quick Installation

### Option 1: Using Makefile (Recommended)

```bash
# From the Kryon source directory
make install
```

### Option 2: Using Installation Script

```bash
# From the Kryon source directory
./scripts/install.sh
```

### Option 3: NixOS/Nix

```bash
# Using Nix expressions
nix-build
nix-shell  # Development environment
```

## What Gets Installed

The installation process installs:

- **CLI Tool**: `~/.local/bin/kryon` - The command-line interface
- **Library**: `~/.local/lib/libkryon.a` - Static library for linking
- **Headers**: `~/.local/include/kryon/` - Nim source files for development
- **pkg-config**: `~/.local/lib/pkgconfig/kryon.pc` - Build system integration
- **Templates**: `~/.local/config/kryon/templates/` - Project templates
- **Config**: `~/.local/config/kryon/config.yaml` - Installation metadata

## Post-Installation Setup

### 1. Update PATH

Add the bin directory to your shell's PATH:

```bash
# For bash/zsh
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Or for fish shell
echo 'set -x PATH $HOME/.local/bin $PATH' >> ~/.config/fish/config.fish
```

### 2. Verify Installation

```bash
kryon doctor
```

This command checks:
- Nim compiler availability
- Rendering backends (Raylib, SDL2)
- Kryon installation status
- System dependencies

## Usage

### Project Creation

Create new projects from templates:

```bash
# Basic app with button
kryon init --templateName=basic myapp

# Todo list application
kryon init --templateName=todo my-todo

# Simple game template
kryon init --templateName=game mygame

# Calculator application
kryon init --templateName=calculator calc

# Run created project
cd myapp
kryon run main.kry
```

### Running Applications

```bash
# From any directory
kryon run main.kry

# With specific renderer
kryon run main.kry --renderer=raylib
kryon run main.kry --renderer=sdl2
kryon run main.kry --renderer=skia
```

### Building Standalone Executables

```bash
# Build for current platform
kryon build main.kry --output=myapp

# Release build
kryon build main.kry --release --output=myapp

# With specific renderer
kryon build main.kry --renderer=raylib --output=myapp
```

## Available Templates

### Basic Template
- Simple window with button and text
- Demonstrates basic UI components
- Good starting point for learning

### Todo Template
- Functional todo list application
- Shows state management
- Input handling and event processing

### Game Template
- Basic game structure
- Input handling framework
- Animation and state management basics

### Calculator Template
- Complete calculator application
- Complex layout and interaction
- Arithmetic operations and display

## Build Variants

The installation system supports different build variants:

### Dynamic Build (Default)
- Smaller binary size (~1-2MB)
- Uses system libraries
- Requires system dependencies to be installed

### Static Build
- Self-contained binary (~5-10MB)
- All backends compiled in
- No external dependencies needed

```bash
# Install static version
make install-static

# Install dynamic version (default)
make install-dynamic
```

## Backend Dependencies

### Raylib Backend
```bash
# Install via nimble
nimble install raylib

# Or system package (varies by system)
# Ubuntu/Debian: sudo apt install libraylib-dev
# macOS: brew install raylib
```

### SDL2 Backend
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev

# macOS
brew install sdl2 sdl2_ttf

# Windows
# Download from libsdl.org
```

## Development Integration

### Using in Other Projects

For other Nim projects that want to use Kryon as a library:

```nim
# In your project's .nimble file
requires "kryon >= 0.2.0"

# In your Nim code
import kryon
import kryon.integration.raylib_backend

# Use Kryon normally
window("My App", 800, 600):
  text("Hello from Kryon library!")
```

### Using pkg-config

```bash
# Get compilation flags
pkg-config --cflags kryon

# Get linking flags
pkg-config --libs kryon
```

## System Integration

### NixOS Integration

For NixOS users, Kryon provides Nix expressions:

```bash
# Development environment
nix-shell

# Build package
nix-build

# Add to NixOS configuration
# (see nix/overlay.nix for details)
```

### System-Wide Installation

For system-wide installation (requires sudo):

```bash
make install-system PREFIX=/usr/local
```

## Troubleshooting

### Common Issues

**1. Command not found**
```bash
# Check if kryon is in PATH
which kryon

# If not found, add to PATH:
export PATH="$HOME/.local/bin:$PATH"
```

**2. Backend not available**
```bash
# Check system dependencies
kryon doctor

# Install missing dependencies
nimble install raylib  # For Raylib backend
sudo apt install libsdl2-dev  # For SDL2 backend
```

**3. Compilation errors**
```bash
# Use verbose output to see errors
kryon run main.kry --verbose

# Check file paths and imports
kryon info main.kry
```

### Getting Help

```bash
# General help
kryon --help

# Command-specific help
kryon run --help
kryon build --help
kryon init --help
kryon doctor --help

# System health check
kryon doctor
```

### Uninstallation

To completely remove Kryon:

```bash
# Using Makefile
make uninstall

# Or using script
./scripts/uninstall.sh
```

This removes:
- CLI tool from `~/.local/bin/`
- Library files from `~/.local/lib/`
- Headers from `~/.local/include/`
- Configuration from `~/.local/config/kryon/`
- pkg-config file

## File Structure After Installation

```
~/.local/
├── bin/
│   └── kryon                          # CLI tool
├── lib/
│   ├── libkryon.a                     # Static library
│   └── pkgconfig/
│       └── kryon.pc                   # pkg-config file
├── include/
│   └── kryon/                         # Nim headers
│       ├── core.nim
│       ├── dsl.nim
│       └── ...
└── config/
    └── kryon/
        ├── config.yaml                # Installation metadata
        └── templates/                 # Project templates
            ├── basic/
            ├── todo/
            ├── game/
            └── calculator/
```

## Examples

After installation, you can run examples from anywhere:

```bash
# Create a new project
kryon init --templateName=basic hello
cd hello

# Run it
kryon run main.kry

# Build standalone executable
kryon build main.kry --output=hello
./hello
```

The installed Kryon system provides a complete, seamless development experience for Kryon applications without the need for complex path configuration or manual library management.