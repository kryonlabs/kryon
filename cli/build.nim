## Build orchestrator for Kryon projects
##
## Universal IR-based build system for different targets

import os, strutils, json, times, sequtils, osproc, tables
import config, tsx_parser, kry_parser, kry_to_kir

# Forward declarations
proc buildLinuxTarget*()
proc buildSTM32Target*()
proc buildWindowsTarget*()
proc buildMacOSTarget*()
proc buildWebTarget*()
proc buildTerminalTarget*()

# IR-based build system
proc buildWithIR*(sourceFile: string, target: string)
proc compileToKIR*(sourceFile: string): string
proc renderIRToTarget*(kirFile: string, target: string)
proc generateWebFromKir*(kirFile: string, outputDir: string, cfg: KryonConfig)

# Legacy support (for transition period)
proc buildNimLinux*()
proc buildLuaLinux*()
proc buildCLinux*()
proc buildNimSTM32*()
proc buildCSTM32*()
proc buildNimTerminal*()
proc buildLuaTerminal*()
proc buildCTerminal*()

proc buildForTarget*(target: string) =
  ## Build project for specified target using universal IR system
  echo "🚀 Building for target: " & target & " (using Universal IR)"

  # Create build directory
  createDir("build")

  # Load project config if available
  let cfg = loadConfig()

  # Find source file - check config entry first, then auto-detect
  var sourceFile = ""
  if cfg.buildEntry != "" and fileExists(cfg.buildEntry):
    sourceFile = cfg.buildEntry
  elif fileExists("index.tsx"):
    sourceFile = "index.tsx"
  elif fileExists("index.jsx"):
    sourceFile = "index.jsx"
  elif fileExists("main.tsx"):
    sourceFile = "main.tsx"
  elif fileExists("main.kry"):
    sourceFile = "main.kry"
  elif fileExists("index.kry"):
    sourceFile = "index.kry"
  elif fileExists("src/main.nim"):
    sourceFile = "src/main.nim"
  elif fileExists("main.nim"):
    sourceFile = "main.nim"
  elif fileExists("src/main.lua"):
    echo "⚠️  Lua frontend not yet implemented in IR system"
    echo "   Falling back to legacy build system..."
    buildLinuxTarget()  # Legacy fallback
    return
  elif fileExists("src/main.c"):
    echo "⚠️  C frontend not yet implemented in IR system"
    echo "   Falling back to legacy build system..."
    buildLinuxTarget()  # Legacy fallback
    return
  else:
    raise newException(ValueError, "No recognized source files found")

  echo "📁 Source: " & sourceFile

  # Use the universal IR pipeline
  case target.toLowerAscii():
    of "linux", "windows", "macos", "stm32f4", "web", "terminal":
      buildWithIR(sourceFile, target)
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
  ## Build for Web using IR system (HTML + CSS + WASM)
  echo "🌐 Building for Web target (HTML + CSS + WASM)..."

  if fileExists("src/main.nim"):
    buildWithIR("src/main.nim", "web")
  else:
    raise newException(ValueError, "Nim source file not found for IR compilation")

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

# ==============================================
# UNIVERSAL IR BUILD SYSTEM
# ==============================================

proc buildWithIR*(sourceFile: string, target: string) =
  ## Universal build pipeline: Source → KIR → Target
  echo "🔄 Starting Universal IR pipeline..."
  echo "   Source: " & sourceFile
  echo "   Target: " & target

  # Step 1: Compile source to KIR
  echo "📝 Step 1: Compiling source to KIR..."
  let kirFile = compileToKIR(sourceFile)

  # Step 2: Render KIR to target
  echo "🎨 Step 2: Rendering KIR to target..."
  renderIRToTarget(kirFile, target)

  echo "✅ Universal IR build complete!"

