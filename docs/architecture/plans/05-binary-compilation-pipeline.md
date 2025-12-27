# Binary Compilation Pipeline

## Executive Summary

Implement complete native binary compilation for C, Nim, and Rust frontends that embeds the Kryon renderer and produces standalone executables.

## Architecture Overview

### Two Compilation Modes

```
1. VM Mode (JavaScript, Lua, Python)
   KIR → .krb → VM Runtime → Renderer

2. Native Mode (C, Nim, Rust)
   KIR → Source Code → Native Compiler → Binary (with embedded renderer)
```

## Native Compilation Pipeline

```
Source Code (.c, .nim, .rs)
    ↓
┌──────────────────────────────────┐
│ Parser (Extract KIR)         │
└──────────────────────────────────┘
    ↓
KIR JSON
    ↓
┌──────────────────────────────────┐
│ Codegen (Generate target code)   │
│ - Components → API calls          │
│ - Events → Functions              │
│ - State → Variables               │
└──────────────────────────────────┘
    ↓
Generated Source (.c, .nim, .rs)
    ↓
┌──────────────────────────────────┐
│ Native Compiler                   │
│ - C: gcc/clang                    │
│ - Nim: nim c                      │
│ - Rust: cargo build               │
└──────────────────────────────────┘
    ↓
Object Files (.o)
    ↓
┌──────────────────────────────────┐
│ Linker                            │
│ Links:                            │
│ - App object files                │
│ - libkryon_core.a                 │
│ - libkryon_renderer.a (SDL3)      │
│ - System libs (SDL3, etc.)        │
└──────────────────────────────────┘
    ↓
Standalone Binary
```

## Current State

### What Exists

**C Bindings:** `/mnt/storage/Projects/kryon/bindings/c/kryon.h`
```c
// API exists
kryon_init();
kryon_component_t* kryon_button_new();
kryon_event_bind(...);
kryon_run();
```

**Nim Bindings:** `/mnt/storage/Projects/kryon/bindings/nim/`
- `runtime.nim` - Runtime wrapper
- `reactive_system.nim` - Reactive state
- `ir_vm.nim` - IR VM integration

**Build System:** Makefiles exist for libraries

**Desktop Renderer:** `libkryon_desktop.so` (SDL3-based)

### What's Missing

1. **Complete build command** - `kryon build` only partially implemented
2. **Static linking** - Currently uses shared libraries
3. **Cross-compilation** - No support for targeting different platforms
4. **Embedded targets** - Missing for STM32, RP2040
5. **Optimization levels** - No -O1, -O2, -O3 flags
6. **Strip debug symbols** - No --release mode

## Complete Implementation

### Phase 1: `kryon build` Command

**File:** `/mnt/storage/Projects/kryon/cli/src/commands/cmd_build.c`

