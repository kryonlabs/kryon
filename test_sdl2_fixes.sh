#!/bin/bash
# Test script for SDL2 renderer fixes

echo "=== Testing SDL2 Renderer Fixes ==="
echo ""

# Test 1: Font loading
echo "Test 1: Font Loading with Fontconfig"
echo "--------------------------------------"
timeout 5 ./build/bin/kryon run examples/button.kry --renderer sdl2 2>&1 | grep -E "(Loading font|SDL2 initialized|Warning.*font)" || echo "No font messages found"
echo ""

# Test 2: Window closing (will need manual testing, but we can check if it starts)
echo "Test 2: Window Creation and Event Loop"
echo "---------------------------------------"
timeout 3 ./build/bin/kryon run examples/button.kry --renderer sdl2 2>&1 | grep -E "(SDL2 Renderer|Window|Starting render)" || echo "No renderer messages found"
echo ""

echo "=== Tests Complete ==="
echo ""
echo "Manual Testing Required:"
echo "1. Run: ./build/bin/kryon run examples/button.kry --renderer sdl2"
echo "2. Verify text is visible in buttons"
echo "3. Click window close button (X) - should exit immediately"
echo "4. Run again and press Ctrl+C - should exit immediately"
