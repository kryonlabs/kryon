## Kryon CLI - The Unified Orchestrator
##
## Single interface for all Kryon development workflows
## From project creation to microcontroller deployment

import os, osproc, strutils, parseopt, times, json
import std/[sequtils, sugar]

# Compile-time backend selection flags
# These allow building lightweight CLI variants for CI/CD
# Use -d:terminalOnly for terminal-only build (no SDL3 deps)
# Use -d:webBackend for web-only build (no SDL3 deps)
# Use -d:desktopBackend for desktop build (requires SDL3)
const
  hasTerminalBackend* = defined(terminalOnly) or defined(staticBackend) or not (defined(desktopOnly) or defined(webBackend))
  hasDesktopBackend* = defined(desktopBackend) or defined(staticBackend) or not (defined(terminalOnly) or defined(webBackend))
  hasWebBackend* = defined(webBackend) or defined(staticBackend)
  hasRaylibBackend* = defined(raylibBackend) or defined(staticBackend)

# CLI modules
import project, build, device, compile, diff, inspect, config, codegen, codegen_tsx, codegen_jsx, html_transpiler, plugin_manager, kir_to_c
import kry_ast, kry_lexer, kry_parser, kry_to_kir, kyt_parser, tsx_parser, md_parser, html_parser
from html_transpiler import transpileKirToHTML, defaultTranspilationOptions, defaultDisplayOptions

# IR and backend bindings (for orchestration)
import ../bindings/nim/ir_core
when hasDesktopBackend:
  import ../bindings/nim/ir_desktop
  import ../bindings/nim/runtime  # Provides Nim bridge functions for desktop backend
import ../bindings/nim/ir_serialization  # For loading .kir files
import ../bindings/nim/ir_logic  # Logic block bindings
import ../bindings/nim/ir_executor  # Executor for .kir handler execution
import ../bindings/nim/ir_krb  # KRB bytecode format

const
  VERSION = "1.2.0"
  # Store the source directory at compile time
  KRYON_SOURCE_DIR = staticExec("cd " & currentSourcePath().parentDir() & "/.. && pwd").strip()
  HELP_TEXT = """
Kryon CLI v""" & VERSION & """ - Unified Development Orchestrator

USAGE:
  kryon <command> [options] [args]

COMMANDS:
  new <name>          Create new project with template
  init <name>         Initialize new project with kryon.toml
  build [targets]      Compile for specified targets
  compile <file>      Compile to IR with caching
  parse <file.kry>    Parse .kry file to .kir (JSON IR)
  parse-html <file>   Parse HTML file to .kir (roundtrip)
  codegen <file.kir>  Generate source code from .kir file
  convert <in> <out>  Convert .kir (JSON) to .kirb (binary)
  krb <file> -o <out>  Compile .kir to .krb bytecode
  disasm <file.krb>    Disassemble .krb bytecode file
  run <target>         Build + execute once
  dev <file>           Development mode with hot reload
  config [show|validate]  Show or validate project configuration
  validate <file>     Validate IR file format
  inspect             Component tree visualization
  inspect-ir <file>   Inspect compiled IR file
  inspect-detailed <file>  Detailed IR analysis with stats
  tree <file>         Show component tree structure
  diff <file1> <file2>  Compare two IR files
  plugin <cmd>        Manage plugins (add, remove, list, update, info)
  add-backend <name>  Integrate new renderer backend
  doctor              Diagnose environment issues

TARGETS:
  stm32f4, rp2040, linux, windows, macos, web

TEMPLATES:
  nim, lua, c, js, ts

EXAMPLES:
  kryon init myapp --template=js
  kryon new myapp --template=nim
  kryon build --targets=stm32f4,linux
  kryon run examples/nim/hello_world.nim
  kryon run app.kry --renderer=terminal
  kryon dev examples/nim/habits.nim
  kryon config show
  kryon inspect --target=stm32f4

For detailed help: kryon <command> --help
"""

proc findInstalledKryon*(): tuple[found: bool, libDir: string, includeDir: string] =
  ## Check if kryon is installed in ~/.local
  let homeLocal = getHomeDir() / ".local"
  let libFile = homeLocal / "lib" / "libkryon.a"
  let includeDir = homeLocal / "include" / "kryon"
  if fileExists(libFile) and dirExists(includeDir):
    return (true, homeLocal / "lib", includeDir)
  return (false, "", "")

proc findKryonRoot*(): string =
  ## Resolve the Kryon root directory even when running outside the repo.
  ## Priority: KRYON_ROOT env -> walk up from cwd looking for core/include/kryon.h ->
  ##           walk up from the CLI executable location -> fallback to cwd.
  let envRoot = getEnv("KRYON_ROOT")
  if envRoot.len > 0 and dirExists(envRoot):
    return envRoot

  proc searchUp(start: string): string =
    var cur = absolutePath(start)
    while true:
      # Check for multiple markers to identify Kryon root
      if fileExists(cur / "core" / "include" / "kryon.h") or
         fileExists(cur / "ir" / "ir_core.h") or
         (dirExists(cur / "bindings" / "lua") and dirExists(cur / "ir")):
        return cur
      let parent = parentDir(cur)
      if parent.len == 0 or parent == cur: break
      cur = parent
    result = ""

  let fromCwd = searchUp(getCurrentDir())
  if fromCwd.len > 0:
    return fromCwd

  let exeDir = parentDir(getAppFilename())
  let fromExe = searchUp(exeDir)
  if fromExe.len > 0:
    return fromExe

  # Use the compile-time source directory
  if dirExists(KRYON_SOURCE_DIR) and
     (dirExists(KRYON_SOURCE_DIR / "bindings" / "lua") or
      fileExists(KRYON_SOURCE_DIR / "ir" / "ir_core.h")):
    return KRYON_SOURCE_DIR

  # Last resort: current dir
  return getCurrentDir()

proc pkgLibFlags*(pkg: string): seq[string] =
  ## Best-effort retrieval of linker flags from pkg-config
  try:
    let (stdout, code) = execCmdEx("pkg-config --libs " & pkg)
    if code == 0:
      result = stdout.strip.splitWhitespace()
  except:
    discard

# =============================================================================
# IR Rendering Helper
# =============================================================================

when hasDesktopBackend:
  proc renderIRFileDesktop*(kirPath: string, title: string = "Kryon App",
                            width: int = 800, height: int = 600): bool =
    ## Unified IR rendering - loads .kir/.kirb and renders with SDL3 backend
    ## Returns true on success, false on failure
    let ctx = ir_create_context()
    ir_set_context(ctx)

    # Load IR tree (supports both .kir JSON and .kirb binary)
    # Try format based on extension first, then fallback to the other format
    var irRoot: ptr IRComponent = nil
    if kirPath.endsWith(".kirb"):
      # Try binary first
      irRoot = ir_serialization.ir_read_binary_file(cstring(kirPath))
      if irRoot.isNil:
        # Fallback to JSON
        irRoot = ir_read_json_v2_file(cstring(kirPath))
    else:
      # Try JSON first
      irRoot = ir_read_json_v2_file(cstring(kirPath))
      if irRoot.isNil:
        # Fallback to binary
        irRoot = ir_serialization.ir_read_binary_file(cstring(kirPath))

    if irRoot.isNil:
      echo "❌ Failed to load IR: " & kirPath
      return false

    # Set up executor for .kir files with logic blocks
    var executor: ptr IRExecutorContext = nil
    if not kirPath.endsWith(".kirb"):
      executor = ir_executor_create()
      if executor != nil:
        if ir_executor_load_kir_file(executor, cstring(kirPath)):
          ir_executor_set_global(executor)
          # Set the root component so conditionals can update visibility
          ir_executor_set_root(executor, irRoot)
          # Apply initial conditionals based on initial variable values
          ir_executor_apply_initial_conditionals(executor)
        else:
          ir_executor_destroy(executor)
          executor = nil

    var config = desktop_renderer_config_sdl3(width.cint, height.cint, cstring(title))

    # Create renderer manually to allow router initialization for multi-page apps
    let renderer = desktop_ir_renderer_create(addr config)
    if renderer.isNil:
      echo "❌ Failed to create renderer"
      return false

    # Initialize page router for multi-page projects
    # Check if .kryon_cache exists (indicates multi-page build system)
    let cacheDir = getCurrentDir() / ".kryon_cache"
    let buildIRDir = getCurrentDir() / "build" / "ir"
    if dirExists(cacheDir) or dirExists(buildIRDir):
      # Multi-page project - initialize router
      let irDir = if dirExists(buildIRDir): buildIRDir & "/" else: cacheDir & "/"
      desktop_init_router(cstring("/"), cstring(irDir), irRoot, renderer)

    # Run main loop
    let renderSuccess = desktop_ir_renderer_run_main_loop(renderer, irRoot)

    # Cleanup renderer
    desktop_ir_renderer_destroy(renderer)

    # Cleanup executor
    if executor != nil:
      ir_executor_set_global(nil)
      ir_executor_destroy(executor)

    ir_destroy_component(irRoot)

    return renderSuccess

