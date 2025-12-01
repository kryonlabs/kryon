#!/bin/bash
# Kryon Example Validation Script
# Validates all Nim examples compile to .kir format

set -euo pipefail

EXAMPLES_DIR="examples/nim"
OUTPUT_DIR="build/ir"
RESULTS_FILE="validation_results.txt"
TIMEOUT_SECONDS=2

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "Kryon Example Validation"
echo "========================================="
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Initialize results file
echo "=== Kryon Example Validation ===" > "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

# Counters
total=0
passed=0
failed=0

# Process each example
for example in "$EXAMPLES_DIR"/*.nim; do
  if [ ! -f "$example" ]; then
    continue
  fi

  basename=$(basename "$example" .nim)
  total=$((total + 1))

  echo -n "[$total] Testing: $basename ... "

  # Compile to .kir using kryon compile with JSON format
  if kryon compile "$example" --format=json --output="$OUTPUT_DIR/$basename.kir" --no-cache >/dev/null 2>&1; then

    # Verify .kir file was created
    if [ -f "$OUTPUT_DIR/$basename.kir" ]; then
      echo -e "${GREEN}✓${NC}"
      echo "✓ $basename" >> "$RESULTS_FILE"
      passed=$((passed + 1))
    else
      echo -e "${YELLOW}⚠ (no .kir output)${NC}"
      echo "⚠ $basename (compiled but no .kir file)" >> "$RESULTS_FILE"
      failed=$((failed + 1))
    fi
  else
    echo -e "${RED}✗ (compilation failed)${NC}"
    echo "✗ $basename (compilation failed)" >> "$RESULTS_FILE"
    failed=$((failed + 1))
  fi
done

# Summary
echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo "Total examples: $total"
echo -e "${GREEN}Passed: $passed${NC}"
echo -e "${RED}Failed: $failed${NC}"
echo ""
echo "Results saved to: $RESULTS_FILE"
echo ".kir files saved to: $OUTPUT_DIR/ (24 files)"
echo ""
echo "To view .kir files:"
echo "  ls -lh $OUTPUT_DIR/*.kir"
echo ""

# Exit with error if any failures
if [ $failed -gt 0 ]; then
  echo -e "${YELLOW}⚠ Some examples failed validation${NC}"
  exit 1
else
  echo -e "${GREEN}✓ All examples validated successfully!${NC}"
  exit 0
fi
