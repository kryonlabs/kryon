# Plan 9 C Compatibility Report

## Executive Summary

This document lists all C99/C11/POSIX features found in the Kryon codebase that are incompatible with Plan 9 C compiler and require refactoring.

**Audit Date:** 2026-01-18
**Codebase:** Kryon (all modules)
**Total Incompatibilities Found:** 254 instances across 4 categories

---

## Compatibility Issues by Severity

### CRITICAL (Blocks Compilation)

#### 1. for Loop Variable Declarations (210 files)

**Pattern:**
```c
for (int i = 0; i < count; i++) { }
```

**Plan 9 Requirement:**
```c
int i;
for (i = 0; i < count; i++) { }
```

**Files Affected:** 210 files (see detailed list below)

**Impact:** High - Affects majority of codebase
**Effort:** Medium - Mechanical refactoring using sed/awk
**Priority:** P0 - Must fix before compilation

**Affected Directories:**
- `ir/` - Core IR library (100+ files)
- `cli/src/` - CLI commands and utilities (30+ files)
- `backends/` - All backends (20+ files)
- `codegens/` - Code generators (20+ files)
- `renderers/` - Rendering backends (15+ files)
- `third_party/` - Third-party code (25+ files)

**Files (Top 50 Most Critical):**
```
cli/src/build/luajit_runtime.c
cli/src/build/luajit_link.c
codegens/web/lua_bundler.c
codegens/web/ir_web_renderer.c
codegens/web/html_generator.c
ir/ir_source_structures.c
ir/ir_serialization.c
ir/ir_component_factory.c
ir/ir_string_builder.c
ir/ir_json_deserialize.c
ir/ir_json_serialize.c
codegens/web/css_generator.c
ir/ir_lua_modules.c
codegens/web/kir_to_html.c
cli/src/commands/cmd_run.c
cli/src/commands/cmd_build.c
ir/ir_foreach_expand.c
ir/parsers/lua/lua_parser.c
codegens/lua/lua_codegen.c
cli/src/commands/cmd_codegen.c
cli/src/commands/cmd_compile.c
ir/parsers/kry/kry_to_ir.c
ir/ir_layout_builder.c
ir/ir_style_builder.c
ir/ir_builder.c
ir/ir_layout.c
codegens/c/ir_c_codegen.c
codegens/kry/kry_codegen.c
codegens/markdown/markdown_codegen.c
codegens/nim/nim_codegen.c
codegens/kotlin/kotlin_codegen.c
backends/desktop/renderers/sdl3/sdl3_fonts.c
backends/desktop/ir_to_commands.c
backends/desktop/ir_desktop_renderer.c
cli/src/config/config.c
cli/src/utils/file_discovery.c
cli/src/commands/cmd_plugin.c
cli/src/build/plugin_discovery.c
ir/ir_state_manager.c
ir/ir_executor.c
ir/ir_tabgroup.c
ir/ir_component_types.c
bindings/c/kryon.c
backends/desktop/renderers/sdl3/sdl3_renderer.c
ir/ir_json.c
ir/ir_event_builder.c
ir/parsers/html/css_parser.c
ir/parsers/html/html_to_ir.c
ir/ir_expression_parser.c
ir/ir_logic.c
```

**Recommended Fix Strategy:**
1. Create automated script to refactor for-loop declarations
2. Test on sample files first
3. Run across all 210 files
4. Verify compilation with Plan 9 compiler

---

#### 2. inline Functions (28 files)

**Pattern:**
```c
inline int square(int x) { return x * x; }
```

**Plan 9 Requirement:**
```c
static int square(int x) { return x * x; }
/* OR use macro for simple cases */
#define square(x) ((x) * (x))
```

**Files Affected:**
```
codegens/web/html_generator.c
ir/ir_serialization.c
ir/ir_json_deserialize.c
ir/ir_json_serialize.c
codegens/web/css_generator.c
ir/parsers/lua/lua_parser.c
ir/ir_layout.c
backends/desktop/ir_to_commands.c
codegens/web/css_generator_core.c
ir/parsers/html/html_parser.c
ir/parsers/markdown/markdown_parser.c
ir/parsers/markdown/markdown_to_ir.c
renderers/common/command_buf.c
codegens/react_common.c
ir/parsers/html/css_parser.c
ir/parsers/html/html_to_ir.c
ir/parsers/html/css_stylesheet.c
ir/ir_module_refs.c
codegens/web/css_class_names.c
codegens/web/css_property_tables.c
cli/src/config/toml_parser.c
ir/parsers/kry/kry_parser.c
backends/android/ir_android_renderer.c
ir/parsers/markdown/markdown_ast.c
ir/tests/serialization/test_css_roundtrip.c
ir/ir_krb.c
third_party/tomlc99/toml.c
third_party/libtickit/src/rect.c
```

