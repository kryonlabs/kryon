# Kryon Refactoring Analysis

## Executive Summary

This document analyzes the Kryon codebase architecture and identifies critical refactoring opportunities. The codebase is ~52,786 LOC with several monolithic files that need decomposition.

## 1. Architecture Overview

### Supported Targets
- **SDL2**: Desktop GUI via SDL2 library
- **Raylib**: Desktop GUI via Raylib library
- **Web**: Static HTML/CSS/JS generation
- **EMU**: TaijiOS emu via KRBVIEW

### Supported Languages (Plugin System)
- **Native**: Kryon's built-in language (always available)
- **sh**: Inferno shell scripts (requires INFERNO env)
- **limbo**: Limbo modules from TaijiOS (requires TaijiOS)

### Renderer Architecture
```
Target Type → Renderer Type → Implementation
SDL2        → SDL2          → src/renderers/sdl2/
Raylib      → Raylib        → src/renderers/raylib/
Web         → Web           → src/renderers/web/ + html/
EMU         → KRBVIEW       → (TaijiOS native viewer)
```

## 2. Critical Issues Requiring Refactoring

### 2.1 Runtime Monolith (CRITICAL - 4,057 LOC)
**File**: `src/runtime/core/runtime.c`

**Problem**: Single file contains:
- Runtime lifecycle management
- Element tree operations
- Event handling
- @for/@if directive expansion
- Layout calculation
- Property updates
- Component state management

**Impact**:
- Hard to maintain and test
- Code reuse difficult
- Merge conflicts common
- New contributors overwhelmed

**Refactoring Strategy**:
Split into focused modules:
```
src/runtime/core/
├── runtime.c              # Core lifecycle only (500 LOC)
├── runtime_lifecycle.c    # Init/shutdown/config (300 LOC)
├── runtime_tree.c         # Element tree operations (800 LOC)
├── runtime_events.c       # Event dispatch and handling (600 LOC)
├── runtime_directives.c   # @for/@if expansion (1000 LOC)
├── runtime_layout.c       # Layout calculation (600 LOC)
└── runtime_updates.c      # Update loop and rendering (500 LOC)
```

**Benefits**:
- Each module has single responsibility
- Easier to test individual components
- Parallel development possible
- Clear API boundaries

### 2.2 Parser Monolith (CRITICAL - 3,396 LOC)
**File**: `src/compiler/parser/parser.c`

**Problem**: Single parser handles:
- Element parsing
- Property parsing
- Event handler parsing
- Directive parsing (@for, @if, etc.)
- Expression parsing
- Component definitions
- Template parsing

**Impact**:
- Incremental compilation impossible
- Error messages unclear
- Grammar changes require full recompile
- Testing specific features difficult

**Refactoring Strategy**:
```
src/compiler/parser/
├── parser.c                  # Main parser coordination (400 LOC)
├── parser_elements.c         # Element parsing (600 LOC)
├── parser_properties.c       # Property parsing (500 LOC)
├── parser_events.c           # Event handler parsing (400 LOC)
├── parser_directives.c       # @for/@if/etc parsing (800 LOC)
├── parser_expressions.c      # Expression parsing (500 LOC)
├── parser_components.c       # Component definitions (400 LOC)
└── parser_templates.c        # Template parsing (400 LOC)
```

**Benefits**:
- Grammar changes isolated
- Better error reporting per feature
- Testable parser stages
- Reusable parsing components

### 2.3 Target-Renderer Coupling (HIGH)
**Files**:
- `include/target.h`
- `src/core/target_resolver.c`
- `src/cli/run_command.c`

**Problem**: Direct 1:1 mapping between targets and renderers:
```c
typedef enum {
    KRYON_TARGET_SDL2,      // → KRYON_RENDERER_TYPE_SDL2
    KRYON_TARGET_RAYLIB,    // → KRYON_RENDERER_TYPE_RAYLIB
    KRYON_TARGET_WEB,       // → KRYON_RENDERER_TYPE_WEB
    KRYON_TARGET_EMU,       // → KRYON_RENDERER_TYPE_KRBVIEW
} KryonTargetType;
```

**Impact**:
- Can't add new targets without new renderers
- Can't reuse renderers across targets
- Testing requires full target setup
- No fallback renderer selection

**Refactoring Strategy**:
Implement Strategy Pattern with factory:
```
src/runtime/rendering/
├── renderer_factory.c      # Factory for creating renderers
├── renderer_selection.c    # Strategy for selecting renderers
└── renderer_fallback.c     # Fallback logic

New API:
KryonRenderer* kryon_renderer_create_for_target(KryonTargetType target);
KryonRenderer* kryon_renderer_create_with_fallback(KryonRendererType* preferences, size_t count);
```

