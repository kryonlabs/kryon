0BSD0BSD0BSD# 0BSD
## Tilemap Plugin Demo
## Demonstrates 2D tile-based rendering with multiple layers

import ../../bindings/nim/runtime
import ../../bindings/nim/kryon_dsl/impl
import ../../bindings/nim/tilemap_generated

# Create app state with tilemap position
var mapX = initReactiveState(0)
var mapY = initReactiveState(0)

proc createApp(): IRComponent =
  Container:
    width = 100.percent
    height = 100.percent
    layoutDirection = 1  # Column
    backgroundColor = 0x2d3436ff'u32

    # Title
    Container:
      height = 60.fixed
      padding = 10.fixed
      backgroundColor = 0x34495eff'u32
      Text:
        content = "Tilemap Plugin Demo"
        fontSize = 24
        color = 0xecf0f1ff'u32

    # Instructions
    Container:
      height = 100.fixed
      padding = 15.fixed
      backgroundColor = 0x2c3e50ff'u32
      Container:
        layoutDirection = 1  # Column
        gap = 5.fixed
        Text:
          content = "Arrow Keys: Move tilemap"
          fontSize = 16
          color = 0xbdc3c7ff'u32
        Text:
          content = "Multi-layer tilemap with colored tiles"
          fontSize = 14
          color = 0x95a5a6ff'u32

    # Tilemap canvas container
    Container:
      width = 100.percent
      height = 100.percent
      backgroundColor = 0x1e272eff'u32

      # Custom canvas rendering via callback
      Canvas:
        width = 100.percent
        height = 100.percent
        onRender = proc() =
          # Create a 16x12 tilemap with 32x32 pixel tiles on layer 0
          createTilemap(16, 12, 32, 32, layer = 0)

          # Create a simple pattern
          # Background layer - checkerboard pattern
          for y in 0'u16..<12'u16:
            for x in 0'u16..<16'u16:
              if (x + y) mod 2 == 0:
                setTile(x, y, 1, layer = 0)  # Tile ID 1 (reddish)
              else:
                setTile(x, y, 2, layer = 0)  # Tile ID 2 (greenish)

          # Add some special tiles
          setTile(0, 0, 5, layer = 0)      # Top-left corner
          setTile(15, 0, 5, layer = 0)     # Top-right corner
          setTile(0, 11, 5, layer = 0)     # Bottom-left corner
          setTile(15, 11, 5, layer = 0)    # Bottom-right corner

          # Add a pattern in the middle
          for i in 6'u16..9'u16:
            setTile(i, 5, 10, layer = 0)   # Horizontal line
            setTile(i, 6, 10, layer = 0)

          # Render the tilemap at current position
          renderTilemap(mapX.value.int32, mapY.value.int32, layer = 0)

proc handleKeyPress(keyCode: int32) =
  case keyCode:
  of 79:  # Right arrow
    mapX.value += 10
  of 80:  # Left arrow
    mapX.value -= 10
  of 81:  # Down arrow
    mapY.value += 10
  of 82:  # Up arrow
    mapY.value -= 10
  else:
    discard

proc main() =
  echo "Starting tilemap demo..."

  # Register key handler
  registerKeyHandler(handleKeyPress)

  # Create and run app
  let app = createApp()
  runApp(app, 800, 600, "Tilemap Plugin Demo")

when isMainModule:
  main()
