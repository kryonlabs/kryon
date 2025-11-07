## C Core Build Configuration for Kryon
##
## This script configures the build system to use the C core engine
## with the Nim compiler, ensuring proper linking and compilation.

import os, strutils, osproc

const
  coreDir = "core"
  buildDir = "build"
  libName = "kryon_core"

proc buildCCore*() =
  ## Build the C core library
  echo "Building C core library..."

  let cwd = getCurrentDir()
  setCurrentDir(coreDir)

  # Build static library
  let (output, exitCode) = execCmdEx("make static TARGET=desktop")
  if exitCode != 0:
    echo "Error building C core: " & output
    quit(1)

  echo "C core built successfully!"
  echo output

  setCurrentDir(cwd)

proc configureNimBuild*() =
  ## Configure Nim build flags for C core integration
  echo "Configuring Nim build for C core integration..."

  # Ensure the C core is built
  buildCCore()

  # Set up environment variables for Nim compilation
  let libPath = getCurrentDir() / buildDir
  let includePath = getCurrentDir() / coreDir / "include"

  putEnv("LIBRARY_PATH", libPath)
  putEnv("C_INCLUDE_PATH", includePath)

  # Create a .nim.cfg file with the required settings
  let cfgFile = getCurrentDir() / "kryon_c_core.nim.cfg"
  let cfgContent = """
# Kryon C Core Configuration
--passL:-L""" & libPath & """ -l""" & libName & """
--passC:-I""" & includePath & """
--define:useCCore
--gc:arc
--threads:on
"""

  writeFile(cfgFile, cfgContent)
  echo "Created Nim configuration: " & cfgFile

proc testIntegration*() =
  ## Test the C core integration
  echo "Testing C core integration..."

  # Build and run the test example
  let testFile = getCurrentDir() / "examples/c_core_test.nim"
  let libPath = getCurrentDir() / buildDir
  let includePath = getCurrentDir() / coreDir / "include"

  let (output, exitCode) = execCmdEx("nim c -r --passL:\"-L" & libPath & " -lkryon_core\" --passC:\"-I" & includePath & "\" " & testFile)
  if exitCode != 0:
    echo "Error testing integration: " & output
    quit(1)

  echo "Integration test output:"
  echo output

when isMainModule:
  echo "Kryon C Core Build Configuration"
  echo "================================"

  buildCCore()
  configureNimBuild()
  testIntegration()

  echo ""
  echo "Build configuration complete!"
  echo "You can now build Kryon applications with the C core engine using:"
  echo "nim c -r your_app.nim --nimble:link:static"