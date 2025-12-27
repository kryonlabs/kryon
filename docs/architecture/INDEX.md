# Kryon Architecture Documentation - Index

## Overview

This directory contains comprehensive architectural documentation and implementation plans for the complete Kryon UI framework redesign.

**Total Documentation:** 4,024 lines across 6 detailed plans

## Quick Links

### Start Here
- **[Complete Architecture Summary](plans/00-complete-architecture-summary.md)** (15KB) - Read this first for the big picture

### Implementation Plans

1. **[KIR Format Specification](plans/01-kir-format-specification.md)** (14KB)
   - Complete KIR format design
   - Event handler preservation
   - Reactive state manifest
   - Metadata system
   - **Effort:** 16 days (3 weeks)

2. **[Parser Pipeline Unification](plans/02-parser-pipeline-unification.md)** (19KB)
   - Fix TSX parser (TypeScript AST)
   - Fix C parser (libclang)
   - Implement Nim parser
   - Implement Lua parser
   - **Effort:** 18 days (3.5 weeks)

3. **[Complete Codegen Implementation](plans/03-codegen-complete-implementation.md)** (26KB)
   - Complete Nim codegen
   - Complete Lua codegen
   - Complete C codegen
   - Complete TSX codegen
   - HTML/JS event handlers
   - **Effort:** 26 days (5 weeks)

4. **[Bytecode VM System](plans/04-bytecode-vm-system.md)** (22KB)
   - .krb bytecode format
   - KIR → KRB compiler
   - C-based VM runtime
   - Desktop integration
   - Web VM updates
   - **Effort:** 19 days (4 weeks)

5. **[Binary Compilation Pipeline](plans/05-binary-compilation-pipeline.md)** (16KB)
   - Complete `kryon build` command
   - Static linking
   - Cross-compilation
   - Embedded targets (STM32, RP2040)
   - Packaging & distribution
   - **Effort:** 22 days (4.5 weeks)

## Architecture Principles

### 1. Universal IR Pipeline

**Every frontend flows through KIR (Kryon Intermediate Representation)**

```
All Frontends → KIR → All Backends
(.kry, .tsx, .c, .nim, .lua, .html, .md)
```

### 2. Dual Execution Modes

**VM Mode:** KIR → .krb bytecode → VM Runtime → Renderer
- JavaScript, Lua, Python
- Cross-platform bytecode
- ~70% smaller than JSON

**Native Mode:** KIR → Source Codegen → Native Compiler → Binary
- C, Nim, Rust
- Fully optimized executables
- No external dependencies (static linking)

### 3. Complete Round-Trip

**Source ↔ KIR ↔ Source (in any language)**

```bash
app.tsx → KIR → app.c → KIR → app.nim → KIR
# All semantically equivalent!
```

### 4. No Language Bypassing

**Clean separation enforced by architecture:**
- Parsers ONLY produce KIR
- Codegens ONLY consume KIR
- No direct language-to-language conversion
- Single source of truth (KIR)

## Implementation Roadmap

### Total Effort: ~22 weeks (5.5 months)

| Phase | Duration | Key Deliverables |
|-------|----------|------------------|
| 1. KIR Foundation | 3 weeks | KIR structures, serialization |
| 2. Parser Unification | 3.5 weeks | All parsers produce KIR |
| 3. Codegen Completion | 5 weeks | All codegens from KIR |
| 4. Bytecode VM | 4 weeks | .krb format + VM runtime |
| 5. Binary Compilation | 4.5 weeks | Full build pipeline |
| 6. Testing & Polish | 2 weeks | Comprehensive testing |

## Key Features

### Parsers (Input)
- ✅ .kry - Kryon DSL (100% complete)
- ✅ .html - HTML with data-* attributes (95% complete)
- ✅ .md - Markdown CommonMark (98% complete)
- ⚠️ .tsx - TypeScript/JSX (needs AST-based rewrite)
- ⚠️ .c - C with Kryon API (needs libclang rewrite)
- ❌ .nim - Nim DSL (needs implementation)
- ❌ .lua - Lua API (needs implementation)

