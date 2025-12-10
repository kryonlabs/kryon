# Kryon Layout Inspector Skill

## Purpose
Deep-dive into layout calculations to debug flex, sizing, width, height, and positioning issues in Kryon UI components.

## When to Activate
This skill should activate when the user mentions:
- Layout problems ("wrong size", "incorrect width/height")
- Flex issues ("not growing", "not shrinking", "flex not working")
- Positioning problems ("wrong position", "misaligned")
- Sizing issues ("too small", "too large", "auto-sizing broken")
- Gap/spacing problems ("gaps not working", "spacing incorrect")

## Workflow

### Step 1: Take Debug Overlay Screenshot
Always start with a visual confirmation:

```bash
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/layout_debug.png \
    KRYON_DEBUG_OVERLAY=1 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

Read the screenshot to see component boundaries and dimensions.

### Step 2: Dump Component Tree
Get the component hierarchy with detailed layout information:

```bash
# Enhanced tree with dimensions and colors
~/.local/bin/kryon tree <file>.kir --visual > /tmp/component_tree.txt

# Or from cached run:
~/.local/bin/kryon tree ~/.cache/kryon/cli_run/<filename>.kir --visual
```

Read the tree output to find:
- Component IDs
- Component types
- Dimensions (width×height)
- Colors (background, text color)
- Hierarchy structure

Example output:
```
TabBar #6 (auto×auto) [bg:#1a1a1a] (4 children)
├── Tab #7 (auto×auto) [bg:#3d3d3d] [Home]
├── Tab #8 (auto×auto) [bg:#3d3d3d] [Profile]
```

This immediately shows if components have auto dimensions or explicit sizes.

### Step 3: Enable Layout Tracing
Run with layout tracing to see actual calculations:

```bash
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_TRACE_LAYOUT=1 \
    KRYON_HEADLESS=1 \
    timeout 1 ~/.local/bin/kryon run <file> 2>&1 | tee /tmp/layout_trace.log
```

Read the trace output to see:
- Calculated dimensions for each component
- Flex grow/shrink distribution
- Gap distribution
- Parent-child layout relationships

### Step 4: Analyze Layout Issues
Look for common problems in the trace:

**Auto-sizing issues**:
- Components with width/height = 0
- Components that should auto-size but have explicit dimensions
- Parent constraining child incorrectly

**Flex issues**:
- Flex grow not applied (check for flex_extra_widths)
- Flex shrink when components should grow
- Total flex_grow = 0 (no distribution)

**Gap issues**:
- Gap value not appearing in layout trace
- Gaps not distributed evenly (SPACE_BETWEEN vs SPACE_EVENLY)

**Parent-child issues**:
- Child larger than parent (overflow)
- Parent auto-sizing but child explicit-sized
- Parent width/height not propagating

### Step 5: Check IR Definition
If the issue seems to be in the IR, inspect the .kir file:

```bash
# For .kry files, check generated KIR
cat ~/.cache/kryon/cli_run/<filename>.kir | grep -A 20 "\"id\": <component_id>"
```

Look for:
- Explicit width/height values
- Flex properties (grow, shrink)
- Layout direction (row vs column)
- Gap values
- Padding/margin

### Step 6: Inspect Default Properties
Check if component has correct defaults in ir_json_v2.c:

```bash
grep -A 10 "IR_COMPONENT_<TYPE>" /home/wao/Projects/kryon/ir/ir_json_v2.c
```

Common issues:
- Missing default width: 100% for containers
- Missing default flex properties for tabs
- Wrong default layout direction

### Step 7: Make Targeted Fix
Based on the analysis, make a specific fix:

**Fix type 1: Default properties** (ir_json_v2.c)
- Add default width/height
- Add default flex properties
- Set layout direction

**Fix type 2: Layout calculation** (desktop_layout.c)
- Add component to auto-sizing special case
- Add component measurement
- Fix flex distribution

**Fix type 3: Rendering** (desktop_rendering.c)
- Apply flex_extra_widths to component
- Fix gap distribution
- Correct child positioning

### Step 8: Verify Fix
Take another debug overlay screenshot and trace:

```bash
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/layout_fixed.png \
    KRYON_DEBUG_OVERLAY=1 \
    KRYON_TRACE_LAYOUT=1 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file> 2>&1 | tee /tmp/layout_fixed_trace.log
