## Build orchestrator for Kryon projects
##
## Handles cross-compilation for different targets

import os, strutils, json, times, sequtils, osproc

# Forward declarations
proc buildLinuxTarget*()
proc buildSTM32Target*()
proc buildWindowsTarget*()
proc buildMacOSTarget*()
proc buildWebTarget*()
proc buildTerminalTarget*()
proc buildNimLinux*()
proc buildLuaLinux*()
proc buildCLinux*()
proc buildNimSTM32*()
proc buildCSTM32*()
proc buildNimTerminal*()
proc buildLuaTerminal*()
proc buildCTerminal*()

proc buildForTarget*(target: string) =
  ## Build project for specified target
  echo "Building for target: " & target

  case target.toLowerAscii():
    of "linux":
      buildLinuxTarget()
    of "windows":
      buildWindowsTarget()
    of "macos":
      buildMacOSTarget()
    of "stm32f4":
      buildSTM32Target()
    # of "rp2040":
    #   buildRP2040Target()  # Not implemented yet
    of "web":
      buildWebTarget()
    of "terminal":
      buildTerminalTarget()
    else:
      raise newException(ValueError, "Unknown target: " & target)

proc buildLinuxTarget*() =
  ## Build for Linux desktop
  echo "Configuring Linux desktop build..."

  # Ensure we have the required libraries
  if not fileExists("build/libkryon_core.a"):
    raise newException(IOError, "Kryon core library not found. Run 'make -C ../../core' first.")

  # Check if we have a Nim project
  if fileExists("src/main.nim"):
    buildNimLinux()
  elif fileExists("src/main.lua"):
    buildLuaLinux()
  elif fileExists("src/main.c"):
    buildCLinux()
  else:
    raise newException(ValueError, "No recognized source files found")

proc buildNimLinux*() =
  ## Build Nim project for Linux
  echo "Building Nim project for Linux..."

  let buildCmd = """
    nim c --path:src \
        --define:KRYON_PLATFORM_DESKTOP \
        --passC:"-I../../core/include" \
        --passL:"-L../../build -lkryon_core -lkryon_fb -lkryon_common" \
        --threads:on \
        --gc:arc \
        --opt:speed \
        --out:build/$1 \
        src/main.nim
  """ % [getCurrentDir().splitPath().tail]

  echo "Executing: " & buildCmd
  let (output, exitCode) = execCmdEx(buildCmd)

  if exitCode != 0:
    echo "Build failed:"
    echo output
    raise newException(IOError, "Nim build failed")

  echo "✓ Nim build successful"

proc buildLuaLinux*() =
  ## Build Lua project for Linux
  echo "Building Lua project for Linux..."

  # First compile the C wrapper
  let wrapperCmd = """
    gcc -shared -fPIC \
        -I../../core/include \
        -L../../build -lkryon_core -lkryon_fb -lkryon_common \
        -o build/kryon_lua.so \
        ../../bindings/lua/kryon_lua.c
  """

  echo "Compiling Lua wrapper..."
  let (wrapperOutput, wrapperExitCode) = execCmdEx(wrapperCmd)

  if wrapperExitCode != 0:
    echo "Lua wrapper compilation failed:"
    echo wrapperOutput
    raise newException(IOError, "Lua wrapper compilation failed")

  echo "✓ Lua wrapper compiled"
  echo "Run with: lua5.4 -C build/ src/main.lua"

proc buildCLinux*() =
  ## Build C project for Linux
  echo "Building C project for Linux..."

  let makeCmd = "make all"

  echo "Executing: " & makeCmd
  let (output, exitCode) = execCmdEx(makeCmd)

  if exitCode != 0:
    echo "Make failed:"
    echo output
    raise newException(IOError, "C build failed")

  echo "✓ C build successful"

