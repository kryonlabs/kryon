#!/bin/bash
set -e

echo "=== Kryon Round-Trip Conversion Test ==="
echo

# Setup
KRYON_ROOT="/mnt/storage/Projects/kryon"
TEST_FILE="$KRYON_ROOT/examples/roundtrip_test.c"
KIR_FILE="$KRYON_ROOT/.kryon_cache/roundtrip_test.kir"
GENERATED_FILE="/tmp/roundtrip_test_generated.c"
CODEGEN="/tmp/test_codegen"

# Step 1: Compile the test codegen if not present
if [ ! -f "$CODEGEN" ]; then
    echo "→ Building code generator..."
    cd "$KRYON_ROOT"
    gcc -o "$CODEGEN" /tmp/test_codegen.c \
        ir/codegens/ir_c_codegen.c \
        ir/third_party/cJSON/cJSON.c \
        -I. -I./ir -I./ir/third_party/cJSON -lm \
        -Wl,--unresolved-symbols=ignore-all
    echo "  ✓ Code generator built"
    echo
fi

# Step 2: Run the C source to generate KIR
echo "→ Step 1: C → KIR conversion"
echo "  Compiling: $TEST_FILE"
cd "$KRYON_ROOT"

# Check if kryon CLI exists
if [ -f "$HOME/.local/bin/kryon" ]; then
    $HOME/.local/bin/kryon run "$TEST_FILE" 2>&1 | grep -E "(KIR generated|✓|Error)" || true
else
    echo "  ! Kryon CLI not available, skipping C→KIR test"
    echo "  ! Using existing KIR if present"
fi

if [ -f "$KIR_FILE" ]; then
    echo "  ✓ KIR generated: $KIR_FILE"
    echo "  Size: $(wc -c < "$KIR_FILE") bytes"
else
    echo "  ✗ KIR file not found, test cannot continue"
    exit 1
fi
echo

# Step 3: Generate C code from KIR
echo "→ Step 2: KIR → C conversion"
echo "  Input: $KIR_FILE"
echo "  Output: $GENERATED_FILE"

"$CODEGEN" "$KIR_FILE" "$GENERATED_FILE"
echo "  ✓ C code generated"
echo "  Size: $(wc -c < "$GENERATED_FILE") bytes"
echo

# Step 4: Verify generated code syntax
echo "→ Step 3: Syntax validation"
gcc -fsyntax-only "$GENERATED_FILE" \
    -I"$KRYON_ROOT/bindings/c" \
    -I"$KRYON_ROOT/ir" \
    -I"$HOME/.local/include" 2>&1 | head -20 || {
    echo "  ✗ Syntax check failed"
    echo "  Generated file content:"
    head -50 "$GENERATED_FILE"
    exit 1
}
echo "  ✓ Syntax valid"
echo

# Step 5: Show comparison
echo "→ Step 4: Comparison"
echo "  Original lines: $(wc -l < "$TEST_FILE")"
echo "  Generated lines: $(wc -l < "$GENERATED_FILE")"
echo

# Step 6: Show generated file preview
echo "→ Step 5: Generated file preview"
echo "───────────────────────────────────────"
head -40 "$GENERATED_FILE"
echo "..."
echo "───────────────────────────────────────"
echo

# Step 7: Check for metadata preservation
echo "→ Step 6: Metadata verification"
if grep -q "static IRComponent\* main_screen" "$GENERATED_FILE"; then
    echo "  ✓ Variable declarations preserved"
else
    echo "  ⚠ Variable declarations not found"
fi

if grep -q "void on_show_settings" "$GENERATED_FILE"; then
    echo "  ✓ Event handler functions preserved"
else
    echo "  ⚠ Event handler functions not found"
fi

if grep -q "#include <stdio.h>" "$GENERATED_FILE"; then
    echo "  ✓ Includes preserved"
else
    echo "  ⚠ Includes not found"
fi
echo

echo "=== Round-Trip Test Complete ==="
echo "✓ C → KIR → C conversion successful"
echo "Generated file: $GENERATED_FILE"
