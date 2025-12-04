#!/bin/bash
# Transpile all .kry examples to .kir and .nim in build/ir/

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
KRY_DIR="$PROJECT_DIR/examples/kry"
OUT_DIR="$PROJECT_DIR/build/ir"
KRYON="$PROJECT_DIR/bin/cli/kryon"

# Check if kryon CLI exists
if [ ! -x "$KRYON" ]; then
  echo "Error: kryon CLI not found at $KRYON"
  echo "Run 'nimble build' or 'nim c -o:bin/cli/kryon cli/main.nim' first"
  exit 1
fi

# Create output directory
mkdir -p "$OUT_DIR"

# Count files
total=$(ls "$KRY_DIR"/*.kry 2>/dev/null | wc -l)
if [ "$total" -eq 0 ]; then
  echo "No .kry files found in $KRY_DIR"
  exit 1
fi

echo "Transpiling $total .kry files..."
echo ""

success=0
failed=0

for kry_file in "$KRY_DIR"/*.kry; do
  name=$(basename "$kry_file" .kry)
  kir_file="$OUT_DIR/$name.kir"
  nim_file="$OUT_DIR/$name.nim"

  echo -n "  $name: "

  # Parse .kry -> .kir
  if "$KRYON" parse "$kry_file" > /dev/null 2>&1; then
    # Move .kir to build/ir
    mv "$KRY_DIR/$name.kir" "$kir_file" 2>/dev/null

    # Generate .nim from .kir
    if "$KRYON" codegen "$kir_file" > "$nim_file" 2>&1; then
      echo "OK"
      success=$((success + 1))
    else
      echo "FAIL (codegen)"
      failed=$((failed + 1))
    fi
  else
    echo "FAIL (parse)"
    failed=$((failed + 1))
  fi
done

echo ""
echo "Done: $success succeeded, $failed failed"
echo "Output: $OUT_DIR/"
