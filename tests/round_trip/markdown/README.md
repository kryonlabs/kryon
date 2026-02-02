# Markdown Round-Trip Tests

This directory contains test cases for validating Markdown round-trip functionality.

## Test Files

### button.md
Basic button component with event handler.

**Expected behavior:**
- Parse `:::button` block to IR button
- Generate back `:::button` block
- Preserve component type and event handler

### form.md
Form layout with inputs, checkbox, and buttons.

**Expected behavior:**
- Parse all components correctly
- Preserve component hierarchy
- Preserve event handlers

### styling.md
Text components with various styling attributes.

**Expected behavior:**
- Parse color attributes (hex format)
- Parse font attributes (fontSize, bold, italic)
- Generate correct styling in round-trip

### layout.md
Row and column components with layout attributes.

**Expected behavior:**
- Parse alignment attributes
- Parse gap and padding attributes
- Preserve layout structure

### nested.md
Deeply nested component structure.

**Expected behavior:**
- Parse nested `:::` blocks correctly
- Preserve nesting hierarchy
- All components present in round-trip

## Running Tests

```bash
# Run all tests
cd tests/round_trip/markdown
./test_markdown_roundtrip.sh

# Run individual test manually
kryon parse button.md -o step1.kir
kryon codegen markdown step1.kir roundtrip.md
kryon parse roundtrip.md -o step2.kir
diff step1.kir step2.kir
```

## Expected Results

Since Markdown parsing is lossy for formatting:
- **Exact match**: Not expected (formatting may differ)
- **Component count**: Should match exactly
- **Component types**: Should be preserved
- **Hierarchy**: Should be preserved
- **Properties**: Should be preserved (except edge cases)
- **Event handlers**: Should be preserved

## Test Output

Test script creates `.markdown_test_output/` directory with:
- `step1.kir` - Initial KIR from markdown
- `roundtrip.md` - Markdown generated from step1
- `step2.kir` - KIR from roundtrip markdown

## Known Limitations

1. **Formatting**: Whitespace and formatting may differ slightly
2. **Attribute ordering**: Attributes may be reordered
3. **Quotes**: Single vs double quotes may vary
4. **Escape sequences**: May be normalized
5. **Metadata**: IDs and timestamps will differ

## Future Enhancements

1. Add semantic comparison (ignoring formatting differences)
2. Add property-by-property validation
3. Add event handler code comparison
4. Generate HTML diff reports
5. Calculate preservation percentages automatically
