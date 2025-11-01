## Kryon CLI Tool
##
## Command-line interface for running and compiling Kryon applications
## with support for multiple renderers.

import std/[os, osproc, strutils, strformat, hashes, md5, asynchttpserver, asyncdispatch]

const VERSION = "0.2.0"

const HELP_INTRO = """
Kryon - A Declarative UI Framework for Nim
===========================================

Kryon is similar to Flutter but more intelligent, supporting multiple backends
from a single codebase. Write your UI once, run anywhere!

Supported Backends:
  • web       - HTML/CSS/JS for browsers (with Canvas support!)
  • native    - Auto-select desktop backend (raylib or sdl2)
  • raylib    - Native desktop with raylib (60 FPS)
  • sdl2      - Native desktop with SDL2 (cross-platform)

Quick Start:
  kryon run -d web --serve -f myapp.nim    # Web with dev server
  kryon run -d native -f myapp.nim         # Desktop app
  kryon devices                            # List all backends

Examples:
  • Canvas graphics and games
  • Multi-page web applications
  • Cross-platform desktop apps
  • Reactive UI with state management

Learn More:
  kryon --help           # This help message
  kryon run --help       # Help for run command
  kryon devices          # List available backends

"""

# Installation detection
const
  CONFIG_DIR = when defined(unix): getHomeDir() / ".config" / "kryon"
               else: getAppDir() / "config"
  BIN_DIR = when defined(unix): getHomeDir() / ".local" / "bin"
           else: getAppDir() / "bin"

type
  Renderer* = enum
    ## Supported renderers
    rRaylib = "raylib"
    rSDL2 = "sdl2"
    rHTML = "html"      # Web backend
    rTerminal = "terminal"  # Future

  Backend* = enum
    ## Backend aliases (Flutter-style naming)
    bWeb = "web"        # Alias for HTML
    bNative = "native"  # Auto-select raylib or sdl2
    bRaylib = "raylib"
    bSDL2 = "sdl2"
    bTerminal = "terminal"

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

  # Check for new integration-based backend imports
  if "integration/sdl2" in content or "sdl2_backend" in content:
    return rSDL2
  elif "integration/raylib" in content or "raylib_backend" in content:
    return rRaylib
  elif "integration/html" in content or "html_backend" in content:
    return rHTML
  elif "terminal_backend" in content:
    return rTerminal
  else:
    # Default to raylib
    echo "Warning: No renderer import detected, defaulting to raylib"
    return rRaylib

# ============================================================================
# Backend/Renderer Conversion
# ============================================================================

proc backendToRenderer*(backend: Backend, filename: string = ""): Renderer =
  ## Convert Flutter-style backend name to Renderer
  case backend:
  of bWeb:
    return rHTML
  of bRaylib:
    return rRaylib
  of bSDL2:
    return rSDL2
  of bTerminal:
    return rTerminal
  of bNative:
    # Auto-detect or default to raylib
    if filename.len > 0 and fileExists(filename):
      return detectRenderer(filename)
    else:
      return rRaylib

proc parseBackendOrRenderer*(value: string, filename: string = ""): Renderer =
  ## Parse a backend or renderer string
  ## Supports both Flutter-style (web, native) and direct (raylib, sdl2, html)
  case value.toLowerAscii():
  of "web":
    return rHTML
  of "native":
    if filename.len > 0 and fileExists(filename):
      return detectRenderer(filename)
    else:
      return rRaylib
  of "raylib":
    return rRaylib
  of "sdl2":
    return rSDL2
  of "html":
    return rHTML
  of "terminal":
    return rTerminal
  of "auto":
    if filename.len > 0:
      return detectRenderer(filename)
    else:
      return rRaylib
  else:
    echo &"Error: Unknown backend/renderer: {value}"
    echo "Available backends:"
    echo "  -d web         (HTML/CSS/JS web application)"
    echo "  -d native      (Auto-select raylib or sdl2)"
    echo "  -d raylib      (Raylib desktop backend)"
    echo "  -d sdl2        (SDL2 desktop backend)"
    quit(1)

# ============================================================================
# Compilation
# ============================================================================

proc ensureTrailingNewline(s: string): string =
  ## Append a newline to the string unless it already ends with one
  if s.len > 0 and s[^1] != '\n':
    result = s & "\n"
  else:
    result = s

