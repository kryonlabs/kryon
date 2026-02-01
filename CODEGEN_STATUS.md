# Kryon Codegen Round-Trip Status

**Last Updated**: 2026-02-01 (C Parser include paths fixed)
**Test Suite Version**: 1.0
**Kryon Version**: Dev (pre-alpha)

## Target Status

Kryon codegen targets are **explicit language+toolkit combinations**:

| Target | Language | Toolkit | Forward | Round-Trip | Preservation | Status |
|--------|----------|---------|---------|-----------|---------------|--------|
| **kry** | KRY | KRY (self) | ✅ Excellent | ✅ **YES** | 95%+ | 🟢 Production Ready |
| **lua** | Lua | Kryon binding | ✅ Good | ❌ **NO** | - | 🔴 Not Implemented |
| **tcl+tk** | Tcl | Tk | ✅ Good | ✅ **YES** | ~30% | 🟡 Limited (scripts lost) |
| **limbo+tk** | Limbo | Tk | ✅ Good | ⚠️ **POOR** | 20-30% | 🟡 FIX THIS|
| **c+sdl3** | C | SDL3 | ✅ Good | ⚠️ **TESTING** | - | 🟡 Fix in Progress |
| **c+raylib** | C | Raylib | ✅ Good | ⚠️ **TESTING** | - | 🟡 Fix in Progress |
| **web** | JavaScript | DOM | ✅ Good | ✅ **YES** | ~40-60% | 🟡 Limited (presentation) |
| **markdown** | Markdown | - | ✅ Good | N/A | 95% | 🔴 Docs Only |
| **android** | Java/Kotlin | Android | ✅ Good | ❌ **NO** | - | 🔴 Not Implemented |

**Legend**:
- 🟢 **Production Ready**: 80%+ preservation, fully functional
- 🟡 **Limited**: 40-79% preservation, works but loses significant info
- 🔴 **Not Working**: <40% preservation or completely broken

---

## Test Results

**Total Tests**: 24/24 passing ✅

| Test File | KRY | Tcl+Tk | Web |
|----------|-----|--------|------|
| hello_world.kry | ✅ | ✅ | ✅ |
| button_demo.kry | ✅ | ✅ | ✅ |
| text_input_simple.kry | ✅ | ✅ | ✅ |
| checkbox.kry | ✅ | ✅ | ✅ |
| counters_demo.kry | ✅ | ✅ | ✅ |
| if_else_test.kry | ✅ | ✅ | ✅ |
| row_alignments.kry | ✅ | ✅ | ✅ |
| column_alignments.kry | ✅ | ✅ | ✅ |

**Test Coverage**: 8 Priority Tier 1 files × 3 targets = 24 tests

**Remaining**: 7 files not yet tested (Priority Tier 2 & 3)

---

## Known Issues & Action Items

### 🟡 HIGH - In Progress

**1. Fix C Parser Build Environment** (COMPLETED: include paths)
- **Issue**: C parser requires compilation but had incorrect include paths
- **Error**: `ir_core.h: No such file or directory`
- **Fix Applied**:
  - ✅ Updated include paths from `-I"%s/ir"` to `-I"%s/ir/include"`
  - ✅ Added `-I"%s/runtime/desktop"` for desktop runtime headers
  - ✅ Added `-I"%s/third_party/tomlc99"` for tomlc99 headers
  - ✅ Fixed `find_kryon_root()` to check for `ir/include/ir_core.h` instead of `ir/ir_core.h`
- **Remaining**: Test C round-trip with actual generated C code

### 🟡 HIGH - Week 2

**2. Document Limbo Decision** (2 hours)
- **Current**: Only 20-30% preservation in round-trip
- **IMPORTANT**: Add `@limbo @end` markers to example files and test

**3. Add @tcl Markers** (1 hour)
- **Current**: Tcl/Tk loses event handler implementations
- **Fix**: Add `@tcl @end` blocks to preserve handler code
- **Example**: button_demo.kry already has the structure
- **Improvement**: 30% → ~50% preservation

**4. Expand Test Coverage** (4-6 hours)
- Test remaining 7 KRY files (Priority Tier 2 & 3)
- All targets: KRY, Tcl+Tk, Web
- Document any new discrepancies found

### 🟢 MEDIUM - Week 3+

5. **CI/CD Integration** (2-3 hours)
   - Add round-trip tests to build pipeline
   - Fail build on critical round-trip errors

6. **Performance Benchmarking** (3-4 hours)
   - Measure parse/codegen time per target
   - Memory usage profiling

7. **Improve Test Infrastructure** (6-8 hours)
   - Automated discrepancy categorization
   - HTML report generation
   - Side-by-side diff viewer

---

## Information Preservation Matrix

| Feature | KRY→KRY | Tcl+Tk | Web | Limbo+Draw | C | Markdown |
|---------|---------|--------|-----|------------|---|----------|
| **Widget Hierarchy** | ✅ 100% | ⚠️ 60% | ⚠️ 70% | ⚠️ 20% | ❌ TBD | ✅ 95% |
| **Properties** | ✅ 95% | ⚠️ 40% | ⚠️ 50% | ⚠️ 10% | ❌ TBD | ✅ 90% |
| **Event Handlers** | ✅ 90% | ❌ 0% | ❌ 0% | ❌ 0% | ❌ TBD | ❌ N/A |
| **Variables** | ✅ 95% | ❌ 0% | ❌ 0% | ❌ 0% | ❌ TBD | ❌ N/A |
| **Comments** | ❌ 0% | ❌ 0% | ❌ 0% | ⚠️ 50% | ❌ TBD | ✅ 100% |
| **Styling** | ✅ 90% | ⚠️ 50% | ⚠️ 60% | ⚠️ 15% | ❌ TBD | ✅ 90% |
| **Layout** | ✅ 95% | ⚠️ 40% | ⚠️ 50% | ⚠️ 20% | ❌ TBD | ✅ 90% |

