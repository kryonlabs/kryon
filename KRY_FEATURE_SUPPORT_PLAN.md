# Kryon Kry Feature Support & Testing Plan

## Goal
Ensure all `.kry` features compile to `.kir`, load correctly, and render properly with full feature preservation.

## Phase 1: Feature Inventory & Status

### Layout Components
- [x] Container - Basic support
- [x] Row - Basic support
- [x] Column - Basic support (needs alignment fix)
- [ ] Center - Needs verification
- [ ] Stack - Status unknown
- [ ] Grid - Status unknown

### UI Components
- [x] Text - Working
- [x] Button - Working (border fixed)
- [x] Checkbox - Rendering fixed, **click interaction broken**
- [ ] Input - Status unknown
- [ ] Dropdown - Status unknown
- [ ] Link - Status unknown
- [ ] Image - Status unknown
- [ ] Table - Status unknown
- [ ] Tabs - Status unknown

### Style Properties
- [x] width, height - Working
- [x] backgroundColor - Working
- [x] color (text) - Working
- [x] fontSize - Working
- [ ] fontWeight - Needs verification
- [ ] fontFamily - Needs verification
- [x] padding - Working
- [ ] margin - Needs verification
- [x] borderWidth - Working
- [x] borderColor - Working
- [x] borderRadius - Working
- [ ] opacity - Needs verification
- [ ] zIndex - Needs verification

### Flexbox Alignment
- [ ] justifyContent (start/center/end/space-between) - **BROKEN**
- [ ] alignItems (start/center/end/stretch) - **BROKEN** (checkbox centering fails)
- [ ] direction (row/column) - Needs verification
- [ ] gap - Working

### State & Interactivity
- [ ] onClick handlers - Needs verification
- [ ] onChange handlers - Needs verification
- [ ] State management - Needs verification
- [ ] Conditional rendering (if/else) - Needs verification

### Advanced Features
- [ ] Animations - Needs verification
- [ ] Transformations - Needs verification
- [ ] Gradients - Needs verification
- [ ] Shadows - Needs verification

## Phase 2: Critical Bugs to Fix

### Bug 1: Checkbox Click Detection (HIGH PRIORITY)
**Status**: Bounds are set but alignment offsets not applied to rendered_bounds
**Fix**: Re-sync rendered_bounds after flexbox alignment in ir_layout.c
**Files**:
- `/mnt/storage/Projects/kryon/ir/ir_layout.c` (lines 2015-2032)

### Bug 2: Column Alignments Not Rendering (HIGH PRIORITY)
**Symptoms**: `column_alignments.kry` renders only 9 commands (should be more)
**Investigation needed**:
1. Check if KIR is being generated correctly
2. Verify alignment properties are in KIR
3. Check if rendering code handles alignments

### Bug 3: Empty Examples
**Examples that need checking**:
- column_alignments.kry
- row_alignments.kry
- if_else_test.kry
- Any others that appear empty

## Phase 3: Comprehensive Test Suite

### Test 1: Kry → KIR Roundtrip
**Purpose**: Ensure `.kry` → `.kir` → `.kry` preserves all features

```bash
#!/bin/bash
# For each example .kry file:
#   1. Compile to KIR
#   2. Load KIR and re-serialize to KRY
#   3. Diff original KRY with regenerated KRY
#   4. Report any differences

for kry_file in examples/kry/*.kry; do
    echo "Testing $kry_file..."

    # Compile to KIR
    kryon compile "$kry_file" > /tmp/test.kir

    # TODO: Implement KIR → KRY converter
    # kryon kir-to-kry /tmp/test.kir > /tmp/regenerated.kry

    # Diff
    # diff "$kry_file" /tmp/regenerated.kry || echo "FAIL: $kry_file"
done
```

### Test 2: Visual Regression Testing
**Purpose**: Ensure rendering is pixel-perfect

```bash
#!/bin/bash
# For each example:
#   1. Render to screenshot
#   2. Compare with golden reference image
#   3. Report visual differences

for kry_file in examples/kry/*.kry; do
    name=$(basename "$kry_file" .kry)

    # Render
    KRYON_HEADLESS=1 \
    KRYON_SCREENSHOT="/tmp/${name}_test.png" \
    KRYON_SCREENSHOT_AFTER_FRAMES=10 \
    timeout 3 kryon run "$kry_file"

    # Compare with golden
    if [ -f "tests/golden/${name}.png" ]; then
        compare -metric RMSE \
            "/tmp/${name}_test.png" \
            "tests/golden/${name}.png" \
            "/tmp/${name}_diff.png" 2>&1 | tee "/tmp/${name}_metrics.txt"
    else
        echo "WARNING: No golden image for $name"
    fi
done
```

