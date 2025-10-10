# Kryon-Nim Roadmap

**Date**: October 9, 2025
**Project**: Kryon UI Framework - Nim Implementation

---

## Overview

This roadmap outlines the implementation plan for porting Kryon from a custom C-based language (klang) to a Nim-based DSL implementation. The goal is to leverage Nim's powerful macro system and proven infrastructure to build a production-ready UI framework in 16 weeks.

---

## Project Goals

1. **Fast Time to Market**: Ship production-ready framework in 4 months
2. **Multiple Renderers**: Support raylib (native), HTML (web), and extensible architecture
3. **Clean Syntax**: Use Nim's clean, Pythonic syntax for .kry files
4. **Reuse Existing Work**: Integrate with existing kryon C rendering code via FFI
5. **Developer Experience**: Excellent tooling, clear errors, fast compilation

---

## Phase 1: Foundation ✅ COMPLETE

### Week 1: Core Infrastructure ✅
- [x] Create project structure
- [x] Set up Nim package (kryon.nimble)
- [x] Core types and data structures
- [x] Basic element system
- [x] Property handling
- [x] All 12 element macros (Container, Text, Button, Column, Row, Center, etc.)
- [x] DSL property parsing with expression support
- [x] Event handler system
- [x] Layout engine (Column, Row, Center)
- [x] Three working examples

**Status**: Completed in 1 session

---

## Phase 2: Rendering Backend ✅ COMPLETE

### Raylib Integration ✅
- [x] FFI bindings to Raylib 5.0.0
- [x] Raylib backend implementation
- [x] Element → Rendering pipeline
- [x] Layout calculation
- [x] 60 FPS event loop
- [x] Mouse input handling
- [x] onClick event handlers working
- [x] Real window rendering

**Status**: Completed in same session as Phase 1

### CLI Tool ✅
- [x] `kryon runKryon` command
- [x] `kryon build` command
- [x] `kryon info` command
- [x] `kryon version` command
- [x] Renderer autodetection
- [x] Renderer selection (--renderer flag)

**Status**: Completed ahead of schedule

---

## Phase 3: Advanced Elements & Layout 🔄 IN PROGRESS

### Remaining Element Types
- [x] Input (text input with keyboard handling) ✅ **COMPLETE**
- [ ] Checkbox (toggle state) 🔄 **NEXT**
- [ ] Dropdown (select menu)
- [ ] Image (load and display images)
- [ ] ScrollView (scrollable content)

### Advanced Layout
- [x] Center layout fixed (two-pass layout algorithm) ✅ **COMPLETE**
- [ ] Grid layout implementation
- [ ] Padding and margin support
- [ ] Min/max width/height constraints
- [ ] Percentage-based sizing
- [ ] Flexbox-like alignment options

**Deliverables**:
- All 12 element types fully functional
- Complete layout system
- More complex example applications

**Progress**: 1/5 elements complete, Center layout fixed

---

## Phase 4: Reactive State & Re-rendering

### Reactive State Management
- [ ] State change detection
- [ ] Automatic re-rendering on state change
- [ ] Component-local state
- [ ] State update batching
- [ ] Efficient diffing algorithm

**Deliverables**:
- State changes trigger UI updates
- Counter example auto-updates
- Performance optimization for re-renders

---

## Phase 5: Styling & Theming

### Styling System
- [ ] CSS-like properties (margin, padding, border-radius, etc.)
- [ ] Style composition
- [ ] Hover/active/focus states
- [ ] Gradients and shadows
- [ ] Custom fonts
- [ ] Theme system

**Deliverables**:
- Rich styling capabilities
- Theme switching
- Beautiful example apps

---

## Phase 6: Multi-Backend Support

### HTML/Web Backend
- [ ] Compile to JavaScript target
- [ ] HTML element generation
- [ ] DOM manipulation
- [ ] CSS generation
- [ ] Web event handling
- [ ] Browser compatibility

**Deliverables**:
- Same .nim code runs in browser
- HTML backend with clean markup
- Interactive web UIs

