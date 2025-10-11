## Kryon CLI Tool
##
## Command-line interface for running and compiling Kryon applications
## with support for multiple renderers.

import os, osproc, strutils, strformat

const VERSION = "0.2.0"

type
  Renderer* = enum
    ## Supported renderers
    rRaylib = "raylib"
    rHTML = "html"      # Future
    rTerminal = "terminal"  # Future

  CompileMode = enum
    cmRun       # Compile and run
    cmBuild     # Compile only

# ============================================================================
# Renderer Detection
# ============================================================================

proc detectRenderer*(filename: string): Renderer =
  ## Detect which renderer to use based on file imports
  if not fileExists(filename):
    echo &"Error: File not found: {filename}"
    quit(1)

  let content = readFile(filename)

  if "raylib_backend" in content:
    return rRaylib
  elif "html_backend" in content:
    return rHTML
  elif "terminal_backend" in content:
    return rTerminal
  else:
    # Default to raylib
    echo "Warning: No renderer import detected, defaulting to raylib"
    return rRaylib

# ============================================================================
# Compilation
# ============================================================================

proc compileKryon*(
  filename: string,
  renderer: Renderer,
  output: string = "",
  release: bool = false,
  verbose: bool = false
): int =
  ## Compile a Kryon application
  ##
  ## Returns: Exit code (0 = success)

  if not fileExists(filename):
    echo &"Error: File not found: {filename}"
    return 1

  # Determine output filename
  let outFile = if output.len > 0:
    output
  else:
    filename.changeFileExt("")

  # Build nim command
  var nimCmd = "nim c"

  # Add flags
  if release:
    nimCmd.add(" -d:release")
  if not verbose:
    nimCmd.add(" --hints:off --warnings:off")

  # Add output path
  nimCmd.add(&" -o:{outFile}")

  # Add source file
  nimCmd.add(&" {filename}")

  # Show compilation info
  echo &"Compiling: {filename}"
  echo &"Renderer: {renderer}"
  if release:
    echo "Mode: Release (optimized)"
  else:
    echo "Mode: Debug"

  # Compile
  if verbose:
    echo &"Command: {nimCmd}"

  let (output, exitCode) = execCmdEx(nimCmd)

  if exitCode != 0:
    echo "Compilation failed:"
    echo output
    return exitCode

  if verbose:
    echo output

  echo &"✓ Compiled successfully: {outFile}"
  return 0

# ============================================================================
# Run Command
# ============================================================================

proc runKryon*(
  filename: string,
  renderer: string = "auto",
  release: bool = false,
  verbose: bool = false
): int =
  ## Compile and run a Kryon application
  ##
  ## Args:
  ##   filename: Path to .nim file to run
  ##   renderer: Renderer to use ("auto", "raylib", "html", "terminal")
  ##   release: Compile in release mode (optimized)
  ##   verbose: Show detailed output
  ##
  ## Returns: Exit code

  if not fileExists(filename):
    echo &"Error: File not found: {filename}"
    return 1

  # Detect or parse renderer
  let detectedRenderer = if renderer == "auto":
    detectRenderer(filename)
  else:
    try:
      parseEnum[Renderer](renderer)
    except ValueError:
      echo &"Error: Unknown renderer: {renderer}"
      echo &"Available renderers: auto, raylib, html, terminal"
      return 1

  # Check renderer support
  case detectedRenderer:
  of rHTML:
    echo "Error: HTML renderer not yet implemented"
    return 1
  of rTerminal:
    echo "Error: Terminal renderer not yet implemented"
    return 1
  else:
    discard

  # Create temporary output file
  let tmpDir = getTempDir()
  let tmpFile = tmpDir / filename.extractFilename().changeFileExt("")

  # Compile
  let compileResult = compileKryon(
    filename,
    detectedRenderer,
    tmpFile,
    release,
    verbose
  )

  if compileResult != 0:
    return compileResult

  # Run
  echo ""
  echo "Running..."
  echo "─".repeat(60)

  let runResult = execCmd(tmpFile)

  # Cleanup
  if fileExists(tmpFile):
    removeFile(tmpFile)

  return runResult