**Benefits**:
- Multiple renderers per target
- Automatic fallback (e.g., Raylib → SDL2 → Web)
- Easier testing with mock renderers
- Plugin renderers possible

### 2.4 Directive Duplication (MEDIUM)
**Files**:
- `src/compiler/codegen/ast_expander.c` (compile-time expansion)
- `src/runtime/core/runtime.c` (runtime expansion)

**Problem**: @for and @if directives expanded in TWO places:
1. Compile-time AST expansion (static @for)
2. Runtime expansion (dynamic @for with state)

**Impact**:
- Duplicate logic
- Bugs in one but not the other
- Unclear when each is used
- Maintenance burden

**Refactoring Strategy**:
```
Option A: Runtime-only expansion
- Remove AST expander
- Always expand at runtime
- Simpler, slower

Option B: Unified expansion engine
- Shared directive expansion library
- Both compiler and runtime use it
- More complex, but consistent

Recommendation: Option B
src/runtime/directives/
├── directive_engine.c      # Core expansion logic
├── directive_for.c         # @for implementation
├── directive_if.c          # @if implementation
└── directive_context.c     # Variable context management
```

**Benefits**:
- Single source of truth
- Consistent behavior
- Easier to add new directives
- Better error messages

### 2.5 Platform Services Single Plugin (MEDIUM)
**Files**:
- `src/services/services_registry.c`
- `src/plugins/inferno/`

**Problem**: Only one platform plugin at a time:
```c
static KryonPlatformServices *g_services = NULL;  // Only one!
```

**Impact**:
- Can't have Inferno + POSIX + Windows features
- Testing requires full platform
- No feature composition
- Hard to port to new platforms

**Refactoring Strategy**:
```c
// Multiple plugins with capability negotiation
typedef struct {
    KryonPlatformServices *plugins[MAX_PLUGINS];
    size_t count;
} KryonPlatformRegistry;

// Service lookup by capability
KryonPlatformServices* kryon_services_find_by_capability(KryonServiceType type);

src/services/
├── services_registry.c     # Multi-plugin registry
├── services_capabilities.c # Capability detection
└── services_fallback.c     # Fallback chain
```

**Benefits**:
- Mix platform features (e.g., Inferno file ops + POSIX networking)
- Gradual platform support
- Better testing
- Plugin marketplace possible

## 3. Moderate Issues

### 3.1 Element Type System (MEDIUM)
**Files**: `include/elements.h`, `src/runtime/elements.c`

**Problem**: Element types are magic numbers:
```c
#define KRYON_ELEMENT_TYPE_BUTTON 1
#define KRYON_ELEMENT_TYPE_TEXT 2
// ... no registry, no reflection
```

**Refactoring Strategy**:
```c
// Element type registry with metadata
typedef struct {
    uint16_t type_id;
    const char *name;
    KryonElementVTable *vtable;
    size_t state_size;
    bool (*validate)(KryonElement*);
} KryonElementTypeInfo;

// Registry API
bool kryon_element_register_type(KryonElementTypeInfo *info);
KryonElementTypeInfo* kryon_element_get_type_info(uint16_t type_id);
KryonElement* kryon_element_create_by_name(const char *name);
```

### 3.2 Multiple IR Formats (MEDIUM)
**Files**:
- `src/compiler/codegen/krb_*` (KRB - binary format)
- `src/compiler/kir/*` (KIR - intermediate representation)
- `src/compiler/krl/*` (KRL - alternative representation)

**Problem**: Three intermediate representations with unclear purpose:
- KRB: Binary runtime format
- KIR: Text-based IR for debugging
- KRL: Alternative text format

**Impact**:
- Code duplication (reader/writer for each)
- Unclear when to use which
- Maintenance burden

**Refactoring Strategy**:
Consolidate to two formats:
- **KRB**: Binary runtime format (keep)
- **KIR**: Unified text format (merge KRL into KIR)

Remove KRL entirely or document clear use cases.

### 3.3 Language Plugin Discovery (LOW)
**Files**: `src/runtime/languages/`

**Problem**: Language plugins work but lack:
- Version compatibility checks
- Capability queries
- Documentation
- Error recovery

**Refactoring Strategy**:
```c
typedef struct {
    const char *identifier;
    const char *name;
    const char *version;
    uint32_t min_runtime_version;
    uint32_t max_runtime_version;
    const char **required_capabilities;
    size_t capability_count;
    // ... existing fields
} KryonLanguagePlugin;

// Enhanced API
bool kryon_language_check_compatibility(KryonLanguagePlugin *plugin);
const char** kryon_language_get_capabilities(const char *identifier);
```

### 3.4 Navigation System Isolation (LOW)
**Files**: `src/runtime/navigation/`

**Problem**: Navigation system (1,096 LOC) is isolated:
- Unclear how it integrates with element tree
- Route storage mechanism opaque
- No documentation on usage

