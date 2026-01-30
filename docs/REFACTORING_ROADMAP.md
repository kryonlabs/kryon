# Kryon Refactoring Roadmap

## Executive Summary

This document provides a step-by-step roadmap for refactoring Kryon's monolithic files. The primary focus is on `runtime.c` (4,057 LOC) and `parser.c` (3,396 LOC), which need to be split into focused, maintainable modules.

## Phase 1: Runtime Refactoring (Priority: CRITICAL)

### Current State

**File**: `src/runtime/core/runtime.c` (4,057 LOC)

**Contains**:
- Runtime lifecycle (init, shutdown, config)
- Element tree creation and management
- Event dispatch and handling
- @for/@if directive expansion
- Layout calculation
- Property updates
- Component state integration
- Render loop

### Target State

Split into 7 focused modules:

```
src/runtime/core/
├── runtime.c              # Core coordination (500 LOC)
├── runtime_lifecycle.c    # Init/shutdown/config (300 LOC)
├── runtime_tree.c         # Element tree operations (800 LOC)
├── runtime_events.c       # Event dispatch (600 LOC)
├── runtime_directives.c   # @for/@if expansion (1000 LOC)
├── runtime_layout.c       # Layout calculation (600 LOC)
└── runtime_updates.c      # Update loop (500 LOC)
```

### Step 1.1: Create Module Headers

Create internal headers for each new module:

**`src/runtime/core/runtime_internal.h`**:
```c
#ifndef KRYON_RUNTIME_INTERNAL_H
#define KRYON_RUNTIME_INTERNAL_H

#include "runtime.h"

// Forward declarations
typedef struct KryonRuntime KryonRuntime;
typedef struct KryonElement KryonElement;

// =============================================================================
// LIFECYCLE MODULE
// =============================================================================

bool kryon_runtime_lifecycle_init(KryonRuntime *runtime, KryonRuntimeConfig *config);
void kryon_runtime_lifecycle_shutdown(KryonRuntime *runtime);
bool kryon_runtime_lifecycle_validate_config(KryonRuntimeConfig *config);

// =============================================================================
// TREE MODULE
// =============================================================================

KryonElement* kryon_runtime_tree_create_element(
    KryonRuntime *runtime,
    uint16_t type,
    const char *element_id
);

void kryon_runtime_tree_destroy_element(KryonRuntime *runtime, KryonElement *element);
void kryon_runtime_tree_add_child(KryonElement *parent, KryonElement *child);
void kryon_runtime_tree_remove_child(KryonElement *parent, KryonElement *child);
KryonElement* kryon_runtime_tree_find_by_id(KryonElement *root, const char *id);

// =============================================================================
// EVENT MODULE
// =============================================================================

bool kryon_runtime_events_dispatch(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonEventType event_type,
    void *event_data
);

void kryon_runtime_events_register_handler(
    KryonElement *element,
    KryonEventType event_type,
    KryonEventHandler handler
);

// =============================================================================
// DIRECTIVE MODULE
// =============================================================================

bool kryon_runtime_directives_expand_for(
    KryonRuntime *runtime,
    KryonElement *element
);

bool kryon_runtime_directives_expand_if(
    KryonRuntime *runtime,
    KryonElement *element
);

void kryon_runtime_directives_cleanup_expanded(
    KryonRuntime *runtime,
    KryonElement *element
);

// =============================================================================
// LAYOUT MODULE
// =============================================================================

void kryon_runtime_layout_calculate(
    KryonRuntime *runtime,
    KryonElement *root
);

void kryon_runtime_layout_mark_dirty(KryonElement *element);

// =============================================================================
// UPDATE MODULE
// =============================================================================

void kryon_runtime_updates_update_frame(KryonRuntime *runtime);
void kryon_runtime_updates_render_frame(KryonRuntime *runtime);

#endif // KRYON_RUNTIME_INTERNAL_H
```

### Step 1.2: Extract Lifecycle Module

**Create**: `src/runtime/core/runtime_lifecycle.c`

**Move functions**:
- `kryon_runtime_create()` → `kryon_runtime_lifecycle_init()`
- `kryon_runtime_destroy()` → `kryon_runtime_lifecycle_shutdown()`
- Config validation
- Initial setup