**Impact:** Medium - Affects multiple core modules
**Effort:** Low - Simple keyword replacement
**Priority:** P0 - Must fix before compilation

**Recommended Fix:**
- Remove `inline` keyword, use `static` instead
- For performance-critical functions, use macros
- Consider `static inline` with `#ifdef PLAN9` guards

---

#### 3. Designated Initializers (16 files)

**Pattern:**
```c
IRValue val = { .type = VAR_TYPE_STRING, .string_val = result_str };
```

**Plan 9 Requirement:**
```c
IRValue val;
memset(&val, 0, sizeof(val));
val.type = VAR_TYPE_STRING;
val.string_val = result_str;
```

**Files Affected:**
```
ir/parsers/kry/kry_to_ir.c
backends/desktop/ir_to_commands.c
ir/ir_state_manager.c
ir/ir_capability.c
ir/tests/test_ir_state_manager.c
ir/test/test_ir_style_types.c
ir/ir_style_types.c
ir/tests/runtime/test_phase3_backend_integration.c
ir/ir_krb.c
third_party/libtickit/t/52tickit-timer.c
third_party/libtickit/t/45window-input.c
third_party/libtickit/t/36renderbuffer-blit.c
third_party/libtickit/t/35renderbuffer-mask.c
third_party/libtickit/src/window.c
third_party/libtickit/src/term.c
ir/tests/test_reactive_manifest.c
```

**Impact:** Low - Affects fewer files
**Effort:** Medium - Manual refactoring required
**Priority:** P0 - Must fix before compilation

**Recommended Fix:**
- Convert all designated initializers to sequential assignments
- Use `memset(&var, 0, sizeof(var))` for zero-initialization
- Verify correctness after refactoring

---

### HIGH (Portability Issues)

#### 4. Standard C Headers

**Current Usage:**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

**Plan 9 Headers:**
```c
#include <u.h>      /* Universal types header */
#include <libc.h>   /* Standard C library */
```

**Files Affected:** All IR and CLI C files

**Impact:** High - Affects all modules
**Effort:** Low - Add conditional compilation
**Priority:** P1 - Required for Plan 9 build

**Recommended Fix:**
```c
#ifdef PLAN9
    #include <u.h>
    #include <libc.h>
#else
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
#endif
```

**Alternative:** Create compatibility header `ir/ir_platform.h`:
```c
/* ir/ir_platform.h */
#ifndef IR_PLATFORM_H
#define IR_PLATFORM_H

#ifdef PLAN9
    #include <u.h>
    #include <libc.h>
    /* Map Plan 9 functions to standard names if needed */
    #define snprintf snprint
    #define fprintf fprint
#else
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
#endif

#endif /* IR_PLATFORM_H */
```

Then all C files include:
```c
#include "ir_platform.h"
```

---

### MEDIUM (Third-Party Dependencies)

#### 5. cJSON Library

**Issue:** cJSON uses `stdio.h` and may have Plan 9 incompatibilities

**Files:**
- `third_party/cJSON/*`
- Used by: `ir/ir_serialization.c`, `ir/ir_json_helpers.c`

**Options:**

**Option A: Port cJSON to Plan 9**
- Replace `stdio.h` with `libc.h`
- Use Plan 9 file I/O functions
- Test thoroughly

**Option B: Create Plan 9-Native JSON Parser**
- Implement minimal JSON parser for KIR format only
- Use `libbio` for buffered I/O
- Keep cJSON for non-Plan 9 builds

**Impact:** High - JSON parsing is core functionality
**Effort:** High (Option B) or Medium (Option A)
**Priority:** P1 - Required for Plan 9 functionality

**Recommended Approach:** Option A (port cJSON first), fall back to Option B if issues persist

---

#### 6. Third-Party Libraries

**libtickit (Terminal Rendering):**
- Status: Already has Plan 9 compatibility issues (found in audit)
- Decision: Exclude from Plan 9 build (terminal backend not needed)
- Action: Add `#ifndef PLAN9` guards around libtickit usage

**tomlc99 (TOML Parser):**
- Status: May have C99 features
- Decision: Audit and fix for Plan 9 compatibility
- Priority: P2 - Used for config files

---

### LOW (Code Style)

#### 7. C++ Style Comments

**Pattern:**
```c
// This is a comment
```

**Plan 9 Preference:**
```c
/* This is a comment */
```

**Impact:** None - Plan 9 C supports both styles
**Effort:** Optional
**Priority:** P3 - Style preference only

**Recommendation:** Leave as-is for now, convert during future refactoring

---

## Variable-Length Arrays (VLAs)

**Status:** Not found in core IR code

**Note:** The grep search found many array declarations like:
```c
char buffer[256];
```

These are **fixed-size arrays** (constants), not VLAs. VLAs would look like:
```c
int count = get_count();
char buffer[count];  /* This is a VLA - NOT FOUND in codebase */
```

