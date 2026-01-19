# IR Directory Reorganization Plan

## Current Problems

The `ir/` directory has **100+ files** scattered at the root level with no clear organization:

1. **No separation between public API and internal implementation**
2. **Mixed responsibilities**: Core structures, builders, serialization, execution, utilities all intermixed
3. **Inconsistent component organization**: Some in `components/`, some at root
4. **Layout split**: Strategies in `layout/`, but main layout code at root
5. **Utilities scattered**: ir_string_builder, ir_buffer, ir_log, ir_json_helpers all over
6. **Binary artifacts in source**: test_expression_parser, test_expression_phase1 binaries
7. **Third-party deps in wrong location**: ir/third_party should be at root

## Proposed New Structure

```
ir/
├── include/                    # PUBLIC API (consumed by CLI, codegens, runtime)
│   ├── ir.h                   # Master include (includes all public headers)
│   ├── ir_core.h              # Core component tree structures (PUBLIC API)
│   ├── ir_types.h             # Component type enums and constants
│   ├── ir_style.h             # Style property types and constants
│   ├── ir_layout.h            # Layout types and constants
│   ├── ir_logic.h             # Logic block (event handlers)
│   ├── ir_executor.h          # Universal logic executor
│   ├── ir_instance.h          # Multi-instance rendering
│   ├── ir_state.h             # State management
│   ├── ir_serialization.h     # KIR JSON serialization API
│   ├── ir_builder.h           # Component builder API
│   ├── ir_expression.h        # Expression evaluation API
│   └── ir_events.h            # Event handling API
│
├── src/                        # INTERNAL IMPLEMENTATION
│   ├── core/                  # Core IR implementation
│   │   ├── ir_core.c         # Component tree implementation
│   │   ├── ir_component_types.c
│   │   ├── ir_component_factory.c
│   │   ├── ir_component_handler.c
│   │   └── ir_properties.c
│   │
│   ├── components/            # Component-specific implementations
│   │   ├── button.c
│   │   ├── text.c
│   │   ├── container.c
│   │   ├── input.c
│   │   ├── image.c
│   │   ├── canvas.c
│   │   ├── tabs.c
│   │   ├── table.c
│   │   ├── modal.c
│   │   ├── dropdown.c
│   │   ├── checkbox.c
│   │   └── registry.c        # Component registration
│   │
│   ├── style/                 # Style system
│   │   ├── ir_style_builder.c
│   │   ├── ir_style_types.c
│   │   ├── ir_stylesheet.c
│   │   ├── ir_style_vars.c
│   │   └── ir_gradient.c
│   │
│   ├── layout/                # Layout system
│   │   ├── ir_layout.c       # Main layout engine
│   │   ├── ir_layout_builder.c
│   │   ├── ir_layout_debug.c
│   │   ├── ir_layout_types.c
│   │   └── strategies/
│   │       ├── container.c
│   │       ├── row.c
│   │       ├── column.c
│   │       ├── text.c
│   │       ├── image.c
│   │       ├── canvas.c
│   │       └── foreach.c
│   │
│   ├── logic/                 # Logic and execution
│   │   ├── ir_logic.c
│   │   ├── ir_executor.c
│   │   ├── ir_expression.c
│   │   ├── ir_expression_parser.c
│   │   ├── ir_expression_compiler.c
│   │   ├── ir_expression_cache.c
│   │   ├── ir_builtin_registry.c
│   │   └── ir_foreach.c
│   │
│   ├── events/                # Event handling
│   │   ├── ir_event_builder.c
│   │   ├── ir_event_types.c
│   │   ├── ir_input.c
│   │   └── ir_hit_test.c
│   │
│   ├── serialization/        # KIR JSON serialization
│   │   ├── ir_serialization.c
│   │   ├── ir_json_serialize.c
│   │   ├── ir_json_deserialize.c
│   │   ├── ir_json_context.c
│   │   └── ir_serialize_types.c
│   │
│   ├── state/                 # State management
│   │   ├── ir_state_manager.c
│   │   ├── ir_instance.c
│   │   ├── ir_hot_reload.c
│   │   ├── ir_reactive_manifest.c
│   │   └── ir_module_refs.c
│   │
│   ├── features/              # Feature-specific code
│   │   ├── animation.c
│   │   ├── animation_builder.c
│   │   ├── transition.c
│   │   ├── tabgroup.c
│   │   ├── table.c
│   │   ├── text_shaping.c
│   │   ├── markdown.c
│   │   ├── native_canvas.c
│   │   └── capability.c
│   │
│   ├── utils/                 # Internal utilities
│   │   ├── ir_string_builder.c
│   │   ├── ir_buffer.c
│   │   ├── ir_log.c
│   │   ├── ir_json_helpers.c
│   │   ├── ir_hashmap.c
│   │   ├── ir_memory.c
│   │   ├── ir_color_utils.c
│   │   ├── ir_validation.c
│   │   ├── ir_format_detect.c
│   │   ├── ir_asset.c
│   │   ├── ir_krb.c
│   │   ├── ir_lua_modules.c
│   │   └── ir_source_structures.c
│   │
│   └── platform/              # Platform abstraction
│       └── ir_platform.c
│
├── parsers/                   # Language parsers (KEEP AS-IS)
│   ├── tsx/
│   ├── kry/
│   ├── html/
│   ├── lua/
│   ├── markdown/
│   ├── c/
│   └── parser_utils.c/h
│
├── Makefile                   # Updated for new structure
└── CMakeLists.txt            # Updated for new structure
```

