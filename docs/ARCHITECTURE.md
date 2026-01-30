# Kryon Architecture Guide

## Overview

Kryon is a declarative UI framework that compiles `.kry` source files to `.krb` bytecode, which can then be executed on multiple target platforms. The framework supports extensible language plugins allowing UI logic to be written in different scripting languages.

## Core Concepts

### 1. Compilation Pipeline

```
┌─────────┐     ┌────────┐     ┌────────┐     ┌─────────┐     ┌─────────┐
│ .kry    │────▶│ Lexer  │────▶│ Parser │────▶│ Codegen │────▶│  .krb   │
│ Source  │     │ Tokens │     │  AST   │     │ KRB IR  │     │ Binary  │
└─────────┘     └────────┘     └────────┘     └─────────┘     └─────────┘
```

**File Formats**:
- `.kry` - Kryon source code (declarative UI markup)
- `.krb` - Kryon bytecode (binary runtime format)
- `.kir` - Kryon IR (text-based debugging format)

### 2. Runtime Execution

```
┌─────────┐     ┌──────────┐     ┌──────────┐     ┌────────┐
│  .krb   │────▶│ Runtime  │────▶│ Renderer │────▶│ Output │
│ Binary  │     │ Engine   │     │ (Target) │     │        │
└─────────┘     └──────────┘     └──────────┘     └────────┘
                      │
                      ▼
               ┌──────────────┐
               │   Language   │
               │   Plugins    │
               └──────────────┘
```

## Target System

Kryon supports four deployment targets, each mapping to a specific renderer:

| Target   | Description              | Renderer  | Use Case                    |
|----------|--------------------------|-----------|----------------------------|
| `sdl2`   | Desktop GUI              | SDL2      | Cross-platform desktop apps |
| `raylib` | Desktop GUI              | Raylib    | Game-like UIs, graphics     |
| `web`    | Static HTML/CSS/JS       | Web       | Browser-based apps          |
| `emu`    | TaijiOS emu viewer       | KRBVIEW   | Plan 9 native environment   |

### Target Selection

```bash
# Compile to bytecode
kryon compile app.kry

# Run with specific target
kryon run app.krb --target=sdl2
kryon run app.krb --target=raylib
kryon run app.krb --target=web      # Generates HTML/CSS/JS
kryon run app.krb --target=emu      # For TaijiOS emu
```

### Renderer Architecture

All renderers implement a common interface (`KryonRendererVTable`):

```c
typedef struct KryonRendererVTable {
    bool (*init)(KryonRenderer *renderer, KryonRuntimeConfig *config);
    void (*shutdown)(KryonRenderer *renderer);
    void (*render_frame)(KryonRenderer *renderer, KryonElement *root);
    bool (*handle_events)(KryonRenderer *renderer);
    // ... more functions
} KryonRendererVTable;
```

**Renderer Implementations**:
- `src/renderers/sdl2/` - SDL2 renderer (1,092 LOC)
- `src/renderers/raylib/` - Raylib renderer (1,161 LOC)
- `src/renderers/web/` - HTML/CSS/JS generator (857 LOC)
- `src/renderers/html/` - HTML generation helpers (912 LOC)

## Language Plugin System

Kryon supports multiple scripting languages for UI logic through a plugin architecture.

### Supported Languages

| Language | Identifier | Availability | Description                        |
|----------|------------|--------------|-----------------------------------|
| Native   | `""`       | Always       | Built-in Kryon scripting          |
| sh       | `"sh"`     | Inferno only | Inferno shell scripts             |
| Limbo    | `"limbo"`  | TaijiOS only | Limbo module integration          |

### Language Declaration in .kry Files

```kry
App {
    # Native Kryon function (default)
    function onClick() {
        console.log("Clicked!")
    }

    # Inferno shell function
    function "sh" loadData() {
        cat /data/config.json
    }

    # Limbo module function
    function "limbo" parseYAML() {
        YAML.loadfile("data.yaml")
    }
}
```

### Plugin Architecture

Each language plugin implements the `KryonLanguagePlugin` interface:

```c
typedef struct KryonLanguagePlugin {
    const char *identifier;        // "sh", "limbo", etc.
    const char *name;              // Display name
    const char *version;           // Plugin version

    bool (*init)(KryonRuntime *runtime);
    void (*shutdown)(KryonRuntime *runtime);
    bool (*is_available)(KryonRuntime *runtime);

    KryonLanguageExecutionResult (*execute)(
        KryonRuntime *runtime,
        KryonElement *element,
        KryonScriptFunction *function,
        char *error_buffer,
        size_t error_buffer_size
    );
} KryonLanguagePlugin;
```

