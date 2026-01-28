#!/usr/bin/env bash
# KIR Test Runner Script

set -e

echo "=== Kryon KIR Test Runner ==="
echo ""

# Check if kryon is built
if [ ! -f "build/bin/kryon" ]; then
    echo "❌ Error: kryon binary not found at build/bin/kryon"
    echo "Please build first with: make"
    exit 1
fi

echo "✓ Found kryon binary"
echo ""

# Build tests
echo "Building tests..."
cd tests
make clean >/dev/null 2>&1 || true
make all
cd ..
echo "✓ Tests compiled"
echo ""

# Run tests
echo "Running test suite..."
echo ""
cd tests && make run
TEST_RESULT=$?
cd ..

if [ $TEST_RESULT -eq 0 ]; then
    echo ""
    echo "==================================="
    echo "  All KIR tests passed! 🎉"
    echo "==================================="
    exit 0
else
    echo ""
    echo "==================================="
    echo "  Some tests failed ❌"
    echo "==================================="
    exit 1
fi