### Terminal Backend (Bonus)
- [ ] Terminal UI library integration
- [ ] ASCII/Unicode rendering
- [ ] Keyboard navigation
- [ ] Color support

**Deliverables**:
- Terminal-based UIs
- CLI applications with rich interfaces

---

## Phase 7: Developer Experience & Tooling

### Hot Reload
- [ ] File watching
- [ ] Incremental compilation
- [ ] State preservation during reload
- [ ] `kryon dev` command

### Documentation & Examples
- [ ] API documentation generation
- [ ] Interactive example gallery
- [ ] Tutorial series
- [ ] Migration guides

**Deliverables**:
- Fast development workflow
- Comprehensive documentation
- Easy onboarding

---

## Phase 8: Polish & Release

### Week 15: Testing & Examples
- [ ] Convert all existing examples
- [ ] Unit tests for core system
- [ ] Integration tests
- [ ] Performance benchmarks
- [ ] Bug fixes

**Deliverables**:
- All examples working perfectly
- Comprehensive test suite
- No critical bugs

### Week 16: Documentation & Launch
- [ ] User guide
- [ ] API documentation
- [ ] Tutorial series
- [ ] Migration guide (from old kryon)
- [ ] 1.0 release!

**Deliverables**:
- Production-ready release
- Complete documentation
- Ready for users

---

## Current Status (2025-10-10)

### ✅ Completed in Session 1 (2025-10-09):
1. **Phases 1 & 2** - Foundation and Rendering
   - Full DSL with 12 element macros
   - Real Raylib integration (60 FPS)
   - Complete CLI tool with multiple commands
   - Three working examples
   - Comprehensive documentation

### ✅ Completed in Session 2 (2025-10-10):
1. **Phase 3 Progress** - Advanced Elements
   - ✅ Input element with keyboard handling
   - ✅ Center layout fixed (two-pass algorithm)
   - ✅ Element hash function for Table keys
   - ✅ Input demo example

### 📋 Next Up (Phase 3):
1. **Remaining Elements**
   - Checkbox (toggle state)
   - Dropdown (select menu)
   - Image (load and display)
   - ScrollView (scrollable content)

2. **Advanced Layout**
   - Grid layout
   - Padding/margin
   - Size constraints
   - Flexbox-like alignment

### 🎯 Immediate Priorities:
- Implement Checkbox element with toggle state
- Implement Dropdown element
- Enhance layout engine with padding/margin
- Create more complex example applications

---

## File Extension Decision

**Options:**

1. **Keep .kry extension** (Kryon files)
   - Pro: Distinct identity
   - Pro: Syntax highlighting can be configured
   - Con: Editors need custom config

2. **Use .nim extension** (Nim files)
   - Pro: Native Nim syntax highlighting
   - Pro: IDE support works immediately
   - Pro: Standard Nim tools work
   - Con: Less distinct identity

**Recommendation**: Start with **.nim extension** for better tooling support. Files can use naming convention like `app_ui.nim` or `myapp.kryon.nim` to indicate they're Kryon UI definitions.

**Alternative**: Support both - .nim for development, .kry for distribution/examples.

---

## Architecture Overview

```
┌─────────────────────────────────────────┐
│     User Application (.nim or .kry)     │
│   Uses Kryon DSL macros                 │
└─────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────┐
│        Kryon Nim Macro System           │
│  src/kryon/dsl.nim                      │
│  - Parse declarative syntax             │
│  - Generate Element tree                │
│  - Compile-time optimizations           │
└─────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────┐
│           Nim Compiler                  │
│  - Type checking                        │
│  - Backend selection                    │
│  - Optimization                         │
└─────────────────────────────────────────┘
                 ↓
    ┌─────────────────────┐
    │  Backend Selection  │
    └─────────────────────┘
       ↓              ↓
┌─────────────┐  ┌──────────────┐
│  C Backend  │  │  JS Backend  │
│  (raylib)   │  │  (HTML/DOM)  │
└─────────────┘  └──────────────┘
       ↓              ↓
┌─────────────┐  ┌──────────────┐
│   Native    │  │   Browser    │
│   Window    │  │   Rendering  │
└─────────────┘  └──────────────┘
```