**Plugin Implementations**:
- `src/runtime/languages/native_language.c` - Built-in Kryon
- `src/runtime/languages/sh_language.c` - Inferno shell (optional)
- `src/runtime/languages/limbo_language.c` - Limbo modules (optional)

**Plugin Registration**:
Plugins use `__attribute__((constructor))` for automatic registration:

```c
__attribute__((constructor))
static void register_sh_plugin(void) {
    kryon_language_register(&sh_plugin);
}
```

### Conditional Compilation

Language plugins are enabled based on build configuration:

```makefile
# Inferno shell plugin (auto-detected)
ifdef INFERNO
    PLUGINS += inferno
    LANGUAGE_SRC += src/runtime/languages/sh_language.c
endif

# Limbo plugin (manual)
ifneq ($(filter limbo,$(PLUGINS)),)
    LANGUAGE_SRC += src/runtime/languages/limbo_language.c
    RUNTIME_SRC += src/runtime/limbo/limbo_runtime.c
endif
```

Build commands:
```bash
# Standard build (sh plugin if INFERNO detected)
make

# Build with Limbo support
make PLUGINS=limbo

# Build with both (if INFERNO set)
INFERNO=/opt/inferno make PLUGINS=limbo
```

## Runtime Architecture

### Element System

Kryon UIs are built from elements organized in a tree structure:

```
App
├── Container
│   ├── Text
│   └── Button
│       └── onClick() handler
├── Grid
│   ├── Image
│   └── Text
└── TabGroup
    ├── Tab
    └── TabPanel
```

**Element Types** (see `src/runtime/elements/`):
- Layout: `Container`, `Grid`, `App`
- Input: `Button`, `Input`, `Checkbox`, `Dropdown`, `Slider`
- Display: `Text`, `Image`, `Link`
- Navigation: `TabGroup`, `Tab`, `TabPanel`, `TabBar`, `TabContent`

### Element Lifecycle

```
CREATED → MOUNTING → MOUNTED → UPDATING → UNMOUNTING → UNMOUNTED → DESTROYED
```

### Directive System

Kryon supports control flow directives:

```kry
App {
    # Conditional rendering
    @if condition="isLoggedIn" {
        Text { content: "Welcome!" }
    }

    # List rendering
    @for item in items {
        Container {
            Text { content: item.name }
        }
    }
}
```

**Implementation**:
- Compile-time expansion: `src/compiler/codegen/ast_expander.c`
- Runtime expansion: `src/runtime/core/runtime.c` (dynamic state)

## Platform Services

Kryon provides platform-specific services through a plugin system:

```c
typedef enum {
    KRYON_SERVICE_EXTENDED_FILE_IO,   // Device files, special filesystems
    KRYON_SERVICE_NAMESPACE,          // mount/bind/9P operations
    KRYON_SERVICE_PROCESS_CONTROL,    // Process monitoring
    KRYON_SERVICE_IPC,                // Named pipes, shared memory
} KryonServiceType;
```

**Inferno Platform Plugin** (`src/plugins/inferno/`):
- Extended file I/O using lib9
- Namespace operations (mount, bind)
- Process control
- Plan 9-style IPC

## Build System

### Build Modes

```bash
# Standard Linux build
make

# With Inferno support (auto-detected if INFERNO env set)
make

# With Limbo plugin
make PLUGINS=limbo

# Debug build
make debug

# Clean
make clean
```

### Dependencies

**Required**:
- gcc or clang
- make
- pkg-config

**Optional Renderers**:
- SDL2 + SDL2_image + SDL2_ttf (for `sdl2` target)
- Raylib (for `raylib` target)
- Web renderer always available

**Optional Language Plugins**:
- Inferno (for `sh` plugin) - auto-detected via `$INFERNO`
- TaijiOS (for `limbo` plugin) - requires `PLUGINS=limbo`

### lib9 Compatibility

Kryon includes a lib9 compatibility layer in `third-party/lib9/`:
- Used when `INFERNO` env not set
- Provides Plan 9 functions (print, sprint, etc.)
- Allows standalone Linux builds

When `INFERNO` is set, native lib9 is used instead.

## Directory Structure

