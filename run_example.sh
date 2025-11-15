#!/usr/bin/env bash

# run_example.sh - Run individual Kryon examples with new architecture
# Usage: ./run_example.sh <example_name> [frontend] [renderer]
# Example: ./run_example.sh hello_world nim sdl3
# Default: ./run_example.sh hello_world nim sdl3

set -e

BUILD_DIR="build"
BIN_DIR="bin"

ensure_core_build() {
    local core_lib="$BUILD_DIR/libkryon_core.a"
    local need_core=0

    if [ ! -f "$core_lib" ]; then
        need_core=1
    else
        local newer_sources
        newer_sources="$(find core -maxdepth 1 -name '*.c' -newer "$core_lib" -print -quit)"
        if [ -n "$newer_sources" ]; then
            need_core=1
        fi
    fi

    if [ $need_core -eq 1 ]; then
        echo "Building Kryon architecture..."
        make -C core > /dev/null
    fi

    local common_lib="$BUILD_DIR/libkryon_common.a"
    if [ ! -f "$common_lib" ]; then
        echo "Installing renderer helpers..."
        make -C renderers/common install > /dev/null
    fi
}

PROJECT_ROOT="$(pwd)"

# Check if example name is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <example_name|number> [frontend] [renderer]"
    echo ""
    echo "Frontends: nim, lua, c"
    echo "Renderers: sdl3, framebuffer, terminal"
    echo ""
    echo "Available examples (alphabetical order):"
    INDEX=1
    for frontend in nim lua c; do
        if [ -d "examples/$frontend" ]; then
            for file in "examples/$frontend"/*.$frontend; do
                if [ -f "$file" ]; then
                    printf "  %2d. %-30s (%s frontend)\n" "$INDEX" "$(basename "$file" .${frontend})" "$frontend"
                    INDEX=$((INDEX + 1))
                fi
            done
        fi
    done
    echo ""
    echo "Usage examples:"
    echo "  ./run_example.sh button_demo          # By name"
    echo "  ./run_example.sh 1                     # By number"
    echo "  ./run_example.sh 1 nim sdl3            # Specify all options"
    echo ""
    echo "Terminal examples:"
    echo "  button_demo_terminal - Interactive button demo (nim frontend)"
    exit 1
fi

INPUT="$1"
FRONTEND="${2:-nim}"
RENDERER="${3:-sdl3}"

# Check if input is a number
if [[ "$INPUT" =~ ^[0-9]+$ ]]; then
    # User provided a number, find the corresponding example
    INDEX=1
    FOUND=0
    for frontend in nim lua c; do
        if [ -d "examples/$frontend" ]; then
            for file in "examples/$frontend"/*.$frontend; do
                if [ -f "$file" ]; then
                    if [ $INDEX -eq $INPUT ]; then
                        EXAMPLE_NAME=$(basename "$file" ."$frontend")
                        # Auto-detect frontend from the file
                        FRONTEND="$frontend"
                        FOUND=1
                        break 2
                    fi
                    INDEX=$((INDEX + 1))
                fi
            done
        fi
    done

    if [ $FOUND -eq 0 ]; then
        echo "Error: Invalid example number $INPUT"
        echo "Run '$0' without arguments to see available examples"
        exit 1
    fi
else
    # User provided a name
    EXAMPLE_NAME="$INPUT"
fi

# Handle case where user passes full path like "examples/nim/foo.nim"
# Extract just the basename without extension
if [[ "$EXAMPLE_NAME" == */* ]]; then
    # Extract the basename first (removes directory path)
    EXAMPLE_NAME=$(basename "$EXAMPLE_NAME")
fi

# Now remove the extension
if [[ "$EXAMPLE_NAME" == *.nim ]]; then
    EXAMPLE_NAME="${EXAMPLE_NAME%.nim}"
elif [[ "$EXAMPLE_NAME" == *.lua ]]; then
    EXAMPLE_NAME="${EXAMPLE_NAME%.lua}"
elif [[ "$EXAMPLE_NAME" == *.c ]]; then
    EXAMPLE_NAME="${EXAMPLE_NAME%.c}"
fi

# Build C core libraries if needed
mkdir -p "$BUILD_DIR"
mkdir -p "$BIN_DIR"
ensure_core_build

# Color variables
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

FONTCONFIG_LIBS="$(pkg-config --libs fontconfig 2>/dev/null || true)"

# Find example file based on frontend directory structure
EXAMPLE_FILE="examples/${FRONTEND}/${EXAMPLE_NAME}.${FRONTEND}"

# Build renderer if needed
case "$RENDERER" in
    "sdl3")
        echo "Building SDL3 renderer..."
        if make -C renderers/sdl3 > /dev/null 2>&1; then
            make -C renderers/framebuffer install > /dev/null 2>&1
            echo "SDL3 renderer built successfully"
        else
            echo "Error: SDL3 renderer not available. Install SDL3 development libraries:"
            echo "  sudo apt install libsdl3-dev libsdl3-ttf-dev"
            echo "  or ensure SDL3 is properly installed on your system"
            exit 1
        fi
        ;;
    "framebuffer")
        echo "Building framebuffer renderer..."
        make -C renderers/framebuffer install > /dev/null 2>&1
        ;;
    "terminal")
        echo "Building terminal renderer..."
        if make -C renderers/terminal install > /dev/null 2>&1; then
            echo "Terminal renderer built successfully"
        else
            echo "Error: Terminal renderer build failed"
            exit 1
        fi
        ;;
    *)
        echo "Error: Unknown renderer '$RENDERER'. Use sdl3, framebuffer, or terminal"
        exit 1
        ;;