**Conclusion:** No VLA usage detected - good!

---

## POSIX-Specific Headers

**Search Results:** None found

**Checked For:**
- `#include <unistd.h>`
- `#include <pthread.h>`
- `#include <sys/types.h>`
- `#include <sys/stat.h>`

**Conclusion:** Kryon core does not use POSIX-specific headers directly.

---

## Refactoring Priority

### Phase 3 Order (Recommended)

1. **P0-Critical (Must Fix First):**
   - for-loop variable declarations (210 files) - Automated script
   - inline keyword removal (28 files) - Find/replace
   - Designated initializers (16 files) - Manual refactoring

2. **P1-High (Required for Plan 9):**
   - Add conditional headers (all C files) - Create ir_platform.h
   - Port cJSON library - Medium effort
   - Audit tomlc99 - Low effort

3. **P2-Medium (Backend-Specific):**
   - Exclude libtickit from Plan 9 build
   - Test third-party libraries

4. **P3-Low (Optional):**
   - Convert C++ comments to C89 style

---

## Automated Refactoring Scripts

### Script 1: Fix for-Loop Declarations

```bash
#!/bin/bash
# fix_for_loops.sh - Convert C99 for-loop declarations to C89 style

for file in $(find . -name "*.c" -not -path "*/third_party/*"); do
    # Use sed to extract loop variable declarations
    # This is a simplified example - production script would be more robust
    sed -i.bak -E 's/for\s*\(\s*int\s+(\w+)\s*=/int \1;\n    for (\1 =/' "$file"
done
```

**Note:** This is a simplified example. A production script would need to:
- Handle multiple loop variables
- Preserve indentation
- Avoid modifying comments or strings
- Validate syntax after changes

### Script 2: Remove inline Keywords

```bash
#!/bin/bash
# remove_inline.sh - Remove inline keywords

for file in $(find . -name "*.c" -not -path "*/third_party/*"); do
    sed -i.bak 's/\binline\b/static/g' "$file"
done
```

### Script 3: Add Platform Headers

```bash
#!/bin/bash
# add_platform_headers.sh - Replace standard headers with ir_platform.h

for file in $(find ir/ cli/src -name "*.c"); do
    if grep -q "#include <stdio.h>" "$file"; then
        # Remove standard headers
        sed -i.bak '/#include <stdio.h>/d' "$file"
        sed -i.bak '/#include <stdlib.h>/d' "$file"
        sed -i.bak '/#include <string.h>/d' "$file"

        # Add platform header at top (after first #include)
        sed -i.bak '0,/#include/{s/#include/#include "ir_platform.h"\n#include/}' "$file"
    fi
done
```

---

## Testing Strategy

### Phase 1: Syntax Validation (plan9port on Linux)

```bash
# Compile with plan9port 9c compiler
export PLAN9=/usr/local/plan9
export PATH=$PLAN9/bin:$PATH

cd ir
for f in *.c; do
    9c -DPLAN9 $f || echo "FAIL: $f"
done
```

### Phase 2: Link Validation

```bash
# Link into library
9ar rvc libir.a *.$O
```

### Phase 3: Runtime Validation (9front VM)

```bash
# Run on actual 9front system
# Test basic IR operations
# Verify no runtime errors
```

---

## Estimated Effort

| Task                           | Files | Effort | Time Estimate |
|--------------------------------|-------|--------|---------------|
| for-loop refactoring (script)  | 210   | Low    | 2-4 hours     |
| inline removal (script)        | 28    | Low    | 1 hour        |
| Designated init (manual)       | 16    | Med    | 4-6 hours     |
| Platform headers (script)      | ~100  | Low    | 2-3 hours     |
| cJSON porting                  | 1     | Med    | 6-8 hours     |
| Testing & validation           | All   | Med    | 8-10 hours    |
| **Total**                      | **~355** | **Mixed** | **23-32 hours** |

---

## Conclusion

The Kryon codebase has **moderate Plan 9 C compatibility issues**, primarily:

1. **for-loop declarations** (210 files) - Fixable via automation
2. **inline functions** (28 files) - Fixable via automation
3. **Designated initializers** (16 files) - Require manual refactoring
4. **Header portability** - Fixable via ir_platform.h wrapper

**Good News:**
- No VLAs detected
- No POSIX-specific headers (unistd, pthread)
- Clean architecture with isolated concerns

**Recommended Approach:**
1. Create automated refactoring scripts for P0 issues
2. Test incrementally on plan9port
3. Create ir_platform.h for header portability
4. Port or replace cJSON library
5. Validate on native 9front

**Risk Level:** Medium - Manageable with systematic approach

---

**Next Steps:** Proceed to Phase 3 (C Compatibility Refactoring) with automated scripts.
