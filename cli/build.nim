## Build orchestrator for Kryon projects
##
## Universal IR-based build system for different targets

import os, strutils, json, times, sequtils, osproc, tables
import config, tsx_parser, kry_parser, kry_to_kir, md_parser, html_transpiler

# FFI bindings to C web backend
type
  IRComponent = ptr object
  HTMLGenerator = ptr object

const libIrPath = when defined(release):
  getHomeDir() & ".local/lib/libkryon_ir.so"
else:
  currentSourcePath().parentDir() & "/../build/libkryon_ir.so"

const libWebPath = when defined(release):
  getHomeDir() & ".local/lib/libkryon_web.so"
else:
  currentSourcePath().parentDir() & "/../build/libkryon_web.so"

# IR deserialization
proc ir_read_json_v2_file(filename: cstring): IRComponent {.
  importc, dynlib: libIrPath.}

proc ir_destroy_component(component: IRComponent) {.
  importc, dynlib: libIrPath.}

# HTML generation
proc html_generator_create(): HTMLGenerator {.
  importc, dynlib: libWebPath.}

proc html_generator_destroy(generator: HTMLGenerator) {.
  importc, dynlib: libWebPath.}

proc html_generator_generate(generator: HTMLGenerator, root: IRComponent): cstring {.
  importc, dynlib: libWebPath.}

# Forward declarations
proc buildLinuxTarget*()
proc buildSTM32Target*()
proc buildWindowsTarget*()
proc buildMacOSTarget*()
proc buildWebTarget*()
proc buildTerminalTarget*()

# IR-based build system
proc buildWithIR*(sourceFile: string, target: string)
proc buildWithIRForPage*(sourceFile: string, target: string, pagePath: string, cfg: KryonConfig, docsFile: string = "")
proc compileToKIR*(sourceFile: string): string
proc compileToKIRForPage*(sourceFile: string, pageName: string): string
proc renderIRToTarget*(kirFile: string, target: string)
proc generateWebFromKir*(kirFile: string, outputDir: string, cfg: KryonConfig)

# Docs auto-generation helpers
proc convertMarkdownToHtml*(markdown: string): string

proc filenameToSlug*(filename: string): string =
  ## Convert filename to URL-friendly slug
  ## "lua-bindings.md" -> "lua-bindings"
  ## "getting-started.md" -> "getting-started"
  result = filename.changeFileExt("").replace("_", "-").toLowerAscii()

proc generateDocsPages*(cfg: KryonConfig): seq[PageEntry] =
  ## Scan docs directory and generate page entries for each markdown file
  result = @[]
  if not cfg.docsConfig.enabled:
    return

  let docsDir = cfg.docsConfig.directory
  if not dirExists(docsDir):
    echo "⚠️  Docs directory not found: " & docsDir
    return

  for file in walkDir(docsDir):
    if file.kind == pcFile and file.path.endsWith(".md"):
      let filename = file.path.extractFilename()
      let slug = filenameToSlug(filename)

      # Index file gets base path, others get /base/slug
      var urlPath: string
      if filename == cfg.docsConfig.indexFile:
        urlPath = cfg.docsConfig.basePath
      else:
        urlPath = cfg.docsConfig.basePath & "/" & slug

      result.add(PageEntry(
        file: cfg.docsConfig.templateFile,
        path: urlPath,
        docsFile: file.path
      ))

  echo "📚 Auto-generated " & $result.len & " doc pages from " & docsDir

proc loadMarkdownContent(filePath: string): string =
  ## Load markdown file content
  if fileExists(filePath):
    try:
      return readFile(filePath)
    except:
      echo "⚠️  Could not read markdown file: " & filePath
  return ""

proc loadProjectPlugins*(cfg: KryonConfig) =
  ## Load and initialize plugins declared in the project config
  ## For now, this just reports which plugins are configured
  ## Full dynamic loading will be implemented when the plugin C library is integrated
  if cfg.plugins.len == 0:
    return

  echo "🔌 Loading " & $cfg.plugins.len & " plugin(s)..."
  for plugin in cfg.plugins:
    case plugin.source:
      of psPath:
        echo "   📦 " & plugin.name & " (path: " & plugin.path & ")"
        # TODO: Load plugin shared library from path
      of psGit:
        echo "   📦 " & plugin.name & " (git: " & plugin.gitUrl & ")"
        # TODO: Clone and build plugin from git
      of psRegistry:
        echo "   📦 " & plugin.name & " (version: " & plugin.version & ")"
        # TODO: Load plugin from registry