# =============================================================================
# Raylib Backend Rendering
# =============================================================================

when hasRaylibBackend:
  # Raylib C backend FFI
  {.passL: "-lraylib -lm".}
  {.passC: "-I" & KRYON_SOURCE_DIR & "/renderers/raylib".}

  type
    kryon_raylib_app_config_t* {.importc, header: "raylib_backend.h", bycopy.} = object
      window_width*: uint16
      window_height*: uint16
      window_title*: cstring
      target_fps*: cint
      background_color*: uint32

    kryon_raylib_app_t* {.importc, header: "raylib_backend.h", incompleteStruct.} = object

  proc kryon_raylib_app_create*(config: ptr kryon_raylib_app_config_t): ptr kryon_raylib_app_t {.
    importc, header: "raylib_backend.h".}
  proc kryon_raylib_app_destroy*(app: ptr kryon_raylib_app_t) {.
    importc, header: "raylib_backend.h".}
  proc kryon_raylib_app_handle_events*(app: ptr kryon_raylib_app_t): bool {.
    importc, header: "raylib_backend.h".}
  proc kryon_raylib_app_begin_frame*(app: ptr kryon_raylib_app_t) {.
    importc, header: "raylib_backend.h".}
  proc kryon_raylib_app_end_frame*(app: ptr kryon_raylib_app_t) {.
    importc, header: "raylib_backend.h".}

  # NativeCanvas callback invocation
  proc ir_native_canvas_invoke_callback*(component_id: uint32): bool {.
    importc, header: "../../ir/ir_native_canvas.h".}

  proc renderIRFileRaylib*(kirPath: string, title: string = "Kryon App",
                           width: int = 800, height: int = 600): bool =
    ## Render IR file using Raylib backend
    ## Returns true on success, false on failure
    let ctx = ir_create_context()
    ir_set_context(ctx)

    # Load IR tree
    var irRoot: ptr IRComponent = nil
    if kirPath.endsWith(".kirb"):
      irRoot = ir_serialization.ir_read_binary_file(cstring(kirPath))
      if irRoot.isNil:
        irRoot = ir_serialization.ir_read_json_v2_file(cstring(kirPath))
    else:
      irRoot = ir_serialization.ir_read_json_v2_file(cstring(kirPath))
      if irRoot.isNil:
        irRoot = ir_serialization.ir_read_binary_file(cstring(kirPath))

    if irRoot.isNil:
      echo "❌ Failed to load IR: " & kirPath
      return false

    # Set up executor for logic blocks
    var executor: ptr IRExecutorContext = nil
    if not kirPath.endsWith(".kirb"):
      executor = ir_executor_create()
      if executor != nil:
        if ir_executor_load_kir_file(executor, cstring(kirPath)):
          ir_executor_set_global(executor)
          ir_executor_set_root(executor, irRoot)
          ir_executor_apply_initial_conditionals(executor)
        else:
          ir_executor_destroy(executor)
          executor = nil

    # Create raylib app
    var config = kryon_raylib_app_config_t(
      window_width: width.uint16,
      window_height: height.uint16,
      window_title: cstring(title),
      target_fps: 60,
      background_color: 0x1a1a2eFF'u32
    )

    let app = kryon_raylib_app_create(addr config)
    if app.isNil:
      echo "❌ Failed to create Raylib app"
      return false

    # Main rendering loop
    while kryon_raylib_app_handle_events(app):
      kryon_raylib_app_begin_frame(app)

      # Invoke NativeCanvas callback for the root component
      discard ir_native_canvas_invoke_callback(irRoot.id)

      kryon_raylib_app_end_frame(app)

    # Cleanup
    kryon_raylib_app_destroy(app)

    if executor != nil:
      ir_executor_set_global(nil)
      ir_executor_destroy(executor)

    ir_destroy_component(irRoot)

    return true

# =============================================================================
# Unified IR Renderer Dispatcher
# =============================================================================

proc renderIRFile*(kirPath: string, title: string = "Kryon App",
                   width: int = 800, height: int = 600): bool =
  ## Unified IR rendering dispatcher - checks KRYON_RENDERER and delegates
  let renderer = getEnv("KRYON_RENDERER", "sdl3")

  case renderer.toLowerAscii()
  of "raylib":
    when hasRaylibBackend:
      return renderIRFileRaylib(kirPath, title, width, height)
    else:
      echo "❌ Raylib backend not available in this build"
      echo "   Use kryon-raylib for Raylib/3D rendering."
      return false

  of "sdl3", "desktop", "":
    when hasDesktopBackend:
      return renderIRFileDesktop(kirPath, title, width, height)
    else:
      echo "❌ Desktop backend not available in this build"
      echo "   Use kryon-desktop for SDL3 rendering."
      return false

  else:
    echo "❌ Unknown renderer: " & renderer
    echo "   Available: sdl3, raylib"
    return false

# =============================================================================
# Command Line Interface
# =============================================================================

proc showHelp*() =
  echo HELP_TEXT
  quit(0)

proc showVersion*() =
  echo "Kryon CLI v" & VERSION
  echo "License: 0BSD"
  echo "Architecture: C99 ABI + Multi-language Frontends"
  quit(0)

proc handleNewCommand*(args: seq[string]) =
  ## Handle 'kryon new' command
  if args.len == 0:
    echo "Error: Project name required"
    echo "Usage: kryon new <name> [--template=<template>]"
    quit(1)

  let projectName = args[0]
  var projectTemplate = "nim"  # Default template

  # Parse template option
  for arg in args[1..^1]:
    if arg.startsWith("--template="):
      projectTemplate = arg[11..^1]

  echo "Creating new Kryon project: " & projectName
  echo "Template: " & projectTemplate

  try:
    createProject(projectName, projectTemplate)
    echo "✓ Project created successfully!"
    echo ""
    echo "Next steps:"
    echo "  cd " & projectName
    echo "  kryon build --target=linux"
  except:
    echo "✗ Failed to create project: " & getCurrentExceptionMsg()
    quit(1)

proc handleInitCommand*(args: seq[string]) =
  ## Handle 'kryon init' command - creates project with kryon.toml
  if args.len == 0:
    echo "Error: Project name required"
    echo "Usage: kryon init <name> [--template=<template>]"
    quit(1)

  let projectName = args[0]
  var projectTemplate = "js"  # Default to JavaScript

  # Parse template option
  for arg in args[1..^1]:
    if arg.startsWith("--template="):
      projectTemplate = arg[11..^1]

  echo "Initializing new Kryon project: " & projectName
  echo "Template: " & projectTemplate

  try:
    # Create project directory
    createDir(projectName)

    # Create kryon.toml configuration
    var config = defaultConfig()
    config.projectName = projectName
    config.buildFrontend = projectTemplate

    # Set target based on template
    if projectTemplate in ["js", "ts"]:
      config.buildTarget = "web"
    else:
      config.buildTarget = "desktop"

    config.metadataCreated = now().format("yyyy-MM-dd")

    writeTomlConfig(config, projectName / "kryon.toml")

    echo "✓ Project initialized successfully!"
    echo "✓ Created " & projectName / "kryon.toml"
    echo ""
    echo "Next steps:"
    echo "  cd " & projectName
    echo "  # Create your " & projectTemplate & " files"
    echo "  kryon build"
  except:
    echo "✗ Failed to initialize project: " & getCurrentExceptionMsg()
    quit(1)

proc handleConfigCommand*(args: seq[string]) =
  ## Handle 'kryon config' command - show or validate configuration
  if args.len == 0:
    # Show current config summary
    try:
      let config = loadProjectConfig()
      echo "Project Configuration:"
      echo "  Name: " & config.projectName
      echo "  Version: " & config.projectVersion
      echo "  Target: " & config.buildTarget
      echo "  Output: " & config.buildOutputDir
      echo ""
      echo "Use 'kryon config show' for full configuration"
      echo "Use 'kryon config validate' to check for errors"
    except:
      echo "No configuration file found (kryon.toml or kryon.json)"
      echo "Use 'kryon init <name>' to create a new project"
    return

  case args[0].toLowerAscii()
  of "show":
    try:
      let config = loadProjectConfig()
      let configJson = configToJson(config)
      echo pretty(configJson, indent=2)
    except:
      echo "✗ Failed to load configuration: " & getCurrentExceptionMsg()
      quit(1)

  of "validate":
    try:
      let configPath = findProjectConfig()
      if configPath.len == 0:
        echo "✗ No configuration file found"
        quit(1)

      echo "Validating: " & configPath
      let config = loadProjectConfig()
      let (valid, errors) = validateConfig(config)

      if valid:
        echo "✓ Configuration is valid"
      else:
        echo "✗ Configuration has errors:"
        for error in errors:
          echo "  - " & error
        quit(1)
    except:
      echo "✗ Validation failed: " & getCurrentExceptionMsg()
      quit(1)

  else:
    echo "Unknown config command: " & args[0]
    echo "Usage: kryon config [show|validate]"
    quit(1)