esac

echo "Running example: $EXAMPLE_NAME"
echo "Frontend: $FRONTEND"
echo "Renderer: $RENDERER"
echo "File: $EXAMPLE_FILE"
echo "────────────────────────────────────────────────────────────"

# Determine linker libraries based on renderer
case "$RENDERER" in
    "sdl3")
        LINK_LIBS="-Lbuild -lkryon_core -lkryon_sdl3 -lkryon_fb -lkryon_common -lSDL3 -lSDL3_ttf -lm"
        if [ -n "$FONTCONFIG_LIBS" ]; then
            LINK_LIBS="$LINK_LIBS $FONTCONFIG_LIBS"
        fi
        ;;
    "framebuffer")
        LINK_LIBS="-Lbuild -lkryon_core -lkryon_fb -lkryon_common"
        ;;
    "terminal")
        LINK_LIBS="-Lbuild -lkryon_core -lkryon_terminal -lkryon_common -lutil -ltickit"
        ;;
    *)
        LINK_LIBS="-Lbuild -lkryon_core -lkryon_fb -lkryon_common"
        ;;
esac

# Build and run based on frontend
case "$FRONTEND" in
    "nim")
        echo "Building and running Nim example..."

        # Set up environment for renderer mode
        if [ "$RENDERER" = "terminal" ]; then
            export KRYON_RENDERER=terminal
            echo -e "${CYAN}Terminal mode enabled${NC}"
        elif [ "$RENDERER" = "sdl3" ]; then
            echo -e "${CYAN}SDL3 mode enabled${NC}"
        fi

        # Build and run separately for better error handling
        echo "Compiling with new architecture bindings..."
        # Add appropriate include paths based on renderer
        INCLUDE_PATHS="--passC:\"-Icore/include\" --passC:\"-I${PROJECT_ROOT}\" --passC:\"-DKRYON_TARGET_PLATFORM=0\" --passC:\"-DKRYON_NO_FLOAT=0\""
        NIM_FLAGS="--threads:on --mm:arc"

        if [ "$RENDERER" = "terminal" ]; then
            INCLUDE_PATHS="$INCLUDE_PATHS --passC:\"-I${PROJECT_ROOT}/renderers/terminal\""
            NIM_FLAGS="$NIM_FLAGS -d:KRYON_TERMINAL"
        elif [ "$RENDERER" = "sdl3" ]; then
            INCLUDE_PATHS="$INCLUDE_PATHS --passC:\"-I${PROJECT_ROOT}/renderers/sdl3\""
            NIM_FLAGS="$NIM_FLAGS -d:KRYON_SDL3"
        fi

        # Capture compilation output for error analysis
        COMPILATION_LOG="$BUILD_DIR/compilation.log"

        # Remove old binary (but keep nimcache for faster recompilation)
        rm -f "$BIN_DIR/${EXAMPLE_NAME}"

        if nim c \
            -d:release \
            --path:bindings/nim \
            $INCLUDE_PATHS \
            --passL:"$LINK_LIBS" \
            $NIM_FLAGS \
            --nimcache:"$BUILD_DIR/nimcache" \
            --outdir:"$BIN_DIR" \
            "$EXAMPLE_FILE" 2>&1 | tee "$COMPILATION_LOG"; then
            echo -e "${GREEN}✓ Compilation successful${NC}"
            echo -e "${YELLOW}Running example (press Ctrl+C to exit)...${NC}"
            LD_LIBRARY_PATH="${PROJECT_ROOT}/build:$LD_LIBRARY_PATH" "$BIN_DIR/${EXAMPLE_NAME}" || true
        else
            # Analyze the compilation error and provide helpful message
            if grep -q "undefined reference" "$COMPILATION_LOG"; then
                echo -e "${RED}✗ Linking error - missing libraries or symbols${NC}"
                echo -e "${YELLOW}This is usually caused by:${NC}"
                echo -e "  • Missing dependencies (try: sudo apt install libtickit-dev)"
                echo -e "  • Incorrect library linking order"
                if [ "$RENDERER" = "terminal" ]; then
                    echo -e "  • Terminal libraries not found (libtickit, libutil)"
                fi
            elif grep -q "Error.*cannot find" "$COMPILATION_LOG" || grep -q "fatal error" "$COMPILATION_LOG"; then
                echo -e "${RED}✗ Header file not found${NC}"
                echo -e "${YELLOW}This is usually caused by:${NC}"
                echo -e "  • Missing include paths"
                echo -e "  • Renderer headers not built"
                if [ "$RENDERER" = "terminal" ]; then
                    echo -e "  • Terminal backend not properly installed"
                fi
            elif grep -q "Error" "$COMPILATION_LOG"; then
                echo -e "${RED}✗ Compilation error in the example code${NC}"
                echo -e "${YELLOW}The example may need updates for the current architecture${NC}"
            else
                echo -e "${RED}✗ Build failed${NC}"
                echo -e "${YELLOW}Check the compilation log for details${NC}"
            fi

            echo -e "${CYAN}Full compilation log saved to: $COMPILATION_LOG${NC}"
        fi
        echo -e "${GREEN}✓ Example processing complete${NC}"
        ;;
    "lua")
        echo -e "${CYAN}Lua frontend not yet implemented for new architecture${NC}"
        echo -e "${YELLOW}Lua bindings need to be generated from C core headers${NC}"
        ;;
    "c")
        echo -e "${CYAN}C frontend not yet implemented for new architecture${NC}"
        echo -e "${YELLOW}C examples need to use the kryon.h header directly${NC}"
        ;;
esac