# ============================================================================
# Build Command
# ============================================================================

proc buildKryon*(
  filename: string,
  renderer: string = "auto",
  output: string = "",
  release: bool = true,
  verbose: bool = false
): int =
  ## Compile a Kryon application to an executable
  ##
  ## Args:
  ##   filename: Path to .nim file to compile
  ##   renderer: Renderer to use ("auto", "raylib", "html", "terminal")
  ##   output: Output executable path (default: same name as input)
  ##   release: Compile in release mode (default: true)
  ##   verbose: Show detailed compilation output
  ##
  ## Returns: Exit code

  if not fileExists(filename):
    echo &"Error: File not found: {filename}"
    return 1

  # Detect or parse renderer
  let detectedRenderer = if renderer == "auto":
    detectRenderer(filename)
  else:
    try:
      parseEnum[Renderer](renderer)
    except ValueError:
      echo &"Error: Unknown renderer: {renderer}"
      echo &"Available renderers: auto, raylib, html, terminal"
      return 1

  # Check renderer support
  case detectedRenderer:
  of rHTML:
    echo "Error: HTML renderer not yet implemented"
    return 1
  of rTerminal:
    echo "Error: Terminal renderer not yet implemented"
    return 1
  else:
    discard

  # Compile
  return compileKryon(
    filename,
    detectedRenderer,
    output,
    release,
    verbose
  )

# ============================================================================
# Info Command
# ============================================================================

proc infoKryon*(filename: string): int =
  ## Show information about a Kryon file
  ##
  ## Args:
  ##   filename: Path to .nim file to analyze
  ##
  ## Returns: Exit code

  if not fileExists(filename):
    echo &"Error: File not found: {filename}"
    return 1

  let renderer = detectRenderer(filename)
  let content = readFile(filename)

  echo "Kryon File Info"
  echo "─".repeat(60)
  echo &"File: {filename}"
  echo &"Size: {getFileSize(filename)} bytes"
  echo &"Renderer: {renderer}"
  echo ""

  # Count elements
  let elements = ["Container", "Text", "Button", "Column", "Row",
                  "Center", "Input", "Checkbox", "Dropdown", "Grid",
                  "Image", "ScrollView"]

  echo "Elements used:"
  for elem in elements:
    let count = content.count(elem & ":")
    if count > 0:
      echo &"  {elem}: {count}"

  # Check for event handlers
  if "onClick" in content:
    echo "  onClick handlers: ", content.count("onClick")

  return 0

# ============================================================================
# Version Command
# ============================================================================

proc versionCmd*(): int =
  ## Show Kryon version
  echo &"Kryon-Nim v{VERSION}"
  echo "A declarative UI framework for Nim"
  echo ""
  echo "Supported renderers:"
  echo "  - raylib (desktop, 60 FPS)"
  echo "  - html (coming soon)"
  echo "  - terminal (coming soon)"
  return 0

# ============================================================================
# Main CLI Entry Point
# ============================================================================

when isMainModule:
  import cligen

  # Configure CLI
  dispatchMulti(
    [runKryon, cmdName = "run",
     help = {
       "filename": "Path to .nim file to run",
       "renderer": "Renderer to use (auto, raylib, html, terminal)",
       "release": "Compile in release mode (optimized)",
       "verbose": "Show detailed compilation output"
     },
     short = {
       "renderer": 'r',
       "release": 'R',
       "verbose": 'v'
     }
    ],
    [buildKryon, cmdName = "build",
     help = {
       "filename": "Path to .nim file to compile",
       "renderer": "Renderer to use (auto, raylib, html, terminal)",
       "output": "Output executable path",
       "release": "Compile in release mode (default: true)",
       "verbose": "Show detailed compilation output"
     },
     short = {
       "renderer": 'r',
       "output": 'o',
       "release": 'R',
       "verbose": 'v'
     }
    ],
    [infoKryon, cmdName = "info",
     help = {
       "filename": "Path to .nim file to analyze"
     }
    ],
    [versionCmd, cmdName = "version"]
  )
