# Kryon Web Test Skill

## Purpose
Systematically test Kryon web output generation, validate HTML structure, verify inline styles, and optionally test in a browser. This ensures the Nim transpiler correctly translates all IR components to self-contained HTML.

## When to Activate
This skill should activate AUTOMATICALLY when:
- User asks to test web output ("test the website", "check web build")
- Making changes to web transpilation (codegen_html.nim, build.nim)
- Adding new components or features that affect HTML generation
- User reports web build issues ("website broken", "HTML not working")
- After modifying HTML/CSS generation logic

## Workflow

### Step 1: Generate HTML from Example or KIR

**For examples (.kry files)**:
```bash
# Generate HTML example
./scripts/generate_examples.sh <example_name> --lang=html

# Example:
./scripts/generate_examples.sh button_demo --lang=html
```

**For website builds**:
```bash
# Build website
cd /mnt/storage/Projects/kryon-website
rm -rf build/
kryon build --targets=web
```

**Direct codegen from KIR**:
```bash
# Generate HTML directly from KIR file
kryon codegen <file>.kir --lang=html > /tmp/output.html
```

### Step 2: Validate HTML Structure

**Read and inspect the generated HTML**:
```
Read: examples/html/<example_name>.html
# or
Read: /mnt/storage/Projects/kryon-website/build/index.html
```

**Check for critical elements**:
- ✅ `<!DOCTYPE html>` declaration
- ✅ Proper `<head>` with charset and viewport
- ✅ No external dependencies (`<link>` or `<script src>`)
- ✅ Inline styles on elements
- ✅ JavaScript functions in `<script>` tags (if applicable)
- ✅ Proper element nesting and closing tags

**Validation checklist**:
```bash
# Check for broken references (should return nothing)
grep -E '(href|src)="[^#]' examples/html/<example>.html | grep -v 'data:'

# Count inline style attributes (should be > 0)
grep -c 'style="' examples/html/<example>.html

# Check for flexbox properties (should be present)
grep 'display: flex' examples/html/<example>.html
```

### Step 3: Verify Inline CSS Properties

**Required inline style properties to check**:
- `display: flex` / `flex-direction: row|column`
- `justify-content`, `align-items` (for alignment)
- `gap` (for spacing)
- `width`, `height` (for dimensions)
- `background-color`, `color` (for colors)
- `padding`, `margin` (for spacing)
- `font-size`, `font-weight` (for typography)

**Example inspection**:
```bash
# Check a specific component's styles
grep -A 1 'id="kryon-1"' examples/html/button_demo.html
```

Expected output should show inline styles:
```html
<div id="kryon-1" class="kryon-column" data-ir-type="COLUMN" style="display: flex; flex-direction: column">
```

### Step 4: Test JavaScript (if applicable)

**Check for JavaScript code**:
```bash
# Extract JavaScript from HTML
sed -n '/<script>/,/<\/script>/p' examples/html/<example>.html
```

**Verify**:
- ✅ Functions defined in `<script>` tag
- ✅ Event handlers reference correct function names
- ✅ onclick/onchange attributes match function names

**Example**:
```html
<script>
  function handleButtonClick() {
    console.log("Button clicked!");
  }
</script>
...
<button onclick="handleButtonClick()">Click Me!</button>
```

### Step 5: Start Local Web Server (Optional)

**For manual browser testing**:
```bash
# Navigate to output directory
cd examples/html
# or
cd /mnt/storage/Projects/kryon-website/build

# Start simple HTTP server
python3 -m http.server 8000
```

**Then open in browser**: http://localhost:8000/<example>.html

**Manual verification**:
- Visual appearance matches expected design
- Flexbox layout works (responsive, aligned)
- Colors and typography render correctly
- Buttons and interactive elements work (if JavaScript present)
- No console errors (check browser DevTools)

### Step 6: Browser Screenshot (Advanced)

**If browser automation is available**:
```bash
# Using headless Chrome/Chromium
chromium --headless --screenshot=/tmp/web_test.png --window-size=800,600 file:///path/to/example.html
```

**Or Firefox**:
```bash
firefox --headless --screenshot /tmp/web_test.png file:///path/to/example.html
```

**Then read the screenshot**:
```
Read: /tmp/web_test.png
```

### Step 7: Compare with Expected Output

**For regression testing**:
```bash
# Save reference screenshot
cp /tmp/web_test.png examples/html/screenshots/<example>_reference.png

# Later, compare with new output
diff /tmp/web_test.png examples/html/screenshots/<example>_reference.png
```

## Quick Test Commands

### Test Single Example
```bash
# Generate and validate in one go
./scripts/generate_examples.sh button_demo --lang=html && \
  echo "=== HTML Structure ===" && \
  head -20 examples/html/button_demo.html && \
  echo "=== Inline Styles ===" && \
  grep -c 'style="' examples/html/button_demo.html && \
  echo "=== JavaScript ===" && \
  sed -n '/<script>/,/<\/script>/p' examples/html/button_demo.html
```

### Test All Examples
```bash
# Generate all HTML examples
./scripts/generate_examples.sh --lang=html

# Count generated files
ls -1 examples/html/*.html | wc -l
```

### Validate Website Build
```bash
cd /mnt/storage/Projects/kryon-website
rm -rf build/
kryon build --targets=web

# Check main page
echo "=== Index Page ==="
head -30 build/index.html

# Check for broken references
echo "=== External References (should be empty) ==="
grep -rh 'src="' build/*.html | grep -v 'data:' || echo "✅ No external script references"
grep -rh 'href="' build/*.html | grep 'stylesheet' || echo "✅ No external CSS references"

# Count pages built
echo "=== Pages Built ==="
ls -1 build/*.html | wc -l
```

