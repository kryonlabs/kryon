## Physical device management and deployment
##
## Handles communication with microcontrollers and debugging

import os, strutils, sequtils, times, osproc

# Forward declarations
proc deployToSTM32*(devicePath: string)
proc deployToRP2040*(devicePath: string)
proc deployWithSTLink*(binaryPath: string)
proc deployWithOpenOCD*(binaryPath: string)
proc deployWithDFU*(binaryPath: string)
proc convertToUF2*(binaryPath: string, uf2Path: string)
proc inspectMCUComponents*(target: string)
proc inspectDesktopComponents*(target: string)
proc findConnectedDevices*(): seq[string]
proc runDiagnostics*()
proc runLocally*(target: string)

proc deployToDevice*(target: string, devicePath: string) =
  ## Deploy and run application on physical device
  echo "Deploying to device: " & devicePath
  echo "Target: " & target

  case target.toLowerAscii():
    of "stm32f4":
      deployToSTM32(devicePath)
    of "rp2040":
      deployToRP2040(devicePath)
    else:
      raise newException(ValueError, "Deployment not supported for target: " & target)

proc deployToSTM32*(devicePath: string) =
  ## Deploy to STM32F4 microcontroller
  echo "Configuring STM32F4 deployment..."

  # Check if device exists
  if not fileExists(devicePath):
    raise newException(IOError, "Device not found: " & devicePath)

  # Check for required tools
  let tools = ["st-flash", "openocd", "dfu-util"]
  var availableTool = ""

  for tool in tools:
    if execCmdEx("which " & tool).exitCode == 0:
      availableTool = tool
      break

  if availableTool == "":
    echo "⚠️  No STM32 programming tool found. Install one of:"
    echo "  - st-link (st-flash)"
    echo "  - OpenOCD"
    echo "  - dfu-util (for STM32 discovery boards)"
    return

  echo "Using programmer: " & availableTool

  # Check if binary exists
  let binaryPath = "build/stm32f4.bin"
  if not fileExists(binaryPath):
    raise newException(IOError, "Binary not found: " & binaryPath)

  # Deploy using available tool
  case availableTool:
    of "st-flash":
      deployWithSTLink(binaryPath)
    of "openocd":
      deployWithOpenOCD(binaryPath)
    of "dfu-util":
      deployWithDFU(binaryPath)
    else:
      raise newException(ValueError, "Unknown programming tool")

proc deployWithSTLink*(binaryPath: string) =
  ## Deploy using ST-Link utility
  echo "Flashing with ST-Link..."

  let flashCmd = "st-flash write " & binaryPath & " 0x08000000"
  echo "Executing: " & flashCmd

  let (output, exitCode) = execCmdEx(flashCmd)

  if exitCode != 0:
    echo "ST-Link flash failed:"
    echo output
    raise newException(IOError, "ST-Link flash failed")

  echo "✓ Successfully flashed to STM32"

  # Reset device
  echo "Resetting device..."
  discard execCmdEx("st-flash reset")

proc deployWithOpenOCD*(binaryPath: string) =
  ## Deploy using OpenOCD
  echo "Flashing with OpenOCD..."

  # Create temporary OpenOCD config
  let openocdConfig = """
# OpenOCD configuration for STM32F4 Discovery
source [find target/stm32f4x.cfg]
source [find board/stm32f4discovery.cfg]

# Flash the binary
init
reset init
flash write_image erase """ & binaryPath & """ 0x08000000
reset run
shutdown
"""

  writeFile("tmp_openocd.cfg", openocdConfig)

  let flashCmd = "openocd -f tmp_openocd.cfg"
  echo "Executing OpenOCD..."

  let (output, exitCode) = execCmdEx(flashCmd)

  # Clean up
  removeFile("tmp_openocd.cfg")

  if exitCode != 0:
    echo "OpenOCD flash failed:"
    echo output
    raise newException(IOError, "OpenOCD flash failed")

  echo "✓ Successfully flashed with OpenOCD"

proc deployWithDFU*(binaryPath: string) =
  ## Deploy using DFU utility
  echo "Flashing with DFU..."

  # Put device in DFU mode first
  echo "Note: Ensure device is in DFU mode (BOOT0=1, reset)"

  let flashCmd = "dfu-util -a 0 -s 0x08000000:leave -D " & binaryPath
  echo "Executing: " & flashCmd

  let (output, exitCode) = execCmdEx(flashCmd)

  if exitCode != 0:
    echo "DFU flash failed:"
    echo output
    raise newException(IOError, "DFU flash failed")

  echo "✓ Successfully flashed with DFU"

