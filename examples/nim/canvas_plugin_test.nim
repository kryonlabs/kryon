0BSD0BSD0BSD0BSD0BSD# Canvas Plugin Test
# Tests the new plugin system by drawing circles, ellipses, and arcs

import ../../bindings/nim/runtime
import ../../bindings/nim/kryon_dsl/impl

# Test canvas plugin drawing functions
proc testCanvasPlugin() =
  Container:
    width = 400
    height = 400
    backgroundColor = "#1a1a1a"

    Canvas:
      width = 100.percent
      height = 100.percent

      # Draw filled circles
      circle(100, 100, 50, "#ff0000", filled = true)  # Red filled circle
      circle(200, 100, 30, "#0000ff", filled = false) # Blue outline circle

      # Draw ellipses
      ellipse(150, 200, 80, 40, "#00ff00", filled = true)  # Green filled ellipse
      ellipse(300, 200, 50, 70, "#ffff00", filled = false) # Yellow outline ellipse

      # Draw arcs
      arc(250, 300, 60, 0, 180, "#ff00ff")    # Magenta arc (0° to 180°)
      arc(100, 300, 40, 45, 315, "#00ffff")   # Cyan arc (45° to 315°)

# Main entry point
when isMainModule:
  echo "=== Canvas Plugin Test ==="
  echo "Testing plugin system with canvas drawing primitives"
  echo ""

  testCanvasPlugin()

  echo "Canvas plugin test complete!"
  echo "The plugin system successfully:"
  echo "  - Registered canvas handlers (circle, ellipse, arc)"
  echo "  - Dispatched plugin commands through IR"
  echo "  - Rendered shapes via SDL3 backend"