proc handleBuildCommand*(args: seq[string]) =
  ## Handle 'kryon build' command
  var targets: seq[string] = @[]
  var sourceFile = ""

  # First, try to read targets from config
  try:
    let cfg = loadConfig()
    if cfg.buildTargets.len > 0:
      targets = cfg.buildTargets
    elif cfg.buildTarget != "" and cfg.buildTarget != "desktop":
      targets = @[cfg.buildTarget]
  except:
    discard

  # Command-line args override config
  if args.len > 0:
    for arg in args:
      if arg.startsWith("--targets="):
        targets = arg[10..^1].split(",")
      elif arg.startsWith("--target="):
        targets = @[arg[9..^1]]
      elif not arg.startsWith("-"):
        # Non-option argument is the source file
        sourceFile = arg

  # Default to linux if still empty
  if targets.len == 0:
    targets = @["linux"]

  # If source file is provided, build that specific file
  if sourceFile != "":
    if not fileExists(sourceFile):
      echo "✗ Source file not found: " & sourceFile
      quit(1)

    echo "Building source file: " & sourceFile
    echo "Targets: " & targets.join(", ")

    try:
      for target in targets:
        echo "Building for " & target & "..."
        buildWithIR(sourceFile, target)
        echo "✓ " & target & " build complete"
      echo ""
      echo "Build artifacts available in build/"
    except:
      echo "✗ Build failed: " & getCurrentExceptionMsg()
      quit(1)
  else:
    # Project-based build (auto-detect source file)
    echo "Building Kryon project for targets: " & targets.join(", ")

    try:
      for target in targets:
        echo "Building for " & target & "..."
        buildForTarget(target)
        echo "✓ " & target & " build complete"
      echo ""
      echo "Build artifacts available in build/"
    except:
      echo "✗ Build failed: " & getCurrentExceptionMsg()
      quit(1)

proc detectFileWithExtensions*(baseName: string): tuple[found: bool, path: string, ext: string] =
  ## Try to find a file with common Kryon frontend extensions
  const extensions = [".kry", ".lua", ".nim", ".md", ".markdown", ".ts", ".tsx", ".js", ".jsx", ".kir", ".c"]

  # If the file already has an extension, check if it exists
  if baseName.contains('.'):
    if fileExists(baseName):
      let ext = splitFile(baseName).ext
      return (true, baseName, ext)

  # Try each extension
  for ext in extensions:
    let testPath = baseName & ext
    if fileExists(testPath):
      return (true, testPath, ext)

  # Not found
  return (false, "", "")

