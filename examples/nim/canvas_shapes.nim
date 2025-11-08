## Canvas Shapes Example - Updated Love2D Style API
##
## Demonstrates the new Love2D-inspired Canvas API with various drawing operations.
## Shows how to draw rectangles, circles, lines, and use different colors.
## Uses immediate mode drawing functions within a Canvas component

import std/math
import kryon_dsl
import canvas

# Drawing procedure that creates various shapes using Love2D-style API
proc drawShapes() =
  # Clear canvas with light blue background
  background(135, 206, 235)  # Sky blue

  # Row 1: Basic filled shapes
  fill(255, 0, 0, 255)  # Red
  rectangle(Fill, 50, 50, 80, 60)

  fill(0, 0, 255, 255)  # Blue
  circle(Fill, 200, 80, 35)

  fill(0, 255, 0, 255)  # Green
  # Triangle as polygon
  let triangleVertices = [(x: 300.0, y: 120.0), (x: 260.0, y: 40.0), (x: 340.0, y: 40.0)]
  polygon(Fill, triangleVertices)

  # Row 2: Arc demos (half circle and quarter circle)
  fill(255, 255, 0, 255)  # Yellow
  arc(Fill, 450, 80, 35, 0, PI)  # Half circle

  stroke(0, 255, 255, 255)  # Cyan
  strokeWeight(3)
  arc(Line, 80, 200, 35, 0, PI / 2)  # Quarter circle

  # Row 3: Sine wave
  stroke(148, 0, 211, 255)  # Purple
  strokeWeight(2)
  let wavePoints: seq[Point] = @[]
  for i in 0..100:
    let x = 50.0 + i * 5.0
    let y = 320.0 + sin(0.08 * float(i)) * 20.0
    wavePoints.add((x, y))
  line(wavePoints)


# Define the UI with multiple canvases using pure Kryon DSL
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Canvas Shapes Example"

  Body:
    Container:
      width = 800
      height = 600
      backgroundColor = "#2C3E50"

      # Title
      Text:
        content = "Canvas Drawing Examples"
        color = "#ECF0F1"

      # First canvas - basic shapes
      Container:
        width = 550
        height = 400
        backgroundColor = "#34495E"

        Canvas:
          width = 550
          height = 400
          backgroundColor = "#3498DB"
          onDraw = drawShapes