## Environment Variables

No special environment variables needed for web testing (unlike desktop rendering).

## When NOT to Use This Skill

- Testing desktop/terminal renderers (use kryon-visual-debug instead)
- Performance/memory issues (use kryon-memory-debug instead)
- Layout computation bugs (use kryon-layout-inspector instead)
- IR serialization issues (no HTML generation involved)

## Common Issues and Solutions

### Issue 1: Missing Inline Styles
**Symptom**: HTML elements have no `style` attribute
**Check**: `generateInlineStyles()` in cli/codegen_html.nim
**Fix**: Ensure all IR properties are mapped to CSS properties

### Issue 2: Empty HTML Output
**Symptom**: HTML file exists but has no content
**Root Cause**: Component type not in `htmlTag` mapping
**Fix**: Add component type to case statement in `generateVanillaHTMLElement()`

### Issue 3: JavaScript Not Included
**Symptom**: No `<script>` tag in HTML
**Root Cause**: KIR file missing `sources.js` field
**Fix**: Ensure .kry file has JavaScript code, or check TSX/JSX compilation

### Issue 4: Event Handlers Missing
**Symptom**: No `onclick` attributes on buttons
**Root Cause**: Missing `events` array in IR or logic functions
**Fix**: Verify event handler registration in source file

### Issue 5: Broken Layout
**Symptom**: Elements not positioned correctly
**Root Cause**: Missing flexbox properties in inline styles
**Fix**: Add flexbox properties to `generateInlineStyles()`

## Real Example: Button Demo Test

```bash
# Step 1: Generate HTML
./scripts/generate_examples.sh button_demo --lang=html

# Step 2: Read the output
cat examples/html/button_demo.html

# Expected output:
# ✅ <script> tag with handleButtonClick function
# ✅ Inline styles with display: flex
# ✅ Button with onclick="handleButtonClick()"
# ✅ Button text: "Click Me!"
# ✅ No external CSS or JS files

# Step 3: Start web server
cd examples/html
python3 -m http.server 8000 &

# Step 4: Open in browser
firefox http://localhost:8000/button_demo.html

# Step 5: Manual verification
# - Button visible and styled
# - Clicking button logs to console
# - Layout centered with flexbox

# Step 6: Stop server
pkill -f "python3 -m http.server 8000"
```

**Time**: 2-3 minutes per example

## Success Criteria

Using this skill, web output testing should:
- Generate valid, self-contained HTML (< 1 minute)
- Verify all styles are inline (< 30 seconds)
- Confirm no external dependencies (< 10 seconds)
- Optionally test in browser (< 2 minutes)
- Catch regressions before user reports them
- Work for both examples and full website builds

## Testing Checklist

For each web output test, verify:

**Structure**:
- [ ] Valid HTML5 DOCTYPE
- [ ] Proper head with meta tags
- [ ] All tags properly closed
- [ ] Correct element nesting

**Styles**:
- [ ] Inline styles on all styled elements
- [ ] Flexbox properties present (display, direction, justify, align)
- [ ] Colors and dimensions specified
- [ ] Typography properties included
- [ ] No external CSS file references

**JavaScript** (if applicable):
- [ ] Script tag in head
- [ ] Functions properly defined
- [ ] Event handlers reference correct functions
- [ ] No external JS file references

**Content**:
- [ ] Text content properly embedded
- [ ] Attributes correctly set (id, class, data-*)
- [ ] Component types mapped to HTML tags
- [ ] All IR components translated

**Validation**:
- [ ] No 404 errors in browser console
- [ ] No JavaScript errors
- [ ] Layout renders correctly
- [ ] Interactive elements work

## File Locations

**Generated HTML Examples**:
- Source: `examples/kry/<example>.kry`
- Output: `examples/html/<example>.html`

**Website Build**:
- Source: `src/*.tsx` or `docs/*.md`
- Output: `build/*.html`

**Transpiler Code**:
- Main: `cli/codegen_html.nim`
- Shared: `cli/codegen_react_common.nim`
- Build: `cli/build.nim`

## Related Skills

- `kryon-visual-debug` - For desktop/terminal rendering issues
- `kryon-screenshot-diff` - Compare visual output changes
- `kryon-layout-inspector` - Debug layout calculations

## Implementation Notes

- Web output is pure HTML/CSS/JS (no binaries)
- No C libraries needed for web testing
- Can test entirely in browser without Kryon CLI
- Inline styles make HTML portable and self-contained
- JavaScript is optional (static pages work without it)

## Advanced Testing

### HTML Validation
```bash
# Install HTML validator (if available)
npm install -g html-validator-cli

# Validate HTML
html-validator examples/html/button_demo.html
```

### CSS Validation
```bash
# Extract inline styles and validate
grep -oP 'style="\K[^"]+' examples/html/button_demo.html > /tmp/styles.css
# Manual review or use CSS validator
```

### Accessibility Testing
```bash
# Check for semantic HTML
grep -E '<(header|nav|main|footer|article|section)' examples/html/button_demo.html

# Check for alt text on images
grep '<img' examples/html/button_demo.html | grep 'alt='
```

### Performance Testing
```bash
# Check file size (should be small, self-contained)
ls -lh examples/html/button_demo.html

# Typical sizes:
# - Simple example: 1-2 KB
# - Complex example: 3-5 KB
# - Full website page: 5-20 KB
```
