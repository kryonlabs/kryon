#!/usr/bin/env bash
# Visual Testing Script for Kryon
# Usage: ./test_visual.sh <example_name> [output_path]

set -e

EXAMPLE=${1:-tabs_reorderable}
OUTPUT=${2:-/tmp/kryon_test_${EXAMPLE}.png}

echo "🧪 Testing: $EXAMPLE"
echo "📸 Screenshot: $OUTPUT"

# Kill any running kryon instances
pkill -9 kryon 2>/dev/null || true

# Run with screenshot capture
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT="$OUTPUT" \
    KRYON_DEBUG_OVERLAY=1 \
    KRYON_SCREENSHOT_AFTER_FRAMES=5 \
    KRYON_HEADLESS=1 \
    timeout 5 bash run_example.sh "$EXAMPLE" 2>&1 | tail -20

# Check if screenshot was created (SDL saves as .bmp)
BMP_FILE="${OUTPUT}.bmp"
if [ -f "$BMP_FILE" ]; then
    echo "✅ Screenshot saved: $BMP_FILE"

    # Convert to PNG for easier viewing
    PNG_FILE="${OUTPUT}"
    if command -v magick &> /dev/null; then
        magick "$BMP_FILE" "$PNG_FILE" 2>/dev/null && echo "   Converted to: $PNG_FILE"
    fi

    ls -lh "$BMP_FILE"
else
    echo "❌ Screenshot not created"
    echo "   Expected: $BMP_FILE"
    exit 1
fi
