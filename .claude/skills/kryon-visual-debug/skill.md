# Kryon Visual Debug Skill

## Purpose
Automatically use screenshot capture and debug overlay when debugging Kryon UI rendering issues. This skill ensures you SEE what's actually rendering before making assumptions or code changes.

## When to Activate
This skill should activate AUTOMATICALLY when the user mentions:
- UI components not visible ("I don't see...", "not showing", "invisible")
- Rendering problems ("not rendering", "doesn't appear", "can't see")
- Layout issues with visual components
- Any UI debugging where you cannot see the actual output

## Workflow

### Step 1: Take Screenshot FIRST
**CRITICAL**: Before making ANY code changes or assumptions, take a screenshot to see the actual rendered output.

```bash
# Basic screenshot (exits after 3 frames by default)
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/kryon_debug.png \
    KRYON_SCREENSHOT_AFTER_FRAMES=3 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

**Then READ the screenshot** using the Read tool:
```
Read: /tmp/kryon_debug.png
```

### Step 2: Enable Debug Overlay
If components are hard to see or you need to verify dimensions and positions:

```bash
# Screenshot with debug overlay showing component boundaries
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/kryon_debug_overlay.png \
    KRYON_SCREENSHOT_AFTER_FRAMES=3 \
    KRYON_HEADLESS=1 \
    KRYON_DEBUG_OVERLAY=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

The debug overlay shows:
- **Colored bounding boxes** around each component
- **Component labels** with type, ID, dimensions, and position
- **Component types** color-coded:
  - Blue: Containers, Columns, Rows
  - Green: Tab components (TabGroup, TabBar, Tab, etc.)
  - Yellow: Text
  - Red: Buttons
  - Orange: Inputs
  - Purple: Checkboxes
  - Cyan: Dropdowns

**Then READ the overlay screenshot**:
```
Read: /tmp/kryon_debug_overlay.png
```

### Step 3: Inspect Component Tree
If you need more detailed information about the component hierarchy:

```bash
# Dump component tree with visual formatting
~/.local/bin/kryon tree <file>.kir --visual --with-colors --with-positions > /tmp/tree.txt
```

Read the tree output to see:
- Component hierarchy
- Dimensions and positions
- Text content
- Child counts

### Step 4: Make Code Changes
**ONLY AFTER** you've seen the actual rendering, make code changes based on what you observed.

### Step 5: Verify Fix with Screenshot
After making changes, take another screenshot to verify the fix:

```bash
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/kryon_after_fix.png \
    KRYON_SCREENSHOT_AFTER_FRAMES=3 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

**Read the after screenshot** and compare with the before screenshot.

## Environment Variables

- `KRYON_SCREENSHOT=/path/to/output.png` - Enable screenshot capture
- `KRYON_SCREENSHOT_AFTER_FRAMES=N` - Take screenshot after N frames (default: 5)
- `KRYON_HEADLESS=1` - Run without window, exit after screenshot
- `KRYON_DEBUG_OVERLAY=1` - Show component boundaries and labels

## When NOT to Use This Skill

- Code-only issues (compilation errors, logic bugs)
- Issues with clear error messages that don't involve visual output
- Non-UI debugging (IR serialization, parsing, etc.)
- When user already provided screenshots showing the problem

## Real Example: Tab Bar Debugging

**User said**: "nothing is fixed, i don't see any tabbar"

**Correct approach WITH this skill**:
1. Take screenshot: `KRYON_SCREENSHOT=/tmp/tabs.png kryon run tabs.kry`
2. Read /tmp/tabs.png → **SEE** tabs are tiny/missing
3. Take overlay screenshot: `KRYON_DEBUG_OVERLAY=1 KRYON_SCREENSHOT=/tmp/overlay.png kryon run tabs.kry`
4. Read overlay → **SEE** TabBar is 289px not 760px with bounding boxes
5. Fix: Set TabBar default width to 100%
6. Take after screenshot → **VERIFY** tabs now render correctly

**Time**: 5 minutes, 1 iteration

**Wrong approach WITHOUT this skill** (what we did before):
1. Assume tabs rendering based on logs
2. Make fix for tab sizing
3. User: "not working"
4. Make fix for flex properties
5. User: "not working"
6. Make fix for TabBar width
7. User: "not working"
8. Continue blind guessing...

**Time**: 2+ hours, 8+ iterations

## Examples

### Example 1: Component Not Visible
```
User: "The button isn't showing up"
Claude: Let me take a screenshot to see what's actually rendering.
[Takes screenshot with KRYON_SCREENSHOT]
[Reads screenshot - sees button is offscreen/clipped]
Claude: The button is rendering at coordinates outside the visible area. Let me check the layout calculation...
[Makes targeted fix based on visual evidence]
[Takes verification screenshot]
Claude: The button now renders correctly in the visible area.
```

### Example 2: Component Too Small
```
User: "The tabs are invisible"
Claude: I'll capture a screenshot with debug overlay to see the tab dimensions.
[Takes screenshot with KRYON_DEBUG_OVERLAY=1]
[Reads overlay - sees TabBar is 50px wide with tiny tabs]
Claude: The TabBar has auto width and is only 50px. Setting it to 100% width.
[Makes fix]
[Takes after screenshot - tabs now span full width]
Claude: Fixed! Tabs now span the full 760px width.
```

### Example 3: Wrong Position
```
User: "The text appears in the wrong place"
Claude: Let me take a debug overlay screenshot to see the actual position.
[Takes screenshot with KRYON_DEBUG_OVERLAY=1]
[Reads overlay - sees Text component label showing position]
Claude: The text is at (50, 200) but should be at (100, 100). The parent container has incorrect padding.
[Fixes padding]
[Verifies with screenshot]
```

## Success Criteria

Using this skill, UI debugging should:
- Take < 5 minutes to identify the problem
- Require < 2 iterations to fix
- Provide visual confirmation of the fix
- Work entirely through screenshots (no live window required)
- Eliminate blind guessing and multiple failed attempts

## Implementation Notes

- Screenshots are saved as PNG files
- Debug overlay adds ~50KB to screenshot size
- Headless mode exits automatically after screenshot
- Works with .kry, .nim, and .kir file formats
- Compatible with all Kryon renderers (SDL3, terminal)

## Related Skills

- `kryon-layout-inspector` - Deep dive into layout calculations
- `kryon-screenshot-diff` - Compare before/after screenshots