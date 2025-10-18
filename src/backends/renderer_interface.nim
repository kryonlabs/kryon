## Renderer Interface
##
## Defines the common interface that all rendering backends must implement.
## This allows different renderers (Raylib, SDL2, Skia) to be used interchangeably.

import ../kryon/core

# ============================================================================
# Renderer Interface - 7 Core Drawing Primitives
# ============================================================================

##[
  All rendering backends must implement these 7 core primitives:

  1. **drawRectangle** - Draw a filled rectangle
  2. **drawRectangleBorder** - Draw a rectangle outline/border
  3. **drawText** - Draw text at a position
  4. **drawLine** - Draw a line between two points
  5. **beginClipping** - Start a clipping region (scissor)
  6. **endClipping** - End the current clipping region
  7. **measureText** - Measure text dimensions for layout
]##

# Note: Nim doesn't have interfaces/traits like Rust or Go, but we can document
# the expected signature for each backend's renderer implementation.

# Example signatures (for documentation):
#
# proc drawRectangle*(backend: var Backend, x, y, width, height: float, color: Color)
# proc drawRectangleBorder*(backend: var Backend, x, y, width, height, borderWidth: float, color: Color)
# proc drawText*(backend: var Backend, text: string, x, y: float, fontSize: int, color: Color)
# proc drawLine*(backend: var Backend, x1, y1, x2, y2, thickness: float, color: Color)
# proc beginClipping*(backend: var Backend, x, y, width, height: float)
# proc endClipping*(backend: var Backend)
# proc measureText*(backend: Backend, text: string, fontSize: int): tuple[width: float, height: float]
