#!/bin/bash

# Build and Test Script for Kryon Examples with New Architecture
# This script builds all frontends with multiple renderers and runs them

set -e  # Exit on any error (but not on Ctrl+C during example runs)

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

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Kryon Examples Build & Test Script${NC}"
echo -e "${BLUE}       (New Architecture v1.2)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to build Kryon architecture
build_kryon_architecture() {
    echo -e "${BLUE}Building Kryon architecture...${NC}"

    echo "  Building C core..."
    if make -C core > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ C core built${NC}"
    else
        echo -e "  ${RED}✗ C core build failed${NC}"
        return 1
    fi

    echo "  Building common utilities..."
    if make -C renderers/common install > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ Common utilities built${NC}"
    else
        echo -e "  ${RED}✗ Common utilities build failed${NC}"
        return 1
    fi

    echo "  Building framebuffer renderer..."
    if make -C renderers/framebuffer install > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ Framebuffer renderer built${NC}"
    else
        echo -e "  ${RED}✗ Framebuffer renderer build failed${NC}"
        return 1
    fi

    echo "  Building SDL3 renderer..."
    if make -C renderers/sdl3 > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ SDL3 renderer built${NC}"
    else
        echo -e "  ${YELLOW}⚠ SDL3 renderer not available (optional)${NC}"
    fi

    echo "  Building terminal renderer..."
    if make -C renderers/terminal install > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ Terminal renderer built${NC}"
    else
        echo -e "  ${YELLOW}⚠ Terminal renderer not available (optional)${NC}"
    fi

    echo "  Building Linux platform..."
    if make -C platforms/linux > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓ Linux platform built${NC}"
    else
        echo -e "  ${YELLOW}⚠ Linux platform build failed (optional)${NC}"
    fi

    return 0
}

# Function to build and run an example
build_and_run_example() {
    local example_file="$1"
    local frontend="$2"
    local renderer="$3"

    if [ ! -f "$example_file" ]; then
        return 1
    fi

    local example_name=$(basename "$example_file")
    example_name="${example_name%.*}"

    echo -e "${PURPLE}Building ${CYAN}$example_name${NC} (${frontend} frontend, ${renderer} renderer)..."

    case "$frontend" in
        "nim")
            # Build Nim example
            # Determine libraries based on renderer
            case "$renderer" in
                "terminal")
                    LINK_LIBS="-Lbuild -lkryon_core -lkryon_terminal -lkryon_common -lutil -ltickit"
                    INCLUDE_FLAGS="--passC:\"-Icore/include\" --passC:\"-Irenderers/terminal\""
                    ;;
                "sdl3")
                    LINK_LIBS="-Lbuild -lkryon_core -lkryon_sdl3 -lkryon_fb -lkryon_common -lSDL3 -lSDL3_ttf"
                    INCLUDE_FLAGS="--passC:\"-Icore/include\" --passC:\"-Irenderers/sdl3\""
                    ;;
                *)
                    LINK_LIBS="-Lbuild -lkryon_core -lkryon_fb -lkryon_common"
                    INCLUDE_FLAGS="--passC:\"-Icore/include\""
                    ;;
            esac

            if nim c -r --path:bindings/nim \
                --define:KRYON_PLATFORM_DESKTOP \
                $INCLUDE_FLAGS \
                --passL:"$LINK_LIBS" \
                --threads:on --gc:arc \
                -o:"$BUILD_DIR/${example_name}_${frontend}_${renderer}" \
                "$example_file"; then
                echo -e "  ${GREEN}✓ Nim compilation successful${NC}"
                echo -e "${YELLOW}Running example (press Ctrl+C to exit)...${NC}"
                "$BUILD_DIR/${example_name}_${frontend}_${renderer}"
                echo -e "${GREEN}✓ Example completed${NC}"
            else
                echo -e "  ${RED}✗ Nim compilation failed${NC}"
                return 1
            fi
            ;;
        "lua")
            # Build Lua example
            if gcc -shared -fPIC -Icore/include -Lbuild -lkryon_core -lkryon_fb -lkryon_common \
                -o "$BUILD_DIR/kryon_lua.so" -lm "$example_file" > /dev/null 2>&1; then
                echo -e "  ${GREEN}✓ Lua compilation successful${NC}"
                if lua5.4 -l"$BUILD_DIR/kryon_lua.so" "$example_file"; then
                    echo -e "  ${GREEN}✓ Example completed${NC}"
                else
                    echo -e "  ${RED}✗ Lua runtime failed${NC}"
                    return 1
                fi
            else
                echo -e "  ${RED}✗ Lua compilation failed${NC}"
                return 1
            fi
            ;;
        "c")
            # Build C example
            if gcc -Icore/include -Lbuild -lkryon_core -lkryon_fb -lkryon_common \
                "$example_file" -o "$BUILD_DIR/${example_name}_${frontend}_${renderer}" -lm; then
                echo -e "  ${GREEN}✓ C compilation successful${NC}"
                echo -e "${YELLOW}Running example (press Ctrl+C to exit)...${NC}"
                "$BUILD_DIR/${example_name}_${frontend}_${renderer}"
                echo -e "  ${GREEN}✓ Example completed${NC}"
            else
                echo -e "  ${RED}✗ C compilation failed${NC}"
                return 1
            fi
            ;;
        *)
            echo -e "  ${RED}✗ Unknown frontend: $frontend${NC}"
            return 1
            ;;
    esac

    return 0
}

