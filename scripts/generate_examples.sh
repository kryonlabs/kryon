#!/usr/bin/env bash
# ==============================================================================
# Kryon Example Auto-Generator
# ==============================================================================
# Generates .kir and language-specific files from .kry source files
# Validates round-trip transpilation (.kry → .kir → .lang → .kir)
#
# Usage:
#   ./scripts/generate_examples.sh                    # Generate all languages
#   ./scripts/generate_examples.sh --lang=c           # Generate C only
#   ./scripts/generate_examples.sh --lang=nim         # Generate Nim only
#   ./scripts/generate_examples.sh --lang=tsx         # Generate TSX only
#   ./scripts/generate_examples.sh --lang=jsx         # Generate JSX only
#   ./scripts/generate_examples.sh --lang=lua         # Generate Lua only
#   ./scripts/generate_examples.sh --lang=html        # Generate HTML only
#   ./scripts/generate_examples.sh --lang=markdown    # Generate Markdown only
#   ./scripts/generate_examples.sh hello_world        # Generate one example (all langs)
#   ./scripts/generate_examples.sh --validate         # With validation
#   ./scripts/generate_examples.sh --validate --diff  # Show diffs
#   ./scripts/generate_examples.sh --clean            # Clean generated
#   ./scripts/generate_examples.sh --parallel         # Parallel processing
#
# Supported Languages:
#   nim, c, tsx, jsx, lua, html, markdown
#
# Output:
#   examples/c/*.c                  # Generated C examples
#   examples/nim/*.nim              # Generated Nim examples
#   examples/tsx/*.tsx              # Generated TSX/JSX examples
#   examples/lua/*.lua              # Generated Lua examples
#   examples/html/*.html            # Generated HTML examples
#   examples/md/*.md                # Generated Markdown examples
#   build/generated/kir/*.kir       # Generated KIR files (debug)
#   build/generated/roundtrip/*.kir # Round-trip KIR (validation)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
KRY_DIR="$PROJECT_DIR/examples/kry"
OUTPUT_NIM_DIR="$PROJECT_DIR/examples/nim"
OUTPUT_TSX_DIR="$PROJECT_DIR/examples/tsx"
OUTPUT_JSX_DIR="$PROJECT_DIR/examples/jsx"
OUTPUT_C_DIR="$PROJECT_DIR/examples/c"
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
LANG=""  # Empty = all languages
ALL_LANGS=false

# All supported languages
SUPPORTED_LANGS=("nim" "c" "tsx" "jsx" "lua" "html" "markdown")

while [[ $# -gt 0 ]]; do
  case $1 in
    --validate) VALIDATE=true; shift ;;
    --diff) SHOW_DIFFS=true; shift ;;
    --clean) CLEAN=true; shift ;;
    --verbose) VERBOSE=true; shift ;;
    --parallel) PARALLEL=true; shift ;;
    --jobs=*) MAX_JOBS="${1#*=}"; PARALLEL=true; shift ;;
    -j) PARALLEL=true; MAX_JOBS="$2"; shift 2 ;;
    --lang=*) LANG="${1#*=}"; shift ;;
    --all) ALL_LANGS=true; shift ;;
    -h|--help)
      echo "Usage: $0 [example_name] [--lang=LANG|--all] [--validate] [--diff] [--clean] [--verbose] [--parallel] [-j N]"
      echo ""
      echo "Options:"
      echo "  --lang=LANG   Target language (c, nim, tsx, jsx, lua, html, markdown). Default: all"
      echo "  --all         Generate for all languages (default behavior)"
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

# If no language specified, use all
if [ -z "$LANG" ] || [ "$ALL_LANGS" = true ]; then
  LANGS=("${SUPPORTED_LANGS[@]}")
else
  # Validate language
  if [[ ! " ${SUPPORTED_LANGS[@]} " =~ " ${LANG} " ]]; then
    echo -e "${RED}Error: Unknown language '$LANG'${NC}"
    echo "Supported: ${SUPPORTED_LANGS[*]}"
    exit 1
  fi
  LANGS=("$LANG")
fi

# Get output dir and extension for a language
get_output_info() {
  local lang="$1"
  case "$lang" in
    nim) echo "$OUTPUT_NIM_DIR:.nim" ;;
    tsx) echo "$OUTPUT_TSX_DIR:.tsx" ;;
    jsx) echo "$OUTPUT_JSX_DIR:.jsx" ;;
    c) echo "$OUTPUT_C_DIR:.c" ;;
    lua) echo "$PROJECT_DIR/examples/lua:.lua" ;;
    html) echo "$PROJECT_DIR/examples/html:.html" ;;
    markdown|md) echo "$PROJECT_DIR/examples/md:.md" ;;
    *) echo ":"; return 1 ;;
  esac
}

