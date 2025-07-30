# Kryon-C: Complete UI System Implementation in C

A comprehensive, cross-platform UI system implemented in C, featuring the Kryon language for declarative UI development.

## 🚀 **Overview**

Kryon-C is a complete implementation of the Kryon UI system, providing:

- **Kryon Language Compiler** - Compile KRY source to KRB binary format
- **Cross-Platform Runtime** - Execute Kryon applications on Web, Desktop, Mobile, Terminal
- **Multi-Backend Rendering** - WebGL, WGPU, Raylib, Terminal, HTML, Software rendering
- **Multi-Language Scripting** - Lua, JavaScript, Python, Kryon Lisp integration
- **Modern Layout Engine** - Flexbox and CSS Grid support
- **Comprehensive Tooling** - CLI, debugger, profiler, formatter, language server

## 📁 **Project Structure**

```
kryon-c/
├── src/                    # Source code
│   ├── compiler/           # KRY → KRB compiler
│   ├── runtime/            # Core runtime system
│   ├── platform/           # Platform abstraction layer
│   ├── renderers/          # Rendering backends
│   ├── script-engines/     # Multi-language scripting
│   ├── layout/             # Layout engine
│   ├── events/             # Event system
│   ├── network/            # Network operations
│   ├── filesystem/         # File system operations
│   ├── audio/              # Audio system
│   ├── graphics/           # Graphics utilities
│   ├── cli/                # Command line interface
│   └── tools/              # Development tools
├── include/
│   ├── kryon/              # Public API headers
│   └── internal/           # Internal headers
├── tests/                  # Test suite
├── examples/               # Example applications
├── docs/                   # Comprehensive documentation
└── third-party/            # External dependencies
```

## 🛠️ **Building**

### Prerequisites

- **CMake** 3.20 or higher
- **GCC** 9.0+ or **Clang** 10.0+ or **MSVC** 2019+
- **Git** for submodules

### Quick Start

```bash
# Clone repository
git clone https://github.com/kryonlabs/kryon-c.git
cd kryon-c

# Initialize submodules
git submodule update --init --recursive

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run tests
ctest

# Install
cmake --install .
```

### Build Options

```bash
# Development build with all features
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DKRYON_BUILD_TESTS=ON \
      -DKRYON_BUILD_EXAMPLES=ON \
      -DKRYON_BUILD_TOOLS=ON \
      ..

# Production build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DKRYON_BUILD_TESTS=OFF \
      -DKRYON_BUILD_EXAMPLES=OFF \
      ..

# Web build (requires Emscripten)
emcmake cmake -DCMAKE_BUILD_TYPE=Release \
              -DKRYON_RENDERER_WEBGL=ON \
              -DKRYON_RENDERER_HTML=ON \
              ..
```

### Platform-Specific Builds

#### Windows
```bash
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

#### macOS
```bash
cmake -G "Xcode" ..
cmake --build . --config Release
```

#### iOS (Cross-compilation)
```bash
cmake -G "Xcode" -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ios.cmake ..
cmake --build . --config Release
```

#### Android (Cross-compilation)
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-21 \
      ..
```

## 🎯 **Quick Example**

### Hello World in Kryon

**hello.kry**
```kry
App {
    title: "Hello Kryon"
    windowWidth: 800
    windowHeight: 600
    
    Container {
        padding: 20
        backgroundColor: "#f0f0f0"
        
        Text {
            text: "Hello, World!"
            fontSize: 24
            fontWeight: 700
            textColor: "#333"
        }
        
        Button {
            text: "Click Me"
            backgroundColor: "#007bff"
            textColor: "white"
            padding: 12
            borderRadius: 6
            onClick: "showAlert('Button clicked!')"
        }
    }
}
```

**Compile and Run**
```bash
# Compile KRY to KRB
./kryon compile hello.kry

# Run the application
./kryon run hello.krb

# Or compile and run in one step
./kryon run hello.kry
```

### C API Usage

```c
#include <kryon/kryon.h>

int main() {
    // Initialize Kryon with default configuration
    KryonConfig config = kryon_config_default();
    config.window_title = "My Kryon App";
    config.debug_mode = true;
    
    if (kryon_init(&config) != KRYON_SUCCESS) {
        return 1;
    }
    
    // Create application from KRY source
    KryonAppHandle app;
    if (kryon_app_create_from_kry("hello.kry", &app) != KRYON_SUCCESS) {
        kryon_shutdown();
        return 1;
    }
    
    // Run the application (blocking)
    KryonResult result = kryon_app_run(app);
    
    // Cleanup
    kryon_app_destroy(app);
    kryon_shutdown();
    
    return result == KRYON_SUCCESS ? 0 : 1;
}
```

## 🔧 **Development**

### Running Tests

