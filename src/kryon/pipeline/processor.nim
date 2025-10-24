## Pipeline Processor
##
## This is the main entry point for the Kryon rendering pipeline.
## It orchestrates all processing stages and produces a list of
## RenderCommands ready for execution by any backend.
##
## Pipeline Stages:
## 1. Preprocessing - Resolve ForLoops and Conditionals
## 2. Layout - Calculate positions and sizes
## 3. Interaction Processing - Handle user input and update state
## 4. Component Extraction - Convert elements to RenderCommands
##
## This clean separation makes the codebase maintainable and testable.

import ../core
import ../layout/layoutEngine
import ../state/backendState
import preprocessor
import interactionProcessor
import componentExtractor
import renderCommands
import options

# ============================================================================
# Pipeline Configuration
# ============================================================================

type
  PipelineConfig* = object
    ## Configuration for the rendering pipeline
    windowWidth*: int
    windowHeight*: int
    mouseX*: float
    mouseY*: float
    mousePressed*: bool
    mouseReleased*: bool

  TextMeasurer* = concept m
    ## Duck-typed interface for text measurement (provided by backend)
    m.measureText(string, int) is tuple[width: float, height: float]

# ============================================================================
# Main Pipeline Processing
# ============================================================================

proc processFrame*(root: Element, measurer: TextMeasurer, state: var BackendState, config: PipelineConfig): renderCommands.RenderCommandList =
  ## Process a single frame through the entire pipeline
  ## Returns a list of RenderCommands ready for rendering
  ##
  ## This is the ONLY function backends need to call!
  ##
  ## Example usage:
  ## ```nim
  ## let commands = processFrame(app, backend, backend.state, config)
  ## for cmd in commands:
  ##   executeCommand(backend, cmd)
  ## ```

  # ========== STAGE 1: PREPROCESSING ==========
  # Resolve ForLoops and Conditionals into a flat tree
  preprocessTree(root)

  # ========== STAGE 2: LAYOUT ==========
  # Calculate positions and sizes for all elements
  calculateLayout(
    measurer,
    root,
    0.0, 0.0,
    config.windowWidth.float,
    config.windowHeight.float
  )

  # ========== STAGE 3: INTERACTION PROCESSING ==========
  # Handle user input and update element state
  processInteractions(
    root,
    state,
    config.mouseX,
    config.mouseY,
    config.mousePressed,
    config.mouseReleased
  )

  # ========== STAGE 4: COMPONENT EXTRACTION ==========
  # Convert element tree to RenderCommands
  result = extractElement(root, measurer, state, none(Color))

  # ========== STAGE 5: DRAG-AND-DROP EFFECTS ==========
  # Add drag-and-drop visual effects on top
  result.add extractDragAndDropEffects(root, measurer)

# ============================================================================
# Simplified API
# ============================================================================

proc newPipelineConfig*(
  windowWidth, windowHeight: int,
  mouseX: float = 0.0,
  mouseY: float = 0.0,
  mousePressed: bool = false,
  mouseReleased: bool = false
): PipelineConfig =
  ## Create a new pipeline configuration
  PipelineConfig(
    windowWidth: windowWidth,
    windowHeight: windowHeight,
    mouseX: mouseX,
    mouseY: mouseY,
    mousePressed: mousePressed,
    mouseReleased: mouseReleased
  )

# ============================================================================
# Debug Support
# ============================================================================

proc countCommands*(commands: renderCommands.RenderCommandList): tuple[
  rectangles: int,
  borders: int,
  texts: int,
  images: int,
  lines: int,
  clips: int
] =
  ## Count commands by type for debugging
  result = (0, 0, 0, 0, 0, 0)

  for cmd in commands:
    case cmd.kind:
    of rcDrawRectangle:
      inc result.rectangles
    of rcDrawBorder:
      inc result.borders
    of rcDrawText:
      inc result.texts
    of rcDrawImage:
      inc result.images
    of rcDrawLine:
      inc result.lines
    of rcBeginClip, rcEndClip:
      inc result.clips
    of rcDrawCanvas, rcDrawPath, rcClearCanvas, rcSaveCanvasState, rcRestoreCanvasState:
      inc result.rectangles  # Count as rectangles for now

proc dumpCommands*(commands: renderCommands.RenderCommandList) =
  ## Dump all commands for debugging
  echo "=== RENDER COMMANDS ==="
  echo "Total commands: ", commands.len

  let counts = countCommands(commands)
  echo "  Rectangles: ", counts.rectangles
  echo "  Borders: ", counts.borders
  echo "  Texts: ", counts.texts
  echo "  Images: ", counts.images
  echo "  Lines: ", counts.lines
  echo "  Clips: ", counts.clips
  echo "======================="