**Example**:
```c
#include "runtime_internal.h"
#include <stdlib.h>
#include <string.h>

bool kryon_runtime_lifecycle_init(KryonRuntime *runtime, KryonRuntimeConfig *config) {
    if (!runtime || !config) return false;

    // Validate config
    if (!kryon_runtime_lifecycle_validate_config(config)) {
        return false;
    }

    // Initialize runtime fields
    memset(runtime, 0, sizeof(KryonRuntime));
    runtime->mode = config->mode;
    runtime->max_elements = config->max_elements;
    runtime->max_update_fps = config->max_update_fps;

    // Initialize subsystems
    if (!kryon_language_init(runtime)) {
        return false;
    }

    return true;
}

void kryon_runtime_lifecycle_shutdown(KryonRuntime *runtime) {
    if (!runtime) return;

    // Cleanup subsystems
    kryon_language_shutdown(runtime);

    // Clear runtime
    memset(runtime, 0, sizeof(KryonRuntime));
}

bool kryon_runtime_lifecycle_validate_config(KryonRuntimeConfig *config) {
    if (!config) return false;
    if (config->max_elements == 0) return false;
    if (config->max_update_fps == 0) return false;
    return true;
}
```

### Step 1.3: Extract Tree Module

**Create**: `src/runtime/core/runtime_tree.c`

**Move functions**:
- Element creation/destruction
- Tree traversal
- Child management
- Element lookup by ID

**Estimated size**: 800 LOC

### Step 1.4: Extract Events Module

**Create**: `src/runtime/core/runtime_events.c`

**Move functions**:
- Event dispatch logic
- Handler registration
- Event propagation (bubble/capture)
- Event queue management

**Estimated size**: 600 LOC

### Step 1.5: Extract Directives Module

**Create**: `src/runtime/core/runtime_directives.c`

**Move functions**:
- @for directive expansion
- @if directive expansion
- Loop variable context
- Conditional rendering
- Template cloning

**Estimated size**: 1000 LOC

**This is the largest module** - may need further splitting:
```
runtime_directives.c         # Main coordination (200 LOC)
runtime_directives_for.c     # @for implementation (400 LOC)
runtime_directives_if.c      # @if implementation (300 LOC)
runtime_directives_context.c # Variable context (100 LOC)
```

### Step 1.6: Extract Layout Module

**Create**: `src/runtime/core/runtime_layout.c`

**Move functions**:
- Layout calculation
- Position calculation
- Dirty tracking
- Flexbox/grid logic

**Estimated size**: 600 LOC

### Step 1.7: Extract Updates Module

**Create**: `src/runtime/core/runtime_updates.c`

**Move functions**:
- Update loop
- Render coordination
- Frame timing
- FPS limiting

**Estimated size**: 500 LOC

### Step 1.8: Update Main Runtime File

**Keep in** `src/runtime/core/runtime.c`:
- Public API wrappers
- Runtime struct definition
- High-level coordination
- Error handling

**New structure**:
```c
#include "runtime.h"
#include "runtime_internal.h"

// Public API - delegates to modules

KryonRuntime* kryon_runtime_create(KryonRuntimeConfig *config) {
    KryonRuntime *runtime = malloc(sizeof(KryonRuntime));
    if (!runtime) return NULL;

    if (!kryon_runtime_lifecycle_init(runtime, config)) {
        free(runtime);
        return NULL;
    }

    return runtime;
}

void kryon_runtime_destroy(KryonRuntime *runtime) {
    if (!runtime) return;
    kryon_runtime_lifecycle_shutdown(runtime);
    free(runtime);
}

void kryon_runtime_update(KryonRuntime *runtime) {
    if (!runtime) return;
    kryon_runtime_updates_update_frame(runtime);
}

void kryon_runtime_render(KryonRuntime *runtime) {
    if (!runtime) return;
    kryon_runtime_updates_render_frame(runtime);
}

// ... other public API functions
```

**Final size**: ~500 LOC

### Step 1.9: Update Makefile

Add new source files to `Makefile`:

```makefile
RUNTIME_SRC = $(SRC_DIR)/runtime/core/runtime.c \
              $(SRC_DIR)/runtime/core/runtime_lifecycle.c \
              $(SRC_DIR)/runtime/core/runtime_tree.c \
              $(SRC_DIR)/runtime/core/runtime_events.c \
              $(SRC_DIR)/runtime/core/runtime_directives.c \
              $(SRC_DIR)/runtime/core/runtime_layout.c \
              $(SRC_DIR)/runtime/core/runtime_updates.c \
              $(SRC_DIR)/runtime/core/validation.c \
              $(SRC_DIR)/runtime/core/component_state.c \
              $(SRC_DIR)/runtime/core/krb_loader.c \
              $(SRC_DIR)/runtime/core/state.c \
              # ... rest of runtime
```

