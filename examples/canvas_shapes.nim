## Canvas Shapes Example
##
## Demonstrates the Canvas API with various drawing operations.
## Shows how to draw rectangles, circles, lines, and use different colors.

import std/math
import ../src/kryon

# Drawing procedure that creates various shapes using high-level drawing functions
proc drawShapes(ctx: DrawingContext, width, height: float) =
  # Clear canvas with light blue background
  ctx.fillStyle = rgba(135.uint8, 206.uint8, 235.uint8, 255.uint8)  # Sky blue
  ctx.drawRectangle(0, 0, width, height)

  # Draw a red filled rectangle
  ctx.fillStyle = rgba(255.uint8, 0.uint8, 0.uint8, 255.uint8)  # Red
  ctx.drawRectangle(50, 50, 100, 80)

  # Draw a blue filled circle
  ctx.fillStyle = rgba(0.uint8, 0.uint8, 255.uint8, 255.uint8)  # Blue
  ctx.drawCircle(250, 90, 40)

  # Draw a green filled triangle
  ctx.fillStyle = rgba(0.uint8, 255.uint8, 0.uint8, 255.uint8)  # Green
  ctx.drawTriangle(400, 130, 350, 50, 450, 50)

  # Draw a purple rectangle outline
  ctx.strokeStyle = rgba(128.uint8, 0.uint8, 128.uint8, 255.uint8)  # Purple
  ctx.lineWidth = 3.0
  ctx.drawRectangleLines(50, 200, 150, 100)

  # Draw an orange circle outline
  ctx.strokeStyle = rgba(255.uint8, 165.uint8, 0.uint8, 255.uint8)  # Orange
  ctx.lineWidth = 2.0
  ctx.drawCircleLines(300, 250, 50)

  # Draw a gold star
  ctx.fillStyle = rgba(255.uint8, 215.uint8, 0.uint8, 255.uint8)  # Gold
  ctx.drawStar(450, 250, 40.0, 20.0, 5)

  # Draw some decorative shapes
  # Pentagon
  ctx.fillStyle = rgba(255.uint8, 20.uint8, 147.uint8, 255.uint8)  # Deep Pink
  ctx.drawPoly((x: 100.0, y: 320.0), 5, 25.0)

  # Hexagon outline
  ctx.strokeStyle = rgba(0.uint8, 128.uint8, 0.uint8, 255.uint8)  # Green
  ctx.lineWidth = 2.0
  ctx.drawPolyLines((x: 200.0, y: 320.0), 6, 25.0)


  # Draw a small ellipse
  ctx.fillStyle = rgba(255.uint8, 140.uint8, 0.uint8, 255.uint8)  # Dark Orange
  ctx.drawEllipse(400, 340, 30, 15)

  # Draw a ring
  ctx.fillStyle = rgba(0.uint8, 100.uint8, 200.uint8, 255.uint8)  # Navy
  ctx.drawRing(500, 340, 10, 20)

  # Draw a sine wave
  ctx.strokeStyle = rgba(148.uint8, 0.uint8, 211.uint8, 255.uint8)  # Medium Violet Red
  ctx.lineWidth = 2.0
  ctx.drawSineWave(50, 380, 500, 15, 0.08)

  # Draw small filled polygons
  ctx.fillStyle = rgba(50.uint8, 205.uint8, 50.uint8, 255.uint8)  # Lime Green
  ctx.drawPoly((x: 150.0, y: 370.0), 4, 12.0)  # Square as polygon
  ctx.drawPoly((x: 250.0, y: 370.0), 3, 12.0)  # Triangle as polygon


# Define the UI with multiple canvases
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Canvas Shapes Example"

  Container:
    width = 800
    height = 600
    backgroundColor = "#2C3E50"

    # Title
    Text:
      text = "Canvas Drawing Examples"
      fontSize = 32
      color = "#ECF0F1"
      posX = 200
      posY = 20

    # First canvas - basic shapes
    Center:
      Canvas:
        width = 550
        height = 400
        backgroundColor = "#34495E"
        onDraw = drawShapes
