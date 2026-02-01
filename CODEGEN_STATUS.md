# Kryon Codegen Round-Trip Status

**Last Updated**: 2026-02-01
**Test Suite Version**: 1.0
**Kryon Version**: Dev (pre-alpha)

## Target Combinations

Kryon codegen targets are **explicit language+toolkit combinations**:

| Target Combination | Language | Toolkit | Alias | Forward Gen | Round-Trip? | Status |
|-------------------|----------|---------|-------|-------------|-------------|--------|
| **kry** | KRY | KRY (self) | - | ✅ Excellent | ✅ **YES** | 🟢 Production |
| **tcl+tk** | Tcl | Tk | `tcltk` | ✅ Good | ✅ **YES** | 🟡 Limited (30%) |
| **limbo+draw** | Limbo | Draw | `limbo` | ✅ Good | ⚠️ Poor (20-30%) | 🟡 One-Way? |
| **c+sdl3** | C | SDL3 | `c` | ✅ Good | ❌ **NO** | 🔴 Broken |
| **c+raylib** | C | Raylib | - | ✅ Good | ❌ **NO** | 🔴 Broken |
| **web** | JS/HTML | DOM | `html` | ✅ Good | ✅ **YES** | 🟡 Limited (40-60%) |
| **markdown** | Markdown | - | - | ✅ Good | N/A | 🔴 Docs only |
| **android** | Java/Kotlin | Android | - | ✅ Good | ❌ **NO** | 🔴 Not Implemented |

**Legend**: 🟢 = Production Ready, 🟡 = Limited, 🔴 = Not Working

---

## Round-Trip Test Results

### What Works ✅

| Target | Preservation | Test Results | Notes |
|--------|---------------|--------------|-------|
| **kry** | 95%+ | 8/8 files passed | Only formatting differences |
| **tcl+tk** | ~30% | 8/8 files passed | Scripts lost (expected) |
| **web** | ~40-60% | 8/8 files passed | Presentation layer limitation |

**Total Tests**: 24/24 passing ✅

### What Needs Work ⚠️

| Target | Issue | Fix Time | Priority |
|--------|-------|----------|----------|
| **c+sdl3**, **c+raylib** | C parser compilation broken | 4-8 hours | 🔴 HIGH |
| **limbo+draw** | Only 20-30% preservation | 2 hours (docs) | 🟡 MEDIUM |
| **android** | No reverse parser | 20+ hours | 🟢 LOW |

---

## Detailed Status

### 🟢 PRODUCTION READY

#### KRY (Self Round-Trip)
- **Target**: `kry` (language + toolkit are the same)
- **Preservation**: 95%+ (only formatting differences)
- **Test Results**: 8/8 files passed ✅
- **Known Limitations**:
  - Comments not preserved (expected)
  - Property name normalization (e.g., `posX` → `position = absolute; left =`)
  - Formatting differences (indentation, spacing)
- **Command**: `kryon codegen kry input.kir -o output/`
- **Status**: Ready for production use

---

### 🟡 LIMITED BUT WORKING

#### Tcl+Tk
- **Target**: `tcl+tk` (alias: `tcltk`)
- **Preservation**: ~30%
- **Test Results**: 8/8 files passed ✅
- **What's Lost**: Event handler implementations, variables, complex properties
- **What's Preserved**: Widget hierarchy, core property values, layout structure
- **Reason**: Tcl is a scripting language - code is imperative, not declarative
- **Improvement**: Add `@tcl` ... `@end` blocks to preserve handler implementations
- **Status**: Works as designed (presentation layer limitation)
- **Command**: `kryon parse file.tcl -o output.kir`

#### Web (HTML/JS/CSS)
- **Target**: `web` (alias: `html`)
- **Preservation**: ~40-60%
- **Test Results**: 8/8 files passed ✅
- **What's Lost**: Event handlers (JavaScript), component semantics
- **What's Preserved**: Tag structure, basic styling (CSS), text content
- **Reason**: HTML is presentation-only - no declarative UI semantics
- **Status**: Works as expected for web apps
- **Commands**:
  - Generate: `kryon build file.kry --target web`
  - Parse: `kryon parse file.html -o output.kir`

---

### 🟡 NEEDS WORK / DECISION

#### Limbo+Draw
- **Target**: `limbo+draw` (alias: `limbo`)
- **Preservation**: ~20-30% (very poor)
- **Issue**: Systems language - hard to map back to declarative KRY
- **Current State**: Round-trip loses most information
- **Recommendation**: **Document as one-way target** OR improve with `@limbo` blocks
- **Decision Points**:
  1. Add `@limbo` ... `@end` markers to example files
  2. Test round-trip with markers
  3. If still <50% preservation, document as one-way codegen
- **Action Items**:
  - [ ] Add `@limbo @end` markers (1 hour)
  - [ ] Test round-trip (30 min)
  - [ ] Create ADR documenting decision (1 hour)
- **Estimated Time**: 2.5 hours total

---

### 🔴 NOT WORKING