proc compileToKIR*(sourceFile: string): string =
  ## Compile source file to KIR JSON format
  ## Handles: .tsx, .jsx, .kry, .nim
  let ext = sourceFile.splitFile().ext.toLowerAscii()
  let projectName = getCurrentDir().splitPath().tail
  createDir(".kryon_cache")
  let kirFile = ".kryon_cache/" & projectName & ".kir"

  echo "   🏗️  Compiling " & ext & " → KIR..."

  case ext:
    of ".tsx", ".jsx":
      # TSX/JSX → KIR via Bun parser
      echo "   📦 Parsing TSX/JSX with Bun..."
      discard parseTsxToKir(sourceFile, kirFile)
      echo "   ✅ KIR generated: " & kirFile
      result = kirFile

    of ".kry":
      # KRY → KIR via native parser
      echo "   📝 Parsing KRY..."
      let kryContent = readFile(sourceFile)
      let ast = parseKry(kryContent, sourceFile)
      let kirJson = transpileToKir(ast, preserveStatic = false)
      writeFile(kirFile, kirJson.pretty())
      echo "   ✅ KIR generated: " & kirFile
      result = kirFile

    of ".nim":
      # Nim → KIR via compilation with KRYON_SERIALIZE_IR
      echo "   🔨 Compiling Nim..."

      # Build the Nim file with IR serialization enabled
      let nimBin = sourceFile.changeFileExt("")
      let env = "KRYON_SERIALIZE_IR=\"" & kirFile & "\""
      let compileCmd = env & " nim c -p:bindings/nim -o:" & nimBin & " " & sourceFile

      let (compileOutput, compileExitCode) = execCmdEx(compileCmd)
      if compileExitCode != 0:
        echo "   ❌ Nim compilation failed:"
        echo compileOutput
        raise newException(IOError, "Nim compilation failed")

      # Run to generate KIR (auto-exits after serialization)
      let runCmd = env & " timeout 1 " & nimBin & " || true"
      discard execCmdEx(runCmd)

      if not fileExists(kirFile):
        raise newException(IOError, "Nim did not generate KIR file: " & kirFile)

      echo "   ✅ KIR generated: " & kirFile
      result = kirFile

    else:
      raise newException(ValueError, "Unsupported source file type: " & ext)

proc renderIRToTarget*(kirFile: string, target: string) =
  ## Render KIR to specific target platform
  echo "   🎯 Rendering KIR → " & target & "..."

  let cfg = loadConfig()
  let projectName = if cfg.projectName != "": cfg.projectName else: getCurrentDir().splitPath().tail
  let outputDir = if cfg.buildOutputDir != "": cfg.buildOutputDir else: "build"

  createDir(outputDir)

  case target.toLowerAscii():
    of "web":
      # Generate web output from KIR
      echo "   🌐 Generating web files..."
      generateWebFromKir(kirFile, outputDir, cfg)

    of "linux":
      # Build desktop backend
      echo "   🖥️  Building desktop backend..."
      let desktopCmd = "make -C ../../backends/desktop ENABLE_SDL3=1"
      let (desktopOutput, desktopExitCode) = execCmdEx(desktopCmd)

      if desktopExitCode != 0:
        echo "   ❌ Desktop backend build failed:"
        echo desktopOutput
        raise newException(IOError, "Desktop backend build failed")

      echo "   ✅ Linux desktop target ready"
      echo "   📝 Run with: ./build/" & projectName & "_desktop"

    of "terminal":
      # Terminal target using IR (placeholder for future implementation)
      echo "   📟 Terminal IR renderer not yet implemented"
      echo "   🔄 Falling back to legacy terminal build..."

    else:
      echo "   ⚠️  Target '" & target & "' IR renderer not yet implemented"
      echo "   🔄 Add IR renderer for " & target & " in ../../backends/"

