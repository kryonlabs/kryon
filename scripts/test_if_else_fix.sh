#!/usr/bin/env bash
# Test script for if/else conditional rendering fix

set -e  # Exit on error

echo "=========================================="
echo "Testing if/else Conditional Rendering Fix"
echo "=========================================="
echo

# Step 1: Rebuild CLI
echo "[1/5] Rebuilding CLI..."
nim c -o:bin/cli/kryon cli/main.nim >/dev/null 2>&1
echo "✓ CLI rebuilt"
echo

# Step 2: Clear cache
echo "[2/5] Clearing compilation cache..."
rm -rf .kryon_cache/
echo "✓ Cache cleared"
echo

# Step 3: Compile Nim example to .kir
echo "[3/5] Compiling build/nim/if_else_test.nim..."
KRYON_SERIALIZE_IR="build/ir/if_else_test_from_nim.kir" \
  nim c -r -p:bindings/nim build/nim/if_else_test.nim 2>&1 | \
  grep -E "(serialization|Captured|Added|components)" || true
echo

# Step 4: Inspect generated .kir tree
echo "[4/5] Checking component tree..."
./bin/cli/kryon tree build/ir/if_else_test_from_nim.kir
echo

# Step 5: Verify reactive manifest
echo "[5/5] Checking reactive manifest..."
echo "Then-branch IDs:"
cat build/ir/if_else_test_from_nim.kir | jq -r '.reactive_manifest.conditionals[0].then_children_ids[]'
echo "Else-branch IDs:"
cat build/ir/if_else_test_from_nim.kir | jq -r '.reactive_manifest.conditionals[0].else_children_ids[]'
echo

# Expected results
echo "=========================================="
echo "Expected Results:"
echo "  - Total components: 8"
echo "  - Parent column should have 4 children:"
echo "    1. Text (Conditional Rendering Demo)"
echo "    2. Button (Toggle Message)"
echo "    3. Column (VISIBLE! - then branch)"
echo "    4. Column (HIDDEN - else branch)"
echo "  - Reactive manifest should reference both branches"
echo "=========================================="
