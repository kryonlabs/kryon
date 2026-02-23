# Kryon Graphics Tests

Test programs for Phase 3 graphics engine.

## Test Programs

### simple_render_test.c
**Direct graphics test - no widget system**

Tests the core memdraw functionality:
- Screen allocation (800x600 RGBA32)
- Rectangle filling with solid colors
- PPM export

**Run:**
```bash
make
gcc -std=c89 -Wall -g -Iinclude -Isrc/transport \
    tests/simple_render_test.c build/libkryon.a \
    -o tests/simple_render_test
tests/simple_render_test
```

**Output:** `tests_output/direct_test.png`

### graphical_test.c
**Full widget system test**

Tests the complete rendering pipeline:
- Window creation
- Widget creation (buttons, labels)
- Property-based rendering
- Widget-to-pixel pipeline

**Run:**
```bash
make
gcc -std=c89 -Wall -g -Iinclude -Isrc/transport \
    tests/graphical_test.c build/libkryon.a \
    -o tests/graphical_test
tests/graphical_test
```

**Output:** `tests_output/test_output.ppm`

## Expected Results

Both tests should generate colored rectangles proving:
- ✅ RGBA32 color format works
- ✅ Rectangle clipping works
- ✅ Multiple shapes render correctly
- ✅ Alpha channel preserved
