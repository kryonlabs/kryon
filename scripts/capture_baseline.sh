#!/bin/bash
#
# capture_baseline.sh
#
# Captures baseline metrics for refactoring validation
# Run this before starting any refactoring phase
#

set -e

BASELINE_DIR=".refactoring/baselines"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BASELINE_FILE="$BASELINE_DIR/baseline_$TIMESTAMP.txt"

# Create baseline directory
mkdir -p "$BASELINE_DIR"

echo "=== Capturing Baseline Metrics ==="
echo "Timestamp: $TIMESTAMP"
echo ""

# Write to file
{
    echo "# Kryon Refactoring Baseline"
    echo "# Generated: $TIMESTAMP"
    echo ""
    echo "## Git Information"
    echo "commit: $(git rev-parse HEAD)"
    echo "branch: $(git rev-parse --abbrev-ref HEAD)"
    echo "uncommitted_changes: $(git status --porcelain | wc -l)"
    echo ""

    echo "## Lines of Code"
    TOTAL_LOC=$(find . -name "*.c" -o -name "*.h" \
        -not -path "*/third_party/*" \
        -not -path "*/.kryon_cache/*" \
        -not -path "*/build/*" \
        | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')

    PROJECT_LOC=$(find . -name "*.c" -not -path "*/third_party/*" \
        -not -path "*/.kryon_cache/*" \
        -not -path "*/build/*" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')

    echo "total_loc: $TOTAL_LOC"
    echo "project_loc: $PROJECT_LOC"
    echo ""

    echo "## Binary Sizes"
    if [ -d "build" ]; then
        echo "build_size: $(du -sh build 2>/dev/null | cut -f1)"
        for lib in build/*.so; do
            if [ -f "$lib" ]; then
                SIZE=$(ls -lh "$lib" | awk '{print $5}')
                BASENAME=$(basename "$lib")
                echo "$BASENAME: $SIZE"
            fi
        done
    fi
    echo ""

    echo "## Test Coverage"
    TEST_FILES=$(find . -name "*test*.c" -o -name "test_*.sh" 2>/dev/null | wc -l)
    echo "test_files: $TEST_FILES"

    if [ -d "tests" ]; then
        TEST_DIRS=$(find tests -type d 2>/dev/null | wc -l)
        TEST_SCRIPTS=$(find tests -name "*.sh" 2>/dev/null | wc -l)
        echo "test_directories: $TEST_DIRS"
        echo "test_scripts: $TEST_SCRIPTS"
    fi
    echo ""

    echo "## Largest Files (Top 20)"
    find . -name "*.c" -not -path "*/third_party/*" \
        -not -path "*/.kryon_cache/*" \
        -not -path "*/build/*" \
        | xargs wc -l 2>/dev/null | sort -rn | head -20 | \
        while read count file; do
            if [ -f "$file" ]; then
                echo "$count $file"
            fi
        done
    echo ""

} > "$BASELINE_FILE"

# Also capture the output to stdout
cat "$BASELINE_FILE"

# Create symlink to latest baseline
ln -sf "baseline_$TIMESTAMP.txt" "$BASELINE_DIR/baseline_latest.txt"

echo ""
echo "=== Baseline Captured ==="
echo "Location: $BASELINE_FILE"
echo ""
echo "To compare after refactoring:"
echo "  ./scripts/compare_baseline.sh $TIMESTAMP"
