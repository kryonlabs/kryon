## Basic Canvas API Test
##
## Simple test to verify the Love2D-style canvas API works correctly

import kryon_dsl
import canvas

# Simple drawing test
proc drawBasicTest() =
  # Clear with dark background
  background(20, 20, 30)

  # Test basic shapes
  print("Canvas API Test", 10, 30)

  # Rectangle
  fill(255, 100, 100, 255)  # Red
  rectangle(Fill, 50, 50, 100, 80)

  # Circle
  fill(100, 255, 100, 255)  # Green
  circle(Fill, 250, 90, 40)

  # Ellipse
  fill(100, 100, 255, 255)  # Blue
  ellipse(Fill, 400, 90, 60, 40)

  # Triangle (polygon)
  let triangle = [(x: 550.0, y: 130.0), (x: 500.0, y: 50.0), (x: 600.0, y: 50.0)]
  fill(255, 255, 100, 255)  # Yellow
  polygon(Fill, triangle)

  # Line
  stroke(255, 255, 255, 255)  # White
  strokeWeight(2)
  line(50, 200, 600, 200)

  # Points
  fill(255, 150, 255, 255)  # Magenta
  for i in 0..<10:
    point(100 + i * 50, 250)

  # Transformed rectangle
  withTransform:
    translate(350, 350)
    rotate(45)  # 45 degrees
    fill(0, 255, 255, 200)  # Cyan
    rectangle(Fill, -50, -50, 100, 100)

  print("If you see shapes above, canvas API works!", 10, 450)

let app = kryonApp:
  Header:
    windowWidth = 700
    windowHeight = 500
    windowTitle = "Canvas Basic API Test"

  Body:
    backgroundColor = "#000000"

    Canvas:
      width = 700
      height = 500
      backgroundColor = "#141414"
      onDraw = drawBasicTest  # No ctx parameter needed!

echo "Basic Canvas Test Running"