#!/bin/bash

# Build and Test Script for Kryon Examples with New Architecture
# Usage: ./run_examples.sh [frontend] [renderer]
# Examples: ./run_examples.sh nim sdl3
#           ./run_examples.sh c framebuffer
#           ./run_examples.sh (defaults to nim sdl3)

set -e  # Exit on any error

# Set up signal handlers for clean exit
cleanup() {
    echo -e "\n${YELLOW}Interrupted by user. Cleaning up...${NC}"
    # Kill any background processes
    jobs -p | xargs -r kill 2>/dev/null || true
    exit 130
}

# Trap SIGINT (Ctrl+C) and SIGTERM
trap cleanup SIGINT SIGTERM

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Create build directories
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"

# Parse arguments
FRONTEND="${1:-nim}"
RENDERER="${2:-sdl3}"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Kryon Examples Build & Test Script${NC}"
echo -e "${BLUE}       (New Architecture v1.2)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${CYAN}Testing ALL $FRONTEND examples with $RENDERER renderer...${NC}"
echo ""

# Function to build Kryon architecture
build_kryon_architecture() {
    echo -e "${BLUE}Building Kryon architecture...${NC}"

    echo "  Building C core..."
    if make -C core; then
        echo -e "  ${GREEN}✓ C core built${NC}"
    else
        echo -e "  ${RED}✗ C core build failed${NC}"
        return 1
    fi

    echo "  Building common utilities..."
    if make -C renderers/common install; then
        echo -e "  ${GREEN}✓ Common utilities built${NC}"
    else
        echo -e "  ${RED}✗ Common utilities build failed${NC}"
        return 1
    fi

    echo "  Building framebuffer renderer..."
    if make -C renderers/framebuffer install; then
        echo -e "  ${GREEN}✓ Framebuffer renderer built${NC}"
    else
        echo -e "  ${RED}✗ Framebuffer renderer build failed${NC}"
        return 1
    fi

    echo "  Building SDL3 renderer..."
    if make -C renderers/sdl3; then
        echo -e "  ${GREEN}✓ SDL3 renderer built${NC}"
    else
        echo -e "  ${YELLOW}⚠ SDL3 renderer not available (missing SDL3 dev libraries)${NC}"
    fi

    echo "  Building terminal renderer..."
    if make -C renderers/terminal install; then
        echo -e "  ${GREEN}✓ Terminal renderer built${NC}"
    else
        echo -e "  ${YELLOW}⚠ Terminal renderer not available (missing libtickit dev libraries)${NC}"
    fi
}

# Main execution
build_kryon_architecture

if [ $? -ne 0 ]; then
    echo -e "\n${RED}Architecture build failed. Exiting.${NC}"
    exit 1
fi

echo -e "\n${GREEN}Architecture build completed successfully!${NC}"

# Set renderer environment variable
case "$RENDERER" in
    "sdl3")
        export KRYON_RENDERER=sdl3
        ;;
    "framebuffer")
        export KRYON_RENDERER=framebuffer
        ;;
    "terminal")
        export KRYON_RENDERER=terminal
        ;;
esac

# Run all examples for the specified frontend and renderer
echo -e "\n${PURPLE}Running ALL $FRONTEND examples with $RENDERER renderer...${NC}"

if [ ! -d "examples/$FRONTEND" ]; then
    echo -e "  ${YELLOW}No examples found for $FRONTEND frontend${NC}"
    exit 1
fi

# Find and run all examples for this frontend
examples_found=0
examples_success=0

for example_file in "examples/$FRONTEND"/*.$FRONTEND; do
    if [ -f "$example_file" ]; then
        examples_found=$((examples_found + 1))
        example_name=$(basename "$example_file" .${FRONTEND})

        echo -e "  ${BLUE}Running: $example_name${NC}"

        # Skip examples that require specific renderers
        case "$example_name" in
            *terminal*)
                if [ "$RENDERER" != "terminal" ]; then
                    echo -e "    ${YELLOW}Skipped (requires terminal renderer)${NC}"
                    continue
                fi
                ;;
        esac

        # Run the example and show output
        echo -e "    ${CYAN}Running: $example_name ($FRONTEND frontend, $RENDERER renderer)${NC}"
        if ./run_example.sh "$example_name" "$FRONTEND" "$RENDERER"; then
            echo -e "    ${GREEN}✓ Success${NC}"
            examples_success=$((examples_success + 1))
        else
            echo -e "    ${RED}✗ Failed${NC}"
        fi
    fi
done

if [ "$examples_found" -eq 0 ]; then
    echo -e "  ${YELLOW}No $FRONTEND examples found${NC}"
    exit 1
fi

echo -e "\n${GREEN}Example tests completed!${NC}"
echo -e "${CYAN}Results: $examples_success/$examples_found examples passed${NC}"

if [ "$examples_success" -eq "$examples_found" ]; then
    echo -e "${GREEN}All examples ran successfully!${NC}"
    exit 0
else
    echo -e "${YELLOW}Some examples failed. Check output above for details.${NC}"
    exit 1
fi