#!/bin/bash
# Example test script for Kryon web output validation
# Usage: ./test_example.sh <example_name>

set -e

EXAMPLE=${1:-button_demo}
HTML_FILE="examples/html/${EXAMPLE}.html"

echo "========================================="
echo "Kryon Web Test: ${EXAMPLE}"
echo "========================================="

# Step 1: Generate HTML
echo ""
echo "Step 1: Generating HTML..."
./scripts/generate_examples.sh "${EXAMPLE}" --lang=html

# Step 2: Validate structure
echo ""
echo "Step 2: Validating HTML structure..."
echo -n "  External references (should be 0): "
grep -E '(href|src)="[^#]' "${HTML_FILE}" | grep -v 'data:' | wc -l || echo "0"

echo -n "  Inline style attributes: "
grep -c 'style="' "${HTML_FILE}" || echo "0"

echo -n "  Flexbox declarations: "
grep -c 'display: flex' "${HTML_FILE}" || echo "0"

# Step 3: Check JavaScript
echo ""
echo "Step 3: Checking JavaScript..."
if grep -q '<script>' "${HTML_FILE}"; then
    echo "  ✅ JavaScript found"
    sed -n '/<script>/,/<\/script>/p' "${HTML_FILE}" | head -10
else
    echo "  ⚠️  No JavaScript (may be expected)"
fi

# Step 4: Check event handlers
echo ""
echo "Step 4: Checking event handlers..."
if grep -q 'onclick=' "${HTML_FILE}"; then
    echo "  ✅ Event handlers found:"
    grep -o 'onclick="[^"]*"' "${HTML_FILE}" || echo "  None"
else
    echo "  ⚠️  No event handlers (may be expected)"
fi

# Step 5: Show HTML head
echo ""
echo "Step 5: HTML head section..."
sed -n '/<head>/,/<\/head>/p' "${HTML_FILE}"

# Summary
echo ""
echo "========================================="
echo "✅ Test completed: ${EXAMPLE}"
echo "========================================="
echo ""
echo "To view in browser:"
echo "  cd examples/html"
echo "  python3 -m http.server 8000"
echo "  Open: http://localhost:8000/${EXAMPLE}.html"
echo ""
