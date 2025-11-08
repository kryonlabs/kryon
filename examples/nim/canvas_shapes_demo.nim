## Canvas Shapes Demo - Love2D Style API
##
## Demonstrates the new Love2D-inspired canvas API with all basic shapes
## Shows immediate-mode drawing, transforms, colors, and advanced features

import std/[math, random]
import kryon_dsl
import canvas

# Animation state
var
  rotation = 0.0
  scale = 1.0
  time = 0.0
  randomPoints: seq[Point]
  polygonSides = 6
  starPoints = 5

# Initialize random points for scatter plot
randomize()
for i in 0..<50:
  randomPoints.add((x: rand(800.0), y: rand(600.0)))

# Drawing procedure using Love2D-style immediate mode API
proc drawCanvasDemo() =
  # Clear canvas with a nice background
  background(25, 25, 35)  # Dark blue-gray background

  # Update animation state
  time += 0.016  # ~60fps timing
  rotation = time * 30.0  # 30 degrees per second
  scale = 1.0 + sin(time) * 0.3  # Breathing effect

  # ============================================================================
  # Basic Shapes Demo
  # ============================================================================

  # Title text
  fill(255, 255, 255)  # White text
  print("Love2D-Style Canvas API Demo", 20, 30)
  print("Basic Shapes & Transforms", 20, 50)

  # Rectangle section
  print("Rectangles:", 20, 90)

  # Filled rectangle
  fill(255, 100, 100, 200)  # Semi-transparent red
  rectangle(Fill, 20, 110, 80, 60)

  # Outline rectangle
  stroke(100, 255, 100, 255)  # Green outline
  strokeWeight(3)
  rectangle(Line, 110, 110, 80, 60)

  # Rounded rectangle effect (using multiple rectangles)
  fill(100, 100, 255, 200)  # Semi-transparent blue
  rectangle(Fill, 200, 110, 80, 60)

  # Circle section
  print("Circles:", 20, 190)

  # Filled circle
  fill(255, 200, 0, 255)  # Yellow
  circle(Fill, 60, 250, 30)

  # Outline circle
  stroke(0, 255, 255, 255)  # Cyan outline
  strokeWeight(2)
  circle(Line, 130, 250, 30)

  # Circle with thick stroke
  stroke(255, 0, 255, 255)  # Magenta outline
  strokeWeight(4)
  circle(Line, 200, 250, 25)

  # Ellipse section
  print("Ellipses:", 20, 310)

  # Filled ellipse
  fill(255, 150, 0, 200)  # Orange
  ellipse(Fill, 60, 350, 40, 25)

  # Outline ellipse
  stroke(150, 255, 0, 255)  # Yellow-green outline
  strokeWeight(2)
  ellipse(Line, 130, 350, 40, 25)

  # Rotated ellipse
  withTransform:
    translate(200, 350)
    rotate(45)  # 45 degrees
    ellipse(Fill, 0, 0, 30, 15)

  # ============================================================================
  # Advanced Shapes Demo
  # ============================================================================

  # Polygon section
  print("Polygons:", 350, 90)

  # Hexagon (filled)
  var hexagonVertices: seq[Point]
  for i in 0..<polygonSides:
    let angle = float(i) * 360.0 / float(polygonSides)
    hexagonVertices.add((
      x: cos(radians(angle)) * 40,
      y: sin(radians(angle)) * 40
    ))

  fill(255, 100, 200, 200)  # Pink
  withTransform:
    translate(430, 140)
    polygon(Fill, hexagonVertices)

  # Pentagon (outline)
  var pentagonVertices: seq[Point]
  for i in 0..<5:
    let angle = float(i) * 360.0 / 5.0 - 90  # Start from top
    pentagonVertices.add((
      x: cos(radians(angle)) * 35,
      y: sin(radians(angle)) * 35
    ))

  stroke(100, 200, 255, 255)  # Light blue outline
  strokeWeight(2)
  withTransform:
    translate(520, 140)
    polygon(Line, pentagonVertices)

  # Lines section
  print("Lines:", 350, 210)

  # Simple line
  stroke(255, 255, 100, 255)  # Light yellow
  strokeWeight(1)
  line(350, 230, 410, 250)

  # Thick line
  strokeWeight(4)
  line(420, 230, 480, 250)

  # Polyline (connected lines)
  strokeWeight(2)
  stroke(200, 255, 200, 255)  # Light green
  let linePoints = [(x: 350.0, y: 270.0), (x: 380.0, y: 290.0),
                   (x: 420.0, y: 280.0), (x: 460.0, y: 300.0), (x: 490.0, y: 270.0)]
  line(linePoints)

  # Points section
  print("Points:", 350, 330)

  # Random scatter plot
  fill(255, 255, 255, 255)  # White points
  for point in randomPoints[0..<20]:
    point(point.x, point.y)

  # Colored points
  for i in 0..<10:
    let color = color(rand(255), rand(255), rand(255))
    withColor(color):
      point(randomPoints[i + 20])

  # ============================================================================
  # Transform Demo
  # ============================================================================

  print("Transforms:", 600, 90)

  # Rotating square
  withTransform:
    translate(680, 140)
    rotate(rotation)
    scale(scale)

    # Draw rotating square
    fill(255, 100, 150, 200)  # Semi-transparent pink
    rectangle(Fill, -30, -30, 60, 60)

    # Draw outline
    stroke(255, 255, 255, 255)  # White outline
    strokeWeight(2)
    rectangle(Line, -30, -30, 60, 60)

  # Transform stack demo
  withTransform:
    translate(680, 250)

    # First transform
    push()
    rotate(rotation / 2)
    fill(100, 255, 100, 200)  # Green
    rectangle(Fill, -20, -20, 40, 40)
    pop()

    # Second transform (independent)
    push()
    translate(50, 0)
    rotate(-rotation / 2)
    fill(100, 100, 255, 200)  # Blue
    circle(Fill, 0, 0, 20)
    pop()

  # ============================================================================
  # Advanced Features Demo
  # ============================================================================

  print("Special:", 600, 320)

  # Arc demonstration
  stroke(255, 200, 100, 255)  # Orange outline
  strokeWeight(3)
  arc(Line, 650, 350, 30, 0, PI * 1.5)  # 3/4 circle

  # Heart shape
  drawHeart(680, 450, 40)

  # Star shape
  drawStar(750, 450, 25, starPoints)

  # ============================================================================
  # Color and Style Demo
  # ============================================================================

  print("Colors & Styles:", 20, 420)

  # Color palette
  let colors = [
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
    COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE, COLOR_PURPLE
  ]

  for i, color in colors:
    withColor(color):
      rectangle(Fill, 20 + i * 60, 440, 50, 30)

  # Line styles (if supported)
  print("Line Widths:", 20, 490)

  for i in 1..5:
    strokeWeight(i)
    stroke(255, 255, 255, 255)
    line(20 + i * 60, 510, 20 + i * 60, 530)

  # ============================================================================
  # Performance Demo
  # ============================================================================

  print("Performance:", 350, 420)

  # Lots of small shapes to test performance
  fill(100, 255, 200, 150)  # Semi-transparent green
  for i in 0..<20:
    for j in 0..<10:
      let x = 350 + i * 15
      let y = 440 + j * 10
      let size = 5 + sin(time + i * 0.5) * 3
      circle(Fill, x, y, size)

# Main application setup
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Kryon Canvas - Love2D Style API Demo"

  Body:
    backgroundColor = "#191919"  # Dark background

    Container:
      width = 800
      height = 600
      backgroundColor = "#0A0F1E"

      Canvas:
        width = 800
        height = 600
        backgroundColor = "#0A0F1E"
        onDraw = drawCanvasDemo  # No ctx parameter needed!

echo "Love2D-Style Canvas Demo Started!"
echo "Features demonstrated:"
echo "- Basic shapes: rectangles, circles, ellipses"
echo "- Advanced shapes: polygons, lines, points"
echo "- Transform system: translate, rotate, scale"
echo "- Color management and styles"
echo "- Special shapes: hearts, stars"
echo "- Performance with many shapes"
echo ""
echo "Animation running - shapes should rotate and scale smoothly!"