```bash
# Build and run all tests
cd build
ctest --verbose

# Run specific test categories
ctest -R "compiler.*"     # Compiler tests
ctest -R "runtime.*"      # Runtime tests
ctest -R "renderer.*"     # Renderer tests

# Run benchmarks
ctest -R "benchmark.*"
```

### Development Tools

```bash
# Format code
make format

# Static analysis
make lint

# Generate documentation
make docs

# Profile performance
./kryon profile app.krb

# Debug application
./kryon debug app.krb
```

### IDE Setup

#### VS Code
1. Install C/C++ extension
2. Install CMake Tools extension
3. Open project folder
4. Use CMake: Configure command

#### CLion
1. Open CMakeLists.txt as project
2. CLion will automatically configure

#### Visual Studio
1. File → Open → CMake
2. Select CMakeLists.txt

## 📚 **Documentation**

Comprehensive documentation is available in the `docs/` directory:

- **[Language Specification](docs/language/KRY_LANGUAGE_SPECIFICATION.md)** - Complete KRY syntax reference
- **[Binary Format](docs/binary-format/KRB_BINARY_FORMAT_SPECIFICATION.md)** - KRB binary format specification
- **[Runtime Architecture](docs/architecture/KRYON_RUNTIME_ARCHITECTURE.md)** - Runtime system architecture
- **[Platform Abstraction](docs/architecture/KRYON_PLATFORM_ABSTRACTION_LAYER.md)** - Cross-platform support
- **[API Reference](docs/reference/)** - Complete API documentation

## 🌟 **Features**

### Core Language
- ✅ **Declarative Syntax** - Clean, HTML-like element definitions
- ✅ **Reactive Variables** - Automatic UI updates with `$variable` syntax
- ✅ **Template System** - Dynamic content generation
- ✅ **Style System** - CSS-like styling with inheritance
- ✅ **Component System** - Reusable UI components

### Runtime Features
- ✅ **Cross-Platform** - Web, Windows, macOS, Linux, iOS, Android, Terminal
- ✅ **Multi-Renderer** - WebGL, WGPU, Raylib, Terminal, HTML, Software
- ✅ **Multi-Script** - Lua, JavaScript, Python, Kryon Lisp integration
- ✅ **Layout Engine** - Flexbox and CSS Grid support
- ✅ **Event System** - Comprehensive input and gesture handling
- ✅ **State Management** - Global stores and reactive state
- ✅ **Network Support** - HTTP, WebSocket, fetch API
- ✅ **Audio Support** - Playback, recording, synthesis
- ✅ **Performance** - Optimized rendering and memory management

### Development Tools
- ✅ **CLI Tools** - Compile, run, build, debug commands
- ✅ **Language Server** - IDE integration with LSP
- ✅ **Debugger** - Interactive debugging support
- ✅ **Profiler** - Performance analysis tools
- ✅ **Hot Reload** - Development server with live updates

## 🚦 **Implementation Status**

| Component | Status | Notes |
|-----------|--------|-------|
| **Compiler** | 🟡 In Progress | Lexer and parser foundation |
| **Runtime Core** | 🔴 Not Started | Core runtime system |
| **Platform Abstraction** | 🔴 Not Started | Cross-platform APIs |
| **WebGL Renderer** | 🔴 Not Started | Web platform renderer |
| **Terminal Renderer** | 🔴 Not Started | TUI renderer |
| **Lua Scripting** | 🔴 Not Started | Lua integration |
| **Layout Engine** | 🔴 Not Started | Flexbox implementation |
| **Event System** | 🔴 Not Started | Input handling |
| **Network Layer** | 🔴 Not Started | HTTP/WebSocket |
| **CLI Tools** | 🔴 Not Started | Command line interface |

## 🤝 **Contributing**

1. **Fork** the repository
2. **Create** a feature branch: `git checkout -b feature/amazing-feature`
3. **Follow** the coding standards (run `make format`)
4. **Add** tests for your changes
5. **Commit** your changes: `git commit -m 'Add amazing feature'`
6. **Push** to the branch: `git push origin feature/amazing-feature`
7. **Open** a Pull Request

### Coding Standards

- **C99** standard compliance
- **Consistent naming** - `snake_case` for functions, `PascalCase` for types
- **Error handling** - Always check return codes
- **Memory safety** - No memory leaks, use valgrind
- **Documentation** - Doxygen comments for public APIs
- **Testing** - Unit tests for all public functions

## 📄 **License**

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 **Acknowledgments**

- **Taffy** - Layout engine inspiration
- **Raylib** - Graphics library integration
- **Lua** - Embedded scripting language
- **STB** - Single-file libraries
- **Emscripten** - Web compilation support

## 📞 **Support**

- **Documentation**: [docs/](docs/)
- **Issues**: [GitHub Issues](https://github.com/kryonlabs/kryon-c/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kryonlabs/kryon-c/discussions)
- **Email**: support@kryonlabs.com

---

**Made with ❤️ by the Kryon Labs team**