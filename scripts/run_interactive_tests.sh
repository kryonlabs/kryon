#!/usr/bin/env bash
# Kryon Interactive Test Runner
# Runs all .kyt test files in a directory and reports results

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

# Default test directory
TESTS_DIR="${1:-tests/interactive}"

# Counters
PASSED=0
FAILED=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Kryon Interactive Test Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Test directory: $TESTS_DIR"
echo ""

# Check if directory exists
if [ ! -d "$TESTS_DIR" ]; then
    echo -e "${RED}✗ Error: Test directory not found: $TESTS_DIR${NC}"
    exit 1
fi

# Find all .kyt files
shopt -s nullglob
TEST_FILES=("$TESTS_DIR"/*.kyt)

if [ ${#TEST_FILES[@]} -eq 0 ]; then
    echo "⚠  No test files found in $TESTS_DIR"
    exit 0
fi

echo "Found ${#TEST_FILES[@]} test file(s)"
echo ""

# Run each test
for test_file in "${TEST_FILES[@]}"; do
    test_name=$(basename "$test_file" .kyt)

    printf "Testing: %-40s " "$test_name"

    # Run the test
    if ./bin/cli/kryon test "$test_file" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ PASSED${NC}"
        ((PASSED++))
    else
        echo -e "${RED}✗ FAILED${NC}"
        ((FAILED++))
    fi
done

# Print summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo "Total:  ${#TEST_FILES[@]}"
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
else
    echo -e "Failed: 0"
fi
echo -e "${BLUE}========================================${NC}"

# Exit with error if any tests failed
if [ $FAILED -gt 0 ]; then
    exit 1
else
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
fi
