# Kryon - Project Guide for Claude Code

This file provides context for Claude Code (AI assistant) when working on the Kryon project.

## Project Overview

**Kryon** is a universal UI framework with a three-stage pipeline architecture:

```
Source (.tsx, .kry, .html, .lua) → KIR (JSON IR) → Target (.tsx, .c, .lua, .nim) → Runtime
```

**Core Philosophy**: Write UI code once in any source language, compile to KIR (Kryon Intermediate Representation), then generate code for any target platform.

## Project Structure

```
kryon/
├── cli/                    # Command-line interface (C)
│   ├── src/commands/       # CLI commands (compile, run, codegen, etc.)
│   └── tsx_parser/         # TypeScript/TSX parser (Bun/TypeScript)
├── ir/                     # Core IR library (C)
│   ├── ir_core.h/c         # Component tree structures
│   ├── ir_logic.h/c        # Logic block (event handlers)
│   ├── ir_executor.h/c     # Universal logic executor
│   └── ir_*_parser.c       # Parsers (Kry, HTML, Markdown)
├── core/                   # Core runtime components
├── codegens/               # Code generators
│   ├── react_common.c      # Shared React/TSX generation
│   ├── tsx/                # TSX-specific codegen
│   ├── lua/                # Lua codegen
│   ├── nim/                # Nim codegen
│   └── c/                  # C codegen
├── backends/               # Runtime backends
│   └── desktop/            # SDL3 desktop renderer
├── renderers/              # Rendering systems
├── platforms/              # Platform-specific code
├── bindings/               # Language bindings (C, Nim, TypeScript)
├── examples/               # Example applications
├── tests/                  # Test files
├── docs/                   # Documentation
├── packages/               # Package components
├── scripts/                # Build/utility scripts
└── third_party/            # External dependencies
```

## Key Technologies

- **C (C99)** - Core IR, parsers, codegens, backends
- **TypeScript/Bun** - TSX parser (`cli/tsx_parser/`)
- **SDL3** - Desktop rendering backend
- **cJSON** - JSON parsing (`ir/third_party/cJSON/`)

## Architecture Layers

### 1. Parsers (Source → KIR)

Convert source languages to KIR JSON format:

- **TSX Parser** (`cli/tsx_parser/tsx_to_kir.ts`) - Parses React/TSX with hooks
- **Kry Parser** (`ir/ir_kry_parser.c`) - Custom .kry DSL
- **HTML Parser** (`ir/ir_html_parser.c`) - HTML5
- **Markdown Parser** (`ir/ir_markdown_parser.c`) - Markdown
- **Lua Parser** (`parsers/lua/ir_lua_parser.c`) - Lua-based UI
- **C Parser** (`parsers/c/ir_c_parser.c`) - C-based UI

### 2. KIR Format

JSON intermediate representation with three sections:

```json
{
  "root": { /* Component tree */ },
  "logic_block": { /* Event handlers */ },
  "reactive_manifest": { /* State & hooks */ }
}
```

### 3. Codegens (KIR → Target)

Generate target language code from KIR:

- **React/TSX** (`codegens/react_common.c`, `codegens/tsx/`)
- **C** (`codegens/c/ir_c_codegen.c`)
- **Lua** (`codegens/lua/lua_codegen.c`)
- **Nim** (`codegens/nim/nim_codegen.c`)

### 4. Runtimes

Execute generated code or KIR directly:

- **Desktop** (`backends/desktop/`) - SDL3 native windows
- **IR Executor** (`ir/ir_executor.c`) - Runs KIR directly with universal logic

## Current State (December 2025)

### ✅ Completed Features

- **TSX Parser** - Full React hook support (useState, useEffect, useCallback, useMemo, useReducer)
- **Type Inference** - Automatic type detection from initial values
- **Event Handlers** - All 9+ event types (onClick, onChange, onFocus, etc.)
- **Universal Logic** - Simple handlers converted to executable IR (count + 1, etc.)
- **TSX Codegen** - Round-trip verified (TSX → KIR → TSX)
- **Desktop Renderer** - SDL3-based native windows
- **Text Interpolation** - Preserves `{variable}` patterns
- **Variable Names** - Preserves useCallback/useMemo variable names

### 🚧 In Progress

- Event handler execution testing
- More complex universal logic patterns
- Component props system

### 📋 Planned

- Conditional rendering preservation
- Array mapping support  
- More component types (Image, Video, Grid)
- Mobile platform support
- WebAssembly runtime

## Code Conventions

### C Code Style

- Follow Linux kernel style
- Use `snake_case` for functions and variables
- Use `PascalCase` for type names
- Header guards: `FILENAME_H`
- Max line length: ~100 characters

Example:
```c
typedef struct IRComponent {
    uint32_t id;
    char* type;
    IRComponent** children;
    int child_count;
} IRComponent;

IRComponent* ir_component_create(const char* type) {
    IRComponent* comp = calloc(1, sizeof(IRComponent));
    comp->type = strdup(type);
    return comp;
}
```

### TypeScript Code Style

- Use TypeScript strict mode
- Use `camelCase` for functions and variables
- Use `PascalCase` for types/interfaces
- Prefer `const` over `let`

Example:
```typescript
interface KIRComponent {
  id?: number;
  type: string;
  children?: KIRComponent[];
}

function parseComponent(node: any): KIRComponent {
  return {
    id: nextId++,
    type: node.type,
    children: node.children?.map(parseComponent)
  };
}
```

### File Naming

- IR core: `ir_<module>.c/h` (e.g., `ir_core.c`)
- Parsers: `<lang>_parser.c` (e.g., `tsx_parser.ts`)
- Codegens: `<lang>_codegen.c` (e.g., `tsx_codegen.c`)
- Tests: `test_<feature>.c`

## Building the Project

