# Android Codegen Test Suite

Test KIR files for validating Android/Kotlin code generation.

## Test Files

### button.kir
Tests button component with:
- Text label
- Styling (background, text color)
- Dimensions (width, height)
- Typography (fontSize)
- Border styling
- Corner radius

### layout.kir
Tests complex layouts with:
- Nested Column and Row components
- Padding (all sides)
- Margin (all sides)
- Gap between children
- Alignment properties
- Multiple container children

### style.kir
Tests various styling properties:
- Typography (fontSize, fontWeight)
- Colors (background, text color)
- Border styling
- Corner radius
- Shadow effects

## Running Tests

```bash
# Run all tests
./test_codegen.sh

# Expected output:
# [Test] Button Test
#   ✓ PASS
# [Test] Layout Test
#   ✓ PASS
# [Test] Style Test
#   ✓ PASS
# ========
# Passed: 3
# Failed: 0
```

## Test Output

Each test generates:
- `MainActivity.kt` - Kotlin DSL code
- `AndroidManifest.xml` - Android manifest
- `build.gradle.kts` - Gradle configuration

Output location: `/tmp/android_codegen_test_output/<test_name>/`

## Adding New Tests

1. Create KIR file:
```json
{
  "format": "kir",
  "metadata": {
    "source_language": "kry",
    "compiler_version": "kryon-alpha"
  },
  "app": {
    "windowTitle": "Test Name",
    "windowWidth": 400,
    "windowHeight": 300
  },
  "logic_block": {},
  "root": {
    "id": 1,
    "type": "Container",
    "children": [...]
  }
}
```

2. Add to `test_codegen.sh` TESTS array:
```bash
TESTS=(
    "button.kir:Button Test:com.example.button:ButtonTest"
    "layout.kir:Layout Test:com.example.layout:LayoutTest"
    "style.kir:Style Test:com.example.style:StyleTest"
    "yourtest.kir:Your Test:com.example.yourtest:YourTest"
)
```

## Validation

Verify generated Kotlin code:
- ✅ Proper DSL structure
- ✅ Correct property values
- ✅ Valid Kotlin syntax
- ✅ No missing components
- ✅ Proper nesting and indentation