**Refactoring Strategy**:
- Add navigation integration tests
- Document navigation API
- Add examples showing navigation usage
- Consider making navigation optional plugin

## 4. Build System Issues

### 4.1 Makefile Variants (MEDIUM)
**Files**:
- `Makefile` (standard Linux)
- `Makefile.inferno` (Inferno support)
- `Makefile.taijios` (TaijiOS support)

**Problem**: Three similar makefiles with duplication

**Refactoring Strategy**:
```makefile
# Single Makefile with detection
include Makefile.common

ifeq ($(PLATFORM),inferno)
    include Makefile.inferno
else ifeq ($(PLATFORM),taijios)
    include Makefile.taijios
else
    include Makefile.linux
endif
```

Or migrate to CMake fully with platform presets.

### 4.2 Shell.nix Incorrect Information (CRITICAL)
**File**: `shell.nix`

**Problem**: Shell environment displays wrong information:
- Claims LuaJIT, Bun, TypeScript frontends (don't exist)
- Shows SDL3 (code uses SDL2)
- Mentions terminal/framebuffer renderers (not in codebase)
- No mention of language plugins (sh, limbo)

**Fix**: Update shell.nix to reflect actual architecture (see next section)

## 5. Recommended Refactoring Phases

### Phase 1: Foundation (Week 1-2)
1. **Fix shell.nix** to show correct information
2. **Split runtime.c** into 7 modules
3. **Add runtime tests** for each new module
4. **Update documentation** to reflect new structure

### Phase 2: Compiler (Week 3-4)
1. **Split parser.c** into 8 modules
2. **Add parser tests** for each grammar component
3. **Consolidate IR formats** (remove KRL or document clearly)
4. **Improve error messages** using new parser structure

### Phase 3: Architecture (Week 5-6)
1. **Implement renderer factory** pattern
2. **Unify directive expansion** engine
3. **Add multi-plugin support** for platform services
4. **Create element type registry**

### Phase 4: Quality (Week 7-8)
1. **Comprehensive test suite**
2. **Architecture documentation**
3. **API reference documentation**
4. **Example applications** for each target/language

## 6. Metrics

### Current State
- **Largest file**: runtime.c (4,057 LOC)
- **Second largest**: parser.c (3,396 LOC)
- **Total codebase**: ~52,786 LOC
- **Test coverage**: Unknown (needs measurement)
- **Build targets**: 4 (SDL2, Raylib, Web, EMU)
- **Language plugins**: 3 (native, sh, limbo)

### Target State (Post-Refactoring)
- **Largest file**: <1,500 LOC
- **Module count**: +20 focused modules
- **Test coverage**: >80%
- **Build time**: <5s (incremental)
- **Documentation**: 100% API coverage

## 7. Risk Assessment

### High Risk Changes
- **Runtime split**: Core system, thorough testing needed
- **Parser split**: Could break compilation, need regression tests
- **Directive consolidation**: Behavioral changes possible

### Medium Risk Changes
- **Renderer factory**: API changes, update all call sites
- **Multi-plugin support**: Registry logic complex

### Low Risk Changes
- **Element type registry**: Additive, backwards compatible
- **Navigation docs**: Documentation only
- **Shell.nix fix**: Environment only

## 8. Success Criteria

Refactoring is successful when:
1. ✅ No file >1,500 LOC
2. ✅ All tests passing (new + existing)
3. ✅ Build time improved or unchanged
4. ✅ Documentation covers all APIs
5. ✅ Each module has single responsibility
6. ✅ No duplicate logic across codebase
7. ✅ New contributors can understand module boundaries
8. ✅ Shell environment shows correct architecture

## 9. Next Steps

1. **Review this analysis** with team
2. **Prioritize phases** based on project needs
3. **Create detailed task breakdown** for Phase 1
4. **Set up branch** for refactoring work
5. **Begin with shell.nix fix** (immediate, low risk)
6. **Start runtime.c split** (high impact, well-scoped)

## Appendix: File Size Distribution

### Top 10 Largest Files
1. `src/runtime/core/runtime.c` - 4,057 LOC ⚠️
2. `src/compiler/parser/parser.c` - 3,396 LOC ⚠️
3. `src/runtime/elements.c` - 4,006 LOC ⚠️
4. `src/compiler/codegen/codegen.c` - 2,575 LOC
5. `src/runtime/elements/container.c` - 1,161 LOC
6. `src/renderers/raylib/raylib_renderer.c` - 1,161 LOC
7. `src/renderers/sdl2/sdl2_renderer.c` - 1,092 LOC
8. `src/runtime/navigation/navigation.c` - 1,096 LOC
9. `src/compiler/lexer/lexer.c` - 1,124 LOC
10. `src/renderers/html/html_renderer.c` - 912 LOC

⚠️ = Critical refactoring target (>3,000 LOC)