proc generateWebFromKir*(kirFile: string, outputDir: string, cfg: KryonConfig) =
  ## Generate HTML/CSS/JS from KIR JSON
  let kirContent = readFile(kirFile)
  let kirJson = parseJson(kirContent)

  # Build component definitions lookup map
  var componentMap = initTable[string, JsonNode]()
  if kirJson.hasKey("component_definitions"):
    for def in kirJson["component_definitions"]:
      if def.hasKey("name") and def.hasKey("template"):
        componentMap[def["name"].getStr()] = def["template"]

  # Extract app metadata from root or metadata section
  var appTitle = cfg.projectName
  var appWidth = 800
  var appHeight = 600
  var appBg = "#ffffff"

  if kirJson.hasKey("root"):
    let root = kirJson["root"]
    if root.hasKey("windowTitle"):
      appTitle = root["windowTitle"].getStr()
    if root.hasKey("width"):
      let w = root["width"]
      if w.kind == JString:
        let ws = w.getStr()
        if ws.endsWith("px"):
          appWidth = parseInt(ws.replace(".0px", "").replace("px", ""))
      elif w.kind == JInt or w.kind == JFloat:
        appWidth = w.getFloat().int
    if root.hasKey("height"):
      let h = root["height"]
      if h.kind == JString:
        let hs = h.getStr()
        if hs.endsWith("px"):
          appHeight = parseInt(hs.replace(".0px", "").replace("px", ""))
      elif h.kind == JInt or h.kind == JFloat:
        appHeight = h.getFloat().int
    if root.hasKey("background"):
      appBg = root["background"].getStr()

  if kirJson.hasKey("metadata"):
    let appMeta = kirJson["metadata"]
    if appMeta.hasKey("title"):
      appTitle = appMeta["title"].getStr()
    if appMeta.hasKey("window_width"):
      appWidth = appMeta["window_width"].getInt()
    if appMeta.hasKey("window_height"):
      appHeight = appMeta["window_height"].getInt()
    if appMeta.hasKey("background"):
      appBg = appMeta["background"].getStr()

  # Generate CSS from component tree
  var cssRules = ""
  var htmlBody = ""
  var idCounter = 1000  # For generated IDs

  proc generateComponentCssAndHtml(node: JsonNode, parentId: string = "", depth: int = 0, props: Table[string, JsonNode] = initTable[string, JsonNode]()): tuple[css: string, html: string] =
    var css = ""
    var html = ""

    let componentType = if node.hasKey("type"): node["type"].getStr() else: "Container"
    let componentId = if node.hasKey("id"): "c" & $node["id"].getInt() else: "c" & $idCounter
    idCounter += 1

    # Check if this is a custom component that needs expansion
    if componentMap.hasKey(componentType):
      # Collect props from the call site
      var callProps = initTable[string, JsonNode]()
      for key, val in node.pairs:
        if key notin ["id", "type", "children"]:
          callProps[key] = val
      # Expand the component template with props
      let componentTemplate = componentMap[componentType]
      return generateComponentCssAndHtml(componentTemplate, parentId, depth, callProps)

    # Built-in component types
    let tagName = case componentType:
      of "Text": "span"
      of "Button": "button"
      of "Link": "a"
      of "Row", "Column", "Container": "div"
      else: "div"

    # Build opening tag with special attributes
    var openingTag = "<" & tagName & " id=\"" & componentId & "\" class=\"kryon-" & componentType.toLowerAscii() & "\""

    # Handle href for Link components
    if componentType == "Link" and node.hasKey("href"):
      openingTag.add(" href=\"" & node["href"].getStr() & "\"")

    # Handle target for Link components
    if componentType == "Link" and node.hasKey("target"):
      openingTag.add(" target=\"" & node["target"].getStr() & "\"")

    openingTag.add(">")
    html.add(openingTag)

    # Extract styles
    var styles: seq[string] = @[]

    # Root element (depth 0) uses responsive dimensions
    if depth == 0:
      styles.add("width: 100%")
      styles.add("min-height: 100vh")
    else:
      if node.hasKey("width"):
        let w = node["width"]
        if w.kind == JString:
          styles.add("width: " & w.getStr())
        elif w.kind == JInt or w.kind == JFloat:
          styles.add("width: " & $w.getFloat().int & "px")

      if node.hasKey("height"):
        let h = node["height"]
        if h.kind == JString:
          styles.add("height: " & h.getStr())
        elif h.kind == JInt or h.kind == JFloat:
          styles.add("height: " & $h.getFloat().int & "px")

    if node.hasKey("background"):
      styles.add("background: " & node["background"].getStr())

    if node.hasKey("backgroundColor"):
      styles.add("background-color: " & node["backgroundColor"].getStr())

    if node.hasKey("color"):
      styles.add("color: " & node["color"].getStr())

    if node.hasKey("fontSize"):
      let fs = node["fontSize"]
      if fs.kind == JInt or fs.kind == JFloat:
        styles.add("font-size: " & $fs.getFloat().int & "px")

    if node.hasKey("fontWeight"):
      styles.add("font-weight: " & node["fontWeight"].getStr())

    if node.hasKey("padding"):
      let p = node["padding"]
      if p.kind == JString:
        styles.add("padding: " & p.getStr())
      elif p.kind == JInt or p.kind == JFloat:
        styles.add("padding: " & $p.getFloat().int & "px")

    if node.hasKey("margin"):
      let m = node["margin"]
      if m.kind == JString:
        styles.add("margin: " & m.getStr())
      elif m.kind == JInt or m.kind == JFloat:
        styles.add("margin: " & $m.getFloat().int & "px")

    if node.hasKey("borderRadius"):
      let br = node["borderRadius"]
      if br.kind == JInt or br.kind == JFloat:
        styles.add("border-radius: " & $br.getFloat().int & "px")

    if node.hasKey("border"):
      styles.add("border: " & node["border"].getStr())

    if node.hasKey("gap"):
      let g = node["gap"]
      if g.kind == JInt or g.kind == JFloat:
        styles.add("gap: " & $g.getFloat().int & "px")

    if node.hasKey("justifyContent"):
      styles.add("justify-content: " & node["justifyContent"].getStr())

    if node.hasKey("alignItems"):
      styles.add("align-items: " & node["alignItems"].getStr())

    # Add flexbox for layout containers
    if componentType in ["Row", "Column", "Container"]:
      styles.add("display: flex")
      if componentType == "Row":
        styles.add("flex-direction: row")
      elif componentType == "Column":
        styles.add("flex-direction: column")

    # Generate CSS rule
    if styles.len > 0:
      css.add("#" & componentId & " { " & styles.join("; ") & " }\n")

    # Add text content
    if node.hasKey("text"):
      let textNode = node["text"]
      if textNode.kind == JString:
        html.add(textNode.getStr())
      elif textNode.kind == JObject and textNode.hasKey("var"):
        # Variable reference - resolve from props
        let varName = textNode["var"].getStr()
        if props.hasKey(varName):
          let propVal = props[varName]
          if propVal.kind == JString:
            html.add(propVal.getStr())
          else:
            html.add($propVal)
        else:
          html.add("{{" & varName & "}}")

    # Process children
    if node.hasKey("children"):
      for child in node["children"]:
        let (childCss, childHtml) = generateComponentCssAndHtml(child, componentId, depth + 1, props)
        css.add(childCss)
        html.add(childHtml)

    # Close HTML element
    html.add("</" & tagName & ">")

    result = (css, html)

  # Generate from root
  if kirJson.hasKey("root"):
    let (css, html) = generateComponentCssAndHtml(kirJson["root"])
    cssRules = css
    htmlBody = html
  elif kirJson.hasKey("children"):
    for child in kirJson["children"]:
      let (css, html) = generateComponentCssAndHtml(child)
      cssRules.add(css)
      htmlBody.add(html)

  # Base CSS reset
  let baseCss = """
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { width: 100%; min-height: 100vh; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
.kryon-row { display: flex; flex-direction: row; }
.kryon-column { display: flex; flex-direction: column; }
.kryon-container { display: flex; }
.kryon-button { cursor: pointer; border: none; }
.kryon-text { display: inline; }
.kryon-link { text-decoration: none; cursor: pointer; }
.kryon-link:hover { text-decoration: underline; }
"""

  # Generate HTML file
  let htmlContent = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>""" & appTitle & """</title>
  <style>
""" & baseCss & """
""" & cssRules & """
  </style>
</head>
<body style="background: """ & appBg & """;">
""" & htmlBody & """
</body>
</html>
"""

  # Write files
  let htmlPath = outputDir / "index.html"
  writeFile(htmlPath, htmlContent)
  echo "   ✅ Generated: " & htmlPath

  # Optionally write separate CSS if configured
  if cfg.webGenerateSeparateFiles:
    let cssPath = outputDir / "styles.css"
    writeFile(cssPath, baseCss & "\n" & cssRules)
    echo "   ✅ Generated: " & cssPath

  echo "   📁 Web files in: " & outputDir & "/"
  echo "   🌐 Open " & htmlPath & " in a browser"