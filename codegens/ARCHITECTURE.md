# Codegen Directory Structure

This document explains the organization of the Kryon codegen system and the distinction between different directory types.

## Overview

The codegen system is organized into three main categories:

1. **Full Codegen Implementations** - Complete code generators for specific languages
2. **Language Emitters** - WIR-based language syntax layers
3. **Toolkit Emitters** - WIR-based widget/property/layout generators
4. **Multi-Language Codegens** - Codegens that output multiple languages

## Directory Structure

```
codegens/
├── ARCHITECTURE.md              # This file
├── codegen_adapter_macros.h     # Macro-based adapter generator
├── codegen_common.h/c            # Shared utilities (I/O, JSON, properties)
├── codegen_io.h/c               # File I/O abstraction layer
├── codegen_options.h/c          # Unified options structures
├── codegen_interface.h          # Unified codegen interface
├── codegen_registry.c           # Centralized registration
│
├── c/                           # C codegen (full implementation)
│   ├── c_codegen_adapter.c      # Adapter using macros
│   ├── ir_c_codegen.h/c         # Main codegen interface
│   ├── c_from_wir.c             # WIR emitter
│   ├── ir_c_*.c                 # Modular components
│   └── mkfile                   # Build configuration
│
├── lua/                         # Lua codegen (full implementation)
│   ├── lua_codegen_adapter.c    # Adapter using macros
│   ├── lua_codegen.h/c          # Single-file implementation
│   └── mkfile
│
├── limbo/                       # Limbo codegen (full implementation)
│   ├── limbo_codegen_adapter.c  # Adapter using macros
│   ├── limbo_codegen.h/c        # Implementation
│   ├── limbo_from_wir.c         # WIR emitter
│   └── mkfile
│
├── markdown/                    # Markdown codegen (full implementation)
│   ├── markdown_codegen_adapter.c
│   ├── markdown_codegen.h/c
│   └── mkfile
│
├── kry/                         # KRY DSL codegen (round-trip format)
│   ├── kry_codegen_adapter.c
│   ├── kry_codegen.h/c
│   └── mkfile
│
├── tcl/                         # Tcl codegen (WIR composer pattern)
│   ├── tcl_codegen_adapter.c    # Uses WIR composer
│   ├── tcl_codegen.h/c          # Implementation
│   ├── tcl_from_wir.c           # WIR emitter
│   └── mkfile
│
├── web/                         # Web codegen (multi-language output)
│   ├── html_generator.c
│   ├── css_generator.c
│   ├── js_generator.c
│   └── mkfile
│
├── languages/                   # Language-specific WIR emitters
│   ├── common/
│   │   └── language_registry.h/c
│   ├── tcl/                     # Tcl language emitter (syntax only)
│   │   └── tcl_emitter.h/c
│   └── limbo/                   # Limbo language emitter (syntax only)
│       └── limbo_emitter.h/c
│
├── toolkits/                    # Toolkit-specific WIR emitters
│   ├── common/
│   │   └── toolkit_registry.h/c
│   ├── tk/                      # Tk toolkit emitter
│   │   ├── tk_wir_emitter.h/c
│   │   └── mkfile
│   └── draw/                    # Draw toolkit emitter
│       ├── draw_ir_builder.h/c
│       └── draw_ir.h
│
├── platforms/                   # Platform-specific configurations
│   ├── common/
│   │   ├── platform_registry.h/c
│   │   └── platform_profiles.h/c
│   ├── android/
│   ├── linux/
│   └── windows/
│
├── wir/                         # WIR (Widget Intermediate Representation)
│   ├── wir.h                    # WIR data structures
│   ├── wir_builder.h/c          # WIR construction
│   ├── wir_emitter.h/c          # WIR traversal
│   └── mkfile
│
└── common/                      # Shared emitter utilities
    ├── wir_composer.h/c         # WIR composer for combining emitters
    ├── color_utils.h/c          # Color conversion utilities
    └── emitter_base.h/c         # Shared emitter context (TODO)
```

## Directory Types Explained

### 1. Full Codegen Implementations (`codegens/{language}/`)

These are complete, self-contained code generators for a specific language.