proc handleRunCommand*(args: seq[string]) =
  ## Handle 'kryon run' command - Multi-frontend support
  # Check for help flag
  for arg in args:
    if arg == "--help" or arg == "-h":
      echo """
Usage:
  kryon run [<file>] [--target=<target>] [--renderer=<renderer>] [--port=<port>]

Arguments:
  <file>        Path to source file (default: main)

Options:
  --target      Target platform (native, web)
  --renderer    Rendering backend (sdl3, terminal) - defaults to sdl3
  --port        Port for web server (default: 3000)

Environment Variables:
  KRYON_RENDERER   Rendering backend (overridden by --renderer flag)

Description:
  Compile and run a Kryon application. Automatically detects the frontend
  type from the file extension and uses the appropriate compilation pipeline.

Examples:
  kryon run app.kry                      # Run with SDL3 renderer (default)
  kryon run app.kry --renderer=terminal  # Run with terminal renderer
  kryon run app.nim --renderer=sdl3      # Run Nim app with SDL3
  kryon run app.ts --target=web          # Run TypeScript app in browser
"""
      quit(0)

  var target = "native"
  var renderer = ""  # CLI flag for --renderer (overrides KRYON_RENDERER env var)
  var file = ""
  var baseName = ""
  var hasWebTarget = false
  var webPort = 3000
  var webOutputDir = "build"

  # Try to load config to get defaults
  var config = defaultConfig()  # Declare outside try block for later access
  try:
    config = loadProjectConfig()
    # Get entry point: use build.entry, or first page file, or default to main
    if config.buildEntry != "":
      baseName = config.buildEntry
    elif config.buildPages.len > 0:
      baseName = config.buildPages[0].file
    else:
      baseName = "main"

    if target == "native":
      target = config.buildTarget
    # Check if this is a web project
    if "web" in config.buildTargets or config.buildTarget == "web":
      hasWebTarget = true
      webPort = config.devPort
      webOutputDir = config.buildOutputDir
  except:
    # No config file, use defaults (already set above)
    discard

  # Parse command-line arguments (override config)
  for arg in args:
    if arg.startsWith("--target="):
      target = arg[9..^1]
      # Validate target value - NO FALLBACKS
      if target == "web":
        hasWebTarget = true
      elif target == "native" or target == "sdl3" or target == "terminal":
        # Accept sdl3/terminal as aliases for native (they specify renderer)
        hasWebTarget = false
        target = "native"
      else:
        echo "❌ Invalid target: " & target
        echo "   Valid targets: native, web"
        echo "   (Use --renderer=sdl3 or --renderer=terminal for native rendering)"
        quit(1)
    elif arg.startsWith("--port="):
      webPort = parseInt(arg[7..^1])
    elif arg.startsWith("--renderer="):
      renderer = arg[11..^1]  # Extract value after "--renderer="
      # Validate renderer value (add new renderers here when implemented)
      if renderer != "sdl3" and renderer != "terminal":
        echo "❌ Invalid renderer: " & renderer
        echo "   Valid options: sdl3, terminal"
        quit(1)
      # Force native mode when --renderer is specified (override config)
      hasWebTarget = false
      target = "native"
    elif arg.startsWith("--device="):
      discard  # For compatibility
    elif not arg.startsWith("-") and arg != "run":
      baseName = arg
      # If arg has an extension, use it directly
      if arg.contains('.'):
        file = arg
      else:
        # Will detect extension below
        file = ""

  # Handle web projects - build and serve
  if hasWebTarget:
    echo "🌐 Web project detected"
    echo "📦 Building for web target..."

    # Build for web
    try:
      buildForTarget("web")
    except Exception as e:
      echo "❌ Build failed: " & e.msg
      quit(1)

    # Start web server
    echo ""
    echo "🚀 Starting development server..."
    echo "   URL: http://localhost:" & $webPort
    echo "   Directory: " & webOutputDir
    echo ""
    echo "   Press Ctrl+C to stop the server"
    echo ""

    # Use Python's http.server for cross-platform compatibility
    let serverCmd = "python3 -m http.server " & $webPort & " --directory " & webOutputDir
    let exitCode = execCmd(serverCmd)
    if exitCode != 0:
      # Try python instead of python3
      let serverCmd2 = "python -m http.server " & $webPort & " --directory " & webOutputDir
      discard execCmd(serverCmd2)
    return

  # If no entry file specified, try "main" as fallback
  if baseName.len == 0:
    baseName = "main"

  # Auto-detect file with extension if not explicitly provided
  if file == "" or not fileExists(file):
    let detected = detectFileWithExtensions(baseName)
    if detected.found:
      file = detected.path
      let ext = detected.ext
      echo "🔍 Detected " & ext & " file: " & file
    else:
      echo "❌ Could not find file: " & baseName
      echo "   Tried extensions: .kry, .lua, .nim, .md, .markdown, .ts, .tsx, .js, .jsx, .kir, .c"
      quit(1)

  # Determine frontend type from extension
  let (_, _, ext) = splitFile(file)
  let frontend = ext.toLowerAscii()

  echo "🚀 Running Kryon application..."
  echo "📁 File: " & file
  echo "🎨 Frontend: " & frontend
  echo "🎯 Target: " & target

  # Handle non-Nim frontends
  if frontend == ".lua":
    # Check for LuaJIT
    let (_, luajitCheck) = execCmdEx("which luajit")
    if luajitCheck != 0:
      echo "❌ LuaJIT not found!"
      echo "   Install LuaJIT or run in nix-shell:"
      echo "   nix-shell -p luajit --run 'kryon run " & file & "'"
      quit(1)

    # Run Lua file with IR capture and rendering
    echo "🌙 Running Lua application with LuaJIT..."

    # Set up library path
    let kryonRoot = findKryonRoot()
    let buildDir = kryonRoot / "build"

    putEnv("KRYON_LIB_PATH", buildDir / "libkryon_ir.so")

    # Add plugin build directories to LD_LIBRARY_PATH
    # Prioritize installed libraries over build artifacts to ensure single library instance
    let homeDir = getEnv("HOME")
    var pluginLibraryPaths = homeDir / ".local/lib"  # Installed libraries first

    # Add build directories as fallback for development
    pluginLibraryPaths.add(":" & buildDir)
    for plugin in config.plugins:
      if plugin.source == psPath:
        let pluginPath = if plugin.path.startsWith(".") or plugin.path.startsWith(".."):
          getCurrentDir() / plugin.path
        else:
          plugin.path

        let pluginBuildDir = pluginPath / "build"
        if dirExists(pluginBuildDir):
          pluginLibraryPaths.add(":" & pluginBuildDir)

    putEnv("LD_LIBRARY_PATH", pluginLibraryPaths & ":" & getEnv("LD_LIBRARY_PATH"))

    # Set Lua module path so require("kryon") works from any directory
    let luaBindingsDir = kryonRoot / "bindings" / "lua"
    let existingPath = getEnv("LUA_PATH")

    # Add plugin bindings paths to LUA_PATH
    var pluginLuaPaths = ""
    for plugin in config.plugins:
      if plugin.source == psPath:
        # Resolve relative paths from current directory
        let pluginPath = if plugin.path.startsWith(".") or plugin.path.startsWith(".."):
          getCurrentDir() / plugin.path
        else:
          plugin.path

        let pluginBindingsDir = pluginPath / "bindings" / "lua"
        if dirExists(pluginBindingsDir):
          pluginLuaPaths.add(pluginBindingsDir / "?.lua;")
          pluginLuaPaths.add(pluginBindingsDir / "?/init.lua;")

    let luaPath = luaBindingsDir / "?.lua;" &
                  luaBindingsDir / "?/init.lua;" &
                  pluginLuaPaths &
                  (if existingPath.len > 0: existingPath else: ";")
    putEnv("LUA_PATH", luaPath)

    # Create wrapper script to capture IR
    let wrapperScript = "/tmp/kryon_wrapper_" & $getCurrentProcessId() & ".lua"

    let absPath = if file.isAbsolute(): file else: getCurrentDir() / file

    # Build plugin loading code
    var pluginLoadCode = ""
    if config.plugins.len > 0:
      pluginLoadCode = "print('📦 Loading ' .. " & $config.plugins.len & " .. ' plugin(s)...')\n"
      for plugin in config.plugins:
        if plugin.source == psPath:
          let pluginName = plugin.name
          pluginLoadCode.add("  print('  Loading " & pluginName & "...')\n")
          pluginLoadCode.add("  local " & pluginName & "_plugin = require('" & pluginName & "')\n")
          pluginLoadCode.add("  if " & pluginName & "_plugin and " & pluginName & "_plugin.init then\n")
          pluginLoadCode.add("    " & pluginName & "_plugin.init()\n")
          pluginLoadCode.add("  end\n")
          pluginLoadCode.add("  print('  ✓ " & pluginName & " loaded')\n")

    let wrapperCode = """
local kryon = require('kryon')

""" & pluginLoadCode & """

-- Load user's app
local app = dofile('""" & absPath & """')

if not app or not app.root then
  error('App must return a valid app object with root component')
end

print('✓ IR tree created')

-- Run desktop renderer (keeps Lua alive during rendering)
local success = kryon.runDesktop(app)

if not success then
  error('Desktop renderer failed')
end

print('✓ Application closed')
"""

    writeFile(wrapperScript, wrapperCode)

    # Run Lua app with desktop renderer (blocks until window closes)
    let (wrapperOutput, wrapperExitCode) = execCmdEx("luajit " & wrapperScript)
    echo wrapperOutput

    # Cleanup temp files
    removeFile(wrapperScript)

    if wrapperExitCode != 0:
      echo "❌ Application failed"
      quit(1)

    quit(0)

  elif frontend == ".ts" or frontend == ".js":
    # Run TypeScript/JavaScript with appropriate runtime
    echo "📦 Running TypeScript/JavaScript application..."

    # Set KRYON_RENDERER environment variable (CLI flag overrides target)
    if renderer != "":
      putEnv("KRYON_RENDERER", renderer)
    else:
      putEnv("KRYON_RENDERER", target)

    var jsCmd: string
    if fileExists("bun.lockb") or fileExists("bunfig.toml"):
      jsCmd = "bun run " & file
    elif fileExists("package.json"):
      jsCmd = "node " & file
    else:
      jsCmd = "bun run " & file  # Default to bun

    let result = execShellCmd(jsCmd)
    quit(result)

  elif frontend == ".c":
    echo "🔧 C frontend not yet implemented"
    quit(1)

  elif frontend == ".kir" or frontend == ".kirb":
    # Run pre-compiled IR file directly
    echo "📦 Loading pre-compiled IR file..."

    if not fileExists(file):
      echo "❌ File not found: " & file
      quit(1)

    # Determine renderer from target
    if target == "web":
      echo "🌐 Web rendering not yet implemented for .kir files"
      quit(1)

    # Extract window properties from KIR root (only for .kir JSON files)
    var windowTitle = "Kryon App"
    var windowWidth = 800
    var windowHeight = 600
    if frontend == ".kir":
      try:
        let kirJson = parseFile(file)
        # Handle both formats: with "root" wrapper (.kry) and without (Nim)
        let root = if kirJson.hasKey("root"): kirJson["root"] else: kirJson
        # Check for windowTitle at kirJson level first (Nim serialization), then root
        if kirJson.hasKey("windowTitle"):
          windowTitle = kirJson["windowTitle"].getStr("Kryon App")
        elif root.hasKey("windowTitle"):
          windowTitle = root["windowTitle"].getStr("Kryon App")
        # Check for windowWidth/windowHeight at kirJson level first, then fall back to width/height
        if kirJson.hasKey("windowWidth"):
          let wStr = kirJson["windowWidth"].getStr("800.0px")
          windowWidth = parseInt(wStr.replace(".0px", "").replace("px", ""))
        elif root.hasKey("width"):
          let wStr = root["width"].getStr("800.0px")
          windowWidth = parseInt(wStr.replace(".0px", "").replace("px", ""))
        if kirJson.hasKey("windowHeight"):
          let hStr = kirJson["windowHeight"].getStr("600.0px")
          windowHeight = parseInt(hStr.replace(".0px", "").replace("px", ""))
        elif root.hasKey("height"):
          let hStr = root["height"].getStr("600.0px")
          windowHeight = parseInt(hStr.replace(".0px", "").replace("px", ""))
      except:
        discard  # Use defaults if parsing fails

    # Render via unified IR pipeline (CLI flag overrides env var)
    if renderer != "":
      putEnv("KRYON_RENDERER", renderer)
    let rendererName = getEnv("KRYON_RENDERER", "sdl3")
    echo "🎨 Rendering with " & rendererName & " backend..."

    let renderSuccess = renderIRFile(file, windowTitle, windowWidth, windowHeight)

    if not renderSuccess:
      echo "❌ Rendering failed"
      quit(1)

    echo "✓ Application closed"
    quit(0)

  elif frontend == ".kry":
    # .kry → .kir → IR renderer
    echo "📦 Running .kry via IR pipeline..."

    if not fileExists(file):
      echo "❌ File not found: " & file
      quit(1)

    # Set renderer environment variable if specified
    if renderer != "":
      putEnv("KRYON_RENDERER", renderer)

    # Set up temp directory
    let homeCache = getHomeDir() / ".cache" / "kryon"
    let runCache = homeCache / "cli_run"
    createDir(runCache)

    let baseName = splitFile(file).name
    let tempKir = runCache / baseName & ".kir"

    # Parse .kry -> .kir
    echo "  → Parsing to KIR..."
    var kirJson: JsonNode
    try:
      let source = readFile(file)
      let ast = parseKry(source, file)
      kirJson = transpileToKir(ast)
      writeFile(tempKir, $kirJson)
    except:
      echo "❌ Failed to parse .kry file: " & getCurrentExceptionMsg()
      quit(1)

    # Extract window properties from KIR root
    var windowTitle = "Kryon App"
    var windowWidth = 800
    var windowHeight = 600
    if kirJson.hasKey("root"):
      let root = kirJson["root"]
      if root.hasKey("windowTitle"):
        windowTitle = root["windowTitle"].getStr("Kryon App")
      if root.hasKey("width"):
        let wStr = root["width"].getStr("800.0px")
        windowWidth = parseInt(wStr.replace(".0px", "").replace("px", ""))
      if root.hasKey("height"):
        let hStr = root["height"].getStr("600.0px")
        windowHeight = parseInt(hStr.replace(".0px", "").replace("px", ""))

    # Render via unified IR pipeline
    echo "  → Rendering..."
    let renderSuccess = renderIRFile(tempKir, windowTitle, windowWidth, windowHeight)

    if not renderSuccess:
      echo "❌ Rendering failed"
      quit(1)

    echo "✓ Application closed"
    quit(0)

  elif frontend == ".tsx" or frontend == ".jsx":
    # .tsx/.jsx → .kir → IR renderer
    echo "📦 Running .tsx via IR pipeline..."

    if not fileExists(file):
      echo "❌ File not found: " & file
      quit(1)

    # Set up temp directory
    let homeCache = getHomeDir() / ".cache" / "kryon"
    let runCache = homeCache / "cli_run"
    createDir(runCache)

    let baseName = splitFile(file).name
    let tempKir = runCache / baseName & ".kir"

    # Parse .tsx -> .kir using Bun/Babel
    echo "  → Parsing TSX to KIR..."
    try:
      let kirContent = parseTsxToKir(file, tempKir)
      if not fileExists(tempKir):
        echo "❌ TSX parser did not create output file"
        quit(1)
    except:
      echo "❌ Failed to parse .tsx file: " & getCurrentExceptionMsg()
      quit(1)

    # Extract window properties from KIR root
    var windowTitle = "Kryon App"
    var windowWidth = 800
    var windowHeight = 600
    try:
      let kirJson = parseFile(tempKir)
      if kirJson.hasKey("root"):
        let root = kirJson["root"]
        if kirJson.hasKey("windowTitle"):
          windowTitle = kirJson["windowTitle"].getStr("Kryon App")
        elif root.hasKey("windowTitle"):
          windowTitle = root["windowTitle"].getStr("Kryon App")
        if root.hasKey("width"):
          let wStr = root["width"].getStr("800.0px")
          windowWidth = parseInt(wStr.replace(".0px", "").replace("px", ""))
        if root.hasKey("height"):
          let hStr = root["height"].getStr("600.0px")
          windowHeight = parseInt(hStr.replace(".0px", "").replace("px", ""))
    except:
      discard  # Use defaults if parsing fails

    # Render via unified IR pipeline
    echo "  → Rendering..."
    let renderSuccess = renderIRFile(tempKir, windowTitle, windowWidth, windowHeight)

    if not renderSuccess:
      echo "❌ Rendering failed"
      quit(1)

    echo "✓ Application closed"
    quit(0)

  elif frontend == ".md" or frontend == ".markdown":
    # .md → .kir → IR renderer
    echo "📦 Running .md via IR pipeline..."

    if not fileExists(file):
      echo "❌ File not found: " & file
      quit(1)

    # Set renderer environment variable if specified
    if renderer != "":
      putEnv("KRYON_RENDERER", renderer)

    # Set up temp directory
    let homeCache = getHomeDir() / ".cache" / "kryon"
    let runCache = homeCache / "cli_run"
    createDir(runCache)

    let baseName = splitFile(file).name
    let tempKir = runCache / baseName & ".kir"

    # Parse .md -> .kir using core parser
    echo "  → Parsing Markdown to KIR..."
    try:
      let kirContent = parseMdToKir(file, tempKir)
      if not fileExists(tempKir):
        echo "❌ Markdown parser did not create output file"
        quit(1)
    except:
      echo "❌ Failed to parse .md file: " & getCurrentExceptionMsg()
      quit(1)

    # Extract window properties from KIR root
    var windowTitle = "Kryon Markdown"
    var windowWidth = 800
    var windowHeight = 600
    try:
      let kirJson = parseFile(tempKir)
      if kirJson.hasKey("root"):
        let root = kirJson["root"]
        if kirJson.hasKey("windowTitle"):
          windowTitle = kirJson["windowTitle"].getStr("Kryon Markdown")
        elif root.hasKey("windowTitle"):
          windowTitle = root["windowTitle"].getStr("Kryon Markdown")
        if root.hasKey("width"):
          let wStr = root["width"].getStr("800.0px")
          windowWidth = parseInt(wStr.replace(".0px", "").replace("px", ""))
        if root.hasKey("height"):
          let hStr = root["height"].getStr("600.0px")
          windowHeight = parseInt(hStr.replace(".0px", "").replace("px", ""))
    except:
      discard  # Use defaults if parsing fails

    # Render via unified IR pipeline
    echo "  → Rendering..."
    let renderSuccess = renderIRFile(tempKir, windowTitle, windowWidth, windowHeight)

    if not renderSuccess:
      echo "❌ Rendering failed"
      quit(1)

    echo "✓ Application closed"
    quit(0)

  elif frontend == ".html" or frontend == ".htm":
    # .html → .kir → IR renderer
    echo "📦 Running .html via IR pipeline..."

    if not fileExists(file):
      echo "❌ File not found: " & file
      quit(1)

    # Set renderer environment variable if specified
    if renderer != "":
      putEnv("KRYON_RENDERER", renderer)

    # Set up temp directory
    let homeCache = getHomeDir() / ".cache" / "kryon"
    let runCache = homeCache / "cli_run"
    createDir(runCache)

    let baseName = splitFile(file).name
    let tempKir = runCache / baseName & ".kir"

    # Parse .html -> .kir using core parser
    echo "  → Parsing HTML to KIR..."
    try:
      let kirContent = parseHTMLToKir(file)
      writeFile(tempKir, kirContent)
      if not fileExists(tempKir):
        echo "❌ HTML parser did not create output file"
        quit(1)
    except:
      echo "❌ Failed to parse .html file: " & getCurrentExceptionMsg()
      quit(1)

    # Extract window properties from KIR root
    var windowTitle = "Kryon Web Application"
    var windowWidth = 800
    var windowHeight = 600
    try:
      let kirJson = parseFile(tempKir)
      if kirJson.hasKey("root"):
        let root = kirJson["root"]
        if kirJson.hasKey("windowTitle"):
          windowTitle = kirJson["windowTitle"].getStr("Kryon Web Application")
        elif root.hasKey("windowTitle"):
          windowTitle = root["windowTitle"].getStr("Kryon Web Application")
        if root.hasKey("width"):
          let wStr = root["width"].getStr("800.0px")
          windowWidth = parseInt(wStr.replace(".0px", "").replace("px", ""))
        if root.hasKey("height"):
          let hStr = root["height"].getStr("600.0px")
          windowHeight = parseInt(hStr.replace(".0px", "").replace("px", ""))
    except:
      discard  # Use defaults if parsing fails

    # Render via unified IR pipeline
    echo "  → Rendering..."
    let renderSuccess = renderIRFile(tempKir, windowTitle, windowWidth, windowHeight)

    if not renderSuccess:
      echo "❌ Rendering failed"
      quit(1)

    echo "✓ Application closed"
    quit(0)

  elif frontend != ".nim":
    echo "❌ Unknown frontend: " & frontend
    echo "   Supported: .nim, .lua, .ts, .js, .tsx, .jsx, .kir, .kirb, .kry, .md, .html"
    quit(1)

  # Continue with Nim compilation for .nim files
  # Set renderer environment variable if specified
  if renderer != "":
    putEnv("KRYON_RENDERER", renderer)

  try:
    # Check for installed kryon first (user project mode)
    let installed = findInstalledKryon()

    # Set up cache directory for compilation
    let homeCache = getHomeDir() / ".cache" / "kryon"
    let runCache = homeCache / "cli_run"
    createDir(runCache)
    let outFile = runCache / "kryon_app_run"

    var nimArgs: seq[string]
    var libFlags: seq[string]

    if installed.found:
      # Installed mode - use pre-built library from ~/.local
      echo "📦 Using installed Kryon from ~/.local"

      let cHeaderDir = installed.includeDir / "c"

      nimArgs = @[
        "nim", "c",
        "--nimcache:" & runCache,
        "--out:" & outFile,
        "--path:" & installed.includeDir,  # Nim bindings in ~/.local/include/kryon
        # C headers
        "--passC:\"-I" & cHeaderDir & "\"",
      ]

      # Single combined library
      libFlags = @[
        "--passL:\"-L" & installed.libDir & "\"",
        "--passL:\"-lkryon\"",
        "--passL:\"-Wl,-rpath," & installed.libDir & "\""
      ]
    else:
      # Dev mode - build from source
      let kryonRoot = findKryonRoot()
      putEnv("KRYON_ROOT", kryonRoot)

      # Ensure C libraries are built
      let buildDir = kryonRoot / "build"
      createDir(buildDir)

      let coreLib = buildDir / "libkryon_core.a"
      let irLib = buildDir / "libkryon_ir.a"
      let desktopLib = buildDir / "libkryon_desktop.a"

      # Build C core library if missing or outdated
      if not fileExists(coreLib):
        echo "🧱 Building C core library..."
        let coreCmd = "make -C " & (kryonRoot / "core") & " all"
        let coreResult = execShellCmd(coreCmd)
        if coreResult != 0:
          echo "❌ Failed to build C core library"
          quit(1)

      # Build IR library if missing
      if not fileExists(irLib):
        echo "🔧 Building IR library..."
        let irCmd = "make -C " & (kryonRoot / "ir") & " all"
        let irResult = execShellCmd(irCmd)
        if irResult != 0:
          echo "❌ Failed to build IR library"
          quit(1)

      # Build desktop library if missing
      if not fileExists(desktopLib):
        echo "🖥️  Building desktop library..."
        let desktopCmd = "make -C " & (kryonRoot / "backends" / "desktop") & " all"
        let desktopResult = execShellCmd(desktopCmd)
        if desktopResult != 0:
          echo "❌ Failed to build desktop library"
          quit(1)

      nimArgs = @[
        "nim", "c",
        "--nimcache:" & runCache,
        "--out:" & outFile,
        "--path:" & (kryonRoot / "bindings" / "nim"),
        # Includes
        "--passC:\"-I" & kryonRoot / "core/include" & "\"",
        "--passC:\"-I" & kryonRoot / "ir" & "\"",
        "--passC:\"-I" & kryonRoot / "backends/desktop" & "\"",
      ]

      # Base linker flags for Kryon libs
      libFlags = @[
        "--passL:\"-L" & kryonRoot / "build" & "\"",
        "--passL:\"-Wl,--start-group -lkryon_core -lkryon_ir -lkryon_desktop -Wl,--end-group\"",
        "--passL:\"-Wl,-rpath," & kryonRoot / "build" & "\""
      ]

    # Gather SDL flags from pkg-config (captures -L/-rpath/-l)
    let sdlFlags = pkgLibFlags("sdl3")
    if sdlFlags.len > 0:
      for flag in sdlFlags:
        libFlags.add "--passL:\"" & flag & "\""
    else:
      # Fallback to default name if pkg-config unavailable
      libFlags.add "--passL:\"-lSDL3\""

    var ttfFlags: seq[string] = @[]
    for pkg in ["sdl3-ttf", "sdl3_ttf", "SDL3_ttf"]:
      let found = pkgLibFlags(pkg)
      if found.len > 0:
        ttfFlags = found
        break
    if ttfFlags.len > 0:
      for flag in ttfFlags:
        libFlags.add "--passL:\"" & flag & "\""
    else:
      libFlags.add "--passL:\"-lSDL3_ttf\""

    nimArgs.add(libFlags)
    nimArgs.add(file)

    # .nim → compile → serialize IR → load IR → render
    let cmd = nimArgs.join(" ")

    echo "🔨 Building..."
    let buildResult = execShellCmd(cmd)

    if buildResult != 0:
      echo "❌ Build failed"
      quit(1)

    echo "✅ Build successful!"

    # Run with KRYON_SERIALIZE_IR to generate .kir (app exits after serializing)
    let baseName = splitFile(file).name
    let tempKir = runCache / baseName & ".kir"
    putEnv("KRYON_SERIALIZE_IR", tempKir)

    echo "📦 Generating IR..."
    let serializeResult = execShellCmd(outFile)
    if serializeResult != 0:
      echo "❌ Failed to generate IR"
      quit(1)

    # Render via unified IR pipeline
    echo "🎨 Rendering via IR pipeline..."
    let renderSuccess = renderIRFile(tempKir)

    if not renderSuccess:
      echo "❌ Rendering failed"
      quit(1)

    echo "✓ Application closed"

  except:
    echo "✗ Run failed: " & getCurrentExceptionMsg()
    quit(1)