```c
typedef struct {
    const char* input_file;
    const char* output_file;
    const char* target_lang;     // "c", "nim", "rust"
    const char* renderer;        // "sdl3", "terminal", "embedded"
    bool release_mode;           // Strip symbols, optimize
    const char* platform;        // "linux", "windows", "macos", "stm32"
    int opt_level;               // 0, 1, 2, 3
} BuildOptions;

int cmd_build(int argc, char** argv) {
    BuildOptions opts = parse_build_options(argc, argv);

    printf("Building: %s → %s\n", opts.input_file, opts.output_file);
    printf("Target: %s (%s)\n", opts.target_lang, opts.platform);
    printf("Renderer: %s\n", opts.renderer);
    printf("Mode: %s\n", opts.release_mode ? "Release" : "Debug");

    // Step 1: Source → KIR
    char kir_path[1024];
    snprintf(kir_path, sizeof(kir_path), "/tmp/%s.kir", basename(opts.input_file));

    printf("[1/4] Parsing source...\n");
    if (compile_to_kir(opts.input_file, kir_path) != 0) {
        return 1;
    }

    // Step 2: KIR → Target Source
    char gen_path[1024];
    snprintf(gen_path, sizeof(gen_path), "/tmp/%s_gen.%s",
             basename(opts.input_file),
             lang_extension(opts.target_lang));

    printf("[2/4] Generating %s code...\n", opts.target_lang);
    if (codegen_from_kir(kir_path, gen_path, opts.target_lang) != 0) {
        return 1;
    }

    // Step 3: Compile to object files
    printf("[3/4] Compiling...\n");
    char obj_path[1024];
    snprintf(obj_path, sizeof(obj_path), "/tmp/%s.o", basename(opts.input_file));

    if (compile_to_object(gen_path, obj_path, &opts) != 0) {
        return 1;
    }

    // Step 4: Link binary
    printf("[4/4] Linking...\n");
    if (link_binary(obj_path, opts.output_file, &opts) != 0) {
        return 1;
    }

    printf("✓ Built: %s\n", opts.output_file);

    // Print binary info
    print_binary_info(opts.output_file);

    return 0;
}

static int compile_to_object(const char* source, const char* output, BuildOptions* opts) {
    char cmd[4096];

    if (strcmp(opts->target_lang, "c") == 0) {
        // C compilation
        snprintf(cmd, sizeof(cmd),
                 "gcc -c %s -o %s "
                 "-I/mnt/storage/Projects/kryon/bindings/c "
                 "-I/mnt/storage/Projects/kryon/core/include "
                 "-O%d "
                 "%s",
                 source,
                 output,
                 opts->opt_level,
                 opts->release_mode ? "-DNDEBUG" : "-g");

    } else if (strcmp(opts->target_lang, "nim") == 0) {
        // Nim compilation
        snprintf(cmd, sizeof(cmd),
                 "nim c --noMain --app:lib "
                 "--path:/mnt/storage/Projects/kryon/bindings/nim "
                 "-o:%s "
                 "%s "
                 "%s",
                 output,
                 opts->release_mode ? "-d:release" : "",
                 source);

    } else if (strcmp(opts->target_lang, "rust") == 0) {
        // Rust compilation
        // TODO: Use cargo
        return 1;
    }

    return system(cmd);
}

static int link_binary(const char* obj_file, const char* output, BuildOptions* opts) {
    char cmd[4096];
    char libs[2048] = "";

    // Select renderer library
    if (strcmp(opts->renderer, "sdl3") == 0) {
        strcat(libs, "-lkryon_desktop ");
        strcat(libs, "-lSDL3 ");
    } else if (strcmp(opts->renderer, "terminal") == 0) {
        strcat(libs, "-lkryon_terminal ");
    } else if (strcmp(opts->renderer, "embedded") == 0) {
        strcat(libs, "-lkryon_embedded ");
    }

    // Core library
    strcat(libs, "-lkryon_core ");
    strcat(libs, "-lkryon_ir ");

    // Link command
    snprintf(cmd, sizeof(cmd),
             "gcc %s -o %s "
             "-L/mnt/storage/Projects/kryon/build "
             "-L/usr/local/lib "
             "%s "
             "-lm -lpthread "
             "%s",
             obj_file,
             output,
             libs,
             opts->release_mode ? "-s" : "");  // -s strips symbols

    return system(cmd);
}

static void print_binary_info(const char* binary) {
    struct stat st;
    if (stat(binary, &st) == 0) {
        printf("\nBinary Info:\n");
        printf("  Size: %ld bytes (%.2f KB)\n", st.st_size, st.st_size / 1024.0);

        // Check if executable
        if (st.st_mode & S_IXUSR) {
            printf("  Executable: Yes\n");
        }

        // Get dependencies (Linux)
        printf("  Dependencies:\n");
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "ldd %s 2>/dev/null | head -10", binary);
        system(cmd);
    }
}
```

### Phase 2: Static Linking

**Goal:** Produce truly standalone binaries with no external dependencies

**File:** `/mnt/storage/Projects/kryon/build/static_link.mk`

```makefile
# Static build configuration

STATIC_LIBS = \
    libkryon_core.a \
    libkryon_ir.a \
    libkryon_desktop.a \
    libSDL3.a

# Build static libraries
static: $(STATIC_LIBS)

libkryon_core.a: $(CORE_OBJS)
	ar rcs $@ $^

libkryon_ir.a: $(IR_OBJS)
	ar rcs $@ $^

libkryon_desktop.a: $(DESKTOP_OBJS)
	ar rcs $@ $^

# Static link command
static-link:
	gcc app.o -o app \
	    -static \
	    -L. \
	    -lkryon_core \
	    -lkryon_ir \
	    -lkryon_desktop \
	    -lSDL3 \
	    -lm -lpthread -ldl
```