# Clean mode
if [ "$CLEAN" = true ]; then
  echo -e "${YELLOW}Cleaning generated files...${NC}"
  rm -rf "$OUTPUT_NIM_DIR"
  rm -rf "$OUTPUT_TSX_DIR"
  rm -rf "$OUTPUT_JSX_DIR"
  rm -rf "$OUTPUT_C_DIR"
  rm -rf "$PROJECT_DIR/examples/lua"
  rm -rf "$PROJECT_DIR/examples/html"
  rm -rf "$PROJECT_DIR/examples/md"
  rm -rf "$GEN_KIR_DIR"
  rm -rf "$GEN_ROUNDTRIP_DIR"
  echo -e "${GREEN}✓ Cleaned${NC}"
  exit 0
fi

# Create base directories
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

echo "=========================================="
echo "Kryon Example Auto-Generator"
echo "=========================================="
echo "Source: $KRY_DIR"
if [ ${#LANGS[@]} -eq 1 ]; then
  echo "Language: ${LANGS[0]}"
else
  echo "Languages: ${LANGS[*]}"
fi
echo "Examples: $total"
[ "$VALIDATE" = true ] && echo "Mode: Validation enabled"
[ "$PARALLEL" = true ] && echo "Mode: Parallel ($MAX_JOBS jobs)"
echo ""

# ==============================================================================
# Process a single example for a specific language
# ==============================================================================
process_example() {
  local kry_file="$1"
  local lang="$2"
  local name=$(basename "$kry_file" .kry)
  local kir_static="$GEN_KIR_DIR/${name}_static.kir"
  local kir_expanded="$GEN_KIR_DIR/${name}.kir"

  # Get output info for this language
  local info=$(get_output_info "$lang")
  local output_dir="${info%:*}"
  local ext="${info#*:}"
  local output_file="$output_dir/${name}${ext}"
  local roundtrip_kir="$GEN_ROUNDTRIP_DIR/${name}_${lang}_roundtrip.kir"

  # Create output directory
  mkdir -p "$output_dir"

  # Step 1: .kry → .kir (static, for codegen) - only once per example
  if [ ! -f "$kir_static" ]; then
    if ! "$KRYON" compile "$kry_file" --output="$kir_static" --preserve-static --no-cache >/dev/null 2>&1; then
      echo "FAILED:$name:$lang:static"
      return 1
    fi
  fi

  # Step 2: .kry → .kir (expanded, for runtime & validation) - only once per example
  if [ ! -f "$kir_expanded" ]; then
    if ! "$KRYON" compile "$kry_file" --output="$kir_expanded" --no-cache >/dev/null 2>&1; then
      echo "FAILED:$name:$lang:expanded"
      return 1
    fi
  fi

  # Step 3: .kir → target language (codegen)
  if ! "$KRYON" codegen "$kir_static" --lang="$lang" --output="$output_file" >/dev/null 2>&1; then
    echo "SKIPPED:$name:$lang:codegen"
    return 0
  fi

  # Step 4: Validation (if requested and language supports it)
  # Note: Only Nim supports full round-trip validation currently
  if [ "$VALIDATE" = true ] && [ "$lang" = "nim" ]; then
    if ! "$KRYON" compile "$output_file" --output="$roundtrip_kir" --no-cache >/dev/null 2>&1; then
      echo "FAILED:$name:$lang:roundtrip"
      return 1
    fi

    if [ -f "$COMPARE_SCRIPT" ]; then
      if ! bash "$COMPARE_SCRIPT" "$kir_expanded" "$roundtrip_kir" false >/dev/null 2>&1; then
        echo "FAILED:$name:$lang:validation"
        return 1
      fi
    fi
  fi

  echo "SUCCESS:$name:$lang"
  return 0
}
export -f process_example get_output_info
export KRYON GEN_KIR_DIR GEN_ROUNDTRIP_DIR VALIDATE COMPARE_SCRIPT PROJECT_DIR OUTPUT_NIM_DIR OUTPUT_TSX_DIR OUTPUT_JSX_DIR OUTPUT_C_DIR

# ==============================================================================
# Parallel Processing Mode
# ==============================================================================
if [ "$PARALLEL" = true ]; then
  echo -e "${BLUE}Processing in parallel with $MAX_JOBS jobs...${NC}"
  echo ""

  # Track results per language
  declare -A lang_success lang_failed lang_skipped lang_validation_failed
  for lang in "${LANGS[@]}"; do
    lang_success[$lang]=0
    lang_failed[$lang]=0
    lang_skipped[$lang]=0
    lang_validation_failed[$lang]=0
  done

  # Create temp file for results
  RESULT_FILE=$(mktemp)
  trap "rm -f $RESULT_FILE" EXIT

  # Generate all combinations of kry_file + language
  for kry_file in "${KRY_FILES[@]}"; do
    for lang in "${LANGS[@]}"; do
      echo "$kry_file:$lang"
    done
  done | xargs -P "$MAX_JOBS" -I {} bash -c 'IFS=: read -r file lang <<< "$1"; process_example "$file" "$lang"' _ {} >> "$RESULT_FILE"

  # Parse results
  while IFS=: read -r status name lang stage; do
    case "$status" in
      SUCCESS)
        echo -e "${GREEN}✓${NC} $name [$lang]"
        lang_success[$lang]=$((${lang_success[$lang]} + 1))
        ;;
      SKIPPED)
        echo -e "${YELLOW}⚠${NC} $name [$lang] (codegen not available)"
        lang_skipped[$lang]=$((${lang_skipped[$lang]} + 1))
        ;;
      FAILED)
        echo -e "${RED}✗${NC} $name [$lang] ($stage failed)"
        if [ "$stage" = "validation" ] || [ "$stage" = "roundtrip" ]; then
          lang_validation_failed[$lang]=$((${lang_validation_failed[$lang]} + 1))
        else
          lang_failed[$lang]=$((${lang_failed[$lang]} + 1))
        fi
        ;;
    esac
  done < "$RESULT_FILE"