proc buildSTM32Target*() =
  ## Build for STM32F4 microcontroller
  echo "Configuring STM32F4 build..."

  # Check if ARM toolchain is available
  let toolchainCheck = execCmdEx("which arm-none-eabi-gcc")
  if toolchainCheck.exitCode != 0:
    echo "⚠️  ARM toolchain not found. Attempting to use bundled toolchain..."

    # Extract bundled toolchain if available
    if fileExists("cli/deps/arm_gcc/toolchain.tar.gz"):
      echo "Extracting bundled ARM toolchain..."
      discard execCmdEx("tar -xzf cli/deps/arm_gcc/toolchain.tar.gz -C cli/deps/arm_gcc/")
    else:
      raise newException(IOError, "ARM toolchain not found and no bundled version available")

  # Build with MCU constraints
  if fileExists("src/main.nim"):
    buildNimSTM32()
  elif fileExists("src/main.c"):
    buildCSTM32()
  else:
    raise newException(ValueError, "No MCU-compatible source found")

proc buildNimSTM32*() =
  ## Build Nim project for STM32F4 with memory constraints
  echo "Building Nim project for STM32F4 (MCU constraints)..."

  let buildCmd = """
    nim c --path:src \
        --cpu:arm \
        --os:none \
        --define:KRYON_PLATFORM_MCU \
        --define:KRYON_NO_HEAP \
        --define:KRYON_NO_FLOAT \
        --passC:"-I../../core/include" \
        --passL:"-L../../build -lkryon_core -lkryon_fb" \
        --deadCodeElim:on \
        --opt:size \
        --genScript \
        --out:build/stm32f4.elf \
        src/main.nim
  """

  echo "Executing MCU build..."
  let (output, exitCode) = execCmdEx(buildCmd)

  if exitCode != 0:
    echo "STM32 build failed:"
    echo output
    raise newException(IOError, "STM32 build failed")

  # Convert ELF to binary
  let objcopyCmd = """
    arm-none-eabi-objcopy -O binary build/stm32f4.elf build/stm32f4.bin
  """

  echo "Converting to binary..."
  let (objcopyOutput, objcopyExitCode) = execCmdEx(objcopyCmd)

  if objcopyExitCode != 0:
    echo "Binary conversion failed:"
    echo objcopyOutput
    raise newException(IOError, "Binary conversion failed")

  # Check size constraints
  let sizeCheck = execCmdEx("stat -c%s build/stm32f4.bin")
  let binarySize = parseInt(sizeCheck.output.strip())

  echo "Binary size: " & $binarySize & " bytes"

  if binarySize > 16384:  # 16KB limit
    echo "⚠️  Warning: Binary size exceeds STM32F4 limit of 16KB"
  else:
    echo "✓ Binary within STM32F4 memory constraints"

proc buildCSTM32*() =
  ## Build C project for STM32F4
  echo "Building C project for STM32F4..."

  # Use ARM GCC with MCU flags
  let compileCmd = """
    arm-none-eabi-gcc \
        -c -mcpu=cortex-m4 -mthumb \
        -I../../core/include \
        -DKRYON_PLATFORM_MCU \
        -DKRYON_NO_HEAP \
        -DKRYON_NO_FLOAT \
        -Os -ffunction-sections -fdata-sections \
        src/main.c -o build/main.o
  """

  let linkCmd = """
    arm-none-eabi-gcc \
        -mcpu=cortex-m4 -mthumb \
        -Wl,--gc-sections \
        -L../../build -lkryon_core -lkryon_fb \
        build/main.o -o build/stm32f4.elf
  """

  echo "Compiling for ARM Cortex-M4..."
  let (compileOutput, compileExitCode) = execCmdEx(compileCmd)

  if compileExitCode != 0:
    echo "ARM compilation failed:"
    echo compileOutput
    raise newException(IOError, "ARM compilation failed")

  echo "Linking..."
  let (linkOutput, linkExitCode) = execCmdEx(linkCmd)

  if linkExitCode != 0:
    echo "Linking failed:"
    echo linkOutput
    raise newException(IOError, "Linking failed")

  echo "✓ STM32 build complete"

proc buildWindowsTarget*() =
  ## Build for Windows
  echo "Building for Windows target..."
  echo "⚠️  Windows cross-compilation not yet implemented"
  echo "Use native Windows build with Visual Studio or MinGW"

proc buildMacOSTarget*() =
  ## Build for macOS
  echo "Building for macOS target..."
  echo "⚠️  macOS cross-compilation not yet implemented"
  echo "Use native macOS build with Xcode"