proc handleDevCommand*(args: seq[string]) =
  ## Handle 'kryon dev' command - Hot reload development mode
  var target = "native"
  var file = ""
  var baseName = "main"

  for arg in args:
    if arg.startsWith("--target="):
      target = arg[9..^1]
    elif not arg.startsWith("-") and arg != "dev":
      baseName = arg
      if arg.contains('.'):
        file = arg
      else:
        file = ""

  # Auto-detect file with extension
  if file == "" or not fileExists(file):
    let detected = detectFileWithExtensions(baseName)
    if detected.found:
      file = detected.path
    else:
      echo "❌ Could not find file: " & baseName
      echo "   Tried extensions: .kry, .lua, .nim, .md, .markdown, .ts, .js, .c"
      quit(1)

  let (_, _, ext) = splitFile(file)
  let frontend = ext.toLowerAscii()

  echo "🔥 Starting Hot Reload Development Mode..."
  echo "📁 File: " & file
  echo "🎨 Frontend: " & frontend
  echo "🎯 Target: " & target
  echo ""
  echo "💡 Edit your code and save to see live updates!"
  echo "   Press Ctrl+C to exit"
  echo ""

  # Handle Lua hot reload (simple re-execution)
  if frontend == ".lua":
    echo "🌙 Lua hot reload mode..."

    let kryonRoot = findKryonRoot()
    let buildDir = kryonRoot / "build"

    putEnv("KRYON_LIB_PATH", buildDir / "libkryon_ir.so")
    putEnv("LD_LIBRARY_PATH", buildDir & ":" & getEnv("LD_LIBRARY_PATH"))

    # Set Lua module path so require("kryon") works from any directory
    let luaBindingsDir = kryonRoot / "bindings" / "lua"
    let existingPath = getEnv("LUA_PATH")
    let luaPath = luaBindingsDir / "?.lua;" &
                  luaBindingsDir / "?/init.lua;" &
                  (if existingPath.len > 0: existingPath else: ";")
    putEnv("LUA_PATH", luaPath)

    var lastModTime = getLastModificationTime(file)
    var appProcess: Process = nil

    # Initial run
    echo "🏃 Running " & file & "..."
    appProcess = startProcess("luajit", args = @[file], workingDir = getCurrentDir(),
                              options = {poUsePath, poParentStreams})

    # Watch for changes
    while true:
      sleep(200)

      try:
        let currentModTime = getLastModificationTime(file)
        if currentModTime != lastModTime:
          lastModTime = currentModTime
          echo ""
          echo "🔄 File changed, restarting..."

          if appProcess != nil:
            appProcess.terminate()
            discard appProcess.waitForExit()

          appProcess = startProcess("luajit", args = @[file], workingDir = getCurrentDir(),
                                    options = {poUsePath, poParentStreams})
      except:
        discard

    return

  # TypeScript/JavaScript hot reload
  if frontend == ".ts" or frontend == ".js":
    echo "📦 TypeScript/JavaScript hot reload not yet implemented"
    echo "   Use 'bun --watch' or similar tool for now"
    quit(1)

  # Continue with Nim hot reload
  try:
    # Check for installed kryon first (user project mode)
    let installed = findInstalledKryon()

    # Set up cache directory for compilation
    let homeCache = getHomeDir() / ".cache" / "kryon"
    let devCache = homeCache / "cli_dev"
    createDir(devCache)
    let outFile = devCache / "kryon_app_dev"

    var nimArgs: seq[string]
    var libFlags: seq[string]

    if installed.found:
      # Installed mode - use pre-built library from ~/.local
      echo "📦 Using installed Kryon from ~/.local"

      let cHeaderDir = installed.includeDir / "c"

      nimArgs = @[
        "nim", "c",
        "--nimcache:" & devCache,
        "--out:" & outFile,
        "--path:" & installed.includeDir,  # Nim bindings in ~/.local/include/kryon
        # C headers
        "--passC:\"-I" & cHeaderDir & "\"",
      ]

      # Single combined library
      libFlags = @[
        "--passL:\"-L" & installed.libDir & "\"",
        "--passL:\"-lkryon\"",
        "--passL:\"-Wl,-rpath," & installed.libDir & "\""
      ]
    else:
      # Dev mode - build from source
      let kryonRoot = findKryonRoot()
      putEnv("KRYON_ROOT", kryonRoot)

      # Ensure C libraries are built
      let buildDir = kryonRoot / "build"
      createDir(buildDir)

      let coreLib = buildDir / "libkryon_core.a"
      let irLib = buildDir / "libkryon_ir.a"
      let desktopLib = buildDir / "libkryon_desktop.a"

      # Build C core library if missing or outdated
      if not fileExists(coreLib):
        echo "🧱 Building C core library..."
        let coreCmd = "make -C " & (kryonRoot / "core") & " all"
        let coreResult = execShellCmd(coreCmd)
        if coreResult != 0:
          echo "❌ Failed to build C core library"
          quit(1)

      # Build IR library if missing
      if not fileExists(irLib):
        echo "🔧 Building IR library..."
        let irCmd = "make -C " & (kryonRoot / "ir") & " all"
        let irResult = execShellCmd(irCmd)
        if irResult != 0:
          echo "❌ Failed to build IR library"
          quit(1)

      # Build desktop library if missing
      if not fileExists(desktopLib):
        echo "🖥️  Building desktop library..."
        let desktopCmd = "make -C " & (kryonRoot / "backends" / "desktop") & " all"
        let desktopResult = execShellCmd(desktopCmd)
        if desktopResult != 0:
          echo "❌ Failed to build desktop library"
          quit(1)

      nimArgs = @[
        "nim", "c",
        "--nimcache:" & devCache,
        "--out:" & outFile,
        "--path:" & (kryonRoot / "bindings" / "nim"),
        # Includes
        "--passC:\"-I" & kryonRoot / "core/include" & "\"",
        "--passC:\"-I" & kryonRoot / "ir" & "\"",
        "--passC:\"-I" & kryonRoot / "backends/desktop" & "\"",
      ]

      # Base linker flags for Kryon libs
      libFlags = @[
        "--passL:\"-L" & kryonRoot / "build" & "\"",
        "--passL:\"-Wl,--start-group -lkryon_core -lkryon_ir -lkryon_desktop -Wl,--end-group\"",
        "--passL:\"-Wl,-rpath," & kryonRoot / "build" & "\""
      ]

    # Gather SDL flags from pkg-config (captures -L/-rpath/-l)
    let sdlFlags = pkgLibFlags("sdl3")
    if sdlFlags.len > 0:
      for flag in sdlFlags:
        libFlags.add "--passL:\"" & flag & "\""
    else:
      # Fallback to default name if pkg-config unavailable
      libFlags.add "--passL:\"-lSDL3\""

    var ttfFlags: seq[string] = @[]
    for pkg in ["sdl3-ttf", "sdl3_ttf", "SDL3_ttf"]:
      let found = pkgLibFlags(pkg)
      if found.len > 0:
        ttfFlags = found
        break
    if ttfFlags.len > 0:
      for flag in ttfFlags:
        libFlags.add "--passL:\"" & flag & "\""
    else:
      libFlags.add "--passL:\"-lSDL3_ttf\""

    nimArgs.add(libFlags)
    nimArgs.add(file)

    # Initial build
    let cmd = nimArgs.join(" ")

    echo "🔨 Initial build..."
    let buildResult = execShellCmd(cmd)

    if buildResult != 0:
      echo "❌ Build failed"
      quit(1)

    echo "✅ Build successful!"
    echo ""
    echo "🏃 Running application with hot reload enabled..."
    echo "   Edit your code and save - app will restart automatically!"
    echo "   Press Ctrl+C to exit"
    echo ""

    # Set environment variable to enable hot reload mode
    putEnv("KRYON_HOT_RELOAD", "1")
    putEnv("KRYON_HOT_RELOAD_FILE", file)
    putEnv("KRYON_HOT_RELOAD_CMD", cmd)

    # Hot reload loop: watch file, restart on changes
    var lastModTime = getLastModificationTime(file)
    var appProcess: Process = nil

    while true:
      # Start/restart the application
      if appProcess != nil:
        appProcess.terminate()
        discard appProcess.waitForExit()
        echo ""
        echo "🔄 Reloading..."

        # Rebuild
        let rebuildResult = execShellCmd(cmd)
        if rebuildResult != 0:
          echo "❌ Rebuild failed - waiting for fixes..."
          sleep(1000)
          continue
        echo "✅ Rebuild successful!"

      appProcess = startProcess(outFile, workingDir = getCurrentDir(),
                                options = {poUsePath, poParentStreams})

      # Watch for file changes
      while true:
        sleep(200)  # Check every 200ms

        try:
          let currentModTime = getLastModificationTime(file)
          if currentModTime != lastModTime:
            lastModTime = currentModTime
            echo ""
            echo "📝 File changed: " & file
            break  # Restart app
        except:
          # File might be temporarily unavailable during save
          discard

  except:
    echo "✗ Dev mode failed: " & getCurrentExceptionMsg()
    quit(1)