proc substituteDocsContent*(kirJson: JsonNode, docsFilePath: string): JsonNode =
  ## Recursively find Markdown components and convert them to IR components using core parser
  ## This makes all markdown content (including Mermaid) become proper Kryon IR elements
  ## docsFilePath: the actual markdown file to use (overrides the file attribute in JSON)
  result = kirJson.copy()

  proc substituteInNode(node: JsonNode): JsonNode =
    if node.kind != JObject:
      return node

    # If this is a Markdown component, convert to IR using proper pipeline
    if node.hasKey("type") and node["type"].getStr() == "Markdown":
      if fileExists(docsFilePath):
        echo "📝 Converting markdown file to IR: ", docsFilePath

        try:
          # Use C markdown parser (NOW FIXED with context initialization)
          let kirJson = parseMdToKir(docsFilePath)
          let kirNode = parseJson(kirJson)

          # Return the IR tree (will contain Heading, Paragraph, Flowchart components, etc.)
          return kirNode
        except:
          echo "⚠️  Markdown parsing failed for: ", docsFilePath
          # Return error text component
          return %* {
            "type": "Text",
            "text": "Error: Failed to parse markdown file: " & docsFilePath,
            "color": "#ff0000"
          }
      else:
        echo "⚠️  Markdown file not found: ", docsFilePath
        # Return error text component
        return %* {
          "type": "Text",
          "text": "Error: Markdown file not found: " & docsFilePath,
          "color": "#ff0000"
        }

    # Recursively process children
    var modifiedNode = node.copy()
    if modifiedNode.hasKey("children"):
      var newChildren = newJArray()
      for child in modifiedNode["children"]:
        newChildren.add(substituteInNode(child))
      modifiedNode["children"] = newChildren

    # Also check template field (for component definitions)
    if modifiedNode.hasKey("template"):
      modifiedNode["template"] = substituteInNode(modifiedNode["template"])

    return modifiedNode

  # Search in root
  if result.hasKey("root"):
    result["root"] = substituteInNode(result["root"])

  # Search in component definitions
  if result.hasKey("component_definitions"):
    var newDefs = newJArray()
    for def in result["component_definitions"]:
      newDefs.add(substituteInNode(def))
    result["component_definitions"] = newDefs

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

  # Load declared plugins
  loadProjectPlugins(cfg)

  # Collect all pages: explicit + auto-generated docs
  var allPages = cfg.buildPages

  # Debug: show docs config state
  echo "📋 Docs config: enabled=" & $cfg.docsConfig.enabled & ", directory=" & cfg.docsConfig.directory

  # Add auto-generated docs pages
  if cfg.docsConfig.enabled:
    let docsPages = generateDocsPages(cfg)
    allPages.add(docsPages)
  else:
    echo "   (docs auto-generation disabled)"

  # Check for multi-page configuration
  if allPages.len > 0 and target.toLowerAscii() == "web":
    echo "📄 Multi-page build configured (" & $allPages.len & " pages)"
    for page in allPages:
      if not fileExists(page.file):
        echo "⚠️  Page source not found: " & page.file
        continue

      echo "📁 Building page: " & page.file & " → " & page.path
      buildWithIRForPage(page.file, target, page.path, cfg, page.docsFile)

    # Copy assets directory if it exists
    if dirExists("assets"):
      let destAssets = "build/assets"
      if dirExists(destAssets):
        removeDir(destAssets)
      copyDir("assets", destAssets)
      echo "📁 Copied assets/ → build/assets/"

    return

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
  elif fileExists("index.md"):
    sourceFile = "index.md"
  elif fileExists("main.md"):
    sourceFile = "main.md"
  elif fileExists("README.md"):
    sourceFile = "README.md"
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
    of "linux", "windows", "macos", "stm32f4", "web", "terminal", "html":
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