### Step 1.10: Testing Strategy

**Create unit tests for each module**:

```
tests/unit/runtime/
├── test_runtime_lifecycle.c
├── test_runtime_tree.c
├── test_runtime_events.c
├── test_runtime_directives.c
├── test_runtime_layout.c
└── test_runtime_updates.c
```

**Test approach**:
1. Extract functions one at a time
2. Add test for extracted function
3. Verify original test suite still passes
4. Commit after each successful extraction

### Timeline: Phase 1

| Step | Task | Estimated Time |
|------|------|----------------|
| 1.1  | Create module headers | 2 hours |
| 1.2  | Extract lifecycle | 4 hours |
| 1.3  | Extract tree | 8 hours |
| 1.4  | Extract events | 6 hours |
| 1.5  | Extract directives | 12 hours |
| 1.6  | Extract layout | 6 hours |
| 1.7  | Extract updates | 6 hours |
| 1.8  | Update main runtime | 4 hours |
| 1.9  | Update build system | 2 hours |
| 1.10 | Write tests | 16 hours |
| **Total** | | **~66 hours (~2 weeks)** |

## Phase 2: Parser Refactoring (Priority: HIGH)

### Current State

**File**: `src/compiler/parser/parser.c` (3,396 LOC)

### Target State

```
src/compiler/parser/
├── parser.c                  # Main coordination (400 LOC)
├── parser_elements.c         # Element parsing (600 LOC)
├── parser_properties.c       # Property parsing (500 LOC)
├── parser_events.c           # Event handler parsing (400 LOC)
├── parser_directives.c       # @for/@if/etc parsing (800 LOC)
├── parser_expressions.c      # Expression parsing (500 LOC)
├── parser_components.c       # Component definitions (400 LOC)
├── parser_templates.c        # Template parsing (400 LOC)
└── parser_internal.h         # Internal parser API
```

### Step 2.1: Create Parser Internal Header

**Create**: `src/compiler/parser/parser_internal.h`

```c
#ifndef KRYON_PARSER_INTERNAL_H
#define KRYON_PARSER_INTERNAL_H

#include "parser.h"
#include "lexer.h"

// Forward declarations
typedef struct KryonParser KryonParser;
typedef struct ASTNode ASTNode;

// Parser state
struct KryonParser {
    KryonLexer *lexer;
    KryonToken current_token;
    KryonToken peek_token;
    KryonDiagnostics *diagnostics;
    const char *source;
};

// =============================================================================
// ELEMENT PARSING
// =============================================================================

ASTNode* kryon_parser_parse_element(KryonParser *parser);
ASTNode* kryon_parser_parse_element_body(KryonParser *parser);

// =============================================================================
// PROPERTY PARSING
// =============================================================================

ASTNode* kryon_parser_parse_property(KryonParser *parser);
ASTNode* kryon_parser_parse_property_value(KryonParser *parser);

// =============================================================================
// EVENT PARSING
// =============================================================================

ASTNode* kryon_parser_parse_event_handler(KryonParser *parser);
ASTNode* kryon_parser_parse_function(KryonParser *parser);

// =============================================================================
// DIRECTIVE PARSING
// =============================================================================

ASTNode* kryon_parser_parse_directive(KryonParser *parser);
ASTNode* kryon_parser_parse_for_directive(KryonParser *parser);
ASTNode* kryon_parser_parse_if_directive(KryonParser *parser);

// =============================================================================
// EXPRESSION PARSING
// =============================================================================

ASTNode* kryon_parser_parse_expression(KryonParser *parser);
ASTNode* kryon_parser_parse_binary_expression(KryonParser *parser, int precedence);
ASTNode* kryon_parser_parse_unary_expression(KryonParser *parser);

// =============================================================================
// COMPONENT PARSING
// =============================================================================

ASTNode* kryon_parser_parse_component(KryonParser *parser);
ASTNode* kryon_parser_parse_component_definition(KryonParser *parser);

// =============================================================================
// TEMPLATE PARSING
// =============================================================================

ASTNode* kryon_parser_parse_template(KryonParser *parser);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

bool kryon_parser_expect(KryonParser *parser, KryonTokenType type);
bool kryon_parser_match(KryonParser *parser, KryonTokenType type);
void kryon_parser_advance(KryonParser *parser);
void kryon_parser_error(KryonParser *parser, const char *message);

#endif // KRYON_PARSER_INTERNAL_H
```