**Legend**: ✅ = Excellent (80%+), ⚠️ = Limited (40-79%), ❌ = No preservation (0-39%)

---

## Per-Target Details

### 🟢 KRY (Self Round-Trip)
- **Preservation**: 95%+ (only formatting differences)
- **Known Limitations**:
  - Comments not preserved (expected)
  - Property name normalization (e.g., `posX` → `position = absolute; left =`)
  - Formatting differences (indentation, spacing)
- **Command**: `kryon codegen kry input.kir -o output/`
- **Status**: Production ready

### 🟡 Tcl+Tk
- **Preservation**: ~30%
- **What's Lost**: Event handler implementations, variables, complex properties
- **What's Preserved**: Widget hierarchy, core property values, layout structure
- **Reason**: Tcl is imperative - scripts are not declarative
- **Improvement**: Add `@tcl @end` blocks to preserve implementations
- **Status**: Works as designed (presentation layer limitation)

### 🟡 Web (HTML/JS/CSS)
- **Preservation**: ~40-60%
- **What's Lost**: Event handlers (JavaScript), component semantics
- **What's Preserved**: Tag structure, basic styling (CSS), text content
- **Reason**: HTML is presentation-only
- **Commands**:
  - Generate: `kryon build file.kry --target web`
  - Parse: `kryon parse file.html -o output.kir`
- **Status**: Works as expected for web apps

### 🟡 Limbo+Draw
- **Preservation**: ~20-30% (very poor)
- **Issue**: Systems language - hard to map back to declarative KRY
- **Recommendation**: Document as one-way target OR add `@limbo @end` markers
- **Decision Points**:
  1. Test round-trip with `@limbo @end` markers
  2. If still <50% preservation, document as one-way
- **Estimated Time**: 2 hours to test and document

### 🟡 C + SDL3 / Raylib
- **Preservation**: Testing in progress
- **Issue**: C parser requires compilation to extract AST
  - **Fixed**: Include path issues (2026-02-01)
    - ✅ Updated: `-I"%s/ir"` → `-I"%s/ir/include"`
    - ✅ Added: `-I"%s/runtime/desktop"` and `-I"%s/third_party/tomlc99"`
    - ✅ Fixed: `find_kryon_root()` path check
  - **Remaining**: Test full round-trip with generated C code
- **Impact**: C is a primary target - round-trip almost working

### 🔴 Markdown
- **Status**: Documentation-only format
- **Reason**: No reverse parser needed
- **Use Case**: Generate documentation from KRY
- **Command**: `kryon codegen markdown input.kir -o output/`

### 🔴 Android
- **Status**: Not implemented
- **Reason**: Would require Java/Kotlin parser
- **Complexity**: High (Android has complex build system)
- **Priority**: Low

---

## Commands Reference

### Target Commands
```bash
# Explicit syntax (recommended)
kryon build file.kry --target=limbo+draw
kryon build file.kry --target=tcl+tk
kryon build file.kry --target=c+sdl3
kryon build file.kry --target=web

# Short aliases
kryon build file.kry --target=limbo    # → limbo+draw
kryon build file.kry --target=tcltk    # → tcl+tk
kryon build file.kry --target=c         # → c+sdl3
```

### Test Commands
```bash
# Run full test suite
bash tests/round_trip/test_roundtrip.sh

# Manual round-trip test
kryon parse hello_world.kry -o step1.kir
kryon codegen kry step1.kir step2_kry/
kryon parse step2_kry/main.kry -o step3.kir
kryon codegen kry step3.kir step4_kry/
diff hello_world.kry step4_kry/main.kry
```

---

## How to Update This File

When fixing codegen issues:
1. Update the main table above with new status
2. Add entry to "Recent Changes" below
3. Update preservation matrix if applicable
4. Check off completed action items
5. Update test coverage numbers

---

## Recent Changes

### 2026-02-01 (Afternoon)
- ✅ **FIXED**: C parser include paths (c_parser.c)
  - Updated compile command: `-I"%s/ir"` → `-I"%s/ir/include"`
  - Added missing include paths: runtime/desktop, third_party/tomlc99
  - Fixed find_kryon_root() path check
- 📝 **NEXT**: Test C round-trip with generated C code
- 📝 **REMOVED**: Extra documentation files (DISCREPANCIES.md, IMPLEMENTATION_SUMMARY.md, tests/round_trip/README.md)

### 2026-02-01 (Morning)
- ✅ Fixed KRY codegen bugs (border syntax, string escaping)
- ✅ All 24 round-trip tests passing (KRY, Tcl+Tk, Web)
- ✅ Created test infrastructure
- ✅ HTML/Web round-trip tested and working
- ✅ Merged HTML and Web targets (same codegen/parser)
- ✅ Updated target structure to be explicit (language+toolkit)
- ✅ Simplified document by consolidating redundant sections
- ✅ Added @tcl @end to button_demo.kry
- 📝 Next: Fix C parser build environment

---

## Status Legend

- 🟢 **PRODUCTION READY**: Fully functional, high preservation (80%+)
- 🟡 **LIMITED**: Works but with significant information loss (40-79%)
- 🔴 **NOT WORKING**: Broken or not implemented (<40% preservation)

---

**Key Files**:
- Status: `/mnt/storage/Projects/TaijiOS/kryon/CODEGEN_STATUS.md` (this file)
- Tests: `/mnt/storage/Projects/TaijiOS/kryon/tests/round_trip/test_roundtrip.sh`
- README: `/mnt/storage/Projects/TaijiOS/kryon/README.md`
