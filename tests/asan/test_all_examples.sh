#!/usr/bin/env bash
# Automated ASAN test suite for all Kryon examples
# Builds entire stack with ASAN+UBSan and runs all examples
# Reports passed/failed with detailed error information

set -euo pipefail

# Configuration
TIMEOUT_SECONDS=5
EXAMPLES_DIR="examples/kry"
BUILD_DIR="build"
ASAN_LOG_DIR="/tmp/kryon_asan_tests"
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create log directory
mkdir -p "$ASAN_LOG_DIR"

echo "========================================"
echo "Kryon ASAN Test Suite"
echo "========================================"
echo ""

# Step 1: Build with ASAN
echo "Step 1: Building with ASAN+UBSan..."
echo ""

if ! make asan-full > "$ASAN_LOG_DIR/build.log" 2>&1; then
    echo -e "${RED}❌ ASAN build failed!${NC}"
    echo "See build log: $ASAN_LOG_DIR/build.log"
    cat "$ASAN_LOG_DIR/build.log"
    exit 1
fi

echo -e "${GREEN}✅ ASAN build complete${NC}"
echo ""

# Step 2: Discover all examples
echo "Step 2: Discovering examples..."
echo ""

if [ ! -d "$EXAMPLES_DIR" ]; then
    echo -e "${RED}Error: Examples directory not found: $EXAMPLES_DIR${NC}"
    exit 1
fi

EXAMPLES=($(find "$EXAMPLES_DIR" -name "*.kry" -exec basename {} .kry \; | sort))

if [ ${#EXAMPLES[@]} -eq 0 ]; then
    echo -e "${YELLOW}Warning: No .kry examples found in $EXAMPLES_DIR${NC}"
    exit 0
fi

echo "Found ${#EXAMPLES[@]} examples:"
for example in "${EXAMPLES[@]}"; do
    echo "  - $example"
done
echo ""

# Step 3: Run each example with ASAN
echo "Step 3: Running ASAN tests..."
echo ""

for example in "${EXAMPLES[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    LOG_FILE="$ASAN_LOG_DIR/${example}.log"

    echo -n "Testing $example... "

    # Run with ASAN options
    if ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:log_path=${ASAN_LOG_DIR}/${example}_asan.log" \
       LD_LIBRARY_PATH="$BUILD_DIR:\$LD_LIBRARY_PATH" \
       timeout "$TIMEOUT_SECONDS" ./run_example.sh "$example" > "$LOG_FILE" 2>&1; then
        # Example ran without timeout

        # Check for ASAN errors
        if grep -q "ERROR: AddressSanitizer" "$LOG_FILE" 2>/dev/null || \
           [ -f "${ASAN_LOG_DIR}/${example}_asan.log" ] && grep -q "ERROR: AddressSanitizer" "${ASAN_LOG_DIR}/${example}_asan.log" 2>/dev/null; then
            echo -e "${RED}FAILED (ASAN errors)${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))

            # Save detailed error info
            echo "=== ASAN Errors in $example ===" >> "$ASAN_LOG_DIR/failures.txt"
            if [ -f "${ASAN_LOG_DIR}/${example}_asan.log" ]; then
                grep "ERROR: AddressSanitizer" "${ASAN_LOG_DIR}/${example}_asan.log" >> "$ASAN_LOG_DIR/failures.txt" 2>/dev/null || true
            fi
            grep "ERROR: AddressSanitizer" "$LOG_FILE" >> "$ASAN_LOG_DIR/failures.txt" 2>/dev/null || true
            echo "" >> "$ASAN_LOG_DIR/failures.txt"
        else
            echo -e "${GREEN}PASSED${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        fi
    else
        # Timeout or other error
        EXIT_CODE=$?

        if [ $EXIT_CODE -eq 124 ]; then
            # Timeout is expected for interactive examples
            # Still check for ASAN errors in the partial output
            if grep -q "ERROR: AddressSanitizer" "$LOG_FILE" 2>/dev/null || \
               [ -f "${ASAN_LOG_DIR}/${example}_asan.log" ] && grep -q "ERROR: AddressSanitizer" "${ASAN_LOG_DIR}/${example}_asan.log" 2>/dev/null; then
                echo -e "${RED}FAILED (ASAN errors before timeout)${NC}"
                FAILED_TESTS=$((FAILED_TESTS + 1))

                # Save detailed error info
                echo "=== ASAN Errors in $example (timeout) ===" >> "$ASAN_LOG_DIR/failures.txt"
                if [ -f "${ASAN_LOG_DIR}/${example}_asan.log" ]; then
                    grep "ERROR: AddressSanitizer" "${ASAN_LOG_DIR}/${example}_asan.log" >> "$ASAN_LOG_DIR/failures.txt" 2>/dev/null || true
                fi
                grep "ERROR: AddressSanitizer" "$LOG_FILE" >> "$ASAN_LOG_DIR/failures.txt" 2>/dev/null || true
                echo "" >> "$ASAN_LOG_DIR/failures.txt"
            else
                # Timeout but no ASAN errors (normal for interactive examples)
                echo -e "${GREEN}PASSED (timeout, no ASAN errors)${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            fi
        else
            # Other error
            echo -e "${RED}FAILED (exit code: $EXIT_CODE)${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))

            echo "=== Runtime Error in $example ===" >> "$ASAN_LOG_DIR/failures.txt"
            echo "Exit code: $EXIT_CODE" >> "$ASAN_LOG_DIR/failures.txt"
            tail -20 "$LOG_FILE" >> "$ASAN_LOG_DIR/failures.txt" 2>/dev/null || true
            echo "" >> "$ASAN_LOG_DIR/failures.txt"
        fi
    fi
done

echo ""
echo "========================================"
echo "Test Results"
echo "========================================"
echo "Total:  $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
echo ""

if [ $FAILED_TESTS -gt 0 ]; then
    echo -e "${RED}❌ Some tests failed${NC}"
    echo ""
    echo "Failed test details saved to: $ASAN_LOG_DIR/failures.txt"
    echo "Individual logs available in: $ASAN_LOG_DIR/"
    echo ""

    # Show summary of failures
    if [ -f "$ASAN_LOG_DIR/failures.txt" ]; then
        echo "=== Failure Summary ==="
        cat "$ASAN_LOG_DIR/failures.txt"
    fi

    exit 1
else
    echo -e "${GREEN}✅ All tests passed!${NC}"
    exit 0
fi
