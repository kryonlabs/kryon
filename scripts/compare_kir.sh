#!/bin/bash
# ==============================================================================
# KIR Semantic Comparison Tool
# ==============================================================================
# Normalizes and compares two .kir files semantically
# Handles acceptable differences (IDs, field ordering, floats, colors)
#
# Usage:
#   ./scripts/compare_kir.sh file1.kir file2.kir [show_diffs]
#
# Exit codes:
#   0 - Files match semantically
#   1 - Files differ
# ==============================================================================

set -e

FILE1="$1"
FILE2="$2"
SHOW_DIFFS="${3:-false}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NORMALIZE_JQ="$SCRIPT_DIR/normalize_kir.jq"

if [ $# -lt 2 ]; then
  echo "Usage: $0 <file1.kir> <file2.kir> [show_diffs]"
  exit 1
fi

if [ ! -f "$FILE1" ]; then
  echo "Error: $FILE1 not found"
  exit 1
fi

if [ ! -f "$FILE2" ]; then
  echo "Error: $FILE2 not found"
  exit 1
fi

if [ ! -f "$NORMALIZE_JQ" ]; then
  echo "Error: $NORMALIZE_JQ not found"
  exit 1
fi

# Create temp directory
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

NORM1="$TEMP_DIR/file1_normalized.json"
NORM2="$TEMP_DIR/file2_normalized.json"

# Normalize using jq
if ! jq -f "$NORMALIZE_JQ" "$FILE1" > "$NORM1" 2>/dev/null; then
  echo "Error: Failed to normalize $FILE1"
  exit 1
fi

if ! jq -f "$NORMALIZE_JQ" "$FILE2" > "$NORM2" 2>/dev/null; then
  echo "Error: Failed to normalize $FILE2"
  exit 1
fi

# Compare
if diff -q "$NORM1" "$NORM2" >/dev/null 2>&1; then
  exit 0  # Files match
else
  if [ "$SHOW_DIFFS" = "true" ]; then
    echo "Differences found between:"
    echo "  File 1: $FILE1"
    echo "  File 2: $FILE2"
    echo ""
    diff -u "$NORM1" "$NORM2" | head -100
  fi
  exit 1  # Files differ
fi