## Detailed File Mapping

### Phase 1: Create Directory Structure (No Moves)

```bash
mkdir -p ir/include
mkdir -p ir/src/{core,components,style,layout,strategies,logic,events,serialization,state,features,utils,platform}
mkdir -p ir/src/layout/strategies
```

### Phase 2: Move Public Headers to `include/`

**Move to `ir/include/`** (these are the public API):
- `ir_core.h` → `include/ir_core.h`
- `ir_logic.h` → `include/ir_logic.h`
- `ir_executor.h` → `include/ir_executor.h`
- `ir_instance.h` → `include/ir_instance.h`
- `ir_state_manager.h` → `include/ir_state.h`
- `ir_serialization.h` → `include/ir_serialization.h`
- `ir_builder.h` → `include/ir_builder.h`
- `ir_expression.h` → `include/ir_expression.h`
- `ir_event_types.h` → `include/ir_events.h`
- `ir_component_types.h` → `include/ir_types.h`
- `ir_style_types.h` → `include/ir_style.h`
- `ir_layout_types.h` → `include/ir_layout.h`
- `ir_properties.h` → `include/ir_properties.h`

**Create master include**:
- `include/ir.h` - Master include that includes all public headers

### Phase 3: Move Core Implementation to `src/core/`

- `ir_core.c` → `src/core/ir_core.c` (may need to rename from .h)
- `ir_component_types.c` → `src/core/ir_component_types.c`
- `ir_component_factory.c` → `src/core/ir_component_factory.c`
- `ir_component_handler.c` → `src/core/ir_component_handler.c`
- `ir_properties.c` → `src/core/ir_properties.c`

### Phase 4: Move Components to `src/components/`

- `components/ir_component_*.c` → `src/components/*.c` (remove ir_component_ prefix)
- `components/ir_component_*.h` → `src/components/*.h`
- `ir_component_registry.c` → `src/components/registry.c`
- `ir_component_registry.h` → `src/components/registry.h`

### Phase 5: Move Style System to `src/style/`

- `ir_style_builder.c` → `src/style/ir_style_builder.c`
- `ir_style_types.c` → `src/style/ir_style_types.c`
- `ir_stylesheet.c` → `src/style/ir_stylesheet.c`
- `ir_style_vars.c` → `src/style/ir_style_vars.c`
- `ir_gradient.c` → `src/style/ir_gradient.c`

