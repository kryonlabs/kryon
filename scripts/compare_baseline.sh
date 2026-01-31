#!/bin/bash
#
# compare_baseline.sh
#
# Compares current metrics with captured baseline
# Usage: ./scripts/compare_baseline.sh [baseline_timestamp]
#

set -e

BASELINE_DIR=".refactoring/baselines"

# Default to latest baseline if none specified
if [ -z "$1" ]; then
    BASELINE_FILE="$BASELINE_DIR/baseline_latest.json"
    if [ ! -L "$BASELINE_FILE" ]; then
        echo "Error: No baseline found. Run ./scripts/capture_baseline.sh first."
        exit 1
    fi
    # Resolve symlink
    BASELINE_FILE=$(readlink -f "$BASELINE_FILE")
else
    BASELINE_FILE="$BASELINE_DIR/baseline_$1.json"
fi

if [ ! -f "$BASELINE_FILE" ]; then
    echo "Error: Baseline file not found: $BASELINE_FILE"
    exit 1
fi

echo "=== Comparing with Baseline ==="
echo "Baseline: $BASELINE_FILE"
echo ""

# Extract baseline values
BASELINE_LOC=$(jq -r '.metrics.project_loc // 0' "$BASELINE_FILE")
BASELINE_TESTS=$(jq -r '.metrics.test_files // 0' "$BASELINE_FILE")
BASELINE_BUILD_TIME=$(jq -r '.metrics.cli_build_time // "N/A"' "$BASELINE_FILE")

echo "Baseline Metrics:"
echo "  Project LOC: $BASELINE_LOC"
echo "  Test files: $BASELINE_TESTS"
echo "  CLI build time: $BASELINE_BUILD_TIME"
echo ""

# Measure current metrics
echo "Current Metrics:"

CURRENT_LOC=$(find . -name "*.c" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')

echo "  Project LOC: $CURRENT_LOC"

# Calculate difference
if [ "$BASELINE_LOC" -gt 0 ]; then
    DIFF=$((CURRENT_LOC - BASELINE_LOC))
    PCT_CHANGE=$(awk "BEGIN {printf \"%.1f\", ($DIFF / $BASELINE_LOC) * 100}")

    if [ $DIFF -lt 0 ]; then
        echo "  ✅ Decreased by: ${DIFF} lines ($PCT_CHANGE%)"
    elif [ $DIFF -gt 0 ]; then
        echo "  ⚠️  Increased by: +$DIFF lines (+$PCT_CHANGE%)"
    else
        echo "  ➡️  No change"
    fi
fi

echo ""
echo "=== Largest Files Changes ==="

# Get current largest files
CURRENT_LARGEST=$(find . -name "*.c" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" | xargs wc -l 2>/dev/null | sort -rn | head -20)

# Compare with baseline
echo "Checking for significant file size changes..."

while read count file; do
    if [ -f "$file" ]; then
        # Extract baseline count for this file
        BASELINE_COUNT=$(jq -r ".metrics.largest_files[] | select(.file == \"$file\") | .lines" "$BASELINE_FILE" 2>/dev/null || echo "0")

        if [ "$BASELINE_COUNT" != "0" ] && [ "$BASELINE_COUNT" != "null" ]; then
            FILE_DIFF=$((count - BASELINE_COUNT))

            if [ $FILE_DIFF -lt -50 ]; then
                echo "  ✅ $file: $BASELINE_COUNT → $count (${FILE_DIFF} lines)"
            elif [ $FILE_DIFF -gt 50 ]; then
                echo "  ⚠️  $file: $BASELINE_COUNT → $count (+$FILE_DIFF lines)"
            fi
        fi
    fi
done <<< "$CURRENT_LARGEST"

echo ""
echo "=== Validation Checklist ==="

# Check for new files
NEW_FILES=$(find . -name "*.c" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" | wc -l)

BASELINE_FILE_COUNT=$(jq '.metrics.largest_files | length' "$BASELINE_FILE")

echo "  File count: $BASELINE_FILE_COUNT → $NEW_FILES"

# Check git status
GIT_CHANGES=$(git status --porcelain | wc -l)
echo "  Uncommitted changes: $GIT_CHANGES"

if [ $GIT_CHANGES -gt 0 ]; then
    echo "  ⚠️  You have uncommitted changes"
fi

echo ""
echo "=== Summary ==="

# Target tolerances
TARGET_REDUCTION=5
MAX_INCREASE=2

if [ "$BASELINE_LOC" -gt 0 ]; then
    PCT_CHANGE=$(awk "BEGIN {printf \"%.1f\", ($DIFF / $BASELINE_LOC) * 100}")

    # Check if within tolerances
    if [ $DIFF -lt 0 ]; then
        # Lines decreased - good!
        ABS_PCT=${PCT_CHANGE#-}
        if (( $(echo "$ABS_PCT >= $TARGET_REDUCTION" | bc -l) )); then
            echo "  ✅ EXCELLENT: Reduced by $PCT_CHANGE% (target: ≥${TARGET_REDUCTION}%)"
        else
            echo "  ✅ GOOD: Reduced by $PCT_CHANGE%"
        fi
    elif [ $DIFF -gt 0 ]; then
        # Lines increased
        if (( $(echo "$PCT_CHANGE <= $MAX_INCREASE" | bc -l) )); then
            echo "  ✅ ACCEPTABLE: Increased by $PCT_CHANGE% (max: ${MAX_INCREASE}%)"
        else
            echo "  ❌ CONCERN: Increased by $PCT_CHANGE% (exceeds ${MAX_INCREASE}%)"
        fi
    else
        echo "  ➡️  No change in LOC"
    fi
fi

echo ""
echo "Recommendation:"
if [ $DIFF -lt 0 ]; then
    echo "  🎉 Refactoring successful! Ready to merge."
elif [ $DIFF -le 0 ]; then
    echo "  ✅ Refactoring complete. No LOC change."
else
    echo "  ⚠️  Review changes before proceeding."
fi

echo ""
echo "Full baseline: $BASELINE_FILE"
