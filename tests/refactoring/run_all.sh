#!/bin/bash
#
# run_all.sh
#
# Master test runner for refactoring validation
# Runs all regression tests and generates a report
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

echo "==================================="
echo " Kryon Refactoring Test Suite"
echo "==================================="
echo ""
echo "Running all regression tests..."
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_SKIPPED=0

# Run a test suite
run_suite() {
    local suite_name="$1"
    local suite_script="$2"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Running: $suite_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    if [ ! -f "$suite_script" ]; then
        echo -e "${YELLOW}⚠️  SKIP: Test script not found: $suite_script${NC}"
        echo ""
        TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
        return
    fi

    if chmod +x "$suite_script" && bash "$suite_script"; then
        echo -e "${GREEN}✅ $suite_name: PASSED${NC}"
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
    else
        echo -e "${RED}❌ $suite_name: FAILED${NC}"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
    fi

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo ""
}

# Run all test suites
run_suite "Layout Engine Tests" "tests/refactoring/test_layout.sh"
run_suite "Renderer Backend Tests" "tests/refactoring/test_renderers.sh"
run_suite "Serialization Tests" "tests/refactoring/test_serialization.sh"

# Add more suites as they're created
# run_suite "CSS Parser/Generator Tests" "tests/refactoring/test_css.sh"
# run_suite "Codegen Tests" "tests/refactoring/test_codegen.sh"
# run_suite "Expression Tests" "tests/refactoring/test_expressions.sh"

# Print summary
echo "==================================="
echo " Test Suite Summary"
echo "==================================="
echo ""
echo "Total Suites:  $TOTAL_TESTS"
echo -e "${GREEN}Passed:        $TOTAL_PASSED${NC}"
echo -e "${RED}Failed:        $TOTAL_FAILED${NC}"
echo -e "${YELLOW}Skipped:       $TOTAL_SKIPPED${NC}"
echo ""

# Overall result
if [ $TOTAL_FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
    echo ""
    echo "Refactoring is safe to proceed."
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    echo ""
    echo "Please review and fix failures before proceeding."
    exit 1
fi
