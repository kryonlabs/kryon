#!/bin/bash
# =============================================================================
# Kryon Example Runner
# =============================================================================
# Runs .kry examples through the full pipeline:
#
# Default mode (.kry → .kir → render):
#   ./run_example.sh hello_world
#
# Nim mode (.nim → .kir → render):
#   ./run_example.sh hello_world nim
#
# This tests the round-trip: .kry → .kir → .nim → .kir → render
#
# Output:
#   build/ir/   - Generated .kir files
#   build/nim/  - Generated .nim files
#
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Ensure build directories exist
mkdir -p build/ir build/nim

# Prioritize local build directory, then installed libraries
export LD_LIBRARY_PATH="$SCRIPT_DIR/build:$HOME/.local/lib:$LD_LIBRARY_PATH"

# Build examples array
EXAMPLES=()
for f in examples/kry/*.kry; do
    EXAMPLES+=("$(basename "$f" .kry)")
done

# Get example name
if [ -z "$1" ]; then
    echo -e "${YELLOW}Usage:${NC} ./run_example.sh <number|name> [nim]"
    echo ""
    echo -e "${BLUE}Modes:${NC}"
    echo "  ./run_example.sh 8                 # Run example #8"
    echo "  ./run_example.sh hello_world       # Run by name"
    echo "  ./run_example.sh 8 nim             # Round-trip test"
    echo ""
    echo -e "${BLUE}Available examples:${NC}"
    i=1
    for name in "${EXAMPLES[@]}"; do
        printf "  %2d. %s\n" "$i" "$name"
        ((i++))
    done
    exit 1
fi

EXAMPLE="$1"
MODE="${2:-kry}"  # Default to kry mode

# If input is a number, convert to example name
if [[ "$EXAMPLE" =~ ^[0-9]+$ ]]; then
    INDEX=$((EXAMPLE - 1))
    if [ "$INDEX" -ge 0 ] && [ "$INDEX" -lt "${#EXAMPLES[@]}" ]; then
        EXAMPLE="${EXAMPLES[$INDEX]}"
    else
        echo -e "${RED}Error:${NC} Invalid example number: $1 (valid: 1-${#EXAMPLES[@]})"
        exit 1
    fi
fi

# Determine basename
if [[ "$EXAMPLE" == *.kry ]]; then
    BASENAME=$(basename "$EXAMPLE" .kry)
elif [[ "$EXAMPLE" == *.nim ]]; then
    BASENAME=$(basename "$EXAMPLE" .nim)
else
    BASENAME="$EXAMPLE"
fi

# Output paths
KIR_FILE="build/ir/${BASENAME}.kir"
NIM_FILE="build/nim/${BASENAME}.nim"

# =============================================================================
# NIM MODE: Run generated .nim through the pipeline
# =============================================================================
if [ "$MODE" = "nim" ]; then
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}Kryon Example Runner - Nim Mode (Round-Trip Test)${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""

    if [ ! -f "$NIM_FILE" ]; then
        echo -e "${RED}Error:${NC} Nim file not found: $NIM_FILE"
        echo -e "${YELLOW}Hint:${NC} Run './run_example.sh $BASENAME' first to generate the .nim file"
        exit 1
    fi

    # Use a separate .kir file for nim-generated IR
    NIM_KIR_FILE="build/ir/${BASENAME}_from_nim.kir"

    # Step 1: Compile .nim to .kir
    echo -e "${YELLOW}[1/2]${NC} Compiling ${GREEN}$NIM_FILE${NC} → ${GREEN}$NIM_KIR_FILE${NC}"
    ~/.local/bin/kryon compile "$NIM_FILE" --output="$NIM_KIR_FILE"

    if [ -f "$NIM_KIR_FILE" ]; then
        echo -e "      ${GREEN}✓${NC} Generated $(wc -c < "$NIM_KIR_FILE") bytes"
    else
        echo -e "      ${RED}✗${NC} Failed to compile .nim to .kir"
        exit 1
    fi

    echo ""

    # Step 2: Run the .kir file
    echo -e "${YELLOW}[2/2]${NC} Running ${GREEN}$NIM_KIR_FILE${NC} (from Nim)"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    ~/.local/bin/kryon run "$NIM_KIR_FILE"

    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}Round-trip files:${NC}"
    echo "  • Source Nim: $NIM_FILE"
    echo "  • Generated IR: $NIM_KIR_FILE"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    exit 0
fi

# =============================================================================
# KRY MODE: Default .kry → .kir → .nim → render
# =============================================================================

# Determine input file path
if [[ "$EXAMPLE" == *.kry ]]; then
    INPUT_FILE="$EXAMPLE"
else
    INPUT_FILE="examples/kry/${EXAMPLE}.kry"
fi

if [ ! -f "$INPUT_FILE" ]; then
    echo -e "${RED}Error:${NC} File not found: $INPUT_FILE"
    exit 1
fi

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Kryon Example Runner${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Step 1a: Compile .kry to .kir with --preserve-static for codegen
echo -e "${YELLOW}[1/3]${NC} Parsing ${GREEN}$INPUT_FILE${NC} → ${GREEN}$KIR_FILE${NC}"
KIR_STATIC="${KIR_FILE%.kir}_static.kir"
~/.local/bin/kryon compile "$INPUT_FILE" --output="$KIR_STATIC" --preserve-static --no-cache

# Step 1b: Compile without --preserve-static for runtime (expanded)
~/.local/bin/kryon compile "$INPUT_FILE" --output="$KIR_FILE" --no-cache

if [ -f "$KIR_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -c < "$KIR_FILE") bytes"
else
    echo -e "      ${RED}✗${NC} Failed to generate .kir file"
    exit 1
fi

# Step 2: Generate .nim from .kir (use preserved static version)
echo -e "${YELLOW}[2/3]${NC} Generating ${GREEN}$KIR_FILE${NC} → ${GREEN}$NIM_FILE${NC}"
~/.local/bin/kryon codegen "$KIR_STATIC" --lang=nim --output="$NIM_FILE"

if [ -f "$NIM_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -l < "$NIM_FILE") lines"
else
    echo -e "      ${YELLOW}⚠${NC} Codegen not available for this format"
fi

echo ""

# Step 3: Run the .kir file (not the original .kry)
echo -e "${YELLOW}[3/3]${NC} Running ${GREEN}$KIR_FILE${NC} (generated IR)"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
~/.local/bin/kryon run "$KIR_FILE"

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}Output files:${NC}"
echo "  • IR:  $KIR_FILE"
echo "  • Nim: $NIM_FILE"
echo ""
echo -e "${YELLOW}To test round-trip:${NC} ./run_example.sh $BASENAME nim"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