**Characteristics:**
- Contains adapter, implementation, and optional WIR integration
- Can be legacy (KIR → Language) or modern (WIR-based)
- Standalone - can generate code without external emitters
- Examples: `c/`, `lua/`, `limbo/`, `markdown/`, `kry/`

**When to use:**
- You need a complete codegen for a language
- The language has unique requirements not covered by WIR
- You want tight control over the entire generation process

**Example:**
```
c/
├── c_codegen_adapter.c      # Implements CodegenInterface
├── ir_c_codegen.c           # Main entry point
├── c_from_wir.c             # WIR integration (optional)
└── ir_c_modules.c           # Module generation
```

### 2. Language Emitters (`codegens/languages/{language}/`)

These are **syntax-only layers** that emit language-specific code from WIR.

**Characteristics:**
- Pure language syntax emitters (no widgets, no properties)
- Used by WIR composer pattern
- Must be combined with a toolkit emitter
- Examples: `languages/tcl/`, `languages/limbo/`

**Purpose:**
- Enable composition: Language + Toolkit = Complete Codegen
- Separate concerns: Syntax vs. Widgets
- Support multiple toolkits per language

**Example:**
```c
// Tcl language emitter only knows Tcl syntax
void tcl_emit_function_call(WIREmitterContext* ctx, const char* func, ...) {
    sb_append_fmt(ctx->output, "%s ", func);
    // ... emit arguments in Tcl syntax
}
```

**Usage:**
```c
// Combine Tcl language + Tk toolkit = Complete Tcl/Tk codegen
char* output = wir_compose_and_emit("tcl", "tk", root, &options);
```

### 3. Toolkit Emitters (`codegens/toolkits/{toolkit}/`)

These emit **widget, property, and layout code** for a specific toolkit.

**Characteristics:**
- Toolkit-specific widget types (buttons, labels, containers)
- Property mapping (color, size, alignment)
- Layout management (pack, grid, place)
- Examples: `toolkits/tk/`, `toolkits/draw/`

**Purpose:**
- Abstract toolkit differences
- Support multiple languages per toolkit
- Reusable across different language emitters

**Example:**
```c
// Tk toolkit emitter knows how to create Tk widgets
void tk_emit_button(WIREmitterContext* ctx, WIRWidget* widget) {
    const char* text = wir_widget_get_property(widget, "text");
    sb_append_fmt(ctx->output, "button .%s -text \"%s\"", widget->id, text);
}
```

### 4. Multi-Language Codegens (`codegens/multi/` or `codegens/web/`)

Codegens that output **multiple languages** in a coordinated way.

**Characteristics:**
- Orchestrates multiple file outputs
- Cross-language coordination
- Complex architecture
- Example: `web/` (HTML + CSS + JS)

**Purpose:**
- Target platforms requiring multiple languages
- Coordinate code generation across languages
- Handle inter-language dependencies

**Example:**
```
web/
├── html_generator.c    # Generates HTML structure
├── css_generator.c     # Generates CSS styling
├── js_generator.c      # Generates JavaScript logic
└── web_codegen.c       # Orchestrator
```

## WIR Composer Pattern

The WIR composer pattern combines language and toolkit emitters:

```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│    KIR      │ ────>   │    WIR      │ ────>   │  Generated  │
│  (Input)    │         │ (Internal)  │         │    Code     │
└─────────────┘         └─────────────┘         └─────────────┘
                               │
                      ┌────────┴────────┐
                      │                 │
              ┌───────▼───────┐ ┌──────▼──────┐
              │  Language     │ │  Toolkit    │
              │  Emitter      │ │  Emitter    │
              │  (Syntax)     │ │  (Widgets)  │
              └───────────────┘ └─────────────┘
                      │                 │
                      └────────┬────────┘
                               │
                        ┌──────▼───────┐
                        │  Composer    │
                        │  (Combine)   │
                        └──────────────┘
```

**Example Usage:**
```c
// Generate Tcl/Tk code
WIRComposerOptions options = {
    .include_comments = true,
    .verbose = false
};
char* output = wir_compose_and_emit("tcl", "tk", wir_root, &options);
```