### Step 2.2: Module Extraction Order

1. **Parser utilities** (200 LOC) - helper functions
2. **Expression parsing** (500 LOC) - expressions used everywhere
3. **Property parsing** (500 LOC) - depends on expressions
4. **Event parsing** (400 LOC) - depends on expressions
5. **Directive parsing** (800 LOC) - depends on expressions
6. **Template parsing** (400 LOC) - depends on directives
7. **Component parsing** (400 LOC) - depends on templates
8. **Element parsing** (600 LOC) - depends on all above
9. **Main parser** (400 LOC) - coordination only

### Timeline: Phase 2

Estimated: **~50 hours (~1.5 weeks)**

## Phase 3: Architecture Improvements

### 3.1: Renderer Factory Pattern

**Goal**: Decouple targets from renderers

**Create**: `src/runtime/rendering/renderer_factory.c`

```c
typedef struct {
    KryonRendererType preferences[4];
    size_t count;
} KryonRendererPreferences;

KryonRenderer* kryon_renderer_create_for_target(KryonTargetType target) {
    KryonTargetResolution resolution = kryon_target_resolve(target);

    if (!resolution.available) {
        fprintf(stderr, "Target not available: %s\n", resolution.error_message);
        return NULL;
    }

    return kryon_renderer_create(resolution.renderer);
}

KryonRenderer* kryon_renderer_create_with_fallback(
    KryonRendererType *preferences,
    size_t count
) {
    for (size_t i = 0; i < count; i++) {
        KryonRenderer *renderer = kryon_renderer_create(preferences[i]);
        if (renderer) return renderer;
    }
    return NULL;
}
```

**Estimated time**: 8 hours

### 3.2: Unified Directive Engine

**Goal**: Single implementation of directive expansion

**Create**: `src/runtime/directives/` module

**Estimated time**: 16 hours

### 3.3: Multi-Plugin Platform Services

**Goal**: Support multiple platform plugins simultaneously

**Modify**: `src/services/services_registry.c`

**Estimated time**: 12 hours

### Timeline: Phase 3

Estimated: **~36 hours (~1 week)**

## Phase 4: Testing and Documentation

### 4.1: Comprehensive Test Suite

**Create**:
```
tests/
├── unit/
│   ├── runtime/
│   ├── parser/
│   ├── compiler/
│   └── renderers/
├── integration/
│   ├── compilation/
│   ├── rendering/
│   └── language_plugins/
└── functional/
    ├── examples/
    └── end_to_end/
```

**Estimated time**: 40 hours

### 4.2: Documentation

**Update/Create**:
- API reference for all modules
- Architecture diagrams
- Developer guide
- Migration guide

**Estimated time**: 24 hours

### Timeline: Phase 4

Estimated: **~64 hours (~2 weeks)**

## Complete Timeline

| Phase | Focus | Estimated Time |
|-------|-------|----------------|
| Phase 1 | Runtime refactoring | 66 hours (~2 weeks) |
| Phase 2 | Parser refactoring | 50 hours (~1.5 weeks) |
| Phase 3 | Architecture improvements | 36 hours (~1 week) |
| Phase 4 | Testing and documentation | 64 hours (~2 weeks) |
| **Total** | | **~216 hours (~6.5 weeks)** |

## Risk Mitigation

### High-Risk Changes

**Runtime split**:
- ✓ Extract incrementally (one module at a time)
- ✓ Add tests before extraction
- ✓ Verify tests pass after each step
- ✓ Use feature branch

**Parser split**:
- ✓ Same incremental approach
- ✓ Regression tests for all grammar features
- ✓ Compare AST output before/after

### Medium-Risk Changes

**Renderer factory**:
- ✓ Keep old code path initially
- ✓ Add factory alongside existing code
- ✓ Migrate gradually
- ✓ Remove old code after migration

### Low-Risk Changes

**Documentation updates**:
- No code changes
- Review with team