proc buildWebTarget*() =
  ## Build for Web (WASM)
  echo "Building for Web target (WASM)..."
  echo "⚠️  Web target not yet implemented"
  echo "Will generate HTML + WASM bundle"

proc buildTerminalTarget*() =
  ## Build for Terminal (TUI)
  echo "Building for Terminal target (TUI)..."

  # Ensure we have the required libraries
  if not fileExists("build/libkryon_core.a"):
    raise newException(IOError, "Kryon core library not found. Run 'make -C ../../core' first.")

  if not fileExists("../renderers/terminal/terminal_backend.c"):
    raise newException(IOError, "Terminal renderer not found.")

  # Build terminal renderer if needed
  if not fileExists("build/libkryon_terminal.a"):
    echo "Building terminal renderer..."
    let rendererCmd = "make -C ../renderers/terminal"
    let (rendererOutput, rendererExitCode) = execCmdEx(rendererCmd)
    if rendererExitCode != 0:
      echo "Terminal renderer build failed:"
      echo rendererOutput
      raise newException(IOError, "Terminal renderer build failed")

  # Check if we have a Nim project
  if fileExists("src/main.nim"):
    buildNimTerminal()
  elif fileExists("src/main.lua"):
    buildLuaTerminal()
  elif fileExists("src/main.c"):
    buildCTerminal()
  else:
    raise newException(ValueError, "No recognized source files found")

proc buildNimTerminal*() =
  ## Build Nim project for Terminal
  echo "Building Nim project for Terminal..."

  let buildCmd = """
    nim c --path:src \
        --define:KRYON_PLATFORM_DESKTOP \
        --define:KRYON_TERMINAL \
        --passC:"-I../../core/include -I../renderers/terminal" \
        --passL:"-L../../build -L../renderers/terminal -lkryon_core -lkryon_terminal -ltickit -lutil -ltinfo" \
        --threads:on \
        --gc:arc \
        --opt:speed \
        --out:build/$1 \
        src/main.nim
  """ % [getCurrentDir().splitPath().tail]

  echo "Executing: " & buildCmd
  let (output, exitCode) = execCmdEx(buildCmd)

  if exitCode != 0:
    echo "Build failed:"
    echo output
    raise newException(IOError, "Nim terminal build failed")

  echo "✓ Nim terminal build successful"

proc buildLuaTerminal*() =
  ## Build Lua project for Terminal
  echo "Building Lua project for Terminal..."

  # First compile the C wrapper with terminal support
  let wrapperCmd = """
    gcc -shared -fPIC \
        -I../../core/include -I../renderers/terminal \
        -L../../build -L../renderers/terminal \
        -lkryon_core -lkryon_terminal -ltickit -lutil -ltinfo \
        -o build/kryon_lua_terminal.so \
        ../bindings/lua/kryon_lua.c
  """

  echo "Compiling Lua wrapper with terminal support..."
  let (wrapperOutput, wrapperExitCode) = execCmdEx(wrapperCmd)

  if wrapperExitCode != 0:
    echo "Lua wrapper compilation failed:"
    echo wrapperOutput
    raise newException(IOError, "Lua wrapper compilation failed")

  echo "✓ Lua wrapper compiled for terminal"
  echo "Run with: lua5.4 -C build/ src/main.lua"

proc buildCTerminal*() =
  ## Build C project for Terminal
  echo "Building C project for Terminal..."

  let compileCmd = """
    gcc -std=c99 -Wall -Wextra \
        -I../../core/include -I../renderers/terminal \
        -DKRYON_PLATFORM_DESKTOP -DKRYON_TERMINAL \
        -L../../build -L../renderers/terminal \
        -lkryon_core -lkryon_terminal -ltickit -lutil -ltinfo \
        src/main.c -o build/terminal_app
  """

  echo "Compiling C application..."
  let (compileOutput, compileExitCode) = execCmdEx(compileCmd)

  if compileExitCode != 0:
    echo "C compilation failed:"
    echo compileOutput
    raise newException(IOError, "C compilation failed")

  echo "✓ C terminal build complete"