proc nimStringLiteral(s: string): string =
  ## Convert a raw string into a Nim string literal with proper escaping.
  result.add('"')
  for ch in s:
    case ch
    of '\\': result.add("\\\\")
    of '\"': result.add("\\\"")
    else: result.add(ch)
  result.add('"')

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
    result = &"""import "{basePath / "backends" / "integration" / "raylib_backend"}"
"""
  of rSDL2:
    result = &"""import "{basePath / "backends" / "integration" / "sdl2"}"
"""
  of rHTML:
    result = &"""import "{basePath / "backends" / "integration" / "html"}"
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

# ============================================================================
# Installation Detection and Enhanced Path Resolution
# ============================================================================

proc isInstalledVersion*(): bool =
  ## Check if we're running from an installed version
  let exePath = getAppFilename()
  exePath.startsWith(BIN_DIR) or fileExists(CONFIG_DIR / "config.yaml")

proc getInstalledKryonPath*(): string =
  ## Get the Kryon library path for installed version
  if isInstalledVersion():
    when defined(unix):
      getHomeDir() / ".local" / "lib"
    else:
      getAppDir() / "lib"
  else:
    ""

proc checkSystemDependencies*(): tuple[nim: bool, raylib: bool, sdl2: bool, skia: bool] =
  ## Check if system dependencies are available
  result.nim = findExe("nim") != ""

  # Check for raylib (can be via nimble or system)
  result.raylib = execCmdEx("nimble list raylib", {poUsePath, poStdErrToStdOut}).exitCode == 0 or
                   execCmdEx("pkg-config --exists raylib", {poUsePath, poStdErrToStdOut}).exitCode == 0

  # Check for SDL2 (system package)
  result.sdl2 = execCmdEx("pkg-config --exists sdl2", {poUsePath, poStdErrToStdOut}).exitCode == 0

  # Check for Skia (typically via SDL2 backend)
  result.skia = fileExists("/usr/include/skia") or fileExists("/usr/local/include/skia")

proc doctorCmd*(): int =
  ## System health check and dependency verification
  echo "Kryon System Health Check"
  echo "========================="
  echo ""
  echo "Version: ", VERSION
  echo "Build: ", hostOS, "-", hostCPU
  echo "Installation: ", if isInstalledVersion(): "Installed" else: "Development"
  echo "Executable: ", getAppFilename()
  echo ""

  if isInstalledVersion():
    echo "Installation paths:"
    echo "  CLI: ", getAppFilename()
    echo "  Config: ", CONFIG_DIR
    echo "  Library: ", getInstalledKryonPath()
    echo ""

  let deps = checkSystemDependencies()
  echo "Dependencies:"
  echo "  Nim: ", if deps.nim: "✓ Available" else: "✗ Not found"
  echo "  Raylib: ", if deps.raylib: "✓ Available" else: "✗ Not found (install: nimble install raylib)"
  echo "  SDL2: ", if deps.sdl2: "✓ Available" else: "✗ Not found (system package required)"
  echo "  Skia: ", if deps.skia: "✓ Available" else: "✗ Not found (optional)"
  echo ""

  if not deps.nim:
    echo "❌ Critical: Nim compiler is required"
    echo "   Install from: https://nim-lang.org/install.html"
  elif not deps.raylib and not deps.sdl2:
    echo "⚠️  Warning: No rendering backends available"
    echo "   Install at least one backend:"
    echo "   - Raylib: nimble install raylib"
    echo "   - SDL2: sudo apt install libsdl2-dev (Ubuntu/Debian)"
    echo "            brew install sdl2 (macOS)"
  else:
    echo "✅ System appears ready for Kryon development"

  echo ""
  if deps.nim:
    return 0
  else:
    return 1

proc initCmd*(templateName: string, projectName: string = ""): int =
  ## Initialize a new Kryon project from a template
  let templates = ["basic", "todo", "game", "calculator"]

  if templateName notin templates:
    echo "Error: Unknown template '", templateName, "'"
    echo "Available templates:"
    for tmpl in templates:
      echo "  - ", tmpl
    return 1

  # Determine project name
  let projName = if projectName.len > 0: projectName else: templateName

  # Check if directory already exists
  if dirExists(projName):
    echo "Error: Directory '", projName, "' already exists"
    return 1

  # Find template directory
  let templateDir = if isInstalledVersion():
      CONFIG_DIR / "templates" / templateName
    else:
      getCurrentDir() / "templates" / templateName

  if not dirExists(templateDir):
    echo "Error: Template not found at ", templateDir
    echo "Try reinstalling Kryon or run from source directory"
    return 1

  # Create project directory
  createDir(projName)
  echo "Creating project: ", projName
  echo "Using template: ", templateName
  echo ""

  # Copy template files
  for file in walkFiles(templateDir / "/*"):
    let filename = splitPath(file).tail
    let destPath = projName / filename
    copyFile(file, destPath)
    echo "  + ", filename

  # Special handling for main.kry - rename if needed
  if projName != templateName:
    let mainFile = projName / "main.kry"
    if fileExists(mainFile):
      echo "  + main.kry (ready to run)"

  echo ""
  echo "Project created successfully!"
  echo ""
  echo "Get started:"
  echo "  cd ", projName
  echo "  kryon run main.kry"
  echo ""
  return 0

proc locateKryonPaths(appDir: string): tuple[basePath: string, kryonPath: string] =
  ## Find the Kryon source directory relative to the app location.
  ## Enhanced with installed version detection.
  ## Supports:
  ##   - Installed version detection (uses system library paths)
  ##   - Environment vars pointing to the repo (KRYON_ROOT/KRYON_HOME/KRYON_PATH)
  ##   - Environment var pointing directly at the src directory (KRYON_SRC)
  ##   - Walking up from the app directory to locate either src/kryon or kryon/src/kryon

  # Check if we're running from installed version
  if isInstalledVersion():
    let libPath = getInstalledKryonPath()
    if dirExists(libPath / "kryon"):
      return (basePath: libPath, kryonPath: libPath / "kryon")

  # Continue with original logic for development versions

  proc normalize(path: string): string =
    var tmp = absolutePath(path)
    normalizePath(tmp)
    result = tmp

  proc isValidKryonDir(dir: string): bool =
    ## Ensure the directory actually contains the Kryon modules.
    fileExists(dir / "core.nim") and fileExists(dir / "dsl.nim")

  proc checkCandidate(base: string, basePath: var string, kryonPath: var string): bool =
    if base.len == 0:
      return false

    let normalizedBase = normalize(base)
    let direct = normalizedBase / "src" / "kryon"
    if dirExists(direct) and isValidKryonDir(direct):
      basePath = normalizedBase / "src"
      kryonPath = direct
      return true

    let nested = normalizedBase / "kryon" / "src" / "kryon"
    if dirExists(nested) and isValidKryonDir(nested):
      basePath = normalizedBase / "kryon" / "src"
      kryonPath = nested
      return true

    return false

  let envSrc = getEnv("KRYON_SRC")
  if envSrc.len > 0:
    var basePath, kryonPath: string
    let normalizedSrc = normalize(envSrc)
    if dirExists(normalizedSrc / "kryon") and isValidKryonDir(normalizedSrc / "kryon"):
      return (basePath: normalizedSrc, kryonPath: normalizedSrc / "kryon")
    elif checkCandidate(envSrc, basePath, kryonPath):
      return (basePath: basePath, kryonPath: kryonPath)

  for key in ["KRYON_ROOT", "KRYON_HOME", "KRYON_PATH"]:
    let value = getEnv(key)
    if value.len > 0:
      var basePath, kryonPath: string
      if checkCandidate(value, basePath, kryonPath):
        return (basePath: basePath, kryonPath: kryonPath)

  var searchDir = if appDir.len > 0: normalize(appDir) else: getCurrentDir()
  while true:
    var basePath, kryonPath: string
    if checkCandidate(searchDir, basePath, kryonPath):
      return (basePath: basePath, kryonPath: kryonPath)

    let parent = parentDir(searchDir)
    if parent.len == 0 or parent == searchDir:
      break
    searchDir = parent

  echo "Error: Unable to locate Kryon library sources."
  echo "Set KRYON_ROOT (or KRYON_SRC) so the CLI can find the Kryon repo."
  quit(1)

proc createWrapperFile*(filename: string, renderer: Renderer): string =
  ## Create a wrapper file that includes the user code and backend initialization
  ## Returns: Path to the wrapper file
  ##
  ## Uses a stable project-local path (.kryon/wrapper.nim) instead of temp file
  ## to enable Nim's incremental compilation and avoid recompiling raylib every time

  let userContent = readFile(filename)
  let absoluteFilename = if filename.isAbsolute():
    filename
  else:
    getCurrentDir() / filename
  let (appDir, _, _) = absoluteFilename.splitFile
  let assetsDir = if appDir.len > 0: appDir / "assets" else: ""
  let (basePath, kryonPath) = locateKryonPaths(appDir)

  # Use stable wrapper file in project directory for incremental compilation
  let projectRoot = if appDir.len > 0: appDir else: getCurrentDir()
  let kryonDir = projectRoot / ".kryon"
  if not dirExists(kryonDir):
    createDir(kryonDir)
  let wrapperFile = kryonDir / "wrapper.nim"
  let hashFile = kryonDir / "wrapper.hash"

  # Check if wrapper needs regeneration by comparing content hash
  # Hash includes: user code + renderer + kryon version
  let contentHash = getMD5($renderer & userContent & VERSION)
  var needsRegeneration = true

  if fileExists(hashFile) and fileExists(wrapperFile):
    let savedHash = readFile(hashFile).strip()
    if savedHash == contentHash:
      # Wrapper is up-to-date, skip regeneration
      needsRegeneration = false

  if not needsRegeneration:
    # Reuse existing wrapper for faster compilation
    return wrapperFile

  # Generate new wrapper and save hash
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

  # Replace relative imports with absolute paths
  var fixedImports = importsContent
  for pattern in [
    "import ../src/kryon",
    "import \"../src/kryon\"",
    "import '../src/kryon'",
    "import ../kryon/src/kryon",
    "import \"../kryon/src/kryon\"",
    "import '../kryon/src/kryon'",
    "import ../kryonlabs/kryon/src/kryon",
    "import \"../kryonlabs/kryon/src/kryon\"",
    "import '../kryonlabs/kryon/src/kryon'"
  ]:
    fixedImports = fixedImports.replace(pattern, "import \"" & kryonPath & "\"")
  for pattern in [
    "from ../src/kryon",
    "from \"../src/kryon\"",
    "from '../src/kryon'",
    "from ../kryon/src/kryon",
    "from \"../kryon/src/kryon\"",
    "from '../kryon/src/kryon'"
  ]:
    fixedImports = fixedImports.replace(pattern, "from \"" & kryonPath & "\"")

  if fixedImports.strip.len > 0:
    wrapperBuilder.add(ensureTrailingNewline(fixedImports))

  let backendImports = buildRendererImports(renderer, basePath)
  if backendImports.len > 0:
    wrapperBuilder.add(ensureTrailingNewline(backendImports))

  # Add main block with backend initialization
  wrapperBuilder.add("\nwhen isMainModule:\n")
  if appDir.len > 0:
    wrapperBuilder.add("  addFontSearchDir(" & nimStringLiteral(appDir) & ")\n")
  if dirExists(assetsDir):
    wrapperBuilder.add("  addFontSearchDir(" & nimStringLiteral(assetsDir) & ")\n")
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

  # Save content hash for future cache checks
  writeFile(hashFile, contentHash)

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

  # Keep wrapper file for incremental compilation (don't clean up)
  # The stable path allows Nim to cache compiled dependencies like raylib

  if exitCode != 0:
    echo "Compilation failed:"
    echo output
    return exitCode

  if verbose:
    echo output

  echo &"✓ Compiled successfully: {outFile}"
  return 0

# ============================================================================
# Development Server
# ============================================================================

proc getMimeType(filename: string): string =
  ## Get MIME type based on file extension
  let ext = filename.splitFile.ext.toLowerAscii()
  case ext:
  of ".html": "text/html; charset=utf-8"
  of ".css": "text/css; charset=utf-8"
  of ".js": "application/javascript; charset=utf-8"
  of ".json": "application/json; charset=utf-8"
  of ".png": "image/png"
  of ".jpg", ".jpeg": "image/jpeg"
  of ".gif": "image/gif"
  of ".svg": "image/svg+xml"
  of ".woff": "font/woff"
  of ".woff2": "font/woff2"
  of ".ttf": "font/ttf"
  of ".otf": "font/otf"
  else: "application/octet-stream"

proc startDevServer(rootDir: string, port: int) {.async.} =
  ## Start a simple HTTP development server
  ## Serves files from rootDir on the specified port

  proc handleRequest(req: Request) {.async.} =
    try:
      var path = req.url.path

      # Default to index.html for root path
      if path == "/" or path == "":
        path = "/index.html"

      # Remove leading slash and construct file path
      let filePath = rootDir / path[1..^1]

      if fileExists(filePath):
        let content = readFile(filePath)
        let mimeType = getMimeType(filePath)
        await req.respond(Http200, content, newHttpHeaders([("Content-Type", mimeType)]))
      else:
        # Try 404.html if it exists
        let notFoundPath = rootDir / "404.html"
        if fileExists(notFoundPath):
          let content = readFile(notFoundPath)
          await req.respond(Http404, content, newHttpHeaders([("Content-Type", "text/html")]))
        else:
          await req.respond(Http404, "404 - File Not Found: " & path)

    except Exception as e:
      await req.respond(Http500, "Internal Server Error: " & e.msg)

  var server = newAsyncHttpServer()

  echo "Serving files from: " & rootDir
  server.listen(Port(port))
  echo "✓ Server running at http://localhost:" & $port
  echo ""

  while true:
    if server.shouldAcceptRequest():
      await server.acceptRequest(handleRequest)
    else:
      await sleepAsync(10)

# ============================================================================
# Run Command
# ============================================================================

proc runKryon*(
  filename: string,
  renderer: string = "auto",
  device: string = "",
  release: bool = false,
  verbose: bool = false,
  serve: bool = false,
  port: int = 8080
): int =
  ## Compile and run a Kryon application
  ##
  ## Args:
  ##   filename: Path to .nim file to run
  ##   renderer: Renderer to use (legacy flag, use -d/--device instead)
  ##   device: Backend to use ("web", "native", "raylib", "sdl2") - Flutter-style
  ##   release: Compile in release mode (optimized)
  ##   verbose: Show detailed output
  ##   serve: Start development server (for web backend)
  ##   port: Port for development server (default: 8080)
  ##
  ## Returns: Exit code

  if not fileExists(filename):
    echo &"Error: File not found: {filename}"
    return 1

  # Determine backend: prioritize -d/--device flag, fall back to --renderer
  let backendValue = if device.len > 0: device else: renderer
  let detectedRenderer = if backendValue == "auto":
    detectRenderer(filename)
  else:
    parseBackendOrRenderer(backendValue, filename)

  # For web backend, default to serving unless explicitly disabled
  var shouldServe = serve
  if detectedRenderer == rHTML and not serve:
    # Web backend defaults to serving for better developer experience
    shouldServe = true
    if verbose:
      echo "Note: Dev server enabled by default for web backend"
      echo "      Use 'kryon build -d web' to generate files only"

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

    # Determine Kryon installation path
    # First check if we're in the kryon dev directory
    let devKryonPath = currentDir / "src" / "kryon"
    let isDevMode = dirExists(devKryonPath)

    var fixedImports = importsContent
    var htmlGeneratorCode = "import os, strformat\n"

    if isDevMode:
      # Development mode - use local sources
      fixedImports = fixedImports.replace("import ../src/kryon", "import \"" & devKryonPath & "\"")
      fixedImports = fixedImports.replace("import ../kryon/src/kryon", "import \"" & devKryonPath & "\"")
      htmlGeneratorCode &= "import \"" & currentDir & "/src/kryon\"\n"
      htmlGeneratorCode &= "import \"" & currentDir & "/src/backends/integration/html\"\n"
    else:
      # Production mode - use installed Kryon
      fixedImports = fixedImports.replace("import ../src/kryon", "import kryon")
      fixedImports = fixedImports.replace("import ../kryon/src/kryon", "import kryon")
      htmlGeneratorCode &= "import kryon\n"
      htmlGeneratorCode &= "import kryon/backends/integration/html\n"
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

    # Get kryon installation path if not in dev mode
    var genCmd = "nim c -r "
    if isDevMode:
      genCmd &= "-d:kryonDevMode "
    else:
      # Use the locally installed kryon from ~/.local/include
      let kryonIncludeDir = getHomeDir() / ".local" / "include"
      if dirExists(kryonIncludeDir / "kryon"):
        genCmd &= "--path:\"" & kryonIncludeDir & "\" "
    genCmd &= tempFile
    let (output, exitCode) = execCmdEx(genCmd)

    # Cleanup
    if fileExists(tempFile):
      removeFile(tempFile)

    if exitCode == 0:
      echo ""
      echo "✓ HTML web app generated successfully!"

      # Start development server if enabled (default for web)
      if shouldServe:
        echo ""
        echo "Starting development server..."
        echo "Server: http://localhost:" & $port
        echo "Press Ctrl+C to stop"
        echo ""

        try:
          waitFor startDevServer(outputDir, port)
          return 0
        except Exception as e:
          echo "Server error: " & e.msg
          return 1
      else:
        # Only reached if user explicitly disabled serving
        echo "Open " & outputDir / "index.html" & " in your browser to view the app."
        echo ""
        echo "Tip: Development server is enabled by default"
        echo "     Just run: kryon run -d web -f " & filename

      if verbose:
        echo output
    else:
      echo "✗ HTML generation failed"
      echo ""
      echo "Error details:"
      echo "─".repeat(60)
      echo output
      echo "─".repeat(60)
      echo ""
      echo "Common issues:"
      echo "  • Check for syntax errors in your Nim code"
      echo "  • Ensure all imports are correct"
      echo "  • Verify the file path is valid"
      echo ""
      echo "Tip: Use --verbose flag for more details"

    return exitCode

  of rTerminal:
    echo "Error: Terminal renderer not yet implemented"
    return 1
  else:
    discard

  # Use stable output path in .kryon directory for better incremental compilation
  let absoluteFilename = if filename.isAbsolute():
    filename
  else:
    getCurrentDir() / filename
  let (appDir, baseName, _) = absoluteFilename.splitFile
  let projectRoot = if appDir.len > 0: appDir else: getCurrentDir()
  let kryonDir = projectRoot / ".kryon"
  if not dirExists(kryonDir):
    createDir(kryonDir)
  let outputFile = kryonDir / baseName

  # Compile
  let compileResult = compileKryon(
    filename,
    detectedRenderer,
    outputFile,
    release,
    verbose
  )

  if compileResult != 0:
    return compileResult

  # Run
  echo ""
  echo "Running..."
  echo "─".repeat(60)

  let runResult = execCmd(outputFile)

  # Keep binary for faster subsequent runs (don't cleanup)

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
      echo &"Available renderers: auto, raylib, sdl2, skia, html, terminal"
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

    # Determine Kryon installation path
    # First check if we're in the kryon dev directory
    let devKryonPath = currentDir / "src" / "kryon"
    let isDevMode = dirExists(devKryonPath)

    var fixedImports = importsContent
    var htmlGeneratorCode = "import os, strformat\n"

    if isDevMode:
      # Development mode - use local sources
      fixedImports = fixedImports.replace("import ../src/kryon", "import \"" & devKryonPath & "\"")
      fixedImports = fixedImports.replace("import ../kryon/src/kryon", "import \"" & devKryonPath & "\"")
      htmlGeneratorCode &= "import \"" & currentDir & "/src/kryon\"\n"
      htmlGeneratorCode &= "import \"" & currentDir & "/src/backends/integration/html\"\n"
    else:
      # Production mode - use installed Kryon
      fixedImports = fixedImports.replace("import ../src/kryon", "import kryon")
      fixedImports = fixedImports.replace("import ../kryon/src/kryon", "import kryon")
      htmlGeneratorCode &= "import kryon\n"
      htmlGeneratorCode &= "import kryon/backends/integration/html\n"
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

    # Get kryon installation path if not in dev mode
    var genCmd = "nim c -r "
    if isDevMode:
      genCmd &= "-d:kryonDevMode "
    else:
      # Use the locally installed kryon from ~/.local/include
      let kryonIncludeDir = getHomeDir() / ".local" / "include"
      if dirExists(kryonIncludeDir / "kryon"):
        genCmd &= "--path:\"" & kryonIncludeDir & "\" "
    genCmd &= tempFile
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
# Devices Command (Flutter-style)
# ============================================================================

proc devicesCmd*(): int =
  ## List available backends/devices (Flutter-style)
  echo "Kryon backends (devices):"
  echo ""
  echo "web • HTML/CSS/JS"
  echo "  - Generates static web applications"
  echo "  - Use: kryon run -d web <file>"
  echo "  - Serve: kryon run -d web --serve <file>"
  echo ""
  echo "native • Desktop"
  echo "  - Auto-selects raylib or sdl2"
  echo "  - Use: kryon run -d native <file>"
  echo ""
  echo "raylib • Raylib Desktop"
  echo "  - 60 FPS desktop applications"
  echo "  - Use: kryon run -d raylib <file>"
  echo ""
  echo "sdl2 • SDL2 Desktop"
  echo "  - Cross-platform desktop"
  echo "  - Use: kryon run -d sdl2 <file>"
  echo ""
  echo "Run 'kryon devices' to see this list anytime"
  return 0

# ============================================================================
# Version Command
# ============================================================================

proc versionCmd*(): int =
  ## Show Kryon version
  echo &"Kryon v{VERSION}"
  echo "A declarative UI framework for Nim"
  echo ""
  echo "Features:"
  echo "  ✓ Flutter-like declarative UI"
  echo "  ✓ Multiple backends (web, desktop)"
  echo "  ✓ Full Canvas API support"
  echo "  ✓ Built-in development server"
  echo "  ✓ Hot-serve capability"
  echo "  ✓ Reactive state management"
  echo ""
  echo "Backends:"
  echo "  • web      - HTML/CSS/JS with Canvas"
  echo "  • native   - Auto-select desktop"
  echo "  • raylib   - Desktop (60 FPS)"
  echo "  • sdl2     - Desktop (cross-platform)"
  echo ""
  echo "Quick Commands:"
  echo "  kryon devices       - List backends"
  echo "  kryon help          - Show help"
  echo "  kryon run -d web    - Run web backend"
  return 0

# ============================================================================
# Main CLI Entry Point
# ============================================================================

when isMainModule:
  import cligen

  # Note: cligen 1.9.3 doesn't easily support making --filename positional
  # The workaround is to make filename the first required parameter with a short -f flag
  # Users can use: kryon run -f main.nim or kryon run main.nim (if we suppress the flag)

  # Configure to allow bare arguments
  clCfg.reqSep = false

  # Set custom usage message
  clCfg.useMulti = """Kryon CLI Tool - A Flutter-like UI Framework for Nim
