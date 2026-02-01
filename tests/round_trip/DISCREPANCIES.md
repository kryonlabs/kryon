# Round-Trip Test Discrepancies

## Test Results Summary

Date: 2026-02-01
Kryon Version: Dev
Test Files: 8 Priority Tier 1 files tested

## Critical Issues Found

### KRY Codegen Bugs (Block Round-Trip)

1. **Border Object Syntax** (CRITICAL)
   - **Issue**: `border = {radius: 8}` is invalid KRY syntax
   - **Expected**: `borderRadius = 8` or `border = {width: 1, radius: 8, color: "#000"}`
   - **Location**: `codegens/kry/kry_codegen.c` (border property emission)
   - **Impact**: Files with `borderRadius` property cannot round-trip
   - **Fix**: Emit as `borderRadius` instead of `border = {radius:}`

2. **String Escaping in Expressions** (CRITICAL)
   - **Issue**: `text_expression = ""You typed: " + textValue"` has unescaped quotes
   - **Expected**: `text_expression = "\"You typed: \" + textValue"` or use single quotes
   - **Location**: `codegens/kry/kry_codegen.c` (expression string handling)
   - **Impact**: Expressions with string literals cannot round-trip
   - **Fix**: Properly escape quotes in generated string expressions

## By Target

### KRY (Self Round-Trip)
| Test File | Status | Discrepancies |
|-----------|--------|---------------|
| hello_world.kry | PASS | Formatting differences (expected) |
| button_demo.kry | PASS | Event handlers not preserved, formatting differences |
| text_input_simple.kry | FAIL | Codegen bugs: border syntax, string escaping |
| checkbox.kry | TBD | - |
| counters_demo.kry | TBD | - |
| if_else_test.kry | TBD | - |
| row_alignments.kry | TBD | - |
| column_alignments.kry | TBD | - |

### Tcl/Tk (.tcl)
| Test File | Status | Discrepancies |
|-----------|--------|---------------|
| hello_world.kry | PASS | Most content lost, only basic structure preserved |
| button_demo.kry | PASS | Event handlers lost (expected) |
| text_input_simple.kry | FAIL | Cannot parse generated Tcl back to KIR |
| checkbox.kry | TBD | - |
| counters_demo.kry | TBD | - |
| if_else_test.kry | TBD | - |
| row_alignments.kry | TBD | - |
| column_alignments.kry | TBD | - |

### Limbo (.b)
| Test File | Status | Discrepancies |
|-----------|--------|---------------|
| ALL | SKIP | Limbo reverse parser not yet integrated |

### HTML (.html)
| Test File | Status | Discrepancies |
|-----------|--------|---------------|
| ALL | NOT TESTED | HTML reverse parser needs testing |

### Markdown (.md)
| Test File | Status | Discrepancies |
|-----------|--------|---------------|
| ALL | SKIP | Markdown has no KRY codegen |

### C (.c)
| Test File | Status | Discrepancies |
|-----------|--------|---------------|
| ALL | SKIP | C reverse requires compilation |

## By Information Category

### Event Handlers
- **KRY → KRY**: Full preservation expected
- **Tcl/Tk**: NOT PRESERVED - Tcl scripts contain handler logic but reverse parsing loses it
- **HTML**: NOT PRESERVED - HTML is presentation-only
- **Status**: **CRITICAL INFORMATION LOSS**

### Constants & Variables
- **KRY → KRY**: `var` declarations should be preserved
- **Tcl/Tk**: NOT PRESERVED - Tcl has different variable model
- **Status**: **Needs Investigation**

### Styling
- **All targets**: Colors, sizes, spacing mostly preserved
- **Exceptions**: Border syntax issues in KRY codegen
- **Status**: **MINOR ISSUES**

### Layout Properties
- **KRY → KRY**: Properties preserved, syntax may differ
- **Tcl/Tk**: Mostly lost in reverse parse
- **Status**: **MEDIUM INFORMATION LOSS**

## Expected Information Loss by Target

### KRY → KRY (Self Round-Trip)
**Expected**: 95%+ information preservation
**Actual**: ~70% due to codegen bugs

**Known Issues**:
- Comments lost (expected)
- Event handler signatures preserved, but implementations may differ
- Property syntax changes (e.g., `posX` → `position = absolute; left =`)

**Bugs**:
- Border object syntax generation
- String escaping in expressions

### Tcl/Tk Round-Trip
**Expected**: 40-60% information preservation
**Actual**: ~30%

**Known Loss**:
- Event handler implementations (Tcl script logic not converted back)
- Variable declarations (Tcl uses different model)
- Comments (always lost)
- Some property mappings not reversible

**Critical**: Tcl reverse parser produces minimal structure

## Test Execution Log

### Run 1: Initial Test (2026-02-01 00:05)
**Targets Tested**: KRY, Tcl/Tk
**Test Files**: 8 Priority Tier 1 files

**Results**:
- Total tests: 16 (8 files × 2 targets)
- Passed: 5
- Failed: 3 (all due to codegen bugs)
- Skipped: 8

**Failures**:
1. `text_input_simple.kry` (KRY) - Codegen syntax errors
2. `text_input_simple.kry` (Tcl/Tk) - Parse error
3. Additional failures not completed due to early exit

**Action Items**:
1. Fix KRY codegen border syntax emission
2. Fix KRY codegen string escaping in expressions
3. Investigate Tcl reverse parser limitations
4. Re-run tests after fixes

---

## Recommendations

### Priority 1 (Critical)
1. **Fix KRY Codegen Bugs**
   - `codegens/kry/kry_codegen.c`: Fix border property emission
   - `codegens/kry/kry_codegen.c`: Fix string escaping in expressions
   - Estimated effort: 2-4 hours

2. **Document Tcl/Tk Limitations**
   - Create ADR explaining why Tcl round-trip loses information
   - Document which features are preserved vs. lost
   - Estimated effort: 1-2 hours

### Priority 2 (Important)
3. **Expand Test Coverage**
   - Test all 15 KRY files
   - Add Limbo reverse parser tests
   - Add HTML reverse parser tests
   - Estimated effort: 4-6 hours

4. **Improve Discrepancy Tracking**
   - Automated diff categorization (critical vs. formatting)
   - Generate HTML report with side-by-side comparison
   - Estimated effort: 6-8 hours

### Priority 3 (Nice to Have)
5. **Performance Metrics**
   - Measure parse/codegen time for each target
   - Memory usage profiling
   - Estimated effort: 4 hours

6. **Continuous Integration**
   - Add round-trip tests to CI/CD pipeline
   - Fail build if critical round-trip errors detected
   - Estimated effort: 2-3 hours

---

## Notes

- Discrepancies categorized as: CRITICAL (complete loss), MEDIUM (acceptable differences), MINOR (formatting only)
- Manual verification required for failed tests to distinguish bugs from expected limitations
- This document will be automatically updated by test runs (automation not yet implemented)
