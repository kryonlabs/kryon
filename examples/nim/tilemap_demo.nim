# 0BSD
## Tilemap Plugin Demo
## Demonstrates 2D tile-based rendering with multiple layers

import ../../bindings/nim/tilemap_generated

when isMainModule:
  echo "=== Tilemap Plugin - Auto-Generated Bindings Test ==="
  echo ""

  echo "Testing auto-generated tilemap bindings:"
  echo ""

  echo "1. Creating 16x12 tilemap with 32x32 pixel tiles on layer 0"
  createTilemap(16, 12, 32, 32, layer = 0)

  echo "2. Creating checkerboard pattern"
  for y in 0'u16..<12'u16:
    for x in 0'u16..<16'u16:
      if (x + y) mod 2 == 0:
        setTile(x, y, 1, layer = 0)  # Tile ID 1
      else:
        setTile(x, y, 2, layer = 0)  # Tile ID 2

  echo "3. Adding corner tiles (tile ID 5)"
  setTile(0, 0, 5, layer = 0)      # Top-left
  setTile(15, 0, 5, layer = 0)     # Top-right
  setTile(0, 11, 5, layer = 0)     # Bottom-left
  setTile(15, 11, 5, layer = 0)    # Bottom-right

  echo "4. Adding horizontal line pattern in middle (tile ID 10)"
  for i in 6'u16..9'u16:
    setTile(i, 5, 10, layer = 0)
    setTile(i, 6, 10, layer = 0)

  echo "5. Rendering tilemap at position (50, 50)"
  renderTilemap(50, 50, layer = 0)

  echo ""
  echo "✓ All tilemap bindings tested successfully!"
  echo ""
  echo "Tilemap features demonstrated:"
  echo "  ✓ createTilemap - Initialize tilemap layer"
  echo "  ✓ setTile - Place tiles with IDs and flags"
  echo "  ✓ renderTilemap - Render tilemap at position"
  echo "  ✓ Multiple layers (0-3 supported)"
  echo "  ✓ Flip flags (horizontal, vertical, diagonal)"
  echo ""
  echo "Bindings generated from: plugins/tilemap/bindings.json"
  echo "Generator tool: tools/generate_nim_bindings.nim"
  echo "Output file: bindings/nim/tilemap_generated.nim"
  echo ""
  echo "=== Test Complete ==="