### Codegens (Output)
- ⚠️ HTML/CSS - 70% complete (needs event handlers)
- ⚠️ TSX/JSX - 60% complete (needs hooks)
- ❌ Nim - 20% complete (needs full implementation)
- ❌ Lua - 30% complete (needs full implementation)
- ❌ C - 10% stub (needs full implementation)

### Backends (Rendering)
- ✅ SDL3 - Desktop rendering (complete)
- ✅ Terminal - Terminal UI (complete)
- ⚠️ Web - DOM/Canvas (partial)
- ⚠️ Embedded - STM32/RP2040 (partial)

## File Structure

```
docs/architecture/
├── INDEX.md (this file)
├── README.md (overview)
└── plans/
    ├── 00-complete-architecture-summary.md
    ├── 01-kir-format-specification.md
    ├── 02-parser-pipeline-unification.md
    ├── 03-codegen-complete-implementation.md
    ├── 04-bytecode-vm-system.md
    └── 05-binary-compilation-pipeline.md
```

## Reading Guide

### For Implementers

**Start here:**
1. Read [00-complete-architecture-summary.md](plans/00-complete-architecture-summary.md)
2. Pick a phase from the roadmap
3. Read the corresponding plan in detail
4. Review affected code files
5. Start implementation

### For Reviewers

**Architecture review:**
1. Check [00-complete-architecture-summary.md](plans/00-complete-architecture-summary.md) for overall design
2. Verify IR pipeline is truly universal
3. Check for language bypassing (should be none)
4. Review success criteria in each plan

### For Project Managers

**Effort estimation:**
- See roadmap above for timeline
- Each plan has detailed effort breakdown
- Dependencies are clearly marked
- Testing time included

## Success Criteria

✅ **Complete IR Pipeline**
- All frontends produce KIR with 100% fidelity
- Event handlers preserved as source code
- Reactive state fully represented

✅ **Round-Trip Capability**
- Source → KIR → Source (equivalent semantics)
- Works for all supported languages

✅ **Dual Execution**
- VM mode: .krb bytecode execution
- Native mode: compiled binaries

✅ **Cross-Platform**
- Desktop (Linux, Windows, macOS)
- Web (Browser, Node.js)
- Embedded (STM32, RP2040)

✅ **Performance**
- Binaries: <1MB for simple apps
- .krb files: 70% smaller than JSON
- Startup time: <100ms

## Dependencies

### Build Tools
- gcc/clang (C compilation)
- nim (Nim compilation)
- cargo (Rust compilation)
- bun (TypeScript/JSX parsing)

### Libraries
- libclang (C AST parsing)
- SDL3 (Desktop rendering)
- @typescript-eslint/typescript-estree (TSX parsing)

### Development
- cJSON (JSON parsing)
- libfmt (String formatting)
- Unity (C testing framework)

## Next Steps

1. ✅ Architecture complete - All plans written
2. ⏭️ Review and approve architecture
3. ⏭️ Set up development environment
4. ⏭️ Begin Phase 1: KIR Foundation
5. ⏭️ Implement remaining phases

## Contributing

When implementing:
1. Read the relevant plan thoroughly
2. Check success criteria before starting
3. Write tests alongside implementation
4. Update documentation as you go
5. Submit for review when complete

## Questions?

- Architecture questions: See [00-complete-architecture-summary.md](plans/00-complete-architecture-summary.md)
- Implementation details: See individual plan files
- File locations: See "Files to Create/Modify" in each plan
- Effort estimates: See "Estimated Effort" in each plan

---

**Documentation Status:** ✅ Complete (4,024 lines)

**Last Updated:** 2025-01-27

**Maintainer:** Architecture Team
