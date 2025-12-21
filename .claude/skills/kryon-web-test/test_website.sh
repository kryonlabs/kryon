#!/bin/bash
# Test Kryon website build and validate output
# Usage: ./test_website.sh

set -e

WEBSITE_DIR="/mnt/storage/Projects/kryon-website"
BUILD_DIR="${WEBSITE_DIR}/build"

echo "========================================="
echo "Kryon Website Build Test"
echo "========================================="

# Step 1: Clean and rebuild
echo ""
echo "Step 1: Building website..."
cd "${WEBSITE_DIR}"
rm -rf build/
kryon build --targets=web 2>&1 | head -30

# Step 2: Check build output
echo ""
echo "Step 2: Checking build output..."
echo -n "  HTML pages generated: "
ls -1 "${BUILD_DIR}"/*.html 2>/dev/null | wc -l || echo "0"

echo -n "  Total build size: "
du -sh "${BUILD_DIR}" | cut -f1

# Step 3: Validate index.html
echo ""
echo "Step 3: Validating index.html..."
INDEX_FILE="${BUILD_DIR}/index.html"

if [ -f "${INDEX_FILE}" ]; then
    echo -n "  External CSS references: "
    grep -c 'rel="stylesheet"' "${INDEX_FILE}" 2>/dev/null || echo "0"

    echo -n "  External JS references: "
    grep -c '<script src=' "${INDEX_FILE}" 2>/dev/null || echo "0"

    echo -n "  Inline style attributes: "
    grep -c 'style="' "${INDEX_FILE}" 2>/dev/null || echo "0"

    echo -n "  Flexbox declarations: "
    grep -c 'display: flex' "${INDEX_FILE}" 2>/dev/null || echo "0"
else
    echo "  ❌ index.html not found!"
    exit 1
fi

# Step 4: Show HTML structure
echo ""
echo "Step 4: HTML structure (first 30 lines)..."
head -30 "${INDEX_FILE}"

# Step 5: Check for broken references
echo ""
echo "Step 5: Checking for broken references..."
echo "  Searching for external src/href attributes..."
BROKEN=$(grep -rh -E '(href|src)="[^#]' "${BUILD_DIR}"/*.html 2>/dev/null | \
         grep -v 'data:' | \
         grep -v 'http' | \
         grep -v 'mailto:' || echo "")

if [ -z "$BROKEN" ]; then
    echo "  ✅ No broken references found"
else
    echo "  ⚠️  Found references:"
    echo "$BROKEN" | head -5
fi

# Step 6: List all generated pages
echo ""
echo "Step 6: Generated pages..."
ls -lh "${BUILD_DIR}"/*.html 2>/dev/null | awk '{print "  ", $9, "(" $5 ")"}'

# Summary
echo ""
echo "========================================="
echo "✅ Website test completed"
echo "========================================="
echo ""
echo "To view in browser:"
echo "  cd ${BUILD_DIR}"
echo "  python3 -m http.server 8000"
echo "  Open: http://localhost:8000/index.html"
echo ""
