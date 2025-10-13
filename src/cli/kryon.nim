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
    rSDL2 = "sdl2"
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

  if "sdl2_backend" in content:
    return rSDL2
  elif "raylib_backend" in content:
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

proc ensureTrailingNewline(s: string): string =
  ## Append a newline to the string unless it already ends with one
  if s.len > 0 and s[^1] != '\n':
    result = s & "\n"
  else:
    result = s

proc indentText(content: string, spaces: int): string =
  ## Indent a block of text by the requested number of spaces.
  ## Keeps empty lines untouched so layout is preserved.
  let indent = repeat(" ", spaces)
  for line in content.splitLines(true):
    case line
    of "", "\n":
      result.add(line)
    else:
      if line[^1] == '\n':
        result.add(indent & line)
      else:
        result.add(indent & line & "\n")

proc renderIndentedBlock(lines: openArray[string], baseIndent: int): string =
  ## Render an array of lines using a shared base indentation.
  let indent = repeat(" ", baseIndent)
  for line in lines:
    if line.len == 0:
      result.add("\n")
    else:
      result.add(indent & line & "\n")

proc buildRendererImports(renderer: Renderer, basePath: string): string =
  ## Produce the backend-specific import statements.
  case renderer
  of rRaylib:
    result = &"""import "{basePath / "backends" / "raylib_backend"}"
"""
  of rSDL2:
    result = &"""import "{basePath / "backends" / "sdl2_backend"}"
"""
  of rHTML:
    result = &"""import "{basePath / "backends" / "html_backend"}"
import os
"""
  else:
    result = ""

proc buildRendererBody(renderer: Renderer): string =
  ## Emit the `when isMainModule` body that boots the chosen renderer.
  case renderer
  of rRaylib:
    result = renderIndentedBlock([
      "var backend: RaylibBackend",
      "backend = newRaylibBackendFromApp(app)",
      "backend.run(app)"
    ], 2)
  of rSDL2:
    result = renderIndentedBlock([
      "var backend: SDL2Backend",
      "backend = newSDL2BackendFromApp(app)",
      "backend.run(app)"
    ], 2)
  of rHTML:
    result = renderIndentedBlock([
      "var backend: HTMLBackend",
      "backend = newHTMLBackendFromApp(app)",
      "let outputDir = if defined(outputDir):",
      "  getEnv(\"OUTPUT_DIR\")",
      "else:",
      "  \".\"",
      "backend.run(app, outputDir)"
    ], 2)
  else:
    result = ""

