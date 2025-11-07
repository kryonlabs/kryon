## Canvas Shapes Example - Pure Kryon DSL
##
## Demonstrates the Canvas API with various drawing operations.
## Shows how to draw rectangles, circles, lines, and use different colors.
## NO direct SDL3 code - clean declarative UI

import std/math
import kryon_dsl

# Drawing procedure that creates various shapes using high-level drawing functions
proc drawShapes(ctx: DrawingContext, width, height: float) =
  # Clear canvas with light blue background
  ctx.clearBackground(width, height, rgba(135.uint8, 206.uint8, 235.uint8, 255.uint8))  # Sky blue

  # Row 1: Basic filled shapes
  ctx.fillStyle = rgba(255.uint8, 0.uint8, 0.uint8, 255.uint8)  # Red
  ctx.drawRectangle(50, 50, 80, 60)

  ctx.fillStyle = rgba(0.uint8, 0.uint8, 255.uint8, 255.uint8)  # Blue
  ctx.drawCircle(200, 80, 35)

  ctx.fillStyle = rgba(0.uint8, 255.uint8, 0.uint8, 255.uint8)  # Green
  ctx.drawTriangle(300, 120, 260, 40, 340, 40)

  # Row 2: Arc demos (half circle and quarter circle)
  ctx.fillStyle = rgba(255.uint8, 255.uint8, 0.uint8, 255.uint8)  # Yellow
  ctx.beginPath()
  ctx.arc(450, 80, 35, 0, PI)  # Half circle
  ctx.fill()

  ctx.strokeStyle = rgba(0.uint8, 255.uint8, 255.uint8, 255.uint8)  # Cyan
  ctx.lineWidth = 3.0
  ctx.beginPath()
  ctx.arc(80, 200, 35, 0, PI / 2)  # Quarter circle
  ctx.stroke()

  # Row 3: Sine wave
  ctx.strokeStyle = rgba(148.uint8, 0.uint8, 211.uint8, 255.uint8)  # Purple
  ctx.lineWidth = 2.0
  ctx.drawSineWave(50, 320, 500, 20, 0.08)


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
          onDraw = drawShapes