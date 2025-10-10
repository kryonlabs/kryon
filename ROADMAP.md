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

## Phase 1: Foundation (Weeks 1-2) ✓ IN PROGRESS

### Week 1: Core Infrastructure
- [x] Create project structure
- [ ] Set up Nim package (kryon.nimble)
- [ ] Core types and data structures
- [ ] Basic element system
- [ ] Property handling

**Deliverables**:
- Working Nim project structure
- Core `Element` type with properties
- Basic macro for creating elements

### Week 2: DSL Macros
- [ ] Container macro
- [ ] Text macro
- [ ] Button macro
- [ ] Column/Row layout macros
- [ ] Property parsing and validation

**Deliverables**:
- All basic UI elements as Nim macros
- Can write declarative UI in Nim syntax
- Type-safe property handling

---

## Phase 2: Rendering Backend (Weeks 3-4)

### Week 3: Raylib Integration
- [ ] FFI bindings to existing kryon C rendering code
- [ ] Raylib backend implementation
- [ ] Element → Rendering pipeline
- [ ] Layout calculation
- [ ] Basic event loop

**Deliverables**:
- Working raylib backend
- Can render simple UIs (Container, Text, Button)
- Event loop running at 60 FPS

### Week 4: Advanced Elements
- [ ] Input element
- [ ] Dropdown element
- [ ] Checkbox element
- [ ] Grid layout
- [ ] Styling system basics

**Deliverables**:
- All core UI elements working
- Basic styling support
- Complex layouts rendering correctly

---

## Phase 3: State & Events (Weeks 5-6)

### Week 5: Reactive State
- [ ] State variable macro
- [ ] Change detection
- [ ] Re-render triggering
- [ ] Component-local state

**Deliverables**:
- `state count = 0` syntax working
- State changes trigger re-renders
- Counter example working

### Week 6: Event Handling
- [ ] onClick handlers
- [ ] onSubmit handlers
- [ ] Event propagation
- [ ] Custom event types

**Deliverables**:
- Full event system working
- Interactive examples (buttons, forms)
- Event bubbling/capturing

---

## Phase 4: Components & Styling (Weeks 7-8)

### Week 7: Component System
- [ ] Component definition macro
- [ ] Component instantiation
- [ ] Props passing
- [ ] Component composition

**Deliverables**:
- Reusable components working
- Can compose complex UIs from components
- Props type-checked at compile time

### Week 8: Styling System
- [ ] Style definition macro
- [ ] Style application
- [ ] Style inheritance
- [ ] Theme support
- [ ] CSS-like properties

**Deliverables**:
- Full styling system
- Reusable styles
- Theme switching capability

---

## Phase 5: Multi-Backend (Weeks 9-12)

### Week 9-10: HTML/Web Backend
- [ ] Compile to JavaScript target
- [ ] HTML generation
- [ ] DOM manipulation
- [ ] CSS generation
- [ ] Web event handling

**Deliverables**:
- Same .kry code runs in browser
- HTML backend generating clean markup
- Interactive web UIs working

### Week 11-12: Backend Abstraction
- [ ] Renderer interface
- [ ] Backend selection at compile-time
- [ ] Platform-specific optimizations
- [ ] Performance tuning

**Deliverables**:
- Clean backend abstraction
- Easy to add new backends
- Both raylib and HTML backends polished

---

## Phase 6: CLI & Tooling (Weeks 13-14)

### Week 13: CLI Tool
- [ ] `kryon compile` command
- [ ] `kryon run` command
- [ ] `kryon dev` with hot reload
- [ ] `kryon build` for production
- [ ] Backend selection flags

**Deliverables**:
- Full-featured CLI tool
- User-friendly commands
- Good error messages

### Week 14: Developer Experience
- [ ] Comprehensive error messages
- [ ] Documentation generation
- [ ] Example gallery
- [ ] VSCode integration guide

**Deliverables**:
- Excellent developer experience
- Clear documentation
- Easy to get started

---

## Phase 7: Polish & Release (Weeks 15-16)

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

## Current Priorities (Week 1)

### Immediate Tasks (This Week):

1. **Core Type System** ✓ IN PROGRESS
   - Element type
   - Property handling
   - Element tree structure

2. **Basic Macros**
   - Container macro
   - Text macro
   - Button macro

3. **Simple CLI**
   - `kryon run` command
   - Compile and execute

4. **First Example**
   - Hello World in Nim syntax
   - Verify end-to-end workflow

### Success Criteria for Week 1:

- [ ] Can write: `Container: Text: text: "Hello World"`
- [ ] Can compile with: `nim c kryon_app.nim`
- [ ] Renders in raylib window
- [ ] Basic CLI working

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

### Week 1:
- [ ] Hello World example working
- [ ] Basic macros functional
- [ ] Can compile and run

### Month 1 (Week 4):
- [ ] All basic elements working
- [ ] Raylib backend rendering correctly
- [ ] 5+ examples converted and working

### Month 2 (Week 8):
- [ ] State management working
- [ ] Component system functional
- [ ] Styling system complete

### Month 3 (Week 12):
- [ ] Multi-backend support (raylib + HTML)
- [ ] CLI tool feature-complete
- [ ] 15+ examples working

### Month 4 (Week 16):
- [ ] Production-ready 1.0 release
- [ ] Complete documentation
- [ ] Ready for external users

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