```
kryon/
├── include/              # Public headers
│   ├── runtime.h        # Runtime API
│   ├── target.h         # Target system
│   ├── elements.h       # Element types
│   └── language_plugins.h  # Language plugin API
├── src/
│   ├── core/            # Core utilities
│   ├── compiler/        # Compilation pipeline
│   │   ├── lexer/       # Tokenization
│   │   ├── parser/      # AST generation
│   │   ├── codegen/     # Code generation
│   │   ├── kir/         # IR format
│   │   └── krb/         # Binary format
│   ├── runtime/         # Runtime engine
│   │   ├── core/        # Runtime core (4,057 LOC - needs refactoring)
│   │   ├── elements/    # Element implementations
│   │   ├── languages/   # Language plugins
│   │   ├── limbo/       # Limbo integration
│   │   └── navigation/  # Navigation system
│   ├── renderers/       # Target renderers
│   │   ├── sdl2/
│   │   ├── raylib/
│   │   ├── html/
│   │   └── web/
│   ├── services/        # Platform services
│   ├── plugins/         # Platform plugins
│   │   └── inferno/     # Inferno platform
│   └── cli/             # Command-line interface
├── third-party/         # External dependencies
│   ├── lib9/            # lib9 compatibility
│   ├── cjson/           # JSON parsing
│   └── stb/             # STB libraries
├── tests/               # Test suite
├── examples/            # Example applications
└── docs/                # Documentation
```

## CLI Commands

```bash
# Compile .kry to .krb
kryon compile <file.kry>

# Decompile .krb to .kir
kryon decompile <file.krb>

# Print .kir to .kry
kryon print <file.kir>

# Run .krb file
kryon run <file.krb> [--target=<target>]

# Development mode with hot reload
kryon dev <file.kry>

# Debug mode
kryon debug <file.krb>

# Package project
kryon package <project>

# List available targets
kryon targets
```

## Key Files to Understand

### Compiler
1. `src/compiler/lexer/lexer.c` (1,124 LOC) - Tokenization
2. `src/compiler/parser/parser.c` (3,396 LOC) - **MONOLITH - needs refactoring**
3. `src/compiler/codegen/codegen.c` (2,575 LOC) - Code generation

### Runtime
1. `src/runtime/core/runtime.c` (4,057 LOC) - **MONOLITH - needs refactoring**
2. `src/runtime/elements.c` (4,006 LOC) - **MONOLITH - needs refactoring**
3. `src/runtime/languages/language_registry.c` - Plugin registry

### Targets
1. `include/target.h` - Target type definitions
2. `src/core/target_resolver.c` - Target resolution logic

### Language Plugins
1. `include/language_plugins.h` - Plugin API
2. `src/runtime/languages/native_language.c` - Native Kryon
3. `src/runtime/languages/sh_language.c` - Inferno shell
4. `src/runtime/languages/limbo_language.c` - Limbo modules

## Design Principles

### 1. Separation of Concerns
- Compilation and runtime are separate phases
- Targets determine WHERE to run, renderers determine HOW to render
- Language plugins are non-invasive extensions

### 2. Zero Overhead
- Language plugins compiled in only when needed
- No performance penalty when plugins not used
- Compile-time feature detection

### 3. Declarative UI
- UI structure declared in .kry files
- Reactive state management
- Directive-based control flow

### 4. Cross-Platform
- Single .krb binary runs on multiple targets
- Renderer abstraction isolates platform differences
- lib9 compatibility for Plan 9 features

## Known Issues and Refactoring Needs

See `REFACTORING_ANALYSIS.md` for detailed analysis. Key issues:

1. **runtime.c monolith** (4,057 LOC) - needs splitting into 7 modules
2. **parser.c monolith** (3,396 LOC) - needs splitting into 8 modules
3. **Target-renderer coupling** - needs factory pattern
4. **Directive duplication** - compile-time and runtime expansion
5. **Single platform plugin** - needs multi-plugin support

## Future Enhancements

### Short-term
- Split monolithic files (runtime.c, parser.c)
- Improve error messages
- Add comprehensive test suite
- Better documentation

### Medium-term
- Renderer factory with fallback logic
- Multi-platform service plugins
- Element type registry with reflection
- Hot reload improvements

### Long-term
- Additional targets (Android, iOS, desktop native)
- More language plugins (Python, JavaScript)
- Plugin marketplace
- Visual editor/IDE integration

## Contributing

When contributing to Kryon:

1. **Understand the architecture** - Read this document and `REFACTORING_ANALYSIS.md`
2. **Keep modules focused** - Single responsibility per file
3. **Avoid monoliths** - Split files over 1,500 LOC
4. **Test thoroughly** - Add tests for new features
5. **Document APIs** - Update docs for public interfaces
6. **Follow patterns** - Use existing patterns (vtables, plugins, registries)

## Resources

- Build instructions: `docs/BUILD.md`
- Refactoring plan: `REFACTORING_ANALYSIS.md`
- Examples: `examples/`
- API reference: `include/` headers