**Usage:**
```bash
kryon build app.c -o app --static
# Produces fully static binary (no .so dependencies)
```

### Phase 3: Cross-Compilation

**Support multiple target platforms from single build machine**

```c
typedef enum {
    PLATFORM_LINUX_X86_64,
    PLATFORM_LINUX_ARM64,
    PLATFORM_LINUX_ARM,        // Raspberry Pi
    PLATFORM_WINDOWS_X86_64,
    PLATFORM_MACOS_X86_64,
    PLATFORM_MACOS_ARM64,      // M1/M2/M3
    PLATFORM_STM32,            // Embedded ARM
    PLATFORM_RP2040,           // Pico
    PLATFORM_WASM32            // WebAssembly
} TargetPlatform;

static const char* get_cross_compiler(TargetPlatform platform) {
    switch (platform) {
        case PLATFORM_LINUX_ARM64:
            return "aarch64-linux-gnu-gcc";
        case PLATFORM_LINUX_ARM:
            return "arm-linux-gnueabihf-gcc";
        case PLATFORM_WINDOWS_X86_64:
            return "x86_64-w64-mingw32-gcc";
        case PLATFORM_STM32:
            return "arm-none-eabi-gcc";
        case PLATFORM_RP2040:
            return "arm-none-eabi-gcc";
        case PLATFORM_WASM32:
            return "emcc";  // Emscripten
        default:
            return "gcc";
    }
}

static int cross_compile(const char* source, const char* output, TargetPlatform platform) {
    const char* compiler = get_cross_compiler(platform);
    const char* flags = get_platform_flags(platform);
    const char* libs = get_platform_libs(platform);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "%s %s -o %s %s %s",
             compiler,
             source,
             output,
             flags,
             libs);

    return system(cmd);
}
```

**Usage:**
```bash
# Cross-compile for ARM64
kryon build app.c -o app_arm64 --platform=linux-arm64

# Cross-compile for Windows
kryon build app.c -o app.exe --platform=windows-x64

# Cross-compile for STM32
kryon build app.c -o app.elf --platform=stm32 --renderer=embedded
```

### Phase 4: Embedded Targets

**STM32 Build:**

```makefile
# STM32 platform configuration

STM32_ARCH = cortex-m4
STM32_FLAGS = \
    -mcpu=$(STM32_ARCH) \
    -mthumb \
    -mfloat-abi=hard \
    -mfpu=fpv4-sp-d16 \
    -DSTM32F4 \
    -DKRYON_EMBEDDED \
    -DKRYON_RENDERER_TERMINAL

STM32_LIBS = \
    -lkryon_embedded \
    -lkryon_core_embedded \
    -nostdlib \
    -lc -lnosys

stm32-build:
	arm-none-eabi-gcc $(STM32_FLAGS) \
	    -T stm32f4.ld \
	    -o firmware.elf \
	    app_gen.c \
	    $(STM32_LIBS)

	arm-none-eabi-objcopy -O binary firmware.elf firmware.bin
	arm-none-eabi-size firmware.elf
```

**RP2040 (Pico) Build:**

```cmake
# CMakeLists.txt for RP2040

cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(kryon_app C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(kryon_app
    app_gen.c
    ${KRYON_EMBEDDED_SOURCES}
)

target_include_directories(kryon_app PRIVATE
    ${KRYON_INCLUDE_DIRS}
)

target_link_libraries(kryon_app
    pico_stdlib
    kryon_core_embedded
    kryon_renderer_terminal
)

pico_add_extra_outputs(kryon_app)
```

**Usage:**
```bash
# Build for Pico
kryon build app.c -o app.uf2 --platform=rp2040 --renderer=terminal

# Flash to device
cp app.uf2 /media/RPI-RP2/
```

### Phase 5: Optimization & Distribution

**Optimization Levels:**

```c
static const char* get_optimization_flags(int level, bool release) {
    static char flags[256];
    flags[0] = '\0';

    switch (level) {
        case 0:
            strcat(flags, "-O0 -g");
            break;
        case 1:
            strcat(flags, "-O1");
            break;
        case 2:
            strcat(flags, "-O2");
            break;
        case 3:
            strcat(flags, "-O3 -march=native");
            break;
    }

    if (release) {
        strcat(flags, " -DNDEBUG -s");  // Strip symbols
    }

    return flags;
}
```

