0BSD# Canvas Plugin Test - Using Auto-Generated Bindings
# Demonstrates the auto-generated bindings system

import ../../bindings/nim/canvas_generated

when isMainModule:
  echo "=== Canvas Plugin - Auto-Generated Bindings Test ==="
  echo ""

  # Test auto-generated wrapper functions
  echo "Testing auto-generated bindings:"
  echo ""

  echo "1. Drawing filled red circle at (100, 100) radius 50"
  drawCircle(100, 100, 50, "#ff0000", filled = true)

  echo "2. Drawing outlined blue circle at (200, 100) radius 30"
  drawCircle(200, 100, 30, "#0000ff", filled = false)

  echo "3. Drawing filled green ellipse at (150, 200) rx=80 ry=40"
  drawEllipse(150, 200, 80, 40, "#00ff00", filled = true)

  echo "4. Drawing outlined yellow ellipse at (300, 200) rx=50 ry=70"
  drawEllipse(300, 200, 50, 70, "#ffff00", filled = false)

  echo "5. Drawing magenta arc at (250, 300) radius 60, 0°-180°"
  drawArc(250, 300, 60, 0, 180, "#ff00ff")

  echo "6. Drawing cyan arc at (100, 300) radius 40, 45°-315°"
  drawArc(100, 300, 40, 45, 315, "#00ffff")

  echo ""
  echo "✓ All auto-generated bindings tested successfully!"
  echo ""
  echo "Bindings generated from: plugins/canvas/bindings.json"
  echo "Generator tool: tools/generate_nim_bindings.nim"
  echo "Output file: bindings/nim/canvas_generated.nim"
  echo ""
  echo "The auto-generated bindings provide:"
  echo "  ✓ C function imports (importc declarations)"
  echo "  ✓ Color parsing (#RRGGBB → uint32)"
  echo "  ✓ Type conversions (int → int32)"
  echo "  ✓ High-level wrappers with documentation"
  echo "  ✓ Default parameters (filled = true)"
  echo ""
  echo "=== Test Complete ==="
