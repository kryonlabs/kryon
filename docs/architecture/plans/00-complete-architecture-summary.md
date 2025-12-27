# Complete Kryon Architecture - Implementation Summary (UPDATED)

## Executive Summary

This document provides a comprehensive overview of the complete Kryon architecture based on the approved final design.

## Core Principle: KIR-Centric Pipeline

**Every frontend flows through KIR. All targets and codegens consume KIR.**

```
┌─────────────────────────────────────────────────────────────────┐
│                    FRONTENDS (Parsers)                          │
│  .kry  .tsx  .html  .md  .c  .nim  .lua  .rs                   │
└─────────────────────────────────────────────────────────────────┘
                            ↓
                ┌───────────────────────┐
                │    KIR JSON       │
                │ (Universal IR Format) │
                └───────────────────────┘
                            ↓
        ┌───────────────────┼───────────────────┬──────────────┐
        ↓                   ↓                   ↓              ↓
   ┌─────────┐        ┌──────────┐       ┌──────────┐   ┌─────────┐
   │VM TARGET│        │NATIVE    │       │CODEGENS  │   │  WASM   │
   │         │        │TARGET    │       │          │   │ TARGET  │
   └─────────┘        └──────────┘       └──────────┘   └─────────┘
        ↓                   ↓                   ↓              ↓
   .krb File          C/Nim/Rust         .nim .lua        .wasm
        ↓              Source Code        .c .html         Module
        ↓                   ↓             .tsx .rs            ↓
  VM Runtime          Native Compiler         │         WASM Runtime
        ↓                   ↓                  ↓              ↓
  ┌─────────┐         Platform Binary    Source Files   Browser/
  │ANY      │              ↓              (for users)    Native
  │RENDERER │         ┌─────────┐                           ↓
  │         │         │Renderer │                      Canvas/
  │SDL3     │         │Embedded │                      WebGL/
  │Terminal │         │in Binary│                      DOM
  │DOM      │         └─────────┘
  │Embedded │
  └─────────┘
```

## Four Execution Paths

### Path 1: VM Target (.krb Bytecode)

**Portable bytecode that runs on ANY renderer**

```
KIR → .krb Compiler → .krb File → VM Runtime → Any Renderer
```

**Key Features:**
- ✅ Platform-independent bytecode
- ✅ Single file runs everywhere
- ✅ Can use ANY renderer (SDL3, Terminal, DOM, Embedded)
- ✅ Runtime selects appropriate renderer

**Example:**
```bash
# Build to bytecode
kryon build app.tsx --target=krb -o app.krb

# Run on different renderers
kryon run app.krb --renderer=sdl3      # Desktop
kryon run app.krb --renderer=terminal  # Terminal UI
kryon run app.krb --renderer=dom       # Web (Node.js VM)
kryon run app.krb --renderer=embedded  # STM32/RP2040
```

### Path 2: Native Target (Platform-Specific Binary)

**Native compilation via IR pipeline**

```
KIR → Source Codegen (in-memory) → Native Compiler → Binary
```

**Key Features:**
- ✅ Uses IR pipeline (KIR → C/Nim/Rust source)
- ✅ Source can be generated in-memory (no files)
- ✅ Platform-specific binary (x86_64, ARM, STM32)
- ✅ Renderer compiled into binary
- ✅ Maximum performance

**Example:**
```bash
# Desktop binary
kryon build app.kry --target=native --lang=c --renderer=sdl3 -o myapp
./myapp  # Runs directly

# Cross-compile for ARM
kryon build app.kry --target=native --lang=c --platform=arm64 -o myapp_arm

# Embedded (STM32)
kryon build app.kry --target=native --lang=c --platform=stm32 --renderer=framebuffer
```

### Path 3: Codegens (Source Files)

**Generate source code in any language**

```
KIR → Language Codegen → Source Files (.nim, .lua, .c, .html, .tsx)
```

**All codegens are equal - HTML is NOT special:**
- Nim codegen → .nim files
- Lua codegen → .lua files
- C codegen → .c files
- **HTML codegen → .html/.css/.js files** (with extra flags)
- TSX codegen → .tsx files