proc handleInspectCommand*(args: seq[string]) =
  ## Handle 'kryon inspect' command
  var target = "linux"

  for arg in args:
    if arg.startsWith("--target="):
      target = arg[9..^1]

  echo "Inspecting component tree for target: " & target

  try:
    inspectComponents(target)
  except:
    echo "✗ Inspection failed: " & getCurrentExceptionMsg()
    quit(1)

proc handleTestCommand*(args: seq[string]) =
  ## Handle 'kryon test' command - Run interactive tests from .kyt files
  if args.len == 0:
    echo "❌ Error: Test file required"
    echo "Usage: kryon test <file.kyt> [--verbose]"
    quit(1)

  var testFile = ""
  var verbose = false

  for arg in args:
    if arg == "--verbose" or arg == "-v":
      verbose = true
    elif not arg.startsWith("-") and arg != "test":
      testFile = arg

  if testFile == "":
    echo "❌ Error: No test file specified"
    quit(1)

  if not fileExists(testFile):
    echo "❌ Error: Test file not found: " & testFile
    quit(1)

  if not testFile.endsWith(".kyt"):
    echo "❌ Error: Test file must have .kyt extension"
    quit(1)

  echo "🧪 Running interactive test..."
  echo "📄 Test file: " & testFile

  try:
    # Parse .kyt file
    let test = parseKytFile(testFile)
    echo "📝 Test: " & test.name
    if test.description.len > 0:
      echo "   " & test.description

    # Convert to JSON
    let eventsJson = kytToJson(test)

    # Write to temporary file
    let tmpEventsFile = getTempDir() / "kryon_test_events.json"
    writeFile(tmpEventsFile, $eventsJson)

    if verbose:
      echo "📋 Generated test events:"
      echo pretty(eventsJson)

    echo "🎯 Target: " & test.target

    # Check if target file exists
    if not fileExists(test.target):
      echo "❌ Error: Target file not found: " & test.target
      removeFile(tmpEventsFile)
      quit(1)

    # Run the test using the event injection system
    let ldPath = KRYON_SOURCE_DIR / "build"
    let kryonBin = KRYON_SOURCE_DIR / "bin/cli/kryon"

    let cmd = "env LD_LIBRARY_PATH=" & ldPath & ":$LD_LIBRARY_PATH " &
              "KRYON_TEST_MODE=1 " &
              "KRYON_TEST_EVENTS=" & tmpEventsFile & " " &
              "KRYON_HEADLESS=1 " &
              "timeout 30 " & kryonBin & " run " & test.target

    if verbose:
      echo "🚀 Executing: " & cmd

    let (output, exitCode) = execCmdEx(cmd)

    if verbose or exitCode != 0:
      echo output

    # Clean up temporary file
    removeFile(tmpEventsFile)

    if exitCode == 0:
      echo "✅ Test passed!"
    else:
      echo "❌ Test failed (exit code: " & $exitCode & ")"
      quit(1)

  except Exception as e:
    echo "❌ Test execution failed: " & e.msg
    quit(1)

