#!/bin/bash
# =============================================================================
# Kryon Example Runner
# =============================================================================
# Runs .kry examples through the full native pipeline:
#   .kry → .kir → .c → native executable
#
# Basic usage:
#   ./run_example.sh hello_world
#
# Specify example by number:
#   ./run_example.sh 8
#
# Output (all in build/ir/):
#   build/ir/<name>.kir      - Intermediate representation
#   build/ir/<name>/main.c   - Generated C code
#   build/ir/<name>/app      - Native executable
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
mkdir -p build/ir

# Always rebuild and install libraries to ensure we're testing latest code
echo -e "${CYAN}Building and installing libraries...${NC}"
if [ -n "$IN_NIX_SHELL" ]; then
    if ! make install; then
        echo -e "${RED}✗ Build failed${NC}"
        exit 1
    fi
else
    if ! nix-shell --run "make install"; then
        echo -e "${RED}✗ Build failed${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓ Libraries built and installed successfully${NC}"

# Prioritize local build directory, then installed libraries
export LD_LIBRARY_PATH="$SCRIPT_DIR/build:$HOME/.local/lib:$LD_LIBRARY_PATH"

# Build examples array
EXAMPLES=()
for f in examples/kry/*.kry; do
    EXAMPLES+=("$(basename "$f" .kry)")
done

# Get example name
if [ -z "$1" ]; then
    echo -e "${YELLOW}Usage:${NC} ./run_example.sh <number|name>"
    echo ""
    echo -e "${BLUE}Pipeline:${NC} .kry → .kir → .c → native"
    echo ""
    echo -e "${BLUE}Examples:${NC}"
    echo "  ./run_example.sh 8            # Run example #8"
    echo "  ./run_example.sh hello_world  # Run by name"
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
else
    BASENAME="$EXAMPLE"
fi

# Output paths (all in build/ir/)
KIR_FILE="build/ir/${BASENAME}.kir"
C_DIR="build/ir/${BASENAME}"
C_FILE="${C_DIR}/main.c"
NATIVE_BIN="${C_DIR}/app"

# =============================================================================
# MAIN: .kry → .kir → .c → native
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
echo -e "${BLUE}Kryon Example Runner${NC} - Native Pipeline"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Step 1: Compile .kry to .kir
echo -e "${YELLOW}[1/4]${NC} Parsing ${GREEN}$INPUT_FILE${NC} → ${GREEN}$KIR_FILE${NC}"
~/.local/bin/kryon compile "$INPUT_FILE" --output="$KIR_FILE" --no-cache

if [ -f "$KIR_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -c < "$KIR_FILE") bytes"
else
    echo -e "      ${RED}✗${NC} Failed to generate .kir file"
    exit 1
fi

echo ""

# Step 2: Generate C code from .kir
echo -e "${YELLOW}[2/4]${NC} Codegen ${GREEN}$KIR_FILE${NC} → ${GREEN}$C_FILE${NC}"
rm -rf "$C_DIR"
~/.local/bin/kryon codegen c "$KIR_FILE" "$C_DIR"

if [ -f "$C_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -l < "$C_FILE") lines of C code"
else
    echo -e "      ${RED}✗${NC} Failed to generate C code"
    exit 1
fi

echo ""

# Step 3: Compile C to native executable
echo -e "${YELLOW}[3/4]${NC} Compiling ${GREEN}$C_FILE${NC} → ${GREEN}$NATIVE_BIN${NC}"

# Compiler flags
CC="${CC:-gcc}"
CFLAGS="-std=gnu99 -O2"
INCLUDES="-I./bindings/c -I./ir/include"
LIBS="-L./build -L./bindings/c -lkryon_desktop -lkryon_c -lkryon_ir -lraylib -lm"

if $CC $CFLAGS $INCLUDES "$C_FILE" -o "$NATIVE_BIN" $LIBS 2>&1; then
    echo -e "      ${GREEN}✓${NC} Compiled native executable"
else
    echo -e "      ${RED}✗${NC} Compilation failed"
    exit 1
fi

echo ""

# Step 4: Run native executable
echo -e "${YELLOW}[4/4]${NC} Running ${GREEN}$NATIVE_BIN${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
"$NATIVE_BIN"

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}Output files:${NC}"
echo "  • KIR:    $KIR_FILE"
echo "  • C:      $C_FILE"
echo "  • Native: $NATIVE_BIN"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