---

## Technology Stack

### Core
- **Language**: Nim 2.0+
- **Build System**: Nimble
- **Metaprogramming**: Nim macros

### Backends
- **Native (C)**: Raylib (via existing kryon C code)
- **Web (JS)**: Nim JavaScript backend → HTML/DOM
- **Future**: SDL2, Terminal, Mobile

### Tooling
- **CLI**: Nim `cligen` library
- **Testing**: Nim unittest
- **Documentation**: Nim doc comments → HTML

---

## Example: Hello World Evolution

### Old Kryon (.kry):
```kry
Container {
    width = 200
    height = 100
    backgroundColor = "#191970FF"

    Text {
        text = "Hello World"
        color = "yellow"
    }
}
```

### New Kryon-Nim (.nim):
```nim
import kryon

kryonApp:
  Container:
    width: 200
    height: 100
    backgroundColor: "#191970FF"

    Text:
      text: "Hello World"
      color: "yellow"
```

**Changes**:
- `=` → `:` (Nim syntax)
- `{ }` → indentation (Nim style)
- Import kryon module
- Wrap in `kryonApp:` macro

---

## Migration Strategy from Old Kryon

### What We Keep:
1. ✅ UI element concepts (Container, Button, Text, etc.)
2. ✅ Property system
3. ✅ Event handling model
4. ✅ Rendering C code (via FFI)
5. ✅ Examples (converted syntax)

### What We Replace:
1. ❌ Custom klang compiler → Nim compiler
2. ❌ Custom parser → Nim macros
3. ❌ Custom type system → Nim types
4. ❌ .krb binary format → Native executables (or keep as intermediate)

### What We Gain:
1. ✅ Nim standard library
2. ✅ Nim package ecosystem
3. ✅ Nim tooling (debugger, profiler, LSP)
4. ✅ Multiple backends (C, JS, etc.)
5. ✅ Compile-time execution
6. ✅ Faster development

---

## Success Metrics

### Phase 1-2 (Session 1): ✅ ACHIEVED
- [x] Hello World example working
- [x] All 12 element macros functional
- [x] Can compile and run
- [x] Raylib backend rendering at 60 FPS
- [x] CLI tool with 4 commands
- [x] 3 examples working
- [x] Comprehensive documentation

### Phase 3 (Next):
- [ ] All 12 elements fully implemented
- [ ] Advanced layout engine complete
- [ ] 10+ examples working

### Phase 4-5:
- [ ] Reactive state management
- [ ] Rich styling system
- [ ] Theme support

### Phase 6-8:
- [ ] Multi-backend support (Raylib + HTML + Terminal)
- [ ] Hot reload system
- [ ] Production-ready 1.0 release

---

## Risk Mitigation

### Technical Risks:

1. **Nim macro system limitations**
   - **Mitigation**: PoC completed in weeks 1-2
   - **Status**: Low risk (macros are very powerful)

2. **Performance issues**
   - **Mitigation**: Nim compiles to C, similar performance
   - **Status**: Low risk

3. **FFI integration problems**
   - **Mitigation**: Nim FFI is excellent, test early
   - **Status**: Low risk

### Project Risks:

1. **Timeline slippage**
   - **Mitigation**: Weekly milestones, adjust scope if needed
   - **Status**: Medium risk

2. **Feature creep**
   - **Mitigation**: Strict prioritization, MVP first
   - **Status**: Medium risk

---

## Next Steps (Immediate)

1. ✅ Create project structure
2. ✅ Write roadmap
3. ⏳ Implement core types (`src/kryon/core.nim`)
4. ⏳ Create basic macros (`src/kryon/dsl.nim`)
5. ⏳ Set up CLI tool skeleton
6. ⏳ Convert first example (hello-world)

**Let's ship Kryon 1.0 in 16 weeks!**
