#!/bin/bash
# =============================================================================
# Kryon Example Runner
# =============================================================================
# Runs .kry examples through the full pipeline:
#
# Basic usage (.kry → .kir → render on desktop):
#   ./run_example.sh hello_world
#
# Specify target platform (.kry → .kir → render on target):
#   ./run_example.sh hello_world terminal
#   ./run_example.sh 10 desktop
#   ./run_example.sh 5 android
#
# Available targets: desktop (default), terminal, android
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

# Always rebuild and install libraries to ensure we're testing latest code
echo -e "${CYAN}Building and installing libraries...${NC}"
if [ -n "$IN_NIX_SHELL" ]; then
    if ! make install > /dev/null 2>&1; then
        echo -e "${RED}✗ Build failed${NC}"
        echo "Run 'make' to see detailed error output"
        exit 1
    fi
else
    if ! nix-shell --run "make install" > /dev/null 2>&1; then
        echo -e "${RED}✗ Build failed${NC}"
        echo "Run 'nix-shell --run make' to see detailed error output"
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
    echo -e "${YELLOW}Usage:${NC} ./run_example.sh <number|name> [target]"
    echo ""
    echo -e "${BLUE}Examples:${NC}"
    echo "  ./run_example.sh 8                 # Run example #8 with desktop (default)"
    echo "  ./run_example.sh 8 terminal        # Run example #8 in terminal"
    echo "  ./run_example.sh hello_world       # Run by name with desktop"
    echo "  ./run_example.sh hello_world terminal  # Run by name in terminal"
    echo ""
    echo -e "${BLUE}Available targets:${NC}"
    echo "  • desktop   - SDL3 desktop window (default)"
    echo "  • terminal  - Terminal UI (TUI)"
    echo "  • android   - Android device"
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
TARGET="${2:-desktop}"  # Default to desktop target

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
# MAIN: .kry → .kir → .nim → render
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

# Step 1: Compile .kry to .kir
echo -e "${YELLOW}[1/3]${NC} Parsing ${GREEN}$INPUT_FILE${NC} → ${GREEN}$KIR_FILE${NC}"
~/.local/bin/kryon compile "$INPUT_FILE" --output="$KIR_FILE" --no-cache

if [ -f "$KIR_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -c < "$KIR_FILE") bytes"
else
    echo -e "      ${RED}✗${NC} Failed to generate .kir file"
    exit 1
fi

# Step 2: Generate .nim from .kir
echo -e "${YELLOW}[2/3]${NC} Generating ${GREEN}$KIR_FILE${NC} → ${GREEN}$NIM_FILE${NC}"
~/.local/bin/kryon codegen "$KIR_FILE" --lang=nim --output="$NIM_FILE"

if [ -f "$NIM_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -l < "$NIM_FILE") lines"
else
    echo -e "      ${YELLOW}⚠${NC} Codegen not available for this format"
fi

echo ""

# Step 3: Run the .kir file (not the original .kry)
echo -e "${YELLOW}[3/3]${NC} Running ${GREEN}$KIR_FILE${NC} on ${CYAN}$TARGET${NC} target"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
~/.local/bin/kryon run "$KIR_FILE" --target="$TARGET"

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}Output files:${NC}"
echo "  • IR:  $KIR_FILE"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
