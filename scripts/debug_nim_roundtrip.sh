#!/usr/bin/env bash
# General script for debugging Nim → KIR round-trip compilation
# Usage: ./scripts/debug_nim_roundtrip.sh <example_name> [--verbose]

set -e

EXAMPLE="${1:-if_else_test}"
VERBOSE="${2}"

echo "=========================================="
echo "Nim → KIR Round-Trip Debugger"
echo "Example: $EXAMPLE"
echo "=========================================="
echo

# Step 1: Rebuild CLI if needed
if [ ! -f "bin/cli/kryon" ] || [ "bindings/nim/runtime.nim" -nt "bin/cli/kryon" ]; then
    echo "[1/6] Rebuilding CLI (bindings changed)..."
    nim c -o:bin/cli/kryon cli/main.nim >/dev/null 2>&1
    echo "✓ CLI rebuilt"
else
    echo "[1/6] CLI is up to date"
fi
echo

# Step 2: Clear cache for fresh compilation
echo "[2/6] Clearing cache..."
rm -rf .kryon_cache/
echo "✓ Cache cleared"
echo

# Step 3: Compile Nim → KIR
echo "[3/6] Compiling build/nim/${EXAMPLE}.nim → build/ir/${EXAMPLE}_from_nim.kir..."
if [ "$VERBOSE" == "--verbose" ]; then
    KRYON_SERIALIZE_IR="build/ir/${EXAMPLE}_from_nim.kir" \
      nim c -r -p:bindings/nim "build/nim/${EXAMPLE}.nim" 2>&1
else
    KRYON_SERIALIZE_IR="build/ir/${EXAMPLE}_from_nim.kir" \
      nim c -r -p:bindings/nim "build/nim/${EXAMPLE}.nim" 2>&1 | \
      grep -E "(serialization|Captured|Added|Serializing|components|reactive)" || true
fi
echo

# Step 4: Show component tree
echo "[4/6] Component Tree:"
./bin/cli/kryon tree "build/ir/${EXAMPLE}_from_nim.kir"
echo

# Step 5: Show reactive manifest (if exists)
echo "[5/6] Reactive Manifest:"
if cat "build/ir/${EXAMPLE}_from_nim.kir" | jq -e '.reactive_manifest' >/dev/null 2>&1; then
    echo "Variables:"
    cat "build/ir/${EXAMPLE}_from_nim.kir" | jq -r '.reactive_manifest.variables[]? | "  - \(.name) (\(.type)) = \(.initial_value)"' || echo "  (none)"
    echo "Conditionals:"
    cat "build/ir/${EXAMPLE}_from_nim.kir" | jq -r '.reactive_manifest.conditionals[]? | "  - parent_id=\(.component_id // .parent_id), then_ids=\(.then_children_ids), else_ids=\(.else_children_ids)"' || echo "  (none)"
else
    echo "  (no reactive manifest)"
fi
echo

# Step 6: Show JSON stats
echo "[6/6] File Stats:"
FILE_SIZE=$(wc -c < "build/ir/${EXAMPLE}_from_nim.kir")
COMPONENT_COUNT=$(./bin/cli/kryon tree "build/ir/${EXAMPLE}_from_nim.kir" 2>/dev/null | grep "Total:" | awk '{print $2}')
echo "  File size: ${FILE_SIZE} bytes"
echo "  Components: ${COMPONENT_COUNT}"
echo

echo "=========================================="
echo "Debug complete!"
echo "=========================================="