proc handleParseCommand*(args: seq[string]) =
  ## Handle 'kryon parse' command - Parse .kry file to .kir
  if args.len == 0:
    echo "Error: Input .kry file required"
    echo "Usage: kryon parse <input.kry> [--output=file.kir] [--tree]"
    quit(1)

  let inputFile = args[0]
  var outputFile = ""
  var showTree = false

  # Parse options
  for arg in args[1..^1]:
    if arg.startsWith("--output=") or arg.startsWith("-o="):
      outputFile = arg.split("=")[1]
    elif arg == "--tree" or arg == "-t":
      showTree = true

  if not fileExists(inputFile):
    echo "Error: File not found: " & inputFile
    quit(1)

  try:
    echo "Parsing: " & inputFile

    # Read and parse .kry file
    let source = readFile(inputFile)
    let ast = parseKry(source, inputFile)

    # Show AST tree if requested
    if showTree:
      echo ""
      echo "AST Tree:"
      echo "========="
      echo treeRepr(ast)
      echo ""

    # Transpile to KIR
    let kir = transpileToKir(ast)

    # Determine output file
    if outputFile == "":
      # Default: same name with .kir extension
      let (dir, name, _) = splitFile(inputFile)
      outputFile = dir / name & ".kir"

    # Write output
    writeFile(outputFile, pretty(kir, indent = 2))
    echo "Generated: " & outputFile

    # Show summary
    let compDefs = kir{"component_definitions"}.len
    let hasRoot = kir.hasKey("root") and kir["root"].kind != JNull
    let logicFns = kir{"logic", "functions"}.len
    let reactiveVars = kir{"reactive_manifest", "variables"}.len

    echo ""
    echo "Summary:"
    echo "  Component definitions: " & $compDefs
    echo "  Has App root: " & $hasRoot
    echo "  Logic functions: " & $logicFns
    echo "  Reactive variables: " & $reactiveVars

  except LexerError as e:
    echo "Lexer error: " & e.msg
    quit(1)
  except ParseError as e:
    echo "Parse error: " & e.msg
    quit(1)
  except CatchableError as e:
    echo "Error: " & e.msg
    quit(1)

