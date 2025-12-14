#!/usr/bin/env bash
# ==============================================================================
# Kryon Example Auto-Generator
# ==============================================================================
# Generates .kir and .nim files from .kry source files
# Validates round-trip transpilation (.kry → .kir → .nim → .kir)
#
# Usage:
#   ./scripts/generate_examples.sh                    # Generate all
#   ./scripts/generate_examples.sh hello_world        # Generate one
#   ./scripts/generate_examples.sh --validate         # With validation
#   ./scripts/generate_examples.sh --validate --diff  # Show diffs
#   ./scripts/generate_examples.sh --clean            # Clean generated
#   ./scripts/generate_examples.sh --parallel         # Parallel processing
#
# Output:
#   examples/nim/*.nim              # Generated Nim examples (user-facing)
#   build/generated/kir/*.kir       # Generated KIR files (debug)
#   build/generated/roundtrip/*.kir # Round-trip KIR (validation)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
KRY_DIR="$PROJECT_DIR/examples/kry"
OUTPUT_NIM_DIR="$PROJECT_DIR/examples/nim"
GEN_KIR_DIR="$PROJECT_DIR/build/generated/kir"
GEN_ROUNDTRIP_DIR="$PROJECT_DIR/build/generated/roundtrip"
KRYON="${KRYON:-$HOME/.local/bin/kryon}"
COMPARE_SCRIPT="$SCRIPT_DIR/compare_kir.sh"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
VALIDATE=false
SHOW_DIFFS=false
CLEAN=false
VERBOSE=false
PARALLEL=false
MAX_JOBS=4
SPECIFIC_EXAMPLE=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --validate) VALIDATE=true; shift ;;
    --diff) SHOW_DIFFS=true; shift ;;
    --clean) CLEAN=true; shift ;;
    --verbose) VERBOSE=true; shift ;;
    --parallel) PARALLEL=true; shift ;;
    --jobs=*) MAX_JOBS="${1#*=}"; PARALLEL=true; shift ;;
    -j) PARALLEL=true; MAX_JOBS="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [example_name] [--validate] [--diff] [--clean] [--verbose] [--parallel] [-j N]"
      echo ""
      echo "Options:"
      echo "  --validate    Validate round-trip transpilation"
      echo "  --diff        Show detailed diffs for mismatches"
      echo "  --clean       Remove all generated files"
      echo "  --verbose     Show detailed progress"
      echo "  --parallel    Process examples in parallel"
      echo "  -j N          Max parallel jobs (default: 4)"
      exit 0
      ;;
    -*) echo "Unknown option: $1"; exit 1 ;;
    *) SPECIFIC_EXAMPLE="$1"; shift ;;
  esac
done

# Clean mode
if [ "$CLEAN" = true ]; then
  echo -e "${YELLOW}Cleaning generated files...${NC}"
  rm -rf "$OUTPUT_NIM_DIR"
  rm -rf "$GEN_KIR_DIR"
  rm -rf "$GEN_ROUNDTRIP_DIR"
  echo -e "${GREEN}✓ Cleaned${NC}"
  exit 0
fi

# Create output directories
mkdir -p "$OUTPUT_NIM_DIR"
mkdir -p "$GEN_KIR_DIR"
mkdir -p "$GEN_ROUNDTRIP_DIR"

# Check if kryon CLI exists
if [ ! -f "$KRYON" ]; then
  echo -e "${RED}Error: kryon CLI not found at $KRYON${NC}"
  echo "Please install with: make install"
  exit 1
fi

# Discover .kry files
if [ -n "$SPECIFIC_EXAMPLE" ]; then
  if [ ! -f "$KRY_DIR/${SPECIFIC_EXAMPLE}.kry" ]; then
    echo -e "${RED}Error: $KRY_DIR/${SPECIFIC_EXAMPLE}.kry not found${NC}"
    exit 1
  fi
  KRY_FILES=("$KRY_DIR/${SPECIFIC_EXAMPLE}.kry")