## Success Metrics

Refactoring complete when:

1. ✅ No file >1,500 LOC
2. ✅ All existing tests pass
3. ✅ New unit tests for all modules (>80% coverage)
4. ✅ Build time ≤ current (or improved)
5. ✅ Documentation complete for all new APIs
6. ✅ Code review approved
7. ✅ No regressions in functionality
8. ✅ Examples still work correctly

## Getting Started

### Immediate Next Steps

1. **Review this roadmap** with team
2. **Create feature branch**: `git checkout -b refactor/runtime-split`
3. **Start with Phase 1.1**: Create module headers
4. **Set up test framework** if not already present
5. **Begin runtime_lifecycle extraction** (easiest module)

### Recommended Workflow

```bash
# 1. Create branch
git checkout -b refactor/runtime-split

# 2. Create module header
touch src/runtime/core/runtime_internal.h
# Add content from Step 1.1

# 3. Create first module
touch src/runtime/core/runtime_lifecycle.c
# Extract lifecycle functions

# 4. Update Makefile
vim Makefile
# Add new source file

# 5. Build and test
make clean && make
make test  # If test suite exists

# 6. Commit
git add src/runtime/core/runtime_lifecycle.c
git add src/runtime/core/runtime_internal.h
git add Makefile
git commit -m "refactor: extract runtime lifecycle module

- Split lifecycle functions from runtime.c
- Create runtime_internal.h for module API
- Add runtime_lifecycle.c (300 LOC)
- runtime.c reduced from 4,057 to 3,757 LOC"

# 7. Repeat for next module
```

### Team Coordination

If multiple people work on refactoring:

1. **Assign modules** to avoid conflicts
2. **Daily sync** to coordinate dependencies
3. **Review incrementally** (don't wait for full completion)
4. **Merge frequently** to avoid large merge conflicts

### Rollback Plan

If issues arise:

1. Each extraction is in separate commit
2. Can rollback specific modules: `git revert <commit>`
3. Feature branch protects main: `git branch -D refactor/runtime-split`
4. Keep old code until all tests pass

## Questions?

For questions about this refactoring roadmap:

1. Check `docs/ARCHITECTURE.md` for overall architecture
2. Check `REFACTORING_ANALYSIS.md` for detailed analysis
3. Check individual file headers for module documentation
4. Open an issue on the project tracker

## Appendix: Module Dependencies

### Runtime Modules (Phase 1)

```
runtime.c (main)
  ├─→ runtime_lifecycle.c
  ├─→ runtime_tree.c
  │     └─→ runtime_lifecycle.c (for memory)
  ├─→ runtime_events.c
  │     └─→ runtime_tree.c (for traversal)
  ├─→ runtime_directives.c
  │     ├─→ runtime_tree.c (for cloning)
  │     └─→ runtime_events.c (for handlers)
  ├─→ runtime_layout.c
  │     └─→ runtime_tree.c (for traversal)
  └─→ runtime_updates.c
        ├─→ runtime_layout.c
        └─→ runtime_events.c
```

**Extraction order** (respecting dependencies):
1. runtime_lifecycle.c (no dependencies)
2. runtime_tree.c (depends on lifecycle)
3. runtime_events.c (depends on tree)
4. runtime_directives.c (depends on tree, events)
5. runtime_layout.c (depends on tree)
6. runtime_updates.c (depends on all)

### Parser Modules (Phase 2)

```
parser.c (main)
  ├─→ parser_expressions.c (no dependencies)
  ├─→ parser_properties.c
  │     └─→ parser_expressions.c
  ├─→ parser_events.c
  │     └─→ parser_expressions.c
  ├─→ parser_directives.c
  │     └─→ parser_expressions.c
  ├─→ parser_templates.c
  │     └─→ parser_directives.c
  ├─→ parser_components.c
  │     └─→ parser_templates.c
  └─→ parser_elements.c
        ├─→ parser_properties.c
        ├─→ parser_events.c
        └─→ parser_directives.c
```

**Extraction order** (respecting dependencies):
1. parser_expressions.c (no dependencies)
2. parser_properties.c (depends on expressions)
3. parser_events.c (depends on expressions)
4. parser_directives.c (depends on expressions)
5. parser_templates.c (depends on directives)
6. parser_components.c (depends on templates)
7. parser_elements.c (depends on all)