**Benefits:**
- **Composition:** Mix any language with any toolkit
- **Reuse:** Share emitters across multiple combinations
- **Clarity:** Separate syntax from widgets
- **Flexibility:** Easy to add new languages/toolkits

## Empty Directories

Some directories in `languages/` may be empty:

- `languages/c/` - Empty (C codegen is a full implementation in `c/`)
- `languages/lua/` - Empty (Lua codegen is a full implementation in `lua/`)

**Why are they empty?**
- These languages have full implementations in the top-level directories
- They don't use the WIR composer pattern (yet)
- The directories are reserved for future WIR-based emitters

**When to populate them:**
- If you want to refactor the codegen to use WIR
- If you want to enable composition with other toolkits
- If you're creating a syntax-only emitter

## Migration Path

### From Full Codegen to WIR Composer

If you have a full codegen and want to migrate to the WIR composer pattern:

1. **Extract language syntax** → `languages/{language}/`
   - Move language-specific code to language emitter
   - Remove widget-specific logic
   - Focus on syntax (functions, variables, etc.)

2. **Extract toolkit logic** → `toolkits/{toolkit}/`
   - Move widget creation code to toolkit emitter
   - Remove language-specific syntax
   - Focus on widgets and properties

3. **Create adapter using composer**
   - Replace adapter to use `wir_compose_and_emit()`
   - Remove complex codegen logic
   - Let composer handle coordination

**Example:**
```c
// Before: Direct codegen
static bool tcl_codegen_generate(const char* kir_path, const char* output_path) {
    // Read KIR, parse JSON, generate Tcl/Tk code (500+ lines)
}

// After: Using composer
static bool tcl_codegen_generate(const char* kir_path, const char* output_path) {
    CodegenInput* input = codegen_load_input(kir_path);
    WIRRoot* root = wir_build_from_cJSON(input->root, false);

    char* output = wir_compose_and_emit("tcl", "tk", root, &options);

    codegen_write_output_file(output_path, output);
    // ... cleanup ...
}
```

## Adding a New Codegen

### Option 1: Full Implementation

Create a new directory in `codegens/{language}/`:

1. Create `{language}_codegen_adapter.c` using macros
2. Implement `{language}_codegen.c` with generation logic
3. Add to Makefile
4. Register in `codegen_registration.c`

### Option 2: WIR Composer Pattern

1. Create language emitter in `languages/{language}/`
2. Create toolkit emitter in `toolkits/{toolkit}/` (or reuse existing)
3. Register language and toolkit in registries
4. Create adapter using `wir_compose_and_emit()`

**Which to choose?**
- **Full implementation:** For complex, unique languages (C, Lua)
- **WIR composer:** For standard GUI frameworks (Tcl/Tk, Qt, GTK)

## Build System Integration

All codegens must:

1. **Use the adapter macros** from `codegen_adapter_macros.h`
2. **Link against common libraries** in the Makefile:
   - `libkryon_codegen_common.a` (utilities, I/O, options)
   - `libkryon_codegen_registry.a` (registries)
3. **Register themselves** in `codegen_registration.c`
4. **Follow the naming convention** `{NAME}_codegen_interface`

**Example Makefile entry:**
```makefile
# MyLang Codegen Adapter
MYLANG_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/mylang_codegen_adapter.o
MYLANG_CODEGEN_ADAPTER_SRC = mylang/mylang_codegen_adapter.c

$(MYLANG_CODEGEN_ADAPTER_OBJ): $(MYLANG_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
    $(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Imylang -c $(MYLANG_CODEGEN_ADAPTER_SRC) -o $@
```

## Summary

| Directory Type | Purpose | Examples | WIR? |
|---------------|---------|----------|------|
| `codegens/{lang}/` | Full codegen implementations | `c/`, `lua/`, `limbo/` | Optional |
| `languages/{lang}/` | Language syntax emitters | `tcl/`, `limbo/` | Required |
| `toolkits/{tk}/` | Toolkit widget emitters | `tk/`, `draw/` | Required |
| `codegens/web/` | Multi-language output | `web/` | N/A |
| `codegens/wir/` | WIR infrastructure | `wir/` | N/A |

**Key Takeaway:** Use WIR composer pattern for new codegens when possible. It provides better code reuse, clearer separation of concerns, and easier maintenance.
