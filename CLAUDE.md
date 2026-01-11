# Kryon - Project Guide for Claude Code

This file provides context for Claude Code (AI assistant) when working on the Kryon project.

## Documentation

**Read these first:**
- `docs/ARCHITECTURE.md` - Full system architecture
- `docs/GETTING_STARTED.md` - User guide

## Development Rules

You are a senior software developer. Follow these rules:

- **No fallbacks** - Never implement fallbacks unless explicitly requested
- **No legacy preservation** - Delete unused code completely; never preserve unless explicitly requested
- **Respect architecture** - Implement fixes and features aligned with the modular design
- **Use Nix for dependencies** - Check for `flake.nix` or `shell.nix` before using ad-hoc installation

## Project Overview

**Kryon** is a universal UI framework with a three-stage pipeline:

```
Source (.tsx, .kry, .html, .lua) → KIR (JSON IR) → Target (.tsx, .c, .lua, .nim) → Runtime
```

**Core Philosophy**: Write UI once, compile to KIR (Kryon Intermediate Representation), generate code for any platform.

## Project Structure

```
kryon/
├── cli/            # Command-line interface (C)
├── ir/             # Core IR library (C)
│   ├── ir_core.h/c         # Component tree structures
│   ├── ir_logic.h/c        # Logic block (event handlers)
│   ├── ir_executor.h/c     # Universal logic executor
│   ├── ir_builder.h/c      # Component builder API
│   ├── ir_layout_builder.c # Layout properties
│   ├── ir_style_builder.c  # Style setters
│   ├── ir_event_builder.c  # Event management
│   ├── ir_tabgroup.c       # Tab component state
│   └── parsers/            # Language parsers
├── codegens/       # Code generators (TSX, Lua, Nim, C)
├── backends/       # Runtime backends
│   └── desktop/    # SDL3 desktop renderer
├── renderers/      # Rendering systems (SDL3, Raylib)
├── bindings/       # Language bindings (C, Nim, TypeScript)
├── examples/       # Example applications
└── docs/           # Documentation
```

## Key Technologies

- **C (C99)** - Core IR, parsers, codegens, backends
- **TypeScript/Bun** - TSX parser
- **SDL3** - Desktop rendering backend
- **cJSON** - JSON parsing

## Shared Utilities

### ir_log - Unified Logging

`ir/ir_log.h`, `ir/ir_log.c`

```c
IR_LOG_DEBUG("TAG", "Debug: %d", value);
IR_LOG_INFO("TAG", "Info");
IR_LOG_WARN("TAG", "Warning: %s", msg);
IR_LOG_ERROR("TAG", "Error");
```

### ir_string_builder - String Builder

`ir/ir_string_builder.h`, `ir/ir_string_builder.c`

```c
IRStringBuilder* sb = ir_sb_create(8192);
ir_sb_append(sb, "Hello ");
ir_sb_appendf(sb, "%s %d", "world", 42);
char* result = ir_sb_build(sb);  // Ownership transfers
ir_sb_free(sb);
```

### ir_json_helpers - Safe cJSON Wrappers

`ir/ir_json_helpers.h`, `ir/ir_json_helpers.c`

```c
cJSON_AddStringOrNull(obj, "key", potentially_null_value);
const char* str = cJSON_GetStringSafe(item, "default");
```

## Code Conventions

### C Code Style

- Linux kernel style
- `snake_case` for functions/variables
- `PascalCase` for types
- Header guards: `FILENAME_H`
- Max line length: ~100 characters

### File Naming

- IR core: `ir_<module>.c/h`
- Parsers: `<lang>_parser.c`
- Codegens: `<lang>_codegen.c`

## Building

```bash
make                    # Build everything
make -C ir              # Build IR library
make -C cli             # Build CLI
make -C cli install     # Install CLI
make clean              # Clean
```

## Common Tasks

### Adding a New Parser

1. Create `parsers/mylang/mylang_parser.c`
2. Implement `parse_mylang_to_kir_json()`
3. Add to `cli/src/commands/cmd_compile.c`

### Adding a New Codegen

1. Create `codegens/mytarget/mytarget_codegen.c`
2. Implement `mytarget_codegen_generate()`
3. Add to `cli/src/commands/cmd_codegen.c`

### Adding a Component Type

1. Add to TSX parser component list
2. Add rendering logic to `codegens/react_common.c`
3. Add desktop rendering in `backends/desktop/`
4. Test round-trip

### Testing Changes

```bash
# Round-trip test
bun ir/parsers/tsx/tsx_to_kir.ts test.tsx > test.kir
./cli/kryon codegen tsx test.kir test_gen.tsx
bun ir/parsers/tsx/tsx_to_kir.ts test_gen.tsx > test2.kir
diff test.kir test2.kir

# Run on desktop
./cli/kryon run test.kir
```

## Important Files

### Core IR
- `ir/ir_core.h` - Component tree structures
- `ir/ir_logic.h` - Logic block (handlers, universal statements)
- `ir/ir_executor.c` - Universal logic execution
- `ir/ir_serialization.c` - KIR JSON parsing

### TSX Pipeline
- `ir/parsers/tsx/tsx_to_kir.ts` - TSX → KIR
- `codegens/react_common.c` - KIR → TSX
- `codegens/tsx/tsx_codegen.c` - TSX wrapper

### Desktop Backend
- `backends/desktop/desktop_renderer.c` - Main renderer
- `backends/desktop/desktop_input.c` - Event handling

## Known Limitations

- Universal Logic: Only simple patterns (`setCount(count + 1)`)
- Component Props: Data passing is limited
- Conditional Rendering: Ternaries evaluated at parse time
- Array Mapping: `.map()` expanded at parse time

## Quick Reference

### File Extensions
- `.kir` - KIR JSON format
- `.kry` - Kryon DSL
- `.tsx` - TypeScript React
- `.c/.h` - C source/header

### CLI Commands
- `kryon compile` - Source → KIR
- `kryon codegen` - KIR → Target
- `kryon run` - Execute KIR on desktop
- `kryon inspect` - View KIR structure
- `kryon diff` - Compare KIR files

---

**Last Updated**: January 11, 2026
**Kryon Version**: 1.0.0