proc buildWithIRForPage*(sourceFile: string, target: string, pagePath: string, cfg: KryonConfig, docsFile: string = "") =
  ## Build a single page for multi-page web builds
  ## pagePath: "/" for root, "/docs" for docs subdirectory, etc.
  ## docsFile: Optional markdown file path for auto-generated docs pages
  # Use page path for unique cache filename
  let pageName = if pagePath == "/":
                   sourceFile.splitFile().name
                 else:
                   pagePath.strip(chars = {'/'}).replace("/", "_")

  # Step 1: Compile source to KIR
  let kirFile = compileToKIRForPage(sourceFile, pageName)

  # Step 1.5: If this is a docs page, embed the markdown content in KIR
  if docsFile.len > 0:
    var kirJson = parseJson(readFile(kirFile))
    kirJson = substituteDocsContent(kirJson, docsFile)
    writeFile(kirFile, kirJson.pretty())

  # Step 2: Determine output directory
  var outputDir = cfg.buildOutputDir
  if pagePath != "/" and pagePath.len > 0:
    # Strip leading/trailing slashes and create subdirectory
    let subdir = pagePath.strip(chars = {'/'})
    outputDir = cfg.buildOutputDir / subdir
  createDir(outputDir)

  # Step 3: Generate web files
  echo "   🌐 Generating web files..."
  generateWebFromKir(kirFile, outputDir, cfg)

  let htmlFile = if pagePath == "/": "index.html" else: outputDir / "index.html"
  echo "   ✅ Generated: " & htmlFile

proc compileToKIR*(sourceFile: string): string =
  ## Compile source file to KIR JSON format
  ## Handles: .tsx, .jsx, .kry, .nim, .md
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

    of ".md", ".markdown":
      # Markdown → KIR via core parser
      echo "   📝 Parsing Markdown..."
      let kirContent = parseMdToKir(sourceFile, kirFile)
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

proc compileToKIRForPage*(sourceFile: string, pageName: string): string =
  ## Compile source file to KIR JSON format with a custom name for multi-page builds
  let ext = sourceFile.splitFile().ext.toLowerAscii()
  createDir(".kryon_cache")
  let kirFile = ".kryon_cache/" & pageName & ".kir"

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

    of ".md", ".markdown":
      # Markdown → KIR via core parser
      echo "   📝 Parsing Markdown..."
      let kirContent = parseMdToKir(sourceFile, kirFile)
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

    of "html":
      # Generate standalone HTML from KIR (transpilation mode)
      echo "   📄 Generating standalone HTML..."
      let htmlOutputPath = outputDir / (projectName & ".html")
      let opts = defaultTranspilationOptions()
      transpileKirToHTMLFile(kirFile, htmlOutputPath, opts)
      echo "   ✅ HTML file generated: " & htmlOutputPath

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

# Forward declaration for markdown inline processing
proc processInlineMarkdown*(text: string): string