### Test 3: Component Feature Matrix
**Purpose**: Test every component with every applicable property

```kry
// Test matrix template
Container {
    // Test Container with all layout modes
    Row { /* test justifyContent: start */ }
    Row { /* test justifyContent: center */ }
    Row { /* test justifyContent: end */ }
    Row { /* test justifyContent: space-between */ }

    Column { /* test alignItems: start */ }
    Column { /* test alignItems: center */ }
    Column { /* test alignItems: end */ }
    Column { /* test alignItems: stretch */ }

    // Test all UI components
    Text { /* with all text styles */ }
    Button { /* with all button styles */ }
    Checkbox { /* checked and unchecked */ }
    Input { /* with placeholder, value */ }
    // ... etc
}
```

### Test 4: Interaction Testing
**Purpose**: Verify all interactive features work

Manual test checklist:
- [ ] Button onClick triggers
- [ ] Checkbox toggles on click
- [ ] Input accepts text input
- [ ] Input caret moves correctly
- [ ] Dropdown opens/closes
- [ ] Dropdown selects items
- [ ] Tab switching works
- [ ] Links navigate correctly

## Phase 4: Implementation Priority

### Week 1: Critical Fixes
1. Fix checkbox click detection (alignment sync issue)
2. Fix column_alignments rendering
3. Verify all basic layout components work

### Week 2: Flexbox Alignment
1. Implement/fix justifyContent (all modes)
2. Implement/fix alignItems (all modes)
3. Test cross-axis and main-axis alignment
4. Verify gap property works correctly

### Week 3: UI Components
1. Audit all UI components (Button, Checkbox, Input, Dropdown, etc.)
2. Fix any broken rendering
3. Fix any broken interaction
4. Add missing default styles

### Week 4: Testing & Documentation
1. Create golden reference images for all examples
2. Implement automated visual regression tests
3. Document any limitations or known issues
4. Create feature compatibility matrix

## Phase 5: KIR Specification Compliance

### Ensure KIR Contains All Required Data
**Check that KIR includes**:
- All component types
- All style properties
- All layout properties
- All event handlers
- All state/logic
- Metadata (source file, compiler version)

### Validate KIR Loading
**Verify that ir_json.c correctly loads**:
- Every component type
- Every property type
- Nested structures
- Arrays and lists
- Event bindings

## Phase 6: Rendering Pipeline Verification

### Layout Engine Checks
- [ ] Single-pass layout computes all dimensions correctly
- [ ] Flexbox alignment applies correctly
- [ ] Intrinsic sizing works for Text, Button, etc.
- [ ] Percentage sizes resolve correctly
- [ ] Padding and margin apply correctly

### Command Generation Checks
- [ ] All components generate correct commands
- [ ] Bounds are computed correctly
- [ ] Colors are resolved correctly
- [ ] Transforms apply correctly
- [ ] Clipping works correctly

### Rendering Checks
- [ ] SDL3 backend renders all command types
- [ ] Text rendering works correctly
- [ ] Image rendering works correctly
- [ ] Shapes render correctly
- [ ] Transparency/opacity works

## Success Metrics

1. **100% of examples render without errors**
2. **All interactive features work correctly**
3. **Kry → KIR → Kry roundtrip preserves all data**
4. **Visual regression tests pass**
5. **No layout or alignment bugs**

## Files to Focus On

### High Priority
- `/mnt/storage/Projects/kryon/ir/ir_layout.c` - Layout engine
- `/mnt/storage/Projects/kryon/ir/ir_json.c` - KIR loading
- `/mnt/storage/Projects/kryon/backends/desktop/ir_to_commands.c` - Rendering
- `/mnt/storage/Projects/kryon/backends/desktop/renderers/sdl3/sdl3_input.c` - Input
- `/mnt/storage/Projects/kryon/codegens/kry/kry_codegen.c` - Kry parser

### Medium Priority
- `/mnt/storage/Projects/kryon/ir/ir_builder.c` - Component creation
- `/mnt/storage/Projects/kryon/backends/desktop/renderers/sdl3/sdl3_renderer.c` - SDL3 rendering
- `/mnt/storage/Projects/kryon/ir/ir_executor.c` - Event handling

## Next Steps

1. **Immediate**: Fix checkbox click and column_alignments bugs
2. **Today**: Create test harness for visual regression testing
3. **This Week**: Audit and fix all layout/alignment issues
4. **Next Week**: Implement comprehensive test suite