proc handleParseHTMLCommand*(args: seq[string]) =
  ## Handle 'kryon parse-html' command - Parse HTML file to .kir
  if args.len == 0:
    echo "Error: Input HTML file required"
    echo "Usage: kryon parse-html <input.html> [--output=file.kir]"
    echo ""
    echo "Converts HTML (generated by Kryon) back to .kir format."
    echo "Useful for roundtrip validation: .kir → HTML → .kir"
    quit(1)

  let inputFile = args[0]
  var outputFile = ""

  # Parse options
  for arg in args[1..^1]:
    if arg.startsWith("--output=") or arg.startsWith("-o="):
      outputFile = arg.split("=")[1]

  if not fileExists(inputFile):
    echo "Error: File not found: " & inputFile
    quit(1)

  try:
    echo "Parsing HTML: " & inputFile

    # Parse HTML to KIR
    let kir = parseHTMLToKir(inputFile)

    # Determine output file
    if outputFile == "":
      # Default: same name with .kir extension
      let (dir, name, _) = splitFile(inputFile)
      outputFile = dir / name & ".kir"

    # Write output
    writeFile(outputFile, kir)
    echo "Generated: " & outputFile

    # Parse JSON to show summary
    try:
      let kirJson = parseJson(kir)
      let compType = kirJson{"type"}.getStr("unknown")
      let childCount = kirJson{"children"}.len

      echo ""
      echo "Summary:"
      echo "  Root type: " & compType
      echo "  Child count: " & $childCount
    except:
      # If JSON parsing fails, just skip summary
      discard

  except IOError as e:
    echo "Error: " & e.msg
    quit(1)
  except ValueError as e:
    echo "Error: " & e.msg
    quit(1)
  except CatchableError as e:
    echo "Error: " & e.msg
    quit(1)

proc handleCodegenCommand*(args: seq[string]) =
  ## Handle 'kryon codegen' command - Generate source code from .kir files
  if args.len == 0:
    echo "Error: Input .kir file required"
    echo "Usage: kryon codegen <input.kir> [--lang=c|lua|nim|tsx|jsx|html|markdown] [--output=file]"
    echo ""
    echo "Languages:"
    echo "  c        - C code with kryon.h API"
    echo "  lua      - Lua code (v5.1+)"
    echo "  nim      - Nim code with DSL"
    echo "  tsx      - TypeScript with JSX (types included)"
    echo "  jsx      - JavaScript with JSX (no types)"
    echo "  html     - HTML with inline CSS (static export)"
    echo "  markdown - Markdown (tables, headings, lists, code blocks)"
    quit(1)

  let inputFile = args[0]
  var lang = "nim"  # Default to Nim
  var outputFile = ""

  # Parse options
  for arg in args[1..^1]:
    if arg.startsWith("--lang="):
      lang = arg[7..^1]
    elif arg.startsWith("--output="):
      outputFile = arg[9..^1]

  if not fileExists(inputFile):
    echo "✗ File not found: " & inputFile
    quit(1)

  try:
    # Read file to detect version
    let kirJson = parseFile(inputFile)
    let version = kirJson{"format_version"}.getStr("2.0")

    # Use V3 codegen for v3.0 format files
    let useV3Codegen = version == "3.0"

    let generatedCode = case lang.toLowerAscii():
      of "lua":
        if useV3Codegen:
          generateLuaFromKirV3(inputFile)
        else:
          generateLuaFromKir(inputFile)
      of "nim":
        if useV3Codegen:
          generateNimFromKirV3(inputFile)
        else:
          generateNimFromKir(inputFile)
      of "tsx":
        generateTsxFromKir(inputFile)
      of "jsx":
        generateJsxFromKir(inputFile)
      of "c":
        generateCFromKir(inputFile)
      of "html":
        # Use C web codegen library via FFI
        let options = defaultDisplayOptions()
        transpileKirToHTML(inputFile, options)
      of "markdown", "md":
        # Generate proper markdown from IR components
        generateMarkdownFromKir(inputFile)
      else:
        echo "✗ Unsupported language: " & lang
        echo "   Supported: lua, nim, tsx, jsx, c, html, markdown"
        quit(1)

    if outputFile != "":
      writeFile(outputFile, generatedCode)
      echo "✓ Generated " & lang & " code from .kir v" & version & ": " & outputFile
    else:
      echo generatedCode

  except:
    echo "✗ Code generation failed: " & getCurrentExceptionMsg()
    quit(1)

proc handleKrbCommand*(args: seq[string]) =
  ## Handle 'kryon krb' command - Compile to .krb bytecode
  if args.len == 0:
    echo "Error: Input file required"
    echo "Usage: kryon krb <input.kir> [-o output.krb]"
    echo ""
    echo "Options:"
    echo "  -o, --output=FILE    Output file path (default: <input>.krb)"
    quit(1)

  let inputFile = args[0]
  var outputFile = ""

  # Parse options
  var i = 1
  while i < args.len:
    let arg = args[i]
    if arg.startsWith("-o=") or arg.startsWith("--output="):
      outputFile = arg.split("=", 1)[1]
    elif arg == "-o" or arg == "--output":
      if i + 1 < args.len:
        i += 1
        outputFile = args[i]
      else:
        echo "Error: -o requires an argument"
        quit(1)
    i += 1

  if not fileExists(inputFile):
    echo "✗ File not found: " & inputFile
    quit(1)

  # Determine output path
  if outputFile == "":
    let base = inputFile.changeFileExt("")
    outputFile = base & ".krb"

  echo "Compiling " & inputFile & " to .krb bytecode..."

  if compileKirToKrb(inputFile, outputFile):
    echo "✓ Created: " & outputFile
  else:
    echo "✗ Compilation failed"
    quit(1)

proc handleDisasmCommand*(args: seq[string]) =
  ## Handle 'kryon disasm' command - Disassemble .krb bytecode
  if args.len == 0:
    echo "Error: Input .krb file required"
    echo "Usage: kryon disasm <input.krb> [--info]"
    echo ""
    echo "Options:"
    echo "  --info    Show module info instead of disassembly"
    quit(1)

  let inputFile = args[0]
  var showInfo = false

  # Parse options
  for arg in args[1..^1]:
    if arg == "--info":
      showInfo = true

  if not fileExists(inputFile):
    echo "✗ File not found: " & inputFile
    quit(1)

  if not inputFile.endsWith(".krb"):
    echo "Warning: File does not have .krb extension"

  if showInfo:
    inspectKrb(inputFile)
  else:
    let disasm = disassembleKrb(inputFile)
    if disasm == "":
      echo "✗ Failed to disassemble: " & inputFile
      quit(1)
    echo disasm

proc handleDoctorCommand*() =
  ## Handle 'kryon doctor' command
  echo "Diagnosing Kryon development environment..."

  try:
    runDiagnostics()
  except:
    echo "✗ Diagnostics failed: " & getCurrentExceptionMsg()
    quit(1)

# =============================================================================
# Main Entry Point
# =============================================================================

proc main*() =
  var args: seq[string] = @[]

  # Parse command line arguments
  for kind, key, val in getopt():
    case kind
    of cmdArgument:
      args.add(key)
    of cmdLongOption:
      case key
      of "help", "h": showHelp()
      of "version", "v": showVersion()
      else:
        # Pass other long options to command handlers
        if val != "":
          args.add("--" & key & "=" & val)
        else:
          args.add("--" & key)
    of cmdShortOption:
      case key
      of "help", "h": showHelp()
      of "version", "v": showVersion()
      else:
        # Pass other short options to command handlers
        if val != "":
          args.add("-" & key & "=" & val)
        else:
          args.add("-" & key)
    of cmdEnd: break

  if args.len == 0:
    showHelp()

  let command = args[0]
  let commandArgs = args[1..^1]

  case command.toLowerAscii()
  of "new":
    handleNewCommand(commandArgs)
  of "init":
    handleInitCommand(commandArgs)
  of "build":
    handleBuildCommand(commandArgs)
  of "compile":
    handleCompileCommand(commandArgs)
  of "parse":
    handleParseCommand(commandArgs)
  of "parse-html":
    handleParseHTMLCommand(commandArgs)
  of "codegen":
    handleCodegenCommand(commandArgs)
  of "convert":
    handleConvertCommand(commandArgs)
  of "krb":
    handleKrbCommand(commandArgs)
  of "disasm":
    handleDisasmCommand(commandArgs)
  of "run":
    handleRunCommand(commandArgs)
  of "dev":
    handleDevCommand(commandArgs)
  of "test":
    handleTestCommand(commandArgs)
  of "config":
    handleConfigCommand(commandArgs)
  of "validate":
    handleValidateCommand(commandArgs)
  of "inspect":
    handleInspectCommand(commandArgs)
  of "inspect-ir":
    handleInspectIRCommand(commandArgs)
  of "inspect-detailed":
    handleInspectDetailedCommand(commandArgs)
  of "tree":
    handleTreeCommand(commandArgs)
  of "diff":
    handleDiffCommand(commandArgs)
  of "plugin":
    handlePluginCommand(commandArgs)
  of "doctor":
    handleDoctorCommand()
  of "add-backend":
    echo "Backend addition not yet implemented"
    quit(1)
  else:
    echo "Unknown command: " & command
    echo "Use 'kryon --help' for available commands"
    quit(1)

when isMainModule:
  main()
