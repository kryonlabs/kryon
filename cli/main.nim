## Kryon CLI - The Unified Orchestrator
##
## Single interface for all Kryon development workflows
## From project creation to microcontroller deployment

import os, strutils, parseopt, times, json
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
    # Set environment for kryon sources
    let kryonRoot = getEnv("KRYON_ROOT", getCurrentDir() / ".." / "..")
    putEnv("KRYON_ROOT", kryonRoot)

    # Simple compilation and execution like Flutter
    let outFile = "kryon_app_run"
    let cmd = "nim compile --out:" & outFile & " " & file

    echo "🔨 Building..."
    let buildResult = execShellCmd(cmd)

    if buildResult == 0:
      echo "✅ Build successful!"
      echo "🏃 Running application..."
      discard execShellCmd("./" & outFile)
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