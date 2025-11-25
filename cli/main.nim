## Kryon CLI - The Unified Orchestrator
##
## Single interface for all Kryon development workflows
## From project creation to microcontroller deployment

import os, osproc, strutils, parseopt, times, json
import std/[sequtils, sugar]

# CLI modules
import project, build, device

const
  VERSION = "1.2.0"
  HELP_TEXT = """
Kryon CLI v""" & VERSION & """ - Unified Development Orchestrator

USAGE:
  kryon <command> [options] [args]

COMMANDS:
  new <name>          Create new project with template
  build [targets]      Compile for specified targets
  run <target>         Build + deploy + live debug
  inspect             Component tree visualization
  add-backend <name>  Integrate new renderer backend
  doctor              Diagnose environment issues

TARGETS:
  stm32f4, rp2040, linux, windows, macos, web

TEMPLATES:
  nim, lua, c

EXAMPLES:
  kryon new myapp --template=nim
  kryon build --targets=stm32f4,linux
  kryon run --target=stm32f4 --device=/dev/ttyUSB0
  kryon inspect --target=stm32f4

For detailed help: kryon <command> --help
"""

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
      if fileExists(cur / "core" / "include" / "kryon.h"):
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

proc handleBuildCommand*(args: seq[string]) =
  ## Handle 'kryon build' command
  var targets: seq[string] = @["linux"]  # Default target

  if args.len > 0:
    for arg in args:
      if arg.startsWith("--targets="):
        targets = arg[10..^1].split(",")
      elif not arg.startsWith("-"):
        targets = @[arg]

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

proc handleRunCommand*(args: seq[string]) =
  ## Handle 'kryon run' command - Flutter-like behavior
  var target = "native"
  var file = "main.nim"

  for arg in args:
    if arg.startsWith("--target="):
      target = arg[9..^1]
    elif arg.startsWith("--device="):
      discard  # For compatibility
    elif arg.endsWith(".nim"):
      file = arg
    elif not arg.startsWith("-"):
      if arg != "run":
        file = arg & ".nim"

  echo "🚀 Running Kryon application..."
  echo "📁 File: " & file
  echo "🎯 Target: " & target

  try:
    # Resolve Kryon root so we can inject include/lib paths for external apps
    let kryonRoot = findKryonRoot()
    putEnv("KRYON_ROOT", kryonRoot)

    # Make sure we have a writable build/cache location even if cwd is read-only
    let runCache = kryonRoot / "tmp_nimcache" / "cli_run"
    createDir(runCache)

    let outFile = runCache / "kryon_app_run"
    var nimArgs = @[
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
    var libFlags: seq[string] = @[
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

    # Simple compilation and execution like Flutter
    let cmd = nimArgs.join(" ")

    echo "🔨 Building..."
    let buildResult = execShellCmd(cmd)

    if buildResult == 0:
      echo "✅ Build successful!"
      echo "🏃 Running application..."
      discard execShellCmd(outFile)
    else:
      echo "❌ Build failed"
      quit(1)

  except:
    echo "✗ Run failed: " & getCurrentExceptionMsg()
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
    of cmdLongOption, cmdShortOption:
      case key
      of "help", "h": showHelp()
      of "version", "v": showVersion()
      else: discard
    of cmdEnd: break

  if args.len == 0:
    showHelp()

  let command = args[0]
  let commandArgs = args[1..^1]

  case command.toLowerAscii()
  of "new":
    handleNewCommand(commandArgs)
  of "build":
    handleBuildCommand(commandArgs)
  of "run":
    handleRunCommand(commandArgs)
  of "inspect":
    handleInspectCommand(commandArgs)
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