# FFI bindings for flowchart rendering
{.passC: "-I" & currentSourcePath().parentDir() & "/../ir".}
{.passL: "-Lbuild -lkryon_ir".}
proc convertMarkdownToHtml*(markdown: string): string =
  ## Convert markdown to HTML with proper CSS classes
  ## Supports: headers, paragraphs, code blocks, lists, blockquotes, links, tables
  echo "🔧 convertMarkdownToHtml called with ", markdown.len, " bytes of markdown"
  result = "<div class=\"kryon-md\">\n"

  # Normalize line endings (handle \r\n, \r, and escaped \n)
  var normalizedMd = markdown.replace("\\n", "\n").replace("\r\n", "\n").replace("\r", "\n")

  let debugBuild = getEnv("KRYON_DEBUG_BUILD") == "1"
  if debugBuild:
    echo "DEBUG convertMarkdownToHtml: input length=", markdown.len
    echo "DEBUG: First 200 chars: ", markdown[0..min(199, markdown.len-1)]

  var lines = normalizedMd.split("\n")
  var i = 0
  var inCodeBlock = false
  var codeBlockLang = ""
  var codeBlockContent = ""
  var inList = false
  var listType = ""

  while i < lines.len:
    var line = lines[i]

    if debugBuild and line.strip().len > 0 and line.contains("|"):
      echo "DEBUG line ", i, ": '", line, "' startsWith('|')=", line.strip().startsWith("|")

    # Code blocks (fenced)
    if line.startsWith("```"):
      if inCodeBlock:
        # End code block
        # Check if this is a Mermaid flowchart
        if codeBlockLang == "mermaid" or codeBlockLang == "mmd":
          # Convert Mermaid to SVG using the core parser
          echo "📊 Converting Mermaid diagram to SVG (", codeBlockContent.len, " bytes)"
          # Remove try/except temporarily to see the actual error
          let svgOutput = convertMermaidToSvg(codeBlockContent, SVGTheme.Dark)
          echo "✅ Mermaid converted to SVG (", svgOutput.len, " bytes)"
          result.add("<div class=\"kryon-flowchart-container\">\n")
          result.add(svgOutput)
          result.add("</div>\n")
        else:
          # Regular code block
          result.add("<pre class=\"kryon-md-pre\"><code")
          if codeBlockLang.len > 0:
            result.add(" class=\"language-" & codeBlockLang & "\"")
          result.add(">")
          result.add(codeBlockContent.replace("<", "&lt;").replace(">", "&gt;"))
          result.add("</code></pre>\n")

        inCodeBlock = false
        codeBlockLang = ""
        codeBlockContent = ""
      else:
        # Start code block
        inCodeBlock = true
        codeBlockLang = line[3..^1].strip()
      i += 1
      continue

    if inCodeBlock:
      if codeBlockContent.len > 0:
        codeBlockContent.add("\n")
      codeBlockContent.add(line)
      i += 1
      continue

    # Close open list if we hit a non-list line
    if inList and not line.startsWith("- ") and not line.startsWith("* ") and
       not (line.len > 2 and line[0].isDigit and line[1] == '.'):
      if listType == "ul":
        result.add("</ul>\n")
      else:
        result.add("</ol>\n")
      inList = false

    # Empty line
    if line.strip().len == 0:
      i += 1
      continue

    # Headers
    if line.startsWith("######"):
      result.add("<h6 class=\"kryon-md-h6\">" & line[6..^1].strip() & "</h6>\n")
    elif line.startsWith("#####"):
      result.add("<h5 class=\"kryon-md-h5\">" & line[5..^1].strip() & "</h5>\n")
    elif line.startsWith("####"):
      result.add("<h4 class=\"kryon-md-h4\">" & line[4..^1].strip() & "</h4>\n")
    elif line.startsWith("###"):
      result.add("<h3 class=\"kryon-md-h3\">" & line[3..^1].strip() & "</h3>\n")
    elif line.startsWith("##"):
      result.add("<h2 class=\"kryon-md-h2\">" & line[2..^1].strip() & "</h2>\n")
    elif line.startsWith("#"):
      result.add("<h1 class=\"kryon-md-h1\">" & line[1..^1].strip() & "</h1>\n")

    # Blockquote
    elif line.startsWith(">"):
      result.add("<blockquote class=\"kryon-md-blockquote\"><p>" & line[1..^1].strip() & "</p></blockquote>\n")

    # Horizontal rule
    elif line.strip() == "---" or line.strip() == "***" or line.strip() == "___":
      result.add("<hr class=\"kryon-md-hr\" />\n")

    # Unordered list
    elif line.startsWith("- ") or line.startsWith("* "):
      if not inList:
        result.add("<ul class=\"kryon-md-ul\">\n")
        inList = true
        listType = "ul"
      result.add("<li class=\"kryon-md-li\">" & processInlineMarkdown(line[2..^1].strip()) & "</li>\n")

    # Ordered list
    elif line.len > 2 and line[0].isDigit and line[1] == '.':
      if not inList:
        result.add("<ol class=\"kryon-md-ol\">\n")
        inList = true
        listType = "ol"
      result.add("<li class=\"kryon-md-li\">" & processInlineMarkdown(line[2..^1].strip()) & "</li>\n")

    # Markdown table
    elif line.strip().startsWith("|"):
      # Start of table - collect all table rows
      var tableRows: seq[string] = @[]
      while i < lines.len and lines[i].strip().startsWith("|"):
        tableRows.add(lines[i])
        i += 1
      i -= 1  # Back up one since we'll increment at end of loop

      if tableRows.len >= 2:
        result.add("<table class=\"kryon-md-table\">\n")

        # Parse header row
        let headerLine = tableRows[0]
        var headerCells = headerLine.split("|").filterIt(it.strip().len > 0)
        result.add("<thead>\n<tr>")
        for cell in headerCells:
          result.add("<th class=\"kryon-md-th\">" & processInlineMarkdown(cell.strip()) & "</th>")
        result.add("</tr>\n</thead>\n")

        # Parse data rows (skip separator row at index 1)
        result.add("<tbody>\n")
        for rowIdx in 2 ..< tableRows.len:
          let row = tableRows[rowIdx]
          var cells = row.split("|").filterIt(it.strip().len > 0)
          result.add("<tr>")
          for cell in cells:
            result.add("<td class=\"kryon-md-td\">" & processInlineMarkdown(cell.strip()) & "</td>")
          result.add("</tr>\n")
        result.add("</tbody>\n")

        result.add("</table>\n")

    # Paragraph
    else:
      result.add("<p class=\"kryon-md-p\">" & processInlineMarkdown(line) & "</p>\n")

    i += 1

  # Close any open list
  if inList:
    if listType == "ul":
      result.add("</ul>\n")
    else:
      result.add("</ol>\n")

  result.add("</div>")