### Phase 6: Move Layout to `src/layout/`

- `ir_layout.c` → `src/layout/ir_layout.c`
- `ir_layout_builder.c` → `src/layout/ir_layout_builder.c`
- `ir_layout_debug.c` → `src/layout/ir_layout_debug.c`
- `ir_layout_types.c` → `src/layout/ir_layout_types.c`
- `layout/strategy_*.c` → `src/layout/strategies/*.c`
- `layout/ir_layout_strategy.*` → `src/layout/ir_layout_strategy.*`

### Phase 7: Move Logic/Execution to `src/logic/`

- `ir_logic.c` → `src/logic/ir_logic.c`
- `ir_executor.c` → `src/logic/ir_executor.c`
- `ir_expression.c` → `src/logic/ir_expression.c`
- `ir_expression_parser.c` → `src/logic/ir_expression_parser.c`
- `ir_expression_compiler.c` → `src/logic/ir_expression_compiler.c`
- `ir_expression_cache.c` → `src/logic/ir_expression_cache.c`
- `ir_builtin_registry.c` → `src/logic/ir_builtin_registry.c`
- `ir_foreach.c` → `src/logic/ir_foreach.c`
- `ir_foreach_expand.c` → `src/logic/ir_foreach_expand.c`

### Phase 8: Move Events to `src/events/`

- `ir_event_builder.c` → `src/events/ir_event_builder.c`
- `ir_event_types.c` → `src/events/ir_event_types.c`
- `ir_input.c` → `src/events/ir_input.c`
- `ir_hit_test.c` → `src/events/ir_hit_test.c`

### Phase 9: Move Serialization to `src/serialization/`

- `ir_serialization.c` → `src/serialization/ir_serialization.c`
- `ir_json_serialize.c` → `src/serialization/ir_json_serialize.c`
- `ir_json_deserialize.c` → `src/serialization/ir_json_deserialize.c`
- `ir_json_context.c` → `src/serialization/ir_json_context.c`
- `ir_serialize_types.c` → `src/serialization/ir_serialize_types.c`
- `ir_json_helpers.c` → `src/utils/ir_json_helpers.c` (utility)
- `ir_json.c` → `src/utils/ir_json.c` (utility)

### Phase 10: Move State Management to `src/state/`

- `ir_state_manager.c` → `src/state/ir_state_manager.c`
- `ir_instance.c` → `src/state/ir_instance.c`
- `ir_hot_reload.c` → `src/state/ir_hot_reload.c`
- `ir_reactive_manifest.c` → `src/state/ir_reactive_manifest.c`
- `ir_module_refs.c` → `src/state/ir_module_refs.c`

### Phase 11: Move Features to `src/features/`

- `ir_animation.c` → `src/features/animation.c`
- `ir_animation_builder.c` → `src/features/animation_builder.c`
- `ir_transition.c` → `src/features/transition.c`
- `ir_tabgroup.c` → `src/features/tabgroup.c`
- `ir_table.c` → `src/features/table.c`
- `ir_text_shaping.c` → `src/features/text_shaping.c`
- `ir_markdown.c` → `src/features/markdown.c`
- `ir_native_canvas.c` → `src/features/native_canvas.c`
- `ir_capability.c` → `src/features/capability.c`

### Phase 12: Move Utils to `src/utils/`

- `ir_string_builder.c` → `src/utils/ir_string_builder.c`
- `ir_buffer.c` → `src/utils/ir_buffer.c`
- `ir_log.c` → `src/utils/ir_log.c`
- `ir_json_helpers.c` → `src/utils/ir_json_helpers.c`
- `ir_hashmap.c` → `src/utils/ir_hashmap.c`
- `ir_memory.c` → `src/utils/ir_memory.c`
- `ir_color_utils.c` → `src/utils/ir_color_utils.c`
- `ir_validation.c` → `src/utils/ir_validation.c`
- `ir_format_detect.c` → `src/utils/ir_format_detect.c`
- `ir_asset.c` → `src/utils/ir_asset.c`
- `ir_krb.c` → `src/utils/ir_krb.c`
- `ir_lua_modules.c` → `src/utils/ir_lua_modules.c`
- `ir_source_structures.c` → `src/utils/ir_source_structures.c`
- `ir_json.c` → `src/utils/ir_json.c`

