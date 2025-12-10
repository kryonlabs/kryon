# Kryon Screenshot Diff Skill

## Purpose
Verify UI fixes by comparing before/after screenshots to ensure changes worked as intended.

## When to Activate
This skill should activate:
- **ALWAYS** after making UI fixes, before claiming success
- When user asks "does it work now?" or "is it fixed?"
- When verifying regression fixes
- When testing visual changes

## Workflow

### Step 1: Take "Before" Screenshot (if not already taken)
If you haven't already captured a before screenshot:

```bash
# Save current state temporarily
git stash

# Take before screenshot
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/before.png \
    KRYON_DEBUG_OVERLAY=1 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>

# Restore changes
git stash pop
```

### Step 2: Take "After" Screenshot
After making your fix:

```bash
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/after.png \
    KRYON_DEBUG_OVERLAY=1 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

### Step 3: Read Both Screenshots
Use the Read tool to examine both images:

```
Read: /tmp/before.png
Read: /tmp/after.png
```

### Step 4: Compare and Verify
Look for the expected changes:

**For layout fixes**:
- Component dimensions changed
- Positions adjusted correctly
- Gaps/spacing now correct
- No unexpected side effects

**For visibility fixes**:
- Component now visible
- Correct rendering order (z-index)
- No overlapping issues

**For flex fixes**:
- Components distributed evenly
- Flex grow applied correctly
- Proper space allocation

### Step 5: Report to User
Show the user both screenshot paths and describe the changes:

```
I've verified the fix with screenshots:
- Before: /tmp/before.png - TabBar was 289px wide
- After: /tmp/after.png - TabBar now spans full 760px
The tabs now distribute evenly across the available space.
```

## Best Practices

### Always Use Debug Overlay
For verification screenshots, always enable `KRYON_DEBUG_OVERLAY=1` so you can:
- See exact component dimensions
- Verify positions numerically
- Spot alignment issues
- Confirm no components are offscreen

### Take Screenshots at Same Frame
Use the same `KRYON_SCREENSHOT_AFTER_FRAMES` value for both screenshots to ensure fair comparison:

```bash
# Both use 3 frames
KRYON_SCREENSHOT_AFTER_FRAMES=3
```

### Save Screenshots with Descriptive Names
Use descriptive names that indicate the issue:

```bash
/tmp/tabs_before_width_fix.png
/tmp/tabs_after_width_fix.png

/tmp/button_before_sizing.png
/tmp/button_after_sizing.png
```

### Check for Regressions
When comparing screenshots, look for:
- Unintended changes to other components
- New layout issues introduced
- Text that shifted unexpectedly
- Components that disappeared

## Examples

### Example 1: Verify TabBar Fix
```
[Before fix: TabBar 289px wide]
[After fix: TabBar 760px wide]

Claude: I've fixed the TabBar width issue. Let me verify with screenshots.
[Takes both screenshots with debug overlay]
[Reads both - confirms change]
Claude: Verified! Before: TabBar was 289px (see /tmp/before.png).
After: TabBar now spans 760px (see /tmp/after.png).
Tabs distribute evenly: 185px, 187px, 201px, 184px.
```

### Example 2: Verify Button Visibility
```
[Before fix: Button offscreen at y=1000]
[After fix: Button visible at y=100]

Claude: Button positioning fixed. Verifying...
[Takes before/after screenshots]
[Reads both - button now visible]
Claude: Confirmed! The button was at (50, 1000) offscreen.
Now at (50, 100) and clearly visible in the UI.
```

### Example 3: Verify Flex Distribution
```
[Before fix: Tabs bunched on left side]
[After fix: Tabs evenly distributed]

Claude: Applied flex_grow to tabs. Let me verify the distribution.
[Takes screenshots with debug overlay]
[Compares tab widths in overlay labels]
Claude: Success! Tabs now span full width:
- Before: 68px, 70px, 75px, 68px (gaps between them)
- After: 185px, 187px, 201px, 184px (evenly distributed)
```

### Example 4: Catch Regression
```
[Fix applied to TabBar]
[Takes after screenshot]
[Notices TabContent also changed unexpectedly]

Claude: Wait, I notice TabContent height also changed from 371px to 300px.
This is an unintended side effect. Let me investigate...
[Makes additional fix to prevent regression]
[Takes final screenshot confirming both correct]
```

## When NOT to Use This Skill

- For non-visual bugs (logic errors, crashes)
- When user already confirmed fix works
- For code-only changes (refactoring, internal APIs)
- For changes that don't affect visual output

## Automation Tip

If you're making multiple iterative fixes, keep a numbered sequence:

```bash
/tmp/tabs_v1.png  # Initial state
/tmp/tabs_v2.png  # After first fix attempt
/tmp/tabs_v3.png  # After second fix
/tmp/tabs_final.png  # Verified working state
```

This helps track the evolution of fixes.

## Success Criteria

Using this skill:
- NEVER claim a UI fix works without visual verification
- Catch regressions before user reports them
- Provide proof of fix with screenshot paths
- Build confidence in claimed fixes

## Related Skills

- `kryon-visual-debug` - Initial problem identification
- `kryon-layout-inspector` - Understanding layout issues