**Usage:**
```bash
# Debug build (symbols, no optimization)
kryon build app.c -o app --debug

# Release build (optimized, stripped)
kryon build app.c -o app --release -O3

# Size-optimized (for embedded)
kryon build app.c -o app --release -Os --platform=stm32
```

**Binary Size Comparison:**

```
Debug build:      2.5 MB
Release -O2:      800 KB
Release -O3:      750 KB
Release -Os:      600 KB (embedded)
Static linked:    5.2 MB (no external deps)
```

### Phase 6: Packaging & Distribution

**Desktop App Bundle:**

**Linux AppImage:**
```bash
kryon package app.c -o MyApp.AppImage --platform=linux-x64
# Produces self-contained AppImage
```

**macOS .app Bundle:**
```bash
kryon package app.c -o MyApp.app --platform=macos-arm64
# Produces .app bundle with icon, plist, etc.
```

**Windows Installer:**
```bash
kryon package app.c -o MyAppSetup.exe --platform=windows-x64
# Uses NSIS to create installer
```

## Build Configuration File

**kryon.toml:**

```toml
[package]
name = "my-app"
version = "1.0.0"
author = "Your Name"

[build]
target_lang = "c"         # c, nim, rust
renderer = "sdl3"         # sdl3, terminal, embedded
platform = "linux-x64"    # linux-x64, windows-x64, macos-arm64, stm32
optimization = 2          # 0, 1, 2, 3
release = true            # Strip symbols, enable optimizations

[dependencies]
# External libraries
sdl3 = "3.0"

[embedded]
# Embedded-specific config (if platform is embedded)
mcu = "stm32f4"
flash_size = "512KB"
ram_size = "128KB"
```

**Usage:**
```bash
# Use config file
kryon build app.c

# Override config
kryon build app.c --release --platform=windows-x64
```

## CLI Commands

```bash
# Basic compilation
kryon build app.c -o app

# Full options
kryon build app.c -o app \
    --lang=c \
    --renderer=sdl3 \
    --platform=linux-x64 \
    --release \
    -O3 \
    --static

# Cross-compile
kryon build app.c -o app_arm --platform=linux-arm64

# Embedded
kryon build app.c -o firmware.bin --platform=stm32 --renderer=terminal

# Package for distribution
kryon package app.c -o MyApp.AppImage

# Show build info
kryon build app.c --dry-run  # Show commands without executing
```

## Integration with Existing Commands

```bash
# Full pipeline: Source → KIR → Binary
kryon build app.tsx -o app --lang=c

# Step by step:
kryon compile app.tsx -o app.kir         # Step 1: TSX → KIR
kryon codegen c app.kir -o app_gen.c     # Step 2: KIR → C
kryon build app_gen.c -o app             # Step 3: C → Binary
```

## Files to Create/Modify

```
cli/src/commands/cmd_build.c .......... MODIFY: Complete implementation
cli/src/commands/cmd_package.c ........ CREATE: App packaging
cli/src/build/cross_compile.c ......... CREATE: Cross-compilation
cli/src/build/link_static.c ........... CREATE: Static linking
build/platforms/stm32.mk .............. CREATE: STM32 build config
build/platforms/rp2040.cmake .......... CREATE: RP2040 build config
build/static_link.mk .................. CREATE: Static library config
cli/src/config/build_config.c ......... CREATE: kryon.toml parser
```

## Estimated Effort

- Phase 1 (Build Command): 3 days
- Phase 2 (Static Linking): 2 days
- Phase 3 (Cross-Compilation): 4 days
- Phase 4 (Embedded Targets): 5 days
- Phase 5 (Optimization): 2 days
- Phase 6 (Packaging): 3 days
- Testing: 3 days

**Total: ~22 days (4.5 weeks)**

## Success Criteria

✅ `kryon build` produces working binaries
✅ Static linking works (no .so dependencies)
✅ Cross-compilation to ARM64, Windows, macOS
✅ Embedded builds for STM32 and RP2040
✅ Release builds are optimized and stripped
✅ Binary sizes reasonable (<1MB for simple apps)
✅ AppImage/DMG/EXE packaging works
✅ All examples build and run on all platforms