# Get all example files from frontend directories
examples=()
for frontend in nim lua c; do
    for file in examples/$frontend/*.$frontend; do
        if [ -f "$file" ]; then
            examples+=("$file")
        fi
    done
done

if [ ${#examples[@]} -eq 0 ]; then
    echo -e "${RED}No example files found in examples/ directory${NC}"
    exit 1
fi

echo -e "${CYAN}Found ${#examples[@]} examples to test:${NC}"
for example in "${examples[@]}"; do
    echo -e "  - $(basename "$example")"
done
echo ""

# Phase 1: Build Kryon architecture
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PHASE 1: BUILDING KRYON ARCHITECTURE${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if build_kryon_architecture; then
    echo -e "${GREEN}✓ Architecture ready for examples${NC}"
else
    echo -e "${RED}✗ Failed to build architecture - cannot continue${NC}"
    exit 1
fi

echo ""

# Phase 2: Build and test examples
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PHASE 2: BUILDING AND TESTING EXAMPLES${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Test configurations: frontend, renderer
test_configs=(
    "nim framebuffer"
    "nim sdl3"
    "nim terminal"
    "lua framebuffer"
    "c framebuffer"
)

failed_examples=()
successful_examples=()

for example_file in "${examples[@]}"; do
    example_name=$(basename "$example_file")
    example_name="${example_name%.*}"

    echo -e "${YELLOW}Processing: $example_name${NC}"

    example_success=false
    for config in "${test_configs[@]}"; do
        frontend=$(echo "$config" | cut -d' ' -f1)
        renderer=$(echo "$config" | cut -d' ' -f2)

        # Skip Lua/C for certain files if they don't exist
        if [ "$frontend" != "nim" ] && [ ! -f "examples/${frontend}/${example_name}.${frontend}" ]; then
            continue
        fi

        # Use the correct file path for the frontend
        if [ "$frontend" = "nim" ]; then
            frontend_file="examples/nim/${example_name}.nim"
        elif [ "$frontend" = "lua" ] && [ -f "examples/lua/${example_name}.lua" ]; then
            frontend_file="examples/lua/${example_name}.lua"
        elif [ "$frontend" = "c" ] && [ -f "examples/c/${example_name}.c" ]; then
            frontend_file="examples/c/${example_name}.c"
        else
            frontend_file="$example_file"  # fallback to original
        fi

        if build_and_run_example "$frontend_file" "$frontend" "$renderer"; then
            successful_examples+=("$example_name ($frontend+$renderer)")
            example_success=true
        else
            failed_examples+=("$example_name ($frontend+$renderer)")
        fi

        # Don't test all configurations for every file to keep it reasonable
        break
    done
done

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  SUMMARY${NC}"
echo -e "${BLUE}========================================${NC}"

echo -e "${GREEN}Successfully tested: ${#successful_examples[@]}${NC}"
for name in "${successful_examples[@]}"; do
    echo -e "  ${GREEN}✓${NC} $name"
done

if [ ${#failed_examples[@]} -gt 0 ]; then
    echo -e "${RED}Failed: ${#failed_examples[@]}${NC}"
    for name in "${failed_examples[@]}"; do
        echo -e "  ${RED}✗${NC} $name"
    fi
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  All examples processed with new architecture!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${CYAN}Build artifacts located in: $BUILD_DIR${NC}"
echo -e "${CYAN}Use individual run_example.sh for specific testing:${NC}"
echo -e "${CYAN}  ./run_example.sh <example> [nim|lua|c] [sdl3|framebuffer|terminal]${NC}"