```bash
# Build everything
make

# Build specific components
make -C ir
make -C cli
make -C codegens/tsx
make -C backends/desktop

# Install CLI
make -C cli install

# Clean
make clean
```

## Common Tasks

### Adding a New Parser

1. Create `parsers/mylang/mylang_parser.c`
2. Implement `parse_mylang_to_kir_json()`
3. Add to `cli/src/commands/cmd_compile.c`
4. Update docs and capability matrix

See `docs/IMPLEMENTATION_GUIDE.md` for detailed steps.

### Adding a New Codegen

1. Create `codegens/mytarget/mytarget_codegen.c`
2. Implement `mytarget_codegen_generate()`
3. Add to `cli/src/commands/cmd_codegen.c`
4. Update CLI Makefile `CODEGEN_OBJS`

### Adding a Component Type

1. Add to TSX parser component list
2. Add rendering logic to `codegens/react_common.c`
3. Add desktop rendering in `backends/desktop/`
4. Test round-trip

### Testing Changes

```bash
# Round-trip test
bun cli/tsx_parser/tsx_to_kir.ts test.tsx > test.kir
./cli/kryon codegen tsx test.kir test_gen.tsx
bun cli/tsx_parser/tsx_to_kir.ts test_gen.tsx > test2.kir
diff test.kir test2.kir

# Run on desktop
./cli/kryon run test.kir

# Full pipeline
bun cli/tsx_parser/tsx_to_kir.ts app.tsx > app.kir
./cli/kryon codegen c app.kir app.c
gcc app.c -lkryon_desktop -o app
./app
```

## Important Files

### Core IR

- `ir/ir_core.h` - Component tree structures
- `ir/ir_logic.h` - Logic block (handlers, universal statements)
- `ir/ir_executor.c` - Universal logic execution engine
- `ir/ir_serialization.c` - KIR JSON parsing

### TSX Pipeline

- `cli/tsx_parser/tsx_to_kir.ts` - Parse TSX → KIR
- `codegens/react_common.c` - Generate KIR → TSX
- `codegens/tsx/tsx_codegen.c` - TSX-specific wrapper

### Desktop Backend

- `backends/desktop/desktop_renderer.c` - Main renderer
- `backends/desktop/desktop_input.c` - Event handling
- `backends/desktop/ir_to_commands.c` - Convert IR to render commands

## Known Issues & Limitations

### Universal Logic

Only simple patterns convert to universal logic:
- ✅ `setCount(count + 1)` - Supported
- ✅ `setCount(0)` - Supported
- ❌ `setCount(count * 2 + foo.bar)` - Too complex

Complex handlers remain as source strings and need language runtime.

### Component Props

Component-to-component data passing is limited. Props system needs enhancement.

### Conditional Rendering

Ternaries and `&&` operators are evaluated at parse time:
- `{condition ? <A/> : <B/>}` → Evaluates to A or B, loses condition

### Array Mapping

`.map()` is evaluated at parse time:
- `{items.map(item => <Text>{item}</Text>)}` → Expands at parse time

## Development Workflow

1. Make changes in appropriate files
2. Rebuild affected components
3. Test with round-trip verification
4. Test on desktop renderer
5. Update documentation
6. Update capability matrix if needed

## Memory Management

### C Code

```c
// Always free allocated memory
char* str = strdup(input);
// ... use str
free(str);

// For cJSON
cJSON* json = cJSON_Parse(input);
// ... use json
cJSON_Delete(json);

// For IRComponent
IRComponent* comp = ir_component_create("Text");
// ... use comp
ir_component_free(comp);
```

### TypeScript/Bun

Garbage collected, but be mindful of large arrays and closures.

## Debugging

### Enable Debug Output

```bash
# TSX parser debug
DEBUG_TSX_PARSER=1 bun cli/tsx_parser/tsx_to_kir.ts input.tsx

# IR executor debug
# (Already prints to stdout)
```

### Common Issues

**"Failed to parse KIR"**: Check JSON validity with `jq . file.kir`

**"Component not found"**: Ensure component type is registered in parser

**"Handler not executing"**: Check if universal logic was generated or if language runtime is needed

**Segfault**: Run with `valgrind` to find memory issues

## Extending Kryon

See `docs/IMPLEMENTATION_GUIDE.md` for detailed guides on:
- Adding parsers
- Adding codegens
- Adding components
- Adding event types
- Adding React hooks
- Adding universal logic patterns

## Documentation

- **Architecture**: `docs/ARCHITECTURE.md` - Full system architecture
- **Getting Started**: `docs/GETTING_STARTED.md` - User guide
- **Implementation**: `docs/IMPLEMENTATION_GUIDE.md` - Developer guide
- **Configuration**: `docs/configuration.md` - Config system

## When Working on This Project

1. **Read the docs** - Check architecture and implementation guides first
2. **Follow conventions** - Match existing code style
3. **Test thoroughly** - Verify round-trip and rendering
4. **Update docs** - Keep documentation current
5. **Maintain capability matrix** - Update when adding features

## Quick Reference

### File Extensions

- `.kir` - KIR JSON format
- `.kry` - Kryon DSL (planned)
- `.krb` - Kryon binary format
- `.tsx` - TypeScript React
- `.c/.h` - C source/header

### CLI Commands

- `kryon compile` - Source → KIR
- `kryon codegen` - KIR → Target
- `kryon run` - Execute KIR on desktop
- `kryon inspect` - View KIR structure
- `kryon diff` - Compare KIR files

### Environment Variables

- `DEBUG_TSX_PARSER` - Enable TSX parser debug output
- `KRYON_ROOT` - Override project root path

---

**Last Updated**: December 28, 2025  
**Kryon Version**: 1.0.0  
**Status**: TSX pipeline complete, desktop renderer working