### Phase 13: Move Platform Code

- `ir_platform.c` → `src/platform/ir_platform.c`

### Phase 14: Cleanup

**Delete binary artifacts**:
- `test_expression_parser` (binary)
- `test_expression_phase1` (binary)
- `test_toml` (binary)
- `test_toml_parse` (binary)

**Delete backup files**:
- `ir_json_v2.c.bak`

**Delete duplicate third_party**:
- `ir/third_party/` → use root `third_party/`

**Delete old directories**:
- `components/` (moved to `src/components/`)

### Phase 15: Update Build System

**Update `ir/Makefile`**:
- Update include paths: `-Iinclude` instead of `-I.`
- Update source paths: `src/core/*.c`, `src/components/*.c`, etc.
- Update object file paths
- Update library build rules

**Update `ir/CMakeLists.txt`** (if used):
- Update include directories
- Update source file lists
- Update target definitions

### Phase 16: Update All Include References

**Files to update** (any file that includes ir headers):

1. **CLI** (`cli/`):
   - Update includes from `#include "../../ir/ir_core.h"` to `#include "../../ir/include/ir_core.h"`
   - Or use master include: `#include "../../ir/include/ir.h"`

2. **Codegens** (`codegens/`):
   - Update all ir includes

3. **Runtime** (`runtime/`):
   - Update all ir includes

4. **Bindings** (`bindings/`):
   - Update all ir includes

5. **Internal ir includes**:
   - Update from `#include "ir_xxx.h"` to `#include "../include/ir_xxx.h"` or relative paths

## Benefits

1. **Clear Public API**: `ir/include/` contains only public interfaces
2. **Organized Implementation**: `ir/src/` has clear module separation
3. **Easier Navigation**: Find code by functional area
4. **Better Build System**: Clear separation of public/private headers
5. **Scalability**: Easy to add new modules
6. **Professional Structure**: Follows C library best practices

## Migration Strategy

### Option 1: Big Bang (Recommended)
- Do all moves in one commit
- Update all includes in same commit
- Update Makefile in same commit
- Test build immediately after

### Option 2: Incremental
- Phase 1-4: Create structure and move core
- Phase 5-8: Move style, layout, logic, events
- Phase 9-12: Move serialization, state, features, utils
- Phase 13-16: Cleanup and updates

## Testing After Reorganization

1. **Build IR library**: `make -C ir`
2. **Build CLI**: `make -C cli`
3. **Build codegens**: `make -C codegens`
4. **Build runtime**: `make -C runtime/desktop`
5. **Run tests**: `make -C tests/unit/ir test`
6. **Full build**: `make`

## Estimated Impact

- **Files to move**: ~100 files
- **Directories to create**: 13 directories
- **Include statements to update**: ~50-100 files
- **Makefiles to update**: 10+ Makefiles
- **Time estimate**: 2-3 hours for full reorganization

## Risk Mitigation

1. **Create backup branch**: `git checkout -b backup-before-ir-reorg`
2. **Test frequently**: Build after each phase
3. **Use git mv**: Preserves file history
4. **Commit atomically**: One commit for entire reorg
5. **Update includes with care**: Search and replace with verification

## Next Steps

1. Get approval on this plan
2. Create backup branch
3. Execute reorganization (all at once or phased)
4. Test thoroughly
5. Update documentation
6. Commit changes

---

**Status**: Plan awaiting approval
**Last Updated**: January 18, 2026