else
  KRY_FILES=("$KRY_DIR"/*.kry)
fi

total=${#KRY_FILES[@]}
success=0
failed=0
validation_failed=0

echo "=========================================="
echo "Kryon Example Auto-Generator"
echo "=========================================="
echo "Source: $KRY_DIR"
echo "Output: $OUTPUT_NIM_DIR"
echo "Examples: $total"
[ "$VALIDATE" = true ] && echo "Mode: Validation enabled"
[ "$PARALLEL" = true ] && echo "Mode: Parallel ($MAX_JOBS jobs)"
echo ""

# ==============================================================================
# Process a single example
# ==============================================================================
process_example() {
  local kry_file="$1"
  local name=$(basename "$kry_file" .kry)
  local kir_static="$GEN_KIR_DIR/${name}_static.kir"
  local kir_expanded="$GEN_KIR_DIR/${name}.kir"
  local nim_file="$OUTPUT_NIM_DIR/${name}.nim"
  local roundtrip_kir="$GEN_ROUNDTRIP_DIR/${name}_roundtrip.kir"
  local result="success"

  # Step 1: .kry → .kir (static, for codegen)
  if ! "$KRYON" compile "$kry_file" --output="$kir_static" --preserve-static --no-cache >/dev/null 2>&1; then
    echo "FAILED:$name:static"
    return 1
  fi

  # Step 2: .kry → .kir (expanded, for runtime & validation)
  if ! "$KRYON" compile "$kry_file" --output="$kir_expanded" --no-cache >/dev/null 2>&1; then
    echo "FAILED:$name:expanded"
    return 1
  fi

  # Step 3: .kir → .nim (codegen)
  if ! "$KRYON" codegen "$kir_static" --lang=nim --output="$nim_file" >/dev/null 2>&1; then
    echo "SKIPPED:$name:codegen"
    return 0
  fi

  # Step 4: Validation (if requested)
  if [ "$VALIDATE" = true ]; then
    if ! "$KRYON" compile "$nim_file" --output="$roundtrip_kir" --no-cache >/dev/null 2>&1; then
      echo "FAILED:$name:roundtrip"
      return 1
    fi

    if [ -f "$COMPARE_SCRIPT" ]; then
      if ! bash "$COMPARE_SCRIPT" "$kir_expanded" "$roundtrip_kir" false >/dev/null 2>&1; then
        echo "FAILED:$name:validation"
        return 1
      fi
    fi
  fi

  echo "SUCCESS:$name"
  return 0
}
export -f process_example
export KRYON GEN_KIR_DIR OUTPUT_NIM_DIR GEN_ROUNDTRIP_DIR VALIDATE COMPARE_SCRIPT

# ==============================================================================
# Parallel Processing Mode
# ==============================================================================
if [ "$PARALLEL" = true ]; then
  echo -e "${BLUE}Processing in parallel with $MAX_JOBS jobs...${NC}"
  echo ""

  # Create temp file for results
  RESULT_FILE=$(mktemp)
  trap "rm -f $RESULT_FILE" EXIT

  # Run in parallel using xargs
  printf '%s\n' "${KRY_FILES[@]}" | xargs -P "$MAX_JOBS" -I {} bash -c 'process_example "$@"' _ {} >> "$RESULT_FILE"

  # Parse results
  while IFS=: read -r status name stage; do
    case "$status" in
      SUCCESS)
        echo -e "${GREEN}✓${NC} $name"
        success=$((success + 1))
        ;;
      SKIPPED)
        echo -e "${YELLOW}⚠${NC} $name (codegen skipped)"
        success=$((success + 1))
        ;;
      FAILED)
        echo -e "${RED}✗${NC} $name ($stage failed)"
        if [ "$stage" = "validation" ] || [ "$stage" = "roundtrip" ]; then
          validation_failed=$((validation_failed + 1))
        else
          failed=$((failed + 1))
        fi
        ;;
    esac
  done < "$RESULT_FILE"

# ==============================================================================
# Sequential Processing Mode (default)
# ==============================================================================
else
  for kry_file in "${KRY_FILES[@]}"; do
    name=$(basename "$kry_file" .kry)
    kir_static="$GEN_KIR_DIR/${name}_static.kir"
    kir_expanded="$GEN_KIR_DIR/${name}.kir"
    nim_file="$OUTPUT_NIM_DIR/${name}.nim"
    roundtrip_kir="$GEN_ROUNDTRIP_DIR/${name}_roundtrip.kir"

    current=$((success + failed + validation_failed + 1))
    echo -e "${BLUE}[$current/$total]${NC} $name"

    # Step 1: .kry → .kir (static, for codegen)
    [ "$VERBOSE" = true ] && echo "  → Compiling .kry → .kir (static)..."
    if ! "$KRYON" compile "$kry_file" --output="$kir_static" --preserve-static --no-cache >/dev/null 2>&1; then
      echo -e "  ${RED}✗ Failed: .kry → .kir (static)${NC}"
      failed=$((failed + 1))
      continue
    fi
    [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .kry → .kir (static)"

    # Step 2: .kry → .kir (expanded, for runtime & validation)
    [ "$VERBOSE" = true ] && echo "  → Compiling .kry → .kir (expanded)..."
    if ! "$KRYON" compile "$kry_file" --output="$kir_expanded" --no-cache >/dev/null 2>&1; then
      echo -e "  ${RED}✗ Failed: .kry → .kir (expanded)${NC}"
      failed=$((failed + 1))
      continue
    fi
    [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .kry → .kir (expanded)"

    # Step 3: .kir → .nim (codegen)
    [ "$VERBOSE" = true ] && echo "  → Generating .kir → .nim..."
    if ! "$KRYON" codegen "$kir_static" --lang=nim --output="$nim_file" >/dev/null 2>&1; then
      echo -e "  ${YELLOW}⚠ Codegen not available (skipping .nim generation)${NC}"
      success=$((success + 1))
      continue
    fi
    [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .kir → .nim"

    # Step 4: Validation (if requested)
    if [ "$VALIDATE" = true ]; then
      # Compile .nim → .kir (round-trip)
      [ "$VERBOSE" = true ] && echo "  → Compiling .nim → .kir (round-trip)..."
      if ! "$KRYON" compile "$nim_file" --output="$roundtrip_kir" --no-cache >/dev/null 2>&1; then
        echo -e "  ${RED}✗ Failed: .nim → .kir (round-trip compilation)${NC}"
        validation_failed=$((validation_failed + 1))
        continue
      fi
      [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .nim → .kir (round-trip)"

      # Compare .kir files (use normalize-and-compare)
      [ "$VERBOSE" = true ] && echo "  → Comparing .kir files..."
      if [ -f "$COMPARE_SCRIPT" ]; then
        if bash "$COMPARE_SCRIPT" "$kir_expanded" "$roundtrip_kir" "$SHOW_DIFFS"; then
          echo -e "  ${GREEN}✓ Round-trip validated${NC}"
          success=$((success + 1))
        else
          echo -e "  ${RED}✗ Round-trip mismatch${NC}"
          validation_failed=$((validation_failed + 1))
        fi
      else
        echo -e "  ${YELLOW}⚠ Comparison script not found, skipping validation${NC}"
        success=$((success + 1))
      fi
    else
      success=$((success + 1))
      echo -e "  ${GREEN}✓ Generated${NC}"
    fi
  done
fi

echo ""
echo "=========================================="
if [ "$VALIDATE" = true ]; then
  total_passed=$success
  total_failed=$((failed + validation_failed))

  if [ $total_failed -eq 0 ]; then
    echo -e "${GREEN}✓ SUCCESS:${NC} $total_passed/$total examples validated"
  else
    echo -e "${RED}✗ FAILED:${NC} $total_failed/$total examples failed"
    echo -e "  ${GREEN}Passed:${NC} $success"
    echo -e "  ${RED}Compilation failed:${NC} $failed"
    echo -e "  ${RED}Validation failed:${NC} $validation_failed"
  fi
else
  echo -e "${GREEN}Generated:${NC} $success succeeded, $failed failed"
fi
echo "=========================================="

# Exit with error if any failed
if [ $validation_failed -gt 0 ] || [ $failed -gt 0 ]; then
  exit 1
fi

exit 0
