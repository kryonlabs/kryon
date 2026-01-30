#!/bin/bash
# =============================================================================
# Kryon Example Runner
# =============================================================================
# Runs .kry examples through the kryon CLI with target selection:
#   .kry → .kir → [target-specific output]
#
# Basic usage:
#   ./run_example.sh hello_world          # Default: limbo target
#   ./run_example.sh hello_world limbo    # Explicit target
#   ./run_example.sh hello_world desktop  # Desktop SDL3
#   ./run_example.sh hello_world raylib   # Desktop Raylib
#
# Specify example by number:
#   ./run_example.sh 8
#   ./run_example.sh 8 web
#
# Supported targets:
#   limbo, dis, emu     - TaijiOS Limbo/DIS bytecode (default)
#   desktop, sdl3       - Desktop SDL3 renderer
#   raylib              - Desktop Raylib renderer
#   web                 - Web browser
#   android, kotlin     - Android APK
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

# Get example name and target
if [ -z "$1" ]; then
    echo -e "${YELLOW}Usage:${NC} ./run_example.sh <number|name> [target]"
    echo ""
    echo -e "${BLUE}Targets:${NC}"
    echo "  (none)              - Default: limbo (TaijiOS VM)"
    echo "  limbo, dis, emu     - TaijiOS Limbo/DIS bytecode"
    echo "  desktop, sdl3       - Desktop SDL3 renderer"
    echo "  raylib              - Desktop Raylib renderer"
    echo "  web                 - Web browser"
    echo "  android, kotlin     - Android APK"
    echo ""
    echo -e "${BLUE}Examples:${NC}"
    echo "  ./run_example.sh 8              # Run example #8 with limbo"
    echo "  ./run_example.sh hello_world    # Run by name with limbo"
    echo "  ./run_example.sh 8 desktop      # Run with desktop target"
    echo "  ./run_example.sh hello_world raylib  # Run with raylib"
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
TARGET="${2:-limbo}"  # Default to limbo if not specified

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

# Validate and resolve target alias
case "$TARGET" in
    limbo|dis|emu)
        TARGET="limbo"
        ;;
    desktop|sdl3)
        TARGET="desktop"
        ;;
    raylib)
        TARGET="desktop"
        RENDERER="raylib"
        ;;
    web)
        TARGET="web"
        ;;
    android|kotlin)
        TARGET="android"
        ;;
    *)
        echo -e "${RED}Error:${NC} Unknown target '$TARGET'"
        echo "Valid targets: limbo, dis, emu, desktop, sdl3, raylib, web, android, kotlin"
        exit 1
        ;;
esac

# Determine basename
if [[ "$EXAMPLE" == *.kry ]]; then
    BASENAME=$(basename "$EXAMPLE" .kry)
else
    BASENAME="$EXAMPLE"
fi

# Output paths
KIR_FILE=".kryon_cache/${BASENAME}.kir"

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
echo -e "${BLUE}Kryon Example Runner${NC} - Target: ${CYAN}${TARGET}${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Step 1: Compile .kry to .kir
echo -e "${YELLOW}[1/2]${NC} Compiling ${GREEN}$INPUT_FILE${NC} → ${GREEN}$KIR_FILE${NC}"
mkdir -p .kryon_cache
~/.local/bin/kryon compile "$INPUT_FILE" --output="$KIR_FILE"

if [ -f "$KIR_FILE" ]; then
    echo -e "      ${GREEN}✓${NC} Generated $(wc -c < "$KIR_FILE") bytes"
else
    echo -e "      ${RED}✗${NC} Failed to generate .kir file"
    exit 1
fi

echo ""

# Step 2: Run with target
echo -e "${YELLOW}[2/2]${NC} Running with target: ${CYAN}$TARGET${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if [ -n "$RENDERER" ]; then
    # Desktop with specific renderer
    ~/.local/bin/kryon run --target="$TARGET" --renderer="$RENDERER" "$KIR_FILE"
else
    # Standard target
    ~/.local/bin/kryon run --target="$TARGET" "$KIR_FILE"
fi

RESULT=$?

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
if [ $RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ Execution complete${NC}"
else
    echo -e "${RED}✗ Execution failed with exit code $RESULT${NC}"
fi
echo -e "${GREEN}Output:${NC}"
echo "  • KIR: $KIR_FILE"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

exit $RESULT
