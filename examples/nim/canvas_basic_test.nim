## Kryon Canvas - Love2D Style Demo
##
## A comprehensive example showing the Love2D-inspired canvas API
## - Immediate mode drawing
## - Colors and fills
## - Basic shapes (rectangles, circles, ellipses, polygons)
## - Lines and curves
## - Text rendering
## - Simple animation

import std/math
import kryon_dsl
import canvas

# Animation state
var rotation = 0.0
var pulseTime = 0.0

# Main drawing function - called each frame
proc draw() =
  # Clear with dark blue background
  background(30, 40, 60)

  # === SECTION 1: Basic Shapes ===

  # Red rectangle
  fill(231, 76, 60)
  rect(50, 50, 100, 80)

  # Blue circle
  fill(52, 152, 219)
  circle(Fill, 200, 90, 40)

  # Green ellipse
  fill(46, 204, 113)
  ellipse(Fill, 350, 90, 50, 35)

  # Yellow triangle (polygon)
  fill(241, 196, 15)
  let triangle = [
    (x: 480.0, y: 120.0),
    (x: 440.0, y: 50.0),
    (x: 520.0, y: 50.0)
  ]
  polygon(Fill, triangle)

  # === SECTION 2: Stroked Shapes ===

  stroke(231, 76, 60)
  strokeWeight(3)
  rectangle(Line, 50, 180, 100, 80)

  stroke(52, 152, 219)
  strokeWeight(2)
  circle(Line, 200, 220, 40)

  # === SECTION 3: Lines and Curves ===

  # Straight line
  stroke(155, 89, 182)
  strokeWeight(4)
  line(50, 320, 150, 320)

  # Connected line segments
  stroke(230, 126, 34)
  strokeWeight(2)
  let zigzag = [
    (x: 200.0, y: 300.0),
    (x: 250.0, y: 320.0),
    (x: 300.0, y: 300.0),
    (x: 350.0, y: 320.0)
  ]
  line(zigzag)

  # Sine wave
  stroke(26, 188, 156)
  strokeWeight(2)
  var wave: seq[Point] = @[]
  for i in 0..50:
    let x = 400.0 + i * 3.0
    let y = 310.0 + sin(0.2 * float(i) + pulseTime) * 15.0
    wave.add((x, y))
  line(wave)

  # === SECTION 4: Arcs ===

  # Half circle (arc)
  fill(192, 57, 43)
  arc(Fill, 100, 420, 30, 0, PI)

  # Quarter circle outline
  stroke(41, 128, 185)
  strokeWeight(3)
  arc(Line, 200, 420, 30, 0, PI / 2)

  # === SECTION 5: Transform and Animation ===

  # Save transform state
  push()

  # Rotating square in center-right
  translate(500, 300)
  rotate(rotation)

  # Draw centered rotating square
  fill(155, 89, 182, 200)  # Semi-transparent purple
  rect(-40, -40, 80, 80)

  # Restore transform
  pop()

  # === SECTION 6: Pulsing Circle ===

  let pulseRadius = 30.0 + sin(pulseTime * 2) * 10.0
  fill(46, 204, 113, 180)  # Semi-transparent green
  circle(Fill, 650, 300, pulseRadius)

  # === SECTION 7: Text Rendering ===

  fill(255, 255, 255)  # White text
  print("Kryon Canvas - Love2D Style API", 50, 500)

  fill(149, 165, 166)  # Gray text
  print("Press ESC to exit", 50, 530)

  # Update animation state
  rotation += 0.02
  pulseTime += 0.05

# Application setup
discard kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Kryon Canvas Demo - Love2D Style"

  Body:
    Container:
      width = 800
      height = 600
      backgroundColor = "#000000"

      Canvas:
        width = 800
        height = 600
        backgroundColor = "#1E283C"
        onDraw = draw
