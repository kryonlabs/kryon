# Kryon Round-Trip Testing

Tests to validate that codegens preserve information correctly through full round-trip conversions:
**KRY → KIR → Target → KIR → KRY**

## Quick Start

```bash
# Run all tests
bash tests/round_trip/test_roundtrip.sh

# Run specific test manually
./cli/kryon parse examples/kry/hello_world.kry -o /tmp/test.kir
./cli/kryon codegen kry /tmp/test.kir /tmp/output
./cli/kryon parse /tmp/output/main.kry -o /tmp/test2.kir
./cli/kryon codegen kry /tmp/test2.kir /tmp/output2
diff examples/kry/hello_world.kry /tmp/output2/main.kry
```

## Test Structure

### Test Files (Priority Tier 1)

Located in: `examples/kry/`

- `hello_world.kry` - Basic structure, properties, colors
- `button_demo.kry` - Widgets, event handlers, multi-language
- `text_input_simple.kry` - Forms, data binding, state
- `checkbox.kry` - Basic checkbox component
- `counters_demo.kry` - Components, state, functions
- `if_else_test.kry` - Conditional logic
- `row_alignments.kry` - Row layout, alignment modes
- `column_alignments.kry` - Column layout, nesting

### Round-Tip Targets

| Target | Forward (KRY→Target) | Reverse (Target→KIR) | Status |
|--------|---------------------|-------------------|--------|
| **KRY** | `kryon codegen kry` | `kryon parse` | ✅ Full round-trip |
| **Tcl/Tk** | `kryon codegen tcltk` | `kryon parse` | ✅ Full round-trip |
| **Limbo** | `kryon codegen limbo` | `kryon parse` | ⚠️ Reverse not integrated |
| **HTML** | `kryon codegen html` | `kryon parse` | ⚠️ Needs testing |
| **Markdown** | `kryon codegen markdown` | N/A | ❌ No KRY codegen |
| **C** | `kryon codegen c` | N/A | ❌ No reverse parser |

## Test Results

See [DISCREPANCIES.md](DISCREPANCIES.md) for detailed results.

### Latest Run (2026-02-01)

- **Total Tests**: 16 (8 files × 2 targets)
- **Passed**: 5
- **Failed**: 3 (due to codegen bugs)
- **Skipped**: 8

### Known Issues

1. **KRY Codegen: Border Syntax Bug**
   - Generates: `border = {radius: 8}`
   - Should be: `borderRadius = 8`
   - Impact: Files with borderRadius cannot round-trip

2. **KRY Codegen: String Escaping Bug**
   - Generates: `text_expression = ""You typed: " + textValue"`
   - Should be: `text_expression = "\"You typed: \" + textValue"`
   - Impact: Expressions with string literals cannot round-trip

3. **Tcl/Tk Reverse Parser: Limited Information Recovery**
   - Event handler implementations lost
   - Variable declarations not preserved
   - Only basic component structure recovered

## Expected Information Loss

### By Category

**CRITICAL** (Complete loss):
- Event handler implementations in reverse parse
- Complex expressions may simplify differently
- Variable declarations (Tcl/Tk only)

**MEDIUM** (Acceptable differences):
- Whitespace and formatting
- Comments (may be preserved or lost)
- Property order (KIR is JSON, order doesn't matter)
- Property syntax (e.g., `posX: 200` vs `position: absolute; left: 200`)

**MINOR** (Should be identical):
- Widget types
- Property names
- Event handler names/signatures
- Component structure and hierarchy
- Styling (colors, sizes, fonts)

### By Target

**KRY → KRY**: Expected 95%+ preservation, currently ~70% due to bugs
**Tcl/Tk**: Expected 40-60% preservation, currently ~30%
**Limbo**: TBD (reverse parser not integrated)
**HTML**: Expected major loss (presentation layer)
**Markdown**: Cannot round-trip (no KRY codegen)

## Manual Verification Checklist

For each test that FAILS or has discrepancies:
- [ ] Review diff output
- [ ] Verify widget hierarchy preserved
- [ ] Verify all property names present
- [ ] Verify event handler names/signatures
- [ ] Check for structural changes (nested components)
- [ ] Validate styling preserved (colors, sizes, fonts)
- [ ] Categorize loss: CRITICAL, MEDIUM, or MINOR
- [ ] Create issue in bug tracker for CRITICAL losses

## Test Script Usage

```bash
# Basic usage
bash tests/round_trip/test_roundtrip.sh

# With output saved
bash tests/round_trip/test_roundtrip.sh 2>&1 | tee test_output.log

# Debug mode (see all commands)
bash -x tests/round_trip/test_roundtrip.sh 2>&1 | less
```

## Adding New Tests

1. Create test KRY file in `examples/kry/`
2. Add to test list in `test_roundtrip.sh`:
   ```bash
   test_files=(
       "your_test.kry"
       # ... other tests
   )
   ```
3. Run tests
4. Document results in `DISCREPANCIES.md`

## Cleanup

Test scripts automatically clean up temporary files in `/tmp/kryon_roundtrip_*`.

## Related Documentation

- [DISCREPANCIES.md](DISCREPANCIES.md) - Detailed test results and known issues
- [CLEAN_ARCHITECTURE.md](../../docs/CLEAN_ARCHITECTURE.md) - System architecture
- [PARSER_INTERFACE.md](../../docs/PARSER_INTERFACE.md) - Parser system design
- [CODEGEN_INTERFACE.md](../../docs/CODEGEN_INTERFACE.md) - Codegen system design