Usage:
  $command {SUBCMD}  [sub-command options & parameters]

Quick Start:
  kryon run -d web -f app.nim           # Web (dev server auto-starts)
  kryon run -d native -f app.nim        # Desktop app
  kryon devices                         # List backends
  kryon version                         # Show version

where {SUBCMD} is one of:
$subcmds
$command {-h|--help} or with no args at all prints this message.
$command --help-syntax gives general cligen syntax help.
Run "$command {help SUBCMD|SUBCMD --help}" to see help for just SUBCMD.
Run "$command help" to get *comprehensive* help.
"""

  dispatchMulti(
    [runKryon, cmdName = "run",
     help = {
       "filename": "Path to .nim file to run",
       "renderer": "Renderer to use (legacy, use -d instead)",
       "device": "Backend device: web, native, raylib, sdl2 (Flutter-style)",
       "release": "Compile in release mode (optimized)",
       "verbose": "Show detailed compilation output",
       "serve": "Start dev server (default=true for web, use --serve=false to disable)",
       "port": "Port for development server (default: 8080)"
     },
     short = {
       "filename": 'f',
       "renderer": 'r',
       "device": 'd',
       "release": 'R',
       "verbose": 'v',
       "serve": 's',
       "port": 'p'
     }
    ],
    [buildKryon, cmdName = "build",
     help = {
       "filename": "Path to .nim file to compile",
       "renderer": "Renderer to use (auto, raylib, sdl2, skia, html, terminal)",
       "output": "Output executable path",
       "release": "Compile in release mode (default: true)",
       "verbose": "Show detailed compilation output"
     },
     short = {
       "filename": 'f',
       "renderer": 'r',
       "output": 'o',
       "release": 'R',
       "verbose": 'v'
     }
    ],
    [infoKryon, cmdName = "info",
     help = {
       "filename": "Path to .nim file to analyze"
     },
     short = {
       "filename": 'f'
     }
    ],
    [initCmd, cmdName = "init",
     help = {
       "templateName": "Template to use (basic, todo, game, calculator)",
       "projectName": "Optional project name (defaults to template name)"
     }
    ],
    [doctorCmd, cmdName = "doctor",
     help = "Check system dependencies and installation status"
    ],
    [devicesCmd, cmdName = "devices",
     help = "List available backends (Flutter-style)"
    ],
    [versionCmd, cmdName = "version"]
  )
