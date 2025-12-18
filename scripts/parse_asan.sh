#!/usr/bin/env bash
# Parse AddressSanitizer output and provide actionable suggestions
# Usage: ./scripts/parse_asan.sh /path/to/asan_output.txt

set -euo pipefail

ASAN_LOG="${1:-/tmp/asan_output.txt}"

if [ ! -f "$ASAN_LOG" ]; then
    echo "Error: ASAN log file not found: $ASAN_LOG"
    echo "Usage: $0 <asan_output_file>"
    exit 1
fi

echo "========================================"
echo "ASAN Output Parser"
echo "========================================"
echo ""

# Extract error summary
echo "=== Error Summary ==="
if grep -q "ERROR: AddressSanitizer" "$ASAN_LOG"; then
    grep "ERROR: AddressSanitizer" "$ASAN_LOG" | head -5
    echo ""
else
    echo "No ASAN errors found in log file."
    exit 0
fi

# Extract crash locations (top of stack traces)
echo "=== Crash Locations (Top of Stack) ==="
grep "^    #0" "$ASAN_LOG" | head -10
echo ""

# Extract all stack frames for first error
echo "=== Full Stack Trace (First Error) ==="
awk '/ERROR: AddressSanitizer/,/SUMMARY:/ {print}' "$ASAN_LOG" | grep "^    #" | head -15
echo ""

# Check for memory leaks
if grep -q "Direct leak" "$ASAN_LOG" || grep -q "Indirect leak" "$ASAN_LOG"; then
    echo "=== Memory Leaks Detected ==="
    grep -E "Direct leak|Indirect leak" "$ASAN_LOG" | head -10
    echo ""
fi

# Provide actionable suggestions based on error type
echo "=== Suggested Actions ==="
echo ""

if grep -q "heap-use-after-free" "$ASAN_LOG"; then
    echo "🔍 USE-AFTER-FREE detected"
    echo "   → Memory was freed but still being accessed"
    echo "   → Check component lifecycle and cleanup order"
    echo "   → Add NULL checks before accessing freed objects"
    echo "   → Set pointers to NULL after freeing"
    echo ""
fi

if grep -q "heap-buffer-overflow" "$ASAN_LOG"; then
    echo "🔍 HEAP-BUFFER-OVERFLOW detected"
    echo "   → Array access beyond allocated bounds"
    echo "   → Check array indices and loop conditions"
    echo "   → Replace sprintf with snprintf"
    echo "   → Validate string lengths before operations"
    echo ""
fi

if grep -q "stack-buffer-overflow" "$ASAN_LOG"; then
    echo "🔍 STACK-BUFFER-OVERFLOW detected"
    echo "   → Local array too small for data"
    echo "   → Increase buffer size or use heap allocation"
    echo "   → Use strncpy/snprintf instead of strcpy/sprintf"
    echo ""
fi

if grep -q "double-free" "$ASAN_LOG"; then
    echo "🔍 DOUBLE-FREE detected"
    echo "   → Same pointer freed twice"
    echo "   → Set pointer to NULL after first free"
    echo "   → Check error paths for duplicate cleanup"
    echo "   → Use RAII patterns or defer for cleanup"
    echo ""
fi

if grep -q "global-buffer-overflow" "$ASAN_LOG"; then
    echo "🔍 GLOBAL-BUFFER-OVERFLOW detected"
    echo "   → Static/global array overflow"
    echo "   → Check global buffer sizes"
    echo "   → Validate indices into global arrays"
    echo ""
fi

if grep -q "stack-use-after-scope" "$ASAN_LOG"; then
    echo "🔍 STACK-USE-AFTER-SCOPE detected"
    echo "   → Using local variable after function returns"
    echo "   → Don't return pointers to local variables"
    echo "   → Use heap allocation for data that outlives function"
    echo ""
fi

if grep -q "Direct leak" "$ASAN_LOG" || grep -q "Indirect leak" "$ASAN_LOG"; then
    echo "🔍 MEMORY LEAK detected"
    echo "   → Allocated memory not freed"
    echo "   → Check cleanup in error paths"
    echo "   → Ensure destructors free all resources"
    echo "   → Use defer/finally for guaranteed cleanup"
    echo ""
fi

if grep -q "initialization-order-fiasco" "$ASAN_LOG"; then
    echo "🔍 INITIALIZATION-ORDER-FIASCO detected"
    echo "   → Global variables initialized in wrong order"
    echo "   → Use lazy initialization or init functions"
    echo "   → Avoid complex constructors for globals"
    echo ""
fi

# Extract ASAN summary
if grep -q "SUMMARY:" "$ASAN_LOG"; then
    echo "=== ASAN Summary ==="
    grep "SUMMARY:" "$ASAN_LOG" | head -5
    echo ""
fi

# Count total errors
ERROR_COUNT=$(grep -c "ERROR: AddressSanitizer" "$ASAN_LOG" || echo "0")
LEAK_COUNT=$(grep -c "Direct leak" "$ASAN_LOG" || echo "0")

echo "========================================"
echo "Total Errors: $ERROR_COUNT"
echo "Total Leaks: $LEAK_COUNT"
echo "========================================"
echo ""

# Suggest next steps
echo "Next Steps:"
echo "1. Review the crash locations and stack traces above"
echo "2. Apply suggested fixes based on error type"
echo "3. Rebuild with: make asan-full"
echo "4. Verify fix with: make asan-test EXAMPLE=<name>"
echo ""

# Exit with error if issues found
if [ "$ERROR_COUNT" -gt 0 ] || [ "$LEAK_COUNT" -gt 0 ]; then
    exit 1
fi

exit 0
