---
name: kryon-interactive-test
description: Create and run interactive tests for Kryon UI components
auto_enable: true
trigger_patterns:
  - "test (clicking|button|tab|interaction)"
  - "create.*test.*for"
  - "interactive test"
  - "test.*component.*click"
  - "verify.*tab.*switch"
---

# Kryon Interactive Test Skill

This skill helps create and run interactive tests for Kryon UI components using the `.kyt` test format.

## Overview

The interactive testing system allows you to:
- **Automate UI interactions** - Clicks, keyboard input, mouse movement
- **Capture screenshots** - Before/after states for visual verification
- **Test component behavior** - Verify tabs, buttons, dropdowns, etc.
- **Run headlessly** - No window required, perfect for CI/CD

## Workflow

### 1. Identify Component IDs

Before writing tests, you need to know the component IDs to interact with.

```bash
# View component tree
kryon tree build/ir/your_app.kir

# Example output:
# TabBar #6 (4 children)
#   ├── Tab #7 [Home]
#   ├── Tab #8 [Profile]
#   ├── Tab #9 [Settings]
#   └── Tab #10 [About]
```

**Note component IDs** - These are used in click events.

### 2. Create `.kyt` Test File

Create a test file in `tests/interactive/` with the `.kyt` extension:

```kyt
# tests/interactive/my_feature.kyt

Test:
  name = "Feature Name Test"
  target = "examples/kry/my_app.kry"
  description = "What this test verifies"

  Setup:
    # Wait for initial render
    wait:
      frames = 10
      comment = "Allow layout computation"

    # Capture initial state
    screenshot:
      path = "/tmp/test_initial.png"

  Scenario:
    # Your test actions here
    click:
      component_id = 8
      comment = "Click the Profile tab"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/test_after_click.png"

  Teardown:
    # Optional cleanup
    wait:
      frames = 2
```

### 3. Run Test

```bash
# Run test
kryon test tests/interactive/my_feature.kyt

# Run with verbose output
kryon test tests/interactive/my_feature.kyt --verbose
```

### 4. Verify Results

```bash
# Check screenshots
ls -lh /tmp/test_*.png.bmp

# Convert to PNG for viewing (if needed)
for f in /tmp/test_*.bmp; do magick "$f" "${f%.bmp}"; done
```

## Test Event Types

### Wait Event
Wait for N frames before next event.

```kyt
wait:
  frames = 10
  comment = "Wait for animation"
```

### Click Event (by Component ID)
Click on a component by its ID (automatically clicks center).

```kyt
click:
  component_id = 8
  comment = "Click Profile tab"
```

### Click Event (by Coordinates)
Click at specific screen coordinates.

```kyt
click:
  x = 400
  y = 200
  comment = "Click at specific position"
```

### Screenshot Event
Capture the current rendered state.

```kyt
screenshot:
  path = "/tmp/my_screenshot.png"
  comment = "Capture current state"
```

### Key Press Event
Send keyboard input.

```kyt
key_press:
  key = "Tab"
  comment = "Navigate to next field"

key_press:
  key = "Enter"
  comment = "Submit form"
```

### Text Input Event
Type text into focused input.

```kyt
text_input:
  text = "Hello World"
  comment = "Enter text in search box"
```

### Mouse Move Event
Move mouse to coordinates.

```kyt
mouse_move:
  x = 300
  y = 150
  comment = "Hover over element"
```

## Common Test Patterns

### Tab Switching Test

```kyt
Test:
  name = "Tab Navigation"
  target = "examples/kry/tabs_demo.kry"

  Scenario:
    # Get component IDs first: kryon tree build/ir/tabs_demo.kir

    wait:
      frames = 10

    screenshot:
      path = "/tmp/tabs_home.png"

    click:
      component_id = 8  # Profile tab
      comment = "Switch to Profile"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/tabs_profile.png"

    click:
      component_id = 9  # Settings tab
      comment = "Switch to Settings"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/tabs_settings.png"
```

### Button Click Test

```kyt
Test:
  name = "Button Interaction"
  target = "examples/nim/button_demo.nim"

  Scenario:
    wait:
      frames = 10

    screenshot:
      path = "/tmp/button_before.png"

    click:
      component_id = 12  # Counter button
      comment = "Increment counter"

    wait:
      frames = 3

    screenshot:
      path = "/tmp/button_after.png"

    # Click multiple times
    click:
      component_id = 12

    wait:
      frames = 2

    click:
      component_id = 12

    wait:
      frames = 2

    screenshot:
      path = "/tmp/button_final.png"
```

### Dropdown Test

```kyt
Test:
  name = "Dropdown Selection"
  target = "examples/nim/dropdown.nim"

  Scenario:
    wait:
      frames = 10

    # Click to open dropdown
    click:
      component_id = 15
      comment = "Open dropdown menu"

    wait:
      frames = 3

    # Navigate with keyboard
    key_press:
      key = "Down"

    wait:
      frames = 2

    key_press:
      key = "Down"

    wait:
      frames = 2

    key_press:
      key = "Enter"
      comment = "Select option"

    wait:
      frames = 3

    screenshot:
      path = "/tmp/dropdown_selected.png"
```

### Form Input Test

```kyt
Test:
  name = "Form Submission"
  target = "examples/nim/form_demo.nim"

  Scenario:
    wait:
      frames = 10

    # Click first input
    click:
      component_id = 20
      comment = "Focus name field"

    wait:
      frames = 2

    text_input:
      text = "John Doe"

    wait:
      frames = 2

    # Tab to next field
    key_press:
      key = "Tab"

    wait:
      frames = 2

    text_input:
      text = "john@example.com"

    wait:
      frames = 2

    screenshot:
      path = "/tmp/form_filled.png"

    # Submit
    key_press:
      key = "Enter"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/form_submitted.png"
```