**HTML Codegen Flags:**
```bash
# Basic HTML generation
kryon codegen html app.kir -o dist/

# Web deployment (with flags)
kryon codegen html app.kir -o dist/ --minify --bundle --inline-css
```

**Key Point:** HTML codegen just has web-specific flags. It's not a "renderer" - the browser's DOM engine is the renderer.

**Round-Trip Example:**
```bash
# HTML → KIR → Lua → KIR → HTML
kryon compile app.html -o app.kir
kryon codegen lua app.kir -o app.lua
kryon compile app.lua -o app2.kir
kryon codegen html app2.kir -o dist/
```

### Path 4: WASM Target

**WebAssembly compilation via native OR VM pipeline**

```
Option A: KIR → C Source → emscripten → .wasm
Option B: KIR → .krb → wasm-compiler → .wasm
```

**Key Features:**
- ✅ Can use native pipeline (C→WASM)
- ✅ Can use VM pipeline (.krb→WASM)
- ✅ Runs in browser or standalone WASM runtime
- ✅ High performance web deployment

**Example:**
```bash
# Via native pipeline
kryon build app.kry --target=wasm --lang=c -o app.wasm

# Via VM pipeline
kryon build app.kry --target=wasm --via-vm -o app.wasm
```

## Target Comparison

| Target | Input | Output | Portable? | Renderer | Performance |
|--------|-------|--------|-----------|----------|-------------|
| **VM** | KIR | .krb | ✅ Yes | Any (runtime choice) | Good |
| **Native** | KIR | Binary | ❌ Platform-specific | Compiled in | Excellent |
| **Codegen** | KIR | Source Files | N/A | N/A (files only) | N/A |
| **WASM** | KIR | .wasm | ✅ Web-portable | Canvas/WebGL/DOM | Excellent |

## The 6 Pillars of Implementation

### 1. KIR Format (Plan 01)

**Complete intermediate representation**

Must support:
- Event handlers as source code
- Reactive state declarations
- Metadata for all targets
- Logic blocks

**Status:** Needs implementation
**Effort:** 3 weeks

### 2. Parser Pipeline (Plan 02)

**All parsers produce KIR**

- ✅ .kry parser (complete)
- ✅ .html parser (good)
- ✅ .md parser (complete)
- ⚠️ .tsx parser (needs AST rewrite)
- ⚠️ .c parser (needs libclang rewrite)
- ❌ .nim parser (needs implementation)
- ❌ .lua parser (needs implementation)

**Status:** Partial
**Effort:** 3.5 weeks

### 3. Codegen Implementation (Plan 03)

**Generate idiomatic source code from KIR**

All codegens are equal:
- Nim codegen
- Lua codegen
- C codegen
- HTML codegen (with --minify, --bundle flags)
- TSX codegen

**Status:** Partial
**Effort:** 5 weeks

### 4. VM Target (.krb) (Plan 04)

**Bytecode compilation and runtime**

- KIR → .krb compiler
- C-based VM runtime
- TypeScript VM runtime
- Runs on ANY renderer

**Status:** Partial
**Effort:** 4 weeks

### 5. Native Target (Plan 05)

**Platform-specific binary compilation**

- KIR → Source codegen (in-memory)
- Native compiler integration
- Cross-compilation support
- Embedded targets (STM32, RP2040)

**Status:** Partial
**Effort:** 4.5 weeks

### 6. WASM Target (New)

**WebAssembly compilation**

- Via native: KIR → C → emscripten → .wasm
- Via VM: KIR → .krb → wasm-compiler → .wasm
- Browser and native WASM runtime support

**Status:** Not implemented
**Effort:** 2 weeks

## Complete Workflow Examples

### Example 1: Portable Application

```bash
# Write once in .kry
cat > app.kry << 'EOF'
Container {
  @state count: int = 0

  Button {
    text: "Count: {count}"
    onClick: { count += 1 }
  }
}
EOF

# Build to bytecode (portable)
kryon build app.kry --target=krb -o app.krb

# Run on different platforms
kryon run app.krb --renderer=sdl3      # Desktop
kryon run app.krb --renderer=terminal  # Terminal
kryon run app.krb --renderer=embedded  # Embedded device
```

