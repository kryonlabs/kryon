#!/bin/bash
#
# metrics.sh
#
# Displays current project metrics
# Useful for quick health checks
#

echo "=== Kryon Project Metrics ==="
echo ""

echo "1. Lines of Code"
echo "   Total (including headers):"
find . -name "*.c" -o -name "*.h" \
    -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" \
    | xargs wc -l 2>/dev/null | tail -1 | sed 's/^/     /'

echo "   Project C files only:"
find . -name "*.c" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" \
    | xargs wc -l 2>/dev/null | tail -1 | sed 's/^/     /'

echo ""
echo "2. File Count"
C_FILES=$(find . -name "*.c" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" | wc -l)
H_FILES=$(find . -name "*.h" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" | wc -l)
echo "     C files: $C_FILES"
echo "     Header files: $H_FILES"
echo "     Total: $((C_FILES + H_FILES))"

echo ""
echo "3. Binary Sizes"
if [ -d "build" ]; then
    echo "     Libraries:"
    find build -name "*.so" -exec ls -lh {} \; 2>/dev/null | \
        awk '{print "       " $9 ": " $5}' | sort
else
    echo "     (build/ directory not found)"
fi

echo ""
echo "4. Test Coverage"
if [ -d "tests" ]; then
    TEST_FILES=$(find tests -name "*.c" -o -name "*.sh" 2>/dev/null | wc -l)
    echo "     Test files: $TEST_FILES"

    if command -v pytest &> /dev/null; then
        echo "     pytest: available"
    fi
else
    echo "     (tests/ directory not found)"
fi

echo ""
echo "5. Largest Files (Top 10)"
find . -name "*.c" -not -path "*/third_party/*" \
    -not -path "*/.kryon_cache/*" \
    -not -path "*/build/*" \
    | xargs wc -l 2>/dev/null | sort -rn | head -10 | \
    awk '{printf "     %4d %s\n", $1, $2}' | sed 's|^\s*||'

echo ""
echo "6. Git Status"
BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "not a git repo")
COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "N/A")
CHANGES=$(git status --porcelain 2>/dev/null | wc -l)
echo "     Branch: $BRANCH"
echo "     Commit: $COMMIT"
echo "     Uncommitted changes: $CHANGES"

echo ""
echo "7. Dependencies"
if [ -f "shell.nix" ]; then
    echo "     Nix shell: configured"
else
    echo "     Nix shell: not configured"
fi

if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc --version | head -1)
    echo "     GCC: $GCC_VERSION"
fi

if command -v pkg-config &> /dev/null; then
    echo "     pkg-config: available"
fi

echo ""
echo "=== End of Metrics ==="
