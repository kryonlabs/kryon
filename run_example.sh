#!/usr/bin/env bash

# run_example.sh - Run individual Kryon examples
# Usage: ./run_example.sh <example_name> [renderer]
# Example: ./run_example.sh todo raylib

set -e

# Check if example name is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <example_name> [renderer]"
    echo ""
    echo "Available examples:"
    for file in examples/*.nim; do
        if [ -f "$file" ]; then
            basename "$file" .nim
        fi
    done
    echo ""
    echo "Available renderers: raylib, html, sdl2"
    echo "Default renderer: raylib"
    exit 1
fi

EXAMPLE_NAME="$1"
RENDERER="${2:-raylib}"
EXAMPLE_FILE="examples/${EXAMPLE_NAME}.nim"

# Check if example exists
if [ ! -f "$EXAMPLE_FILE" ]; then
    echo "Error: Example '$EXAMPLE_NAME' not found (looking for $EXAMPLE_FILE)"
    echo ""
    echo "Available examples:"
    for file in examples/*.nim; do
        if [ -f "$file" ]; then
            basename "$file" .nim
        fi
    done
    exit 1
fi

# Check if kryon binary exists
if [ ! -f "./bin/cli/kryon" ]; then
    echo "Error: kryon binary not found. Building first..."
    nimble build
fi

echo "Running example: $EXAMPLE_NAME"
echo "Renderer: $RENDERER"
echo "File: $EXAMPLE_FILE"
echo "────────────────────────────────────────────────────────────"

# Run the example
./bin/cli/kryon run --filename "$EXAMPLE_FILE" --renderer "$RENDERER"