## Debugging Tests

### Find Component IDs

```bash
# View full tree
kryon tree build/ir/app.kir

# Search for specific component
kryon tree build/ir/app.kir | grep "Tab #"

# With visual info (positions, colors)
kryon tree build/ir/app.kir --visual
```

### Check Screenshot Timing

If screenshots are blank or show wrong state:

```kyt
# Increase wait frames
wait:
  frames = 20  # Instead of 10
```

### Component Not Found

If you see "Component #X not found":
1. Verify component ID with `kryon tree`
2. Check if component is rendered (has dimensions)
3. Increase initial wait time for layout

### Screenshots Not Created

Screenshots are saved as `.bmp` files with `.png` extension appended:
- Requested: `/tmp/test.png`
- Actual: `/tmp/test.png.bmp`

Convert to PNG:
```bash
magick /tmp/test.png.bmp /tmp/test.png
```

## Best Practices

### 1. Use Meaningful Comments

```kyt
click:
  component_id = 8
  comment = "Click Profile tab - should show user info"
```

### 2. Wait After User Actions

```kyt
click:
  component_id = 8

wait:
  frames = 5  # Allow animation/state update

screenshot:
  path = "/tmp/after_click.png"
```

### 3. Organize Tests by Feature

```
tests/interactive/
  ├── tabs_navigation.kyt
  ├── button_counters.kyt
  ├── dropdown_selection.kyt
  └── form_validation.kyt
```

### 4. Use Descriptive Test Names

```kyt
Test:
  name = "Tab Navigation with State Preservation"
  # Not: "Test 1"
```

### 5. Capture Before/After States

```kyt
Setup:
  screenshot:
    path = "/tmp/initial_state.png"

Scenario:
  click:
    component_id = 8

  screenshot:
    path = "/tmp/after_action.png"
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Interactive Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libsdl3-dev

      - name: Build Kryon
        run: make build && make install

      - name: Run interactive tests
        run: |
          for test in tests/interactive/*.kyt; do
            echo "Running $test"
            kryon test "$test"
          done

      - name: Upload screenshots on failure
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: test-screenshots
          path: /tmp/*.bmp
```

### Makefile Target

```makefile
.PHONY: test-interactive
test-interactive:
	@echo "Running interactive tests..."
	@for test in tests/interactive/*.kyt; do \
		echo "Testing: $$test"; \
		kryon test "$$test" || exit 1; \
	done
	@echo "All tests passed!"
```

## Tips for Test Creation

1. **Start Simple** - Test one interaction at a time
2. **Verify Component IDs** - Always check with `kryon tree` first
3. **Use Wait Events** - Don't rush - give UI time to update
4. **Capture Evidence** - Screenshots before/after every important action
5. **Document Intent** - Use comments to explain what you're testing
6. **Test Edge Cases** - Multiple clicks, rapid input, invalid states
7. **Keep Tests Focused** - One feature per test file

## Quick Reference

```bash
# Find component IDs
kryon tree build/ir/app.kir

# Create test file
vim tests/interactive/my_test.kyt

# Run test
kryon test tests/interactive/my_test.kyt

# Run with details
kryon test tests/interactive/my_test.kyt --verbose

# Check screenshots
ls -lh /tmp/*.bmp

# Convert screenshots
magick /tmp/test.png.bmp /tmp/test.png
```

## Example: Complete Tab Test

```kyt
# tests/interactive/tabs_complete.kyt

Test:
  name = "Complete Tab Navigation Test"
  target = "examples/kry/tabs_reorderable.kry"
  description = "Verify all tabs are clickable and show correct content"

  Setup:
    wait:
      frames = 10
      comment = "Initial render"

    screenshot:
      path = "/tmp/tabs_initial.png"
      comment = "Should show Home tab active"

  Scenario:
    # Test Profile tab
    click:
      component_id = 8
      comment = "Click Profile tab"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/tabs_profile.png"
      comment = "Profile content visible"

    # Test Settings tab
    click:
      component_id = 9
      comment = "Click Settings tab"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/tabs_settings.png"
      comment = "Settings content visible"

    # Test About tab
    click:
      component_id = 10
      comment = "Click About tab"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/tabs_about.png"
      comment = "About content visible"

    # Return to Home
    click:
      component_id = 7
      comment = "Return to Home tab"

    wait:
      frames = 5

    screenshot:
      path = "/tmp/tabs_home_return.png"
      comment = "Back to Home - verify state preserved"

  Teardown:
    wait:
      frames = 2
      comment = "Final cleanup"
```

## Troubleshooting

### Test Hangs/Timeouts

- Increase timeout: `timeout 60` in the generated command
- Check if app enters infinite loop
- Verify test events are valid

### Wrong Component Clicked

- Verify component IDs with `kryon tree`
- Check if component has rendered_bounds (dimensions)
- Try coordinate-based click instead

### Screenshots Show Blank Screen

- Increase initial wait frames
- Check if headless mode is working correctly
- Verify SDL3 renderer is initialized

### Test Passes but Behavior Wrong

- Add more screenshots to see intermediate states
- Use verbose mode to see all events
- Check component IDs match expected elements

## Summary

The Kryon interactive testing system provides:
- ✅ Declarative test format (`.kyt`)
- ✅ Automated UI interaction
- ✅ Screenshot capture
- ✅ CI/CD integration
- ✅ Simple CLI workflow

Write tests once, run them automatically, catch regressions early!