proc createWrapperFile*(filename: string, renderer: Renderer): string =
  ## Create a wrapper file that includes the user code and backend initialization
  ## Returns: Path to the wrapper file

  let wrapperFile = getTempDir() / "kryon_wrapper.nim"
  let userContent = readFile(filename)

  # Add imports and other non-app code from user file
  let lines = userContent.split('\n')
  var inAppBlock = false
  var appContent = ""
  var importsContent = ""

  for line in lines:
    if line.startsWith("let app = kryonApp:"):
      inAppBlock = true
      continue
    elif inAppBlock:
      # Collect app content (preserve indentation)
      if line.strip().len > 0:
        appContent &= line & "\n"
      else:
        appContent &= "\n"
    else:
      # Add imports and other non-app code
      importsContent &= line & "\n"

  # Start wrapper file
  var wrapperBuilder = "# Auto-generated wrapper for Kryon application\n"

  # Fix import paths to be absolute
  let basePath = getCurrentDir() / "src"
  let kryonPath = getCurrentDir() / "src" / "kryon"

  # Replace relative imports with absolute paths
  var fixedImports = importsContent
  fixedImports = fixedImports.replace("import ../src/kryon", "import \"" & kryonPath & "\"")

  if fixedImports.strip.len > 0:
    wrapperBuilder.add(ensureTrailingNewline(fixedImports))

  let backendImports = buildRendererImports(renderer, basePath)
  if backendImports.len > 0:
    wrapperBuilder.add(ensureTrailingNewline(backendImports))

  # Add main block with backend initialization
  wrapperBuilder.add("\nwhen isMainModule:\n")
  wrapperBuilder.add("  var app = kryonApp:\n")

  if appContent.strip.len > 0:
    wrapperBuilder.add(indentText(appContent, 4))
  else:
    wrapperBuilder.add("    # Empty kryonApp block\n")

  let rendererBody = buildRendererBody(renderer)
  if rendererBody.len > 0:
    wrapperBuilder.add("\n")
    wrapperBuilder.add(rendererBody)

  writeFile(wrapperFile, wrapperBuilder)
  return wrapperFile

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

  # Create wrapper file with backend code
  let wrapperFile = createWrapperFile(filename, renderer)

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

  # Add renderer-specific linking flags
  case renderer:
  of rSDL2:
    nimCmd.add(" --passL:\"-lSDL2 -lSDL2_ttf\"")
  of rHTML:
    # For HTML, no special linking needed
    discard
  else:
    discard

  # Add wrapper source file
  nimCmd.add(&" {wrapperFile}")

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

  # Cleanup wrapper file
  if fileExists(wrapperFile):
    removeFile(wrapperFile)

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
  ##   renderer: Renderer to use ("auto", "raylib", "sdl2", "html", "terminal")
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
      echo &"Available renderers: auto, raylib, sdl2, html, terminal"
      return 1

  # Check renderer support
  case detectedRenderer:
  of rHTML:
    # HTML renderer generates files directly
    echo "Generating HTML web app..."
    echo "Renderer: HTML"

    # Determine output directory
    let outputDir = if defined(outputDir):
      getEnv("OUTPUT_DIR")
    else:
      filename.splitFile.dir / (filename.splitFile.name & "_html")

    echo "Output directory: " & outputDir

    # Create and run HTML generator
    let currentDir = getCurrentDir()
    let absoluteFilename = if filename.isAbsolute():
      filename
    else:
      currentDir / filename

    # Read and parse the user file content
    let userContent = readFile(absoluteFilename)
    let lines = userContent.split('\n')
    var appContent = ""
    var importsContent = ""
    var inAppBlock = false

    for line in lines:
      if line.startsWith("let app = kryonApp:"):
        inAppBlock = true
        continue
      elif inAppBlock:
        # Collect app content (preserve indentation)
        if line.strip().len > 0:
          appContent &= line & "\n"
        else:
          appContent &= "\n"
      else:
        # Add imports and other non-app code
        importsContent &= line & "\n"

    # Fix import paths to be absolute
    let kryonPath = currentDir / "src" / "kryon"
    var fixedImports = importsContent
    fixedImports = fixedImports.replace("import ../src/kryon", "import \"" & kryonPath & "\"")

    var htmlGeneratorCode = "import os, strformat\n"
    htmlGeneratorCode &= "import \"" & currentDir & "/src/kryon\"\n"
    htmlGeneratorCode &= "import \"" & currentDir & "/src/backends/html_backend\"\n"
    htmlGeneratorCode &= "\n"
    htmlGeneratorCode &= fixedImports
    htmlGeneratorCode &= "\n"
    htmlGeneratorCode &= "var app = kryonApp:\n"
    htmlGeneratorCode &= appContent
    htmlGeneratorCode &= "\n"
    htmlGeneratorCode &= "var backend: HTMLBackend\n"
    htmlGeneratorCode &= "backend = newHTMLBackendFromApp(app)\n"
    htmlGeneratorCode &= "backend.run(app, \"" & outputDir & "\")\n"

    let tempFile = getTempDir() / "html_gen.nim"
    writeFile(tempFile, htmlGeneratorCode)

    let genCmd = "nim c -r " & tempFile
    let (output, exitCode) = execCmdEx(genCmd)

    # Cleanup
    if fileExists(tempFile):
      removeFile(tempFile)

    if exitCode == 0:
      echo ""
      echo "✓ HTML web app generated successfully!"
      echo "Open " & outputDir / "index.html" & " in your browser to view the app."
      if verbose:
        echo output
    else:
      echo "HTML generation failed:"
      echo output

    return exitCode

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
  ##   renderer: Renderer to use ("auto", "raylib", "sdl2", "html", "terminal")
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
      echo &"Available renderers: auto, raylib, sdl2, html, terminal"
      return 1

  # Check renderer support
  case detectedRenderer:
  of rHTML:
    # HTML renderer generates files directly
    echo "Building HTML web app..."
    echo "Renderer: HTML"

    # Determine output directory
    let outputDir = if output.len > 0:
      output
    else:
      filename.splitFile.dir / (filename.splitFile.name & "_html")

    echo "Output directory: " & outputDir

    # Create and run HTML generator
    let currentDir = getCurrentDir()
    let absoluteFilename = if filename.isAbsolute():
      filename
    else:
      currentDir / filename

    # Read and parse the user file content
    let userContent = readFile(absoluteFilename)
    let lines = userContent.split('\n')
    var appContent = ""
    var importsContent = ""
    var inAppBlock = false

    for line in lines:
      if line.startsWith("let app = kryonApp:"):
        inAppBlock = true
        continue
      elif inAppBlock:
        # Collect app content (preserve indentation)
        if line.strip().len > 0:
          appContent &= line & "\n"
        else:
          appContent &= "\n"
      else:
        # Add imports and other non-app code
        importsContent &= line & "\n"

    # Fix import paths to be absolute
    let kryonPath = currentDir / "src" / "kryon"
    var fixedImports = importsContent
    fixedImports = fixedImports.replace("import ../src/kryon", "import \"" & kryonPath & "\"")

    var htmlGeneratorCode = "import os, strformat\n"
    htmlGeneratorCode &= "import \"" & currentDir & "/src/kryon\"\n"
    htmlGeneratorCode &= "import \"" & currentDir & "/src/backends/html_backend\"\n"
    htmlGeneratorCode &= "\n"
    htmlGeneratorCode &= fixedImports
    htmlGeneratorCode &= "\n"
    htmlGeneratorCode &= "var app = kryonApp:\n"
    htmlGeneratorCode &= appContent
    htmlGeneratorCode &= "\n"
    htmlGeneratorCode &= "var backend: HTMLBackend\n"
    htmlGeneratorCode &= "backend = newHTMLBackendFromApp(app)\n"
    htmlGeneratorCode &= "backend.run(app, \"" & outputDir & "\")\n"

    let tempFile = getTempDir() / "html_gen.nim"
    writeFile(tempFile, htmlGeneratorCode)

    let genCmd = "nim c -r " & tempFile
    let (genOutput, exitCode) = execCmdEx(genCmd)

    # Cleanup
    if fileExists(tempFile):
      removeFile(tempFile)

    if exitCode == 0:
      echo ""
      echo "✓ HTML web app built successfully!"
      echo "Open " & outputDir / "index.html" & " in your browser to view the app."
      if verbose:
        echo genOutput
    else:
      echo "HTML build failed:"
      echo genOutput

    return exitCode

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
  echo "  - sdl2 (desktop, cross-platform)"
  echo "  - html (web, generates HTML/CSS/JS)"
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
       "renderer": "Renderer to use (auto, raylib, sdl2, html, terminal)",
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
       "renderer": "Renderer to use (auto, raylib, sdl2, html, terminal)",
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