proc replaceDelimited(text: string, delim: string, openTag: string, closeTag: string): string =
  ## Replace text between delimiters with HTML tags
  ## e.g. replaceDelimited("**bold**", "**", "<strong>", "</strong>") -> "<strong>bold</strong>"
  result = text
  var pos = 0
  while pos < result.len:
    let startPos = result.find(delim, pos)
    if startPos < 0: break
    let endPos = result.find(delim, startPos + delim.len)
    if endPos < 0: break
    let content = result[startPos + delim.len ..< endPos]
    let replacement = openTag & content & closeTag
    result = result[0 ..< startPos] & replacement & result[endPos + delim.len .. ^1]
    pos = startPos + replacement.len

proc replaceLinks(text: string): string =
  ## Replace markdown links [text](url) with <a> tags
  result = text
  var pos = 0
  while pos < result.len:
    let bracketStart = result.find('[', pos)
    if bracketStart < 0: break
    let bracketEnd = result.find(']', bracketStart + 1)
    if bracketEnd < 0: break
    # Check for (url) immediately after ]
    if bracketEnd + 1 >= result.len or result[bracketEnd + 1] != '(':
      pos = bracketEnd + 1
      continue
    let parenEnd = result.find(')', bracketEnd + 2)
    if parenEnd < 0:
      pos = bracketEnd + 1
      continue
    let linkText = result[bracketStart + 1 ..< bracketEnd]
    let linkUrl = result[bracketEnd + 2 ..< parenEnd]
    let replacement = "<a class=\"kryon-md-link\" href=\"" & linkUrl & "\">" & linkText & "</a>"
    result = result[0 ..< bracketStart] & replacement & result[parenEnd + 1 .. ^1]
    pos = bracketStart + replacement.len

proc processInlineMarkdown*(text: string): string =
  ## Process inline markdown: bold, italic, code, links
  result = text

  # Inline code (backticks)
  var pos = 0
  while pos < result.len:
    let codeStart = result.find('`', pos)
    if codeStart < 0: break
    let codeEnd = result.find('`', codeStart + 1)
    if codeEnd < 0: break
    let code = result[codeStart + 1 ..< codeEnd]
    let replacement = "<code class=\"kryon-md-code\">" & code.replace("<", "&lt;").replace(">", "&gt;") & "</code>"
    result = result[0 ..< codeStart] & replacement & result[codeEnd + 1 .. ^1]
    pos = codeStart + replacement.len

  # Bold (**text** or __text__) - must be before italic since ** contains *
  result = replaceDelimited(result, "**", "<strong>", "</strong>")
  result = replaceDelimited(result, "__", "<strong>", "</strong>")

  # Italic (*text* or _text_)
  result = replaceDelimited(result, "*", "<em>", "</em>")
  result = replaceDelimited(result, "_", "<em>", "</em>")

  # Links [text](url)
  result = replaceLinks(result)

proc generateWebFromKir*(kirFile: string, outputDir: string, cfg: KryonConfig) =
  ## Generate HTML from KIR using C web backend
  ## This ensures proper rendering of all IR components including Flowchart → SVG
  echo "🌐 Generating web output from: ", kirFile

  # Load .kir file → IRComponent using C deserialization
  echo "   📥 Deserializing IR from .kir file..."
  let component = ir_read_json_v2_file(kirFile.cstring)
  if component == nil:
    raise newException(IOError, "Failed to read KIR file: " & kirFile)

  defer:
    echo "   🧹 Cleaning up IR component..."
    ir_destroy_component(component)

  # Generate HTML using C web backend
  echo "   🎨 Generating HTML with web backend (includes SVG for flowcharts)..."
  let generator = html_generator_create()
  if generator == nil:
    raise newException(IOError, "Failed to create HTML generator")

  defer:
    html_generator_destroy(generator)

  let htmlCstr = html_generator_generate(generator, component)
  if htmlCstr == nil:
    raise newException(IOError, "Failed to generate HTML from IR")

  let htmlContent = $htmlCstr

  # Write HTML to output file
  let htmlPath = outputDir / "index.html"
  writeFile(htmlPath, htmlContent)
  echo "   ✅ Generated: " & htmlPath
  echo "   📁 Web files in: " & outputDir & "/"
  echo "   🌐 Open " & htmlPath & " in a browser"
