# Kryon Modularity Improvement Plan

## Problem Statement

Several files have grown too large, making them difficult to maintain, navigate, and understand:

- **bindings/nim/kryon_dsl/impl.nim**: 4,923 lines
- **backends/desktop/ir_desktop_renderer.c**: 4,655 lines
- **ir/ir_builder.c**: 2,238 lines
- **bindings/nim/runtime.nim**: 1,491 lines
- **backends/web/css_generator.c**: 1,369 lines

## Completed Cleanup

✅ Moved all standalone test files (test_*.c) to `tests/` directory:
- test_auto_height.c
- test_bidi.c, test_bidi_dir.c, test_bidi_reorder.c, test_bidi_simple.c
- test_file_watcher.c
- test_text_shaping.c

---

## ✅ Priority 1: Split impl.nim (4,923 lines → 8 modules) - COMPLETE!

**Previous Structure**: Single monolithic file with DSL macros (4,923 lines)

**Completed Split** (implemented on Nov 30, 2025):

###  `kryon_dsl/` (new structure)
```
kryon_dsl/
├── core.nim           # Core DSL types and base macros (~400 lines)
├── properties.nim     # Property parsing (width, height, colors, etc.) (~800 lines)
├── layout.nim         # Layout-related DSL (Container, Row, Column, etc.) (~600 lines)
├── components.nim     # Component macros (Button, Text, Input, etc.) (~800 lines)
├── animations.nim     # Animation DSL (pulse, fadeInOut, etc.) (~400 lines)
├── reactive.nim       # Reactive DSL integration (~300 lines)
├── utils.nim          # Helper functions (colorNode, parseUnit, etc.) (~600 lines)
└── impl.nim           # Main entry point that exports all modules (~100 lines)
```

**Benefits**:
- Each module is 300-800 lines (manageable size)
- Clear separation of concerns
- Easier to find and modify specific functionality
- Better compile times (only recompile changed modules)

**Implementation Steps**:
1. Create new module files with proper exports
2. Move functions to appropriate modules
3. Update imports in main impl.nim
4. Update kryon_dsl.nim to import new structure
5. Test all examples compile correctly

---

## Priority 2: Split ir_desktop_renderer.c (4,655 lines → 4-5 modules)

**Current Structure**: Single file handling SDL3 rendering, input, events, fonts, etc.

**Proposed Split**:

### `backends/desktop/`
```
backends/desktop/
├── ir_desktop_renderer.c      # Main renderer (~800 lines) - initialization, main loop
├── desktop_rendering.c         # Rendering primitives (~1000 lines) - draw functions
├── desktop_input.c             # Input handling (~600 lines) - mouse, keyboard events
├── desktop_fonts.c             # Font management (~400 lines) - loading, caching
├── desktop_effects.c           # Visual effects (~800 lines) - shadows, gradients, filters
├── desktop_debug.c             # Debug rendering (~300 lines) - layout overlay, inspector
├── desktop_internal.h          # Shared internal structures/functions
└── ir_desktop_renderer.h       # Public API (unchanged)
```

**Benefits**:
- Each file has single responsibility
- Easier to optimize specific rendering paths
- Better code organization
- Simpler debugging

**Implementation Steps**:
1. Create internal header with shared structures
2. Extract rendering functions to desktop_rendering.c
3. Extract input handling to desktop_input.c
4. Extract font management to desktop_fonts.c
5. Extract effects to desktop_effects.c
6. Extract debug code to desktop_debug.c
7. Update Makefile to compile new files
8. Test rendering still works correctly

---

## Priority 3: Split ir_builder.c (2,238 lines → 3 modules)

**Current Structure**: Component creation, tree management, property setters all mixed

**Proposed Split**:

### `ir/`
```
ir/
├── ir_builder.c          # Component creation, tree ops (~800 lines)
├── ir_properties.c       # Property getters/setters (~800 lines)
├── ir_component_types.c  # Type-specific creation (Text, Button, etc.) (~600 lines)
└── ir_builder.h          # Public API (update with new functions)
```

**Benefits**:
- Logical separation of creation vs configuration
- Easier to add new component types
- Better organization of property functions

---

## Priority 4: Reorganize runtime.nim (1,491 lines → 3 modules)

**Current Structure**: Event handlers, reactive system, tab groups, C bridge all mixed

**Proposed Split**:

### `bindings/nim/`
```
bindings/nim/
├── runtime/
│   ├── events.nim        # Button/checkbox handlers (~400 lines)
│   ├── tabs.nim          # Tab group management (~500 lines)
│   ├── c_bridge.nim      # C function imports (~300 lines)
│   └── app_lifecycle.nim # App init, main loop (~200 lines)
└── runtime.nim           # Main entry point (~100 lines)
```

---

## Priority 5: Split css_generator.c (1,369 lines → 2-3 modules)

**Current Structure**: CSS generation for all component types in one file

**Proposed Split**:

### `backends/web/`
```
backends/web/
├── css_generator.c       # Main CSS generation (~500 lines)
├── css_properties.c      # Property-to-CSS conversion (~500 lines)
├── css_animations.c      # Animation/keyframe CSS (~400 lines)
└── css_generator.h       # Public API (unchanged)
```

---

## Implementation Priority Order

1. ✅ **impl.nim** (COMPLETED - highest impact - most frequently edited)
2. **ir_desktop_renderer.c** (second priority - complex rendering)
3. **ir_builder.c** (third priority - core functionality)
4. **runtime.nim** (fourth priority - relatively clean already)
5. **css_generator.c** (lowest priority - web backend less critical)

---

## General Principles

### File Size Guidelines
- **Target**: 300-800 lines per file
- **Maximum**: 1,000 lines before considering split
- **Minimum**: 200 lines (don't over-modularize)

### Module Organization
- One clear responsibility per module
- Related functions grouped together
- Minimal dependencies between modules
- Clear, descriptive file names

### Naming Conventions
- Modules named after their primary function
- Prefix with parent module name (desktop_*, css_*, ir_*)
- Internal headers end with _internal.h
- Public APIs remain stable

---

## Testing Strategy

After each split:
1. ✅ Verify all examples compile
2. ✅ Run existing applications
3. ✅ Check hot reload still works
4. ✅ Verify no performance regression
5. ✅ Update documentation if needed

---

## Expected Benefits

### Immediate
- Easier code navigation
- Faster to find specific functionality
- Better IDE/editor performance
- Clearer code ownership

### Long-term
- Easier to onboard new contributors
- Simpler to add new features
- Better separation of concerns
- Reduced merge conflicts
- Faster incremental compilation

---

## Next Steps

1. Start with impl.nim split (highest priority)
2. Create feature branch for each major split
3. Test thoroughly before merging
4. Update documentation
5. Move to next priority file

## Estimated Effort

- **impl.nim split**: ~3-4 hours
- **ir_desktop_renderer.c split**: ~4-5 hours
- **ir_builder.c split**: ~2-3 hours
- **runtime.nim split**: ~2 hours
- **css_generator.c split**: ~2 hours

**Total**: ~15-20 hours of focused refactoring work
