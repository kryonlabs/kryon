#!/bin/bash
# kry_to_kir.sh - Compile .kry files to .kir format
# Usage: kry_to_kir.sh input.kry output.kir

set -e

INPUT="$1"
OUTPUT="$2"

if [ -z "$INPUT" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: kry_to_kir.sh <input.kry> <output.kir>"
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "Error: Input file not found: $INPUT"
    exit 1
fi

# Create temporary Nim file
TEMP_NIM=$(mktemp --suffix=.nim)
trap "rm -f $TEMP_NIM" EXIT

# Convert .kry to Nim DSL
# This is a simple sed-based converter for now
cat "$INPUT" | sed 's/^App {/let app = kryonApp:/' | \
               sed 's/^}//' | \
               sed 's/^\([A-Z][a-zA-Z]*\) {/  \1:/' | \
               sed 's/^  \([a-z][a-zA-Z]*\) = \(.*\)$/    \1 = \2/' > $TEMP_NIM

# Add Nim DSL import at the beginning
sed -i '1i import kryon_dsl\nimport ir_serialization' $TEMP_NIM

# Add code to write .kir file at the end
cat >> $TEMP_NIM <<'EOF'

# Write to .kir format
discard ir_write_json_v2_file(app, "$OUTPUT".cstring)
EOF

# Replace $OUTPUT with actual output path
sed -i "s|\$OUTPUT|$OUTPUT|g" $TEMP_NIM

# Compile and run the Nim file to generate .kir
nim c --path:bindings/nim -r --hints:off --warnings:off "$TEMP_NIM" 2>&1 > /dev/null

if [ -f "$OUTPUT" ]; then
    echo "✓ Generated: $OUTPUT"
    exit 0
else
    echo "✗ Failed to generate .kir file"
    exit 1
fi