proc deployToRP2040*(devicePath: string) =
  ## Deploy to RP2040 microcontroller
  echo "Configuring RP2040 deployment..."

  # Check for picotool
  if execCmdEx("which picotool").exitCode != 0:
    echo "⚠️  picotool not found. Install with: pip install picotool"
    return

  # Check if UF2 file exists (RP2040 uses UF2 format)
  let uf2Path = "build/rp2040.uf2"
  if not fileExists(uf2Path):
    echo "UF2 file not found. Converting from binary..."
    convertToUF2("build/stm32f4.bin", uf2Path)

  echo "RP2040 deployment instructions:"
  echo "1. Hold BOOTSEL button while connecting USB"
  echo "2. Device will appear as USB storage"
  echo "3. Copy " & uf2Path & " to the RPI-RP2 drive"
  echo ""
  echo "Or use picotool if USB serial is available"

proc convertToUF2*(binaryPath: string, uf2Path: string) =
  ## Convert binary to UF2 format for RP2040
  echo "Converting binary to UF2 format..."

  # This is a simplified UF2 conversion
  # Real implementation would need proper UF2 block format
  let convertCmd = "cp " & binaryPath & " " & uf2Path
  let (output, exitCode) = execCmdEx(convertCmd)

  if exitCode != 0:
    echo "UF2 conversion failed:"
    echo output
    raise newException(IOError, "UF2 conversion failed")

proc runLocally*(target: string) =
  ## Run application locally (desktop targets)
  echo "Running application locally for target: " & target

  let executable = case target.toLowerAscii():
    of "linux":
      "build/" & getCurrentDir().splitPath().tail
    of "windows":
      "build/" & getCurrentDir().splitPath().tail & ".exe"
    of "macos":
      "build/" & getCurrentDir().splitPath().tail
    else:
      raise newException(ValueError, "Unknown local target: " & target)

  if not fileExists(executable):
    raise newException(IOError, "Executable not found: " & executable)

  echo "Running: " & executable
  let (output, exitCode) = execCmdEx(executable)

  if exitCode != 0:
    echo "Application execution failed:"
    echo output
    raise newException(IOError, "Application execution failed")

proc inspectComponents*(target: string) =
  ## Inspect component tree at runtime
  echo "Inspecting component tree for target: " & target

  case target.toLowerAscii():
    of "stm32f4", "rp2040":
      inspectMCUComponents(target)
    else:
      inspectDesktopComponents(target)

proc inspectMCUComponents*(target: string) =
  ## Inspect components on microcontroller via UART/USB
  echo "Connecting to microcontroller for component inspection..."

  # Try to find connected device
  let devices = findConnectedDevices()

  if devices.len == 0:
    echo "No connected devices found"
    return

  echo "Found devices:"
  for i, device in devices:
    echo "  " & $(i+1) & ". " & device

  # For now, just show available devices
  # Real implementation would connect and read component tree data

proc inspectDesktopComponents*(target: string) =
  ## Inspect components on desktop application
  echo "Launching desktop application with inspection mode..."

  # This would start the application with debug flags
  # Real implementation would integrate with the running app

proc findConnectedDevices*(): seq[string] =
  ## Find connected microcontroller devices
  result = @[]

  # Common device paths
  let devicePaths = [
    "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2",
    "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2"
  ]

  for devicePath in devicePaths:
    if fileExists(devicePath):
      result.add(devicePath)

proc runDiagnostics*() =
  ## Run comprehensive environment diagnostics
  echo "=== Kryon Development Environment Diagnostics ==="

  # Check core library
  if fileExists("../../core/include/kryon.h"):
    echo "✓ Kryon core header found"
  else:
    echo "✗ Kryon core header missing"

  if fileExists("../../build/libkryon_core.a"):
    echo "✓ Kryon core library built"
  else:
    echo "✗ Kryon core library missing (run 'make -C ../../core')"

  # Check renderers
  if fileExists("../../build/libkryon_fb.a"):
    echo "✓ Framebuffer renderer built"
  else:
    echo "✗ Framebuffer renderer missing"

  if fileExists("../../build/libkryon_common.a"):
    echo "✓ Common utilities built"
  else:
    echo "✗ Common utilities missing"

  # Check build tools
  let tools = ["gcc", "clang", "nim", "lua5.4"]
  for tool in tools:
    if execCmdEx("which " & tool).exitCode == 0:
      echo "✓ " & tool & " available"
    else:
      echo "✗ " & tool & " not found"

  # Check MCU tools
  let mcuTools = ["arm-none-eabi-gcc", "st-flash", "openocd"]
  echo ""
  echo "MCU Development Tools:"
  for tool in mcuTools:
    if execCmdEx("which " & tool).exitCode == 0:
      echo "✓ " & tool & " available"
    else:
      echo "- " & tool & " not found (required for MCU development)"

  echo ""
  echo "=== Diagnostics Complete ==="