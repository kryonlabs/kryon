#!/bin/bash
# Test Android Codegen

echo "====================================="
echo "Android/Kotlin Codegen Test Suite"
echo "====================================="
echo ""

CODEGEN_DIR="$(dirname "$0")"
TEST_DIR="$CODEGEN_DIR"
OUTPUT_DIR="/tmp/android_codegen_test_output"

# Create output directory
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# Compile test program
echo "[1/5] Compiling test program..."
gcc -o /tmp/test_android_codegen << 'CTEST'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool ir_generate_android_project(const char* kir_path, 
                                        const char* output_dir,
                                        const char* package_name,
                                        const char* app_name);

int main(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <kir_file> <output_dir> <package> <app_name>\n", argv[0]);
        return 1;
    }

    const char* kir_file = argv[1];
    const char* output_dir = argv[2];
    const char* package_name = argv[3];
    const char* app_name = argv[4];

    printf("Testing: %s\n", kir_file);
    
    if (ir_generate_android_project(kir_file, output_dir, package_name, app_name)) {
        printf("✓ SUCCESS\n");
        return 0;
    } else {
        printf("✗ FAILED\n");
        return 1;
    }
}
CTEST

if [ $? -ne 0 ]; then
    echo "Failed to compile test program"
    exit 1
fi

# Link with Android codegen
gcc -o /tmp/test_android_codegen /tmp/test_android_codegen.c \
    /mnt/storage/Projects/TaijiOS/kryon/codegens/android/ir_android_codegen.o \
    -I/mnt/storage/Projects/TaijiOS/kryon/codegens/android \
    -I/mnt/storage/Projects/TaijiOS/kryon/third_party/cJSON \
    /mnt/storage/Projects/TaijiOS/kryon/third_party/cJSON/cJSON.c \
    -lm

if [ $? -ne 0 ]; then
    echo "Failed to link test program"
    exit 1
fi

echo "✓ Test program compiled"
echo ""

# Run tests
TESTS=(
    "button.kir:Button Test:com.example.button:ButtonTest"
    "layout.kir:Layout Test:com.example.layout:LayoutTest"
    "style.kir:Style Test:com.example.style:StyleTest"
)

PASS=0
FAIL=0

for test in "${TESTS[@]}"; do
    IFS=':' read -r kir_file desc package app_name <<< "$test"
    
    echo "[Test] $desc"
    echo "  Input: $kir_file"
    
    KIR_PATH="$TEST_DIR/$kir_file"
    OUTPUT_SUBDIR="$OUTPUT_DIR/${kir_file%.kir}"
    
    if [ ! -f "$KIR_PATH" ]; then
        echo "  ✗ SKIP: File not found"
        FAIL=$((FAIL + 1))
        continue
    fi
    
    /tmp/test_android_codegen "$KIR_PATH" "$OUTPUT_SUBDIR" "$package" "$app_name" > /tmp/test_output.txt 2>&1
    
    if [ $? -eq 0 ]; then
        echo "  ✓ PASS"
        
        # Show generated Kotlin snippet
        if [ -f "$OUTPUT_SUBDIR/MainActivity.kt" ]; then
            echo "  Generated MainActivity.kt:"
            head -20 "$OUTPUT_SUBDIR/MainActivity.kt" | sed 's/^/    /'
        fi
        
        PASS=$((PASS + 1))
    else
        echo "  ✗ FAIL"
        cat /tmp/test_output.txt | sed 's/^/    /'
        FAIL=$((FAIL + 1))
    fi
    echo ""
done

# Summary
echo "====================================="
echo "Test Results"
echo "====================================="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo "Total:  $((PASS + FAIL))"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
