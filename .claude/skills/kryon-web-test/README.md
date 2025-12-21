# Kryon Web Test Skill

Systematic testing workflow for Kryon web output generation.

## Quick Start

### Test an Example
```bash
# Generate and inspect HTML
./scripts/generate_examples.sh button_demo --lang=html
cat examples/html/button_demo.html
```

### Test Website Build
```bash
cd /mnt/storage/Projects/kryon-website
kryon build --targets=web
cat build/index.html
```

### Serve and View in Browser
```bash
cd examples/html
python3 -m http.server 8000
# Open http://localhost:8000/button_demo.html
```

## What This Skill Tests

✅ **HTML Structure** - Valid, self-contained HTML5
✅ **Inline Styles** - All CSS properties embedded
✅ **JavaScript** - Event handlers and functions included
✅ **No External Files** - No CSS/JS file dependencies
✅ **Content Translation** - All IR components mapped correctly

## Key Commands

```bash
# Generate HTML from example
./scripts/generate_examples.sh <name> --lang=html

# Validate structure
grep 'style="' examples/html/<name>.html

# Check JavaScript
sed -n '/<script>/,/<\/script>/p' examples/html/<name>.html

# Test in browser
cd examples/html && python3 -m http.server 8000
```

## File Paths

- **Examples**: `examples/html/<name>.html`
- **Website**: `/mnt/storage/Projects/kryon-website/build/*.html`
- **Transpiler**: `cli/codegen_html.nim`

## Integration with Claude Code

This skill automatically activates when:
- User asks to test web output
- Making changes to HTML generation
- Debugging web build issues

See `skill.md` for complete workflow and examples.