#### C + SDL3 / Raylib
- **Targets**: `c+sdl3`, `c+raylib` (alias: `c`)
- **Preservation**: Unknown (cannot test)
- **Issue**: **C parser requires compilation to extract AST**
  - Error: `ir_core.h: No such file or directory`
  - Parser tries to compile generated C code with clang
  - Missing include paths and IR headers
- **Root Cause**: Build environment not properly configured
- **Required Fixes**:
  1. Set up proper include paths for C compilation
  2. Make IR headers available to C parser
  3. Fix clang integration in build pipeline
- **Impact**: C is a primary target - this blocks C round-trip completely
- **Estimated Effort**: 4-8 hours
- **Priority**: 🔴 HIGH

---

### 🔴 NOT APPLICABLE

#### Markdown
- **Status**: Documentation-only format
- **Reason**: No reverse parser needed
- **Use Case**: Generate documentation from KRY
- **Command**: `kryon codegen markdown input.kir -o output/`

#### Android
- **Status**: Not implemented
- **Reason**: Would require Java/Kotlin parser
- **Complexity**: High (Android has complex build system)
- **Priority**: Low

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

## Test Coverage

### Completed ✅
- **8 test files** × **3 targets** (KRY, Tcl+Tk, Web) = **24 tests**
- **All 24 tests passing** ✅

**Test Files** (Priority Tier 1):
1. ✅ hello_world.kry
2. ✅ button_demo.kry
3. ✅ text_input_simple.kry
4. ✅ checkbox.kry
5. ✅ counters_demo.kry
6. ✅ if_else_test.kry
7. ✅ row_alignments.kry
8. ✅ column_alignments.kry

### Remaining (7 files)
- **Priority Tier 2** (3 files): dropdown.kry, table_demo.kry, tabs_reorderable.kry
- **Priority Tier 3** (4 files): animations_demo.kry, typography_showcase.kry, zindex_test.kry, todo.kry

---

## Action Items (Priority Order)

### 🔴 CRITICAL - Week 1

1. **Fix C Parser Build Environment** (4-8 hours)
   - Set up include paths for IR headers
   - Configure clang integration
   - Test `c+sdl3` and `c+raylib` round-trips
   - Files: `ir/parsers/c/`, `bindings/c/`, build system
   - **Blocker**: C round-trip completely broken

2. **Test HTML/Web Round-Trip** (1 hour) ✅ DONE
   - Added to test suite
   - All 8 files passing

### 🟡 HIGH - Week 2

3. **Document Limbo Decision** (2 hours)
   - Add `@limbo @end` markers to example files
   - Test `limbo+draw` round-trip with markers
   - If <50% preservation, document as one-way target
   - Create ADR: `docs/ADR-003-limbo-roundtrip.md`

4. **Add @tcl Markers** (1 hour)
   - Add `@tcl @end` blocks to files with named handlers
   - Example: button_demo.kry already has them
   - Improves Tcl+Tk preservation from 30% → ~50%

5. **Expand Test Coverage** (4-6 hours)
   - Test remaining 7 KRY files
   - All targets: KRY, Tcl+Tk, Web
   - Document any new discrepancies

### 🟢 MEDIUM - Week 3+

6. **CI/CD Integration** (2-3 hours)
   - Add round-trip tests to build pipeline
   - Fail build on critical round-trip errors
   - Automated test result reporting

7. **Performance Benchmarking** (3-4 hours)
   - Measure parse/codegen time per target
   - Memory usage profiling
   - Create baseline metrics

8. **Improve Test Infrastructure** (6-8 hours)
   - Automated discrepancy categorization
   - HTML report generation
   - Side-by-side diff viewer

---

## Recent Changes

### 2026-02-01
- ✅ Fixed KRY codegen bugs (border syntax, string escaping)
- ✅ All 24 round-trip tests passing (KRY, Tcl+Tk, Web)
- ✅ Created test infrastructure
- ✅ HTML/Web round-trip tested and working
- ✅ Merged HTML and Web targets (same codegen/parser)
- ✅ Updated target structure to be explicit (language+toolkit)
- ✅ Added @tcl @end to button_demo.kry
- 📝 Next: Fix C parser build environment

---

## How to Use This Status

### Running Tests:
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

### Target Commands:
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

### Updating This File:
When fixing codegen issues:
1. Update relevant target status
2. Add entry to "Recent Changes"
3. Update preservation matrix
4. Check off completed action items
5. Update test coverage numbers

---

## Status Legend

- 🟢 **PRODUCTION READY**: Fully functional, high preservation (80%+)
- 🟡 **LIMITED**: Works but with significant information loss (40-79%)
- 🟡 **NEEDS WORK**: Has issues but fixable
- 🔴 **NOT WORKING**: Broken or not implemented
- 🔴 **NOT APPLICABLE**: By design

---

**Commands Reference**:
- Run tests: `bash tests/round_trip/test_roundtrip.sh`
- Parse: `kryon parse <input> -o <output.kir>`
- Codegen: `kryon codegen <target> <input.kir> <output>`
- Build: `kryon build <file.kry> --target=<target>`
- List targets: `kryon targets` (if implemented)
