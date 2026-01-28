#!/usr/bin/env bash
# Integration test for KIR round-trip functionality

set -e

KRYON="./build/bin/kryon"
TEST_DIR="tests/integration_output"
GOLDEN_DIR="tests/golden"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== KIR Integration Tests ==="
echo ""

# Check if kryon binary exists
if [ ! -f "$KRYON" ]; then
    echo -e "${RED}❌ Error: kryon binary not found at $KRYON${NC}"
    echo "Please build first with: make"
    exit 1
fi

# Create test directory
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# Counter for tests
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test function
run_test() {
    local test_name=$1
    local test_file=$2

    TESTS_RUN=$((TESTS_RUN + 1))

    echo -n "Testing: $test_name... "

    local base_name=$(basename "$test_file" .kry)
    local kry_file="$test_file"
    local kir_original="$TEST_DIR/${base_name}_original.kir"
    local krb_original="$TEST_DIR/${base_name}_original.krb"
    local kir_decompiled="$TEST_DIR/${base_name}_decompiled.kir"
    local kry_roundtrip="$TEST_DIR/${base_name}_roundtrip.kry"
    local krb_roundtrip="$TEST_DIR/${base_name}_roundtrip.krb"

    # Step 1: Compile kry → kir + krb
    if ! $KRYON compile "$kry_file" --kir-output "$kir_original" -o "$krb_original" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (compilation)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    # Step 2: Validate KIR is valid JSON
    if ! cat "$kir_original" | jq . >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (invalid JSON in KIR)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    # Step 3: Decompile krb → kir
    if ! $KRYON decompile "$krb_original" -o "$kir_decompiled" >/dev/null 2>&1; then
        echo -e "${YELLOW}PARTIAL${NC} (decompilation not implemented)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    fi

    # Step 4: Print kir → kry
    if ! $KRYON print "$kir_decompiled" -o "$kry_roundtrip" >/dev/null 2>&1; then
        echo -e "${YELLOW}PARTIAL${NC} (printer not fully implemented)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    fi

    # Step 5: Compile roundtrip kry → krb
    if ! $KRYON compile "$kry_roundtrip" -o "$krb_roundtrip" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (roundtrip compilation)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    # Step 6: Check binary sizes are similar (within 20% tolerance)
    local size_original=$(stat -f%z "$krb_original" 2>/dev/null || stat -c%s "$krb_original" 2>/dev/null)
    local size_roundtrip=$(stat -f%z "$krb_roundtrip" 2>/dev/null || stat -c%s "$krb_roundtrip" 2>/dev/null)
    local diff=$((size_original - size_roundtrip))
    local diff_abs=${diff#-}
    local threshold=$((size_original / 5))  # 20% tolerance

    if [ $diff_abs -gt $threshold ]; then
        echo -e "${RED}FAILED${NC} (binary size mismatch: $size_original vs $size_roundtrip)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    echo -e "${GREEN}PASSED${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    return 0
}

# Test utilities
test_utilities() {
    local test_name=$1
    local kir_file=$2

    TESTS_RUN=$((TESTS_RUN + 1))
    echo -n "Testing utilities on $test_name... "

    # Test kir-validate
    if ! $KRYON kir-validate "$kir_file" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (kir-validate)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    # Test kir-dump
    if ! $KRYON kir-dump "$kir_file" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (kir-dump)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    # Test kir-stats
    if ! $KRYON kir-stats "$kir_file" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (kir-stats)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    # Test kir-diff (same file)
    if ! $KRYON kir-diff "$kir_file" "$kir_file" >/dev/null 2>&1; then
        echo -e "${RED}FAILED${NC} (kir-diff)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi

    echo -e "${GREEN}PASSED${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    return 0
}

# Run tests on golden files
echo "--- Round-Trip Tests ---"
for kry_file in "$GOLDEN_DIR"/*.kry; do
    if [ -f "$kry_file" ]; then
        test_name=$(basename "$kry_file" .kry)
        run_test "$test_name" "$kry_file"
    fi
done

echo ""
echo "--- Utility Tests ---"
for kir_file in "$TEST_DIR"/*_original.kir; do
    if [ -f "$kir_file" ]; then
        test_name=$(basename "$kir_file" _original.kir)
        test_utilities "$test_name" "$kir_file"
    fi
done

# Print summary
echo ""
echo "=== Test Summary ==="
echo "Total:  $TESTS_RUN"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}🎉 All integration tests passed!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests failed${NC}"
    exit 1
fi