### Example 2: Native Binary

```bash
# Write in TypeScript
cat > app.tsx << 'EOF'
import { Container, Button } from 'kryon';

export default function App() {
  const [count, setCount] = useState(0);

  return (
    <Container>
      <Button onClick={() => setCount(count + 1)}>
        Count: {count}
      </Button>
    </Container>
  );
}
EOF

# Build to native binary
kryon build app.tsx --target=native --lang=c --renderer=sdl3 -o myapp

# Run
./myapp  # Standalone executable, no dependencies
```

### Example 3: Multi-Platform Deployment

```bash
# Single source
kryon compile app.kry -o app.kir

# Deploy everywhere:
kryon build app.kir --target=krb -o app.krb                    # Portable bytecode
kryon build app.kir --target=native --lang=c -o app_linux      # Linux binary
kryon build app.kir --target=native --platform=windows -o app.exe  # Windows
kryon build app.kir --target=wasm -o app.wasm                  # WebAssembly
kryon codegen html app.kir -o dist/ --minify                   # Static website
```

### Example 4: Round-Trip Conversion

```bash
# TypeScript → KIR → Nim → KIR → Lua
kryon compile app.tsx -o app.kir
kryon codegen nim app.kir -o app.nim
kryon compile app.nim -o app2.kir
kryon codegen lua app2.kir -o app.lua

# All KIR files are semantically equivalent
diff app.kir app2.kir  # Should match
```

## Implementation Roadmap

### Total Effort: ~22 weeks (5.5 months)

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| 1. KIR | 3 weeks | Complete IR format |
| 2. Parsers | 3.5 weeks | All parsers → KIR |
| 3. Codegens | 5 weeks | All codegens from KIR |
| 4. VM Target | 4 weeks | .krb bytecode system |
| 5. Native Target | 4.5 weeks | Binary compilation |
| 6. WASM Target | 2 weeks | WebAssembly support |

## Success Criteria

✅ **KIR-Centric Pipeline:**
- All frontends produce KIR
- All targets consume KIR
- No language bypassing

✅ **Four Targets Work:**
- VM: .krb runs on any renderer
- Native: Platform-specific binaries
- Codegen: Source files in any language
- WASM: WebAssembly modules

✅ **Round-Trip Capability:**
- Source → KIR → Source (any language)
- Semantically equivalent

✅ **HTML is Just a Codegen:**
- Not treated specially
- Has web deployment flags
- Browser DOM is the renderer

✅ **Performance:**
- Binaries: <1MB for simple apps
- .krb files: 70% smaller than JSON
- Startup: <100ms

## File Organization

```
/mnt/storage/Projects/kryon/
├── ir/                          # IR Core
│   ├── ir_core.h               # Component system
│   ├── ir_logic.h              # Logic blocks
│   ├── ir_json_v2.c            # KIR serialization
│   ├── ir_krb.c                # .krb compiler
│   ├── ir_krb_vm.c             # VM runtime
│   └── parsers/                # All parsers → KIR
├── codegens/                    # All codegens from KIR
│   ├── nim/
│   ├── lua/
│   ├── c/
│   ├── html/                    # HTML codegen (with flags)
│   └── tsx/
├── targets/                     # Target implementations
│   ├── vm/                      # .krb bytecode target
│   ├── native/                  # Native binary target
│   └── wasm/                    # WASM target
├── renderers/                   # Rendering backends
│   ├── sdl3/
│   ├── terminal/
│   └── framebuffer/
└── cli/                         # CLI commands
    └── src/commands/
        ├── cmd_compile.c        # Source → KIR
        ├── cmd_codegen.c        # KIR → Source
        ├── cmd_build.c          # KIR → Target
        └── cmd_run.c            # Execute
```

## Next Steps

1. ✅ Architecture approved
2. ⏭️ Update all 6 implementation plans
3. ⏭️ Begin Phase 1: KIR implementation
4. ⏭️ Implement parsers (Phase 2)
5. ⏭️ Implement codegens (Phase 3)
6. ⏭️ Implement targets (Phases 4-6)

---

**Architecture Status:** ✅ Approved and Ready for Implementation

**Last Updated:** 2025-01-27
