#!/bin/bash
# Test DIS codegen from KIR file

set -e

KRYON_CLI="./cli/kryon"
KIR_FILE=".kryon_cache/hello_libdraw.kir"
DIS_FILE="build/hello_libdraw.dis"

echo "=== Testing DIS Codegen ==="
echo ""

# Compile KRY to KIR
echo "1. Compiling hello_libdraw.kry to KIR..."
$KRYON_CLI compile examples/hello_libdraw.kry

# Check if KIR file exists
if [ ! -f "$KIR_FILE" ]; then
    echo "Error: KIR file not found at $KIR_FILE"
    exit 1
fi

echo "✓ KIR file generated: $KIR_FILE"
echo ""

# Show KIR content
echo "2. KIR content (first 30 lines):"
head -30 "$KIR_FILE"
echo ""
echo "... (truncated)"
echo ""

# Generate DIS from KIR using a simple C program
echo "3. Generating DIS bytecode..."
cat > /tmp/test_dis_codegen.c << 'EOF'
#include "../codegens/dis/include/dis_codegen.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    const char* kir_file = ".kryon_cache/hello_libdraw.kir";
    const char* dis_file = "build/hello_libdraw.dis";

    printf("Generating DIS from: %s\n", kir_file);
    printf("Output file: %s\n", dis_file);

    if (!dis_codegen_generate(kir_file, dis_file)) {
        fprintf(stderr, "Error: %s\n", dis_codegen_get_error());
        return 1;
    }

    printf("✓ DIS file generated: %s\n", dis_file);
    return 0;
}
EOF

# Compile test program
gcc -I./codegens/dis/include -I./ir/include \
    /tmp/test_dis_codegen.c \
    -o /tmp/test_dis_codegen \
    -L./codegens/dis -ldiscodegen \
    -L./build -lkryon_ir \
    -Wl,-rpath,./codegens/dis

# Run test
/tmp/test_dis_codegen

echo ""
echo "=== Test Complete ==="
