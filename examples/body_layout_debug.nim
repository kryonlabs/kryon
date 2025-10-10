## Body Layout Debug - Show layout calculation
##
## This debug version prints the element tree structure to understand layout

import ../src/kryon
import ../src/backends/raylib_backend
import ../src/backends/raylib_ffi  # For InitWindow, CloseWindow
import strutils  # For repeat

# Define a very simple UI
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Debug Layout"

  Text:
    text = "Hello World"
    color = "yellow"

  Text:
    text = "Second Line"
    color = "cyan"

# Run the application
when isMainModule:
  echo "=== APP STRUCTURE ==="
  app.printTree()
  echo "\n=== CREATING BACKEND ==="
  var backend = newRaylibBackendFromApp(app)
  echo "Backend created: ", backend.windowWidth, "x", backend.windowHeight
  echo "Window title: ", backend.windowTitle

  echo "\n=== RUNNING APP ==="

  # Add a debug proc to print layout info
  proc printLayout(elem: Element, indent: int = 0) =
    let ind = "  ".repeat(indent)
    echo ind, elem.kind, " @ (", elem.x, ", ", elem.y, ") size: ", elem.width, "x", elem.height
    for child in elem.children:
      printLayout(child, indent + 1)

  # Initialize Raylib so we can measure text
  echo "Initializing window..."
  InitWindow(800, 600, "Debug")

  # Calculate layout
  echo "\n=== CALCULATING LAYOUT ==="
  calculateLayout(app, 0, 0, 800, 600)

  echo "\n=== LAYOUT RESULTS ==="
  printLayout(app)

  # Close window
  CloseWindow()
  echo "\nDone!"