```

Compare before/after:
- Dimensions changed as expected
- Flex distribution now correct
- Gaps properly distributed
- Components positioned correctly

## Environment Variables

- `KRYON_TRACE_LAYOUT=1` - Enable layout calculation tracing
- `KRYON_TRACE_COMPONENTS=1` - Trace component creation
- `KRYON_TRACE_COORDINATES_FLOW=1` - Trace coordinate flow
- `KRYON_DEBUG_OVERLAY=1` - Show component boundaries

## Common Layout Issues and Fixes

### Issue: TabBar only auto-sizing to content width
**Symptom**: TabBar 289px instead of 760px
**Trace**: `TabBar rect=(20,117,289,29)` instead of `(20,117,760,29)`
**Fix**: Set TabBar default width to 100% in ir_json_v2.c
```c
if (component->type == IR_COMPONENT_TAB_BAR) {
    component->style->width.type = IR_DIMENSION_PERCENT;
    component->style->width.value = 100.0f;
}
```

### Issue: Tabs not growing to fill TabBar
**Symptom**: Tabs all have equal fixed width, gaps between them
**Trace**: `Tab rect width=68` without flex_extra_widths applied
**Fix**: Add default flex_grow=1 in ir_json_v2.c AND apply flex in desktop_rendering.c
```c
// ir_json_v2.c
if (component->type == IR_COMPONENT_TAB) {
    component->layout->flex.grow = 1;
}

// desktop_rendering.c
if (component->type == IR_COMPONENT_TAB_BAR && flex_extra_widths) {
    rect_for_child.width += flex_extra_widths[i];
}
```

### Issue: Text component has 0 dimensions
**Symptom**: Text not visible, width=0 height=0
**Trace**: `Text rect=(0,0,0,0)`
**Fix**: Add TEXT to auto-sizing special case in desktop_layout.c
```c
if (component->type == IR_COMPONENT_TEXT) {
    rect.width = 0;
    rect.height = 0;
}
```

### Issue: Button not measured correctly
**Symptom**: Button width/height = 0 or too small
**Trace**: `Button rect=(0,0,0,0)` after layout
**Fix**: Add Button measurement in get_child_size() in desktop_layout.c
```c
if (child->type == IR_COMPONENT_BUTTON) {
    if (layout.width <= 0.0f) {
        layout.width = get_child_dimension(child, parent_rect, false);
    }
}
```

## Examples

### Example 1: TabBar Width Issue
```
User: "Tab bar is too narrow"
Claude: Let me check the layout with debug overlay.
[Takes screenshot with KRYON_DEBUG_OVERLAY=1]
[Reads screenshot - sees TabBar is 289px instead of 760px]
Claude: The TabBar is auto-sizing. Let me check the layout trace.
[Runs with KRYON_TRACE_LAYOUT=1]
[Sees: "TabBar rect=(20,117,289,29)"]
Claude: TabBar needs default width: 100%. Fixing ir_json_v2.c.
[Makes fix]
[Verifies with screenshot - TabBar now 760px]
```

### Example 2: Flex Not Working
```
User: "Tabs aren't growing to fill space"
Claude: I'll check if flex_grow is applied.
[Runs with KRYON_TRACE_LAYOUT=1]
[Sees: "flex_grow=1" in trace but no flex_extra_widths applied]
Claude: Flex is calculated but not applied to Tabs in rendering.
[Adds flex_extra_widths application in desktop_rendering.c]
[Verifies - tabs now span full width evenly]
```

### Example 3: Component Too Small
```
User: "Button is tiny"
Claude: Let me see the actual dimensions.
[Takes screenshot with KRYON_DEBUG_OVERLAY=1]
[Sees: "Button #12 (10x5 at 50,100)"]
Claude: Button has near-zero dimensions. Checking measurement.
[Runs with KRYON_TRACE_LAYOUT=1]
[Sees: "Button rect=(50,100,0,0)" - not measured]
Claude: Button not in measurement code. Adding to get_child_size().
[Makes fix]
[Verifies - button now has proper dimensions]
```

## Success Criteria

Using this skill, layout issues should:
- Be identified in < 10 minutes
- Require 1-2 targeted fixes
- Be verified with visual and trace evidence
- Not require multiple blind attempts

## Related Skills

- `kryon-visual-debug` - Visual confirmation of issues
- `kryon-screenshot-diff` - Compare before/after layouts