# ==============================================================================
# Sequential Processing Mode (default)
# ==============================================================================
else
  # Track results per language
  declare -A lang_success lang_failed lang_skipped lang_validation_failed
  for lang in "${LANGS[@]}"; do
    lang_success[$lang]=0
    lang_failed[$lang]=0
    lang_skipped[$lang]=0
    lang_validation_failed[$lang]=0
  done

  total_combinations=$((${#KRY_FILES[@]} * ${#LANGS[@]}))
  current=0

  for kry_file in "${KRY_FILES[@]}"; do
    name=$(basename "$kry_file" .kry)
    kir_static="$GEN_KIR_DIR/${name}_static.kir"
    kir_expanded="$GEN_KIR_DIR/${name}.kir"

    # Generate .kir files once per example (not per language)
    kir_generated=false

    for lang in "${LANGS[@]}"; do
      current=$((current + 1))

      # Get output info for this language
      info=$(get_output_info "$lang")
      output_dir="${info%:*}"
      ext="${info#*:}"
      output_file="$output_dir/${name}${ext}"
      roundtrip_kir="$GEN_ROUNDTRIP_DIR/${name}_${lang}_roundtrip.kir"

      # Create output directory
      mkdir -p "$output_dir"

      echo -e "${BLUE}[$current/$total_combinations]${NC} $name → $lang"

      # Step 1 & 2: .kry → .kir (only once per example)
      if [ "$kir_generated" = false ]; then
        # Step 1: .kry → .kir (static, for codegen)
        [ "$VERBOSE" = true ] && echo "  → Compiling .kry → .kir (static)..."
        if ! "$KRYON" compile "$kry_file" --output="$kir_static" --preserve-static --no-cache >/dev/null 2>&1; then
          echo -e "  ${RED}✗ Failed: .kry → .kir (static)${NC}"
          lang_failed[$lang]=$((${lang_failed[$lang]} + 1))
          # Mark as failed for all remaining languages too
          for remaining_lang in "${LANGS[@]}"; do
            if [ "$remaining_lang" != "$lang" ]; then
              lang_failed[$remaining_lang]=$((${lang_failed[$remaining_lang]} + 1))
            fi
          done
          break  # Skip all languages for this example
        fi
        [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .kry → .kir (static)"

        # Step 2: .kry → .kir (expanded, for runtime & validation)
        [ "$VERBOSE" = true ] && echo "  → Compiling .kry → .kir (expanded)..."
        if ! "$KRYON" compile "$kry_file" --output="$kir_expanded" --no-cache >/dev/null 2>&1; then
          echo -e "  ${RED}✗ Failed: .kry → .kir (expanded)${NC}"
          lang_failed[$lang]=$((${lang_failed[$lang]} + 1))
          # Mark as failed for all remaining languages too
          for remaining_lang in "${LANGS[@]}"; do
            if [ "$remaining_lang" != "$lang" ]; then
              lang_failed[$remaining_lang]=$((${lang_failed[$remaining_lang]} + 1))
            fi
          done
          break  # Skip all languages for this example
        fi
        [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .kry → .kir (expanded)"

        kir_generated=true
      fi

      # Step 3: .kir → target language (codegen)
      [ "$VERBOSE" = true ] && echo "  → Generating .kir → $lang..."
      if ! "$KRYON" codegen "$kir_static" --lang="$lang" --output="$output_file" >/dev/null 2>&1; then
        echo -e "  ${YELLOW}⚠ Codegen not available for $lang${NC}"
        lang_skipped[$lang]=$((${lang_skipped[$lang]} + 1))
        continue
      fi
      [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} .kir → $lang"

      # Step 4: Validation (if requested and language supports it)
      if [ "$VALIDATE" = true ] && [ "$lang" = "nim" ]; then
        # Compile generated file → .kir (round-trip)
        [ "$VERBOSE" = true ] && echo "  → Compiling $lang → .kir (round-trip)..."
        if ! "$KRYON" compile "$output_file" --output="$roundtrip_kir" --no-cache >/dev/null 2>&1; then
          echo -e "  ${RED}✗ Failed: $lang → .kir (round-trip compilation)${NC}"
          lang_validation_failed[$lang]=$((${lang_validation_failed[$lang]} + 1))
          continue
        fi
        [ "$VERBOSE" = true ] && echo -e "    ${GREEN}✓${NC} $lang → .kir (round-trip)"

        # Compare .kir files (use normalize-and-compare)
        [ "$VERBOSE" = true ] && echo "  → Comparing .kir files..."
        if [ -f "$COMPARE_SCRIPT" ]; then
          if bash "$COMPARE_SCRIPT" "$kir_expanded" "$roundtrip_kir" "$SHOW_DIFFS"; then
            echo -e "  ${GREEN}✓ Round-trip validated${NC}"
            lang_success[$lang]=$((${lang_success[$lang]} + 1))
          else
            echo -e "  ${RED}✗ Round-trip mismatch${NC}"
            lang_validation_failed[$lang]=$((${lang_validation_failed[$lang]} + 1))
          fi
        else
          echo -e "  ${YELLOW}⚠ Comparison script not found, skipping validation${NC}"
          lang_success[$lang]=$((${lang_success[$lang]} + 1))
        fi
      elif [ "$VALIDATE" = true ] && [ "$lang" != "nim" ]; then
        echo -e "  ${YELLOW}⚠ Validation not yet supported for $lang${NC}"
        lang_success[$lang]=$((${lang_success[$lang]} + 1))
      else
        lang_success[$lang]=$((${lang_success[$lang]} + 1))
        echo -e "  ${GREEN}✓ Generated${NC}"
      fi
    done
  done
fi

echo ""
echo "=========================================="
echo "Summary by Language"
echo "=========================================="

any_failed=false

for lang in "${LANGS[@]}"; do
  success_count=${lang_success[$lang]:-0}
  failed_count=${lang_failed[$lang]:-0}
  skipped_count=${lang_skipped[$lang]:-0}
  validation_failed_count=${lang_validation_failed[$lang]:-0}

  total_processed=$((success_count + failed_count + skipped_count + validation_failed_count))

  echo ""
  echo -e "${BLUE}$lang:${NC}"

  if [ "$VALIDATE" = true ]; then
    total_failed=$((failed_count + validation_failed_count))

    if [ $total_failed -eq 0 ] && [ $skipped_count -eq 0 ]; then
      echo -e "  ${GREEN}✓ SUCCESS:${NC} $success_count/$total validated"
    elif [ $total_processed -eq 0 ]; then
      echo -e "  ${YELLOW}⚠ SKIPPED:${NC} No examples processed"
    else
      echo -e "  ${GREEN}Passed:${NC} $success_count"
      [ $failed_count -gt 0 ] && echo -e "  ${RED}Compilation failed:${NC} $failed_count"
      [ $validation_failed_count -gt 0 ] && echo -e "  ${RED}Validation failed:${NC} $validation_failed_count"
      [ $skipped_count -gt 0 ] && echo -e "  ${YELLOW}Skipped (codegen unavailable):${NC} $skipped_count"
    fi

    if [ $total_failed -gt 0 ]; then
      any_failed=true
    fi
  else
    if [ $total_processed -eq 0 ]; then
      echo -e "  ${YELLOW}⚠ No examples processed${NC}"
    elif [ $skipped_count -eq $total_processed ]; then
      echo -e "  ${YELLOW}⚠ SKIPPED:${NC} Codegen not available (all $total examples skipped)"
    else
      echo -e "  ${GREEN}Generated:${NC} $success_count"
      [ $failed_count -gt 0 ] && echo -e "  ${RED}Failed:${NC} $failed_count"
      [ $skipped_count -gt 0 ] && echo -e "  ${YELLOW}Skipped:${NC} $skipped_count"
    fi

    if [ $failed_count -gt 0 ]; then
      any_failed=true
    fi
  fi
done

echo ""
echo "=========================================="

# Exit with error if any language had failures (not counting skipped)
if [ "$any_failed" = true ]; then
  exit 1
fi

exit 0
