## Build orchestrator for Kryon projects
##
## Universal IR-based build system for different targets

import os, strutils, json, times, sequtils, osproc, tables, re
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
proc buildWithIRForPage*(sourceFile: string, target: string, pagePath: string, cfg: KryonConfig, docsFile: string = "")
proc compileToKIR*(sourceFile: string): string
proc compileToKIRForPage*(sourceFile: string, pageName: string): string
proc renderIRToTarget*(kirFile: string, target: string)
proc generateWebFromKir*(kirFile: string, outputDir: string, cfg: KryonConfig)

# Docs auto-generation helpers
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
  ## Recursively find Markdown components and embed the markdown content
  ## This allows the plugin web renderer to render the content without dynamic loading
  result = kirJson.copy()

  # Load the markdown content
  let markdownContent = loadMarkdownContent(docsFilePath)

  proc substituteInNode(node: JsonNode) =
    if node.kind != JObject:
      return

    # If this is a Markdown component, embed the content
    if node.hasKey("type") and node["type"].getStr() == "Markdown":
      node["file"] = newJString(docsFilePath)
      if markdownContent.len > 0:
        node["text_content"] = newJString(markdownContent)

    # Recurse into children
    if node.hasKey("children"):
      for child in node["children"]:
        substituteInNode(child)

    # Also check template field (for component definitions)
    if node.hasKey("template"):
      substituteInNode(node["template"])

  # Search in root
  if result.hasKey("root"):
    substituteInNode(result["root"])

  # Search in component definitions
  if result.hasKey("component_definitions"):
    for def in result["component_definitions"]:
      substituteInNode(def)

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

# Forward declaration for markdown inline processing
proc processInlineMarkdown*(text: string): string

proc convertMarkdownToHtml*(markdown: string): string =
  ## Convert markdown to HTML with proper CSS classes
  ## Supports: headers, paragraphs, code blocks, lists, blockquotes, links
  result = "<div class=\"kryon-md\">\n"

  var lines = markdown.split("\n")
  var i = 0
  var inCodeBlock = false
  var codeBlockLang = ""
  var codeBlockContent = ""
  var inList = false
  var listType = ""

  while i < lines.len:
    var line = lines[i]

    # Code blocks (fenced)
    if line.startsWith("```"):
      if inCodeBlock:
        # End code block
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

  # Bold (**text** or __text__)
  result = result.replacef(re"\*\*([^*]+)\*\*", "<strong>$1</strong>")
  result = result.replacef(re"__([^_]+)__", "<strong>$1</strong>")

  # Italic (*text* or _text_)
  result = result.replacef(re"\*([^*]+)\*", "<em>$1</em>")
  result = result.replacef(re"_([^_]+)_", "<em>$1</em>")

  # Links [text](url)
  result = result.replacef(re"\[([^\]]+)\]\(([^)]+)\)", "<a class=\"kryon-md-link\" href=\"$2\">$1</a>")

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
      of "Markdown": "div"
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

      if node.hasKey("maxWidth"):
        let mw = node["maxWidth"]
        if mw.kind == JString:
          styles.add("max-width: " & mw.getStr())
        elif mw.kind == JInt or mw.kind == JFloat:
          styles.add("max-width: " & $mw.getFloat().int & "px")

      if node.hasKey("minWidth"):
        let mw = node["minWidth"]
        if mw.kind == JString:
          styles.add("min-width: " & mw.getStr())
        elif mw.kind == JInt or mw.kind == JFloat:
          styles.add("min-width: " & $mw.getFloat().int & "px")

      if node.hasKey("maxHeight"):
        let mh = node["maxHeight"]
        if mh.kind == JString:
          styles.add("max-height: " & mh.getStr())
        elif mh.kind == JInt or mh.kind == JFloat:
          styles.add("max-height: " & $mh.getFloat().int & "px")

      if node.hasKey("minHeight"):
        let mh = node["minHeight"]
        if mh.kind == JString:
          styles.add("min-height: " & mh.getStr())
        elif mh.kind == JInt or mh.kind == JFloat:
          styles.add("min-height: " & $mh.getFloat().int & "px")

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

    if node.hasKey("paddingTop"):
      let p = node["paddingTop"]
      if p.kind == JInt or p.kind == JFloat:
        styles.add("padding-top: " & $p.getFloat().int & "px")

    if node.hasKey("paddingBottom"):
      let p = node["paddingBottom"]
      if p.kind == JInt or p.kind == JFloat:
        styles.add("padding-bottom: " & $p.getFloat().int & "px")

    if node.hasKey("paddingLeft"):
      let p = node["paddingLeft"]
      if p.kind == JInt or p.kind == JFloat:
        styles.add("padding-left: " & $p.getFloat().int & "px")

    if node.hasKey("paddingRight"):
      let p = node["paddingRight"]
      if p.kind == JInt or p.kind == JFloat:
        styles.add("padding-right: " & $p.getFloat().int & "px")

    if node.hasKey("margin"):
      let m = node["margin"]
      if m.kind == JString:
        styles.add("margin: " & m.getStr())
      elif m.kind == JInt or m.kind == JFloat:
        styles.add("margin: " & $m.getFloat().int & "px")

    if node.hasKey("marginTop"):
      let m = node["marginTop"]
      if m.kind == JInt or m.kind == JFloat:
        styles.add("margin-top: " & $m.getFloat().int & "px")

    if node.hasKey("marginBottom"):
      let m = node["marginBottom"]
      if m.kind == JInt or m.kind == JFloat:
        styles.add("margin-bottom: " & $m.getFloat().int & "px")

    if node.hasKey("marginLeft"):
      let m = node["marginLeft"]
      if m.kind == JInt or m.kind == JFloat:
        styles.add("margin-left: " & $m.getFloat().int & "px")

    if node.hasKey("marginRight"):
      let m = node["marginRight"]
      if m.kind == JInt or m.kind == JFloat:
        styles.add("margin-right: " & $m.getFloat().int & "px")

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

    if node.hasKey("flexGrow"):
      let fg = node["flexGrow"]
      if fg.kind == JInt or fg.kind == JFloat:
        styles.add("flex-grow: " & $fg.getFloat().int)

    if node.hasKey("flexShrink"):
      let fs = node["flexShrink"]
      if fs.kind == JInt or fs.kind == JFloat:
        styles.add("flex-shrink: " & $fs.getFloat().int)

    if node.hasKey("flexBasis"):
      let fb = node["flexBasis"]
      if fb.kind == JString:
        styles.add("flex-basis: " & fb.getStr())
      elif fb.kind == JInt or fb.kind == JFloat:
        styles.add("flex-basis: " & $fb.getFloat().int & "px")

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

    # Handle Markdown source content (file or inline)
    if componentType == "Markdown":
      var mdSource = ""

      # File prop takes precedence over inline source
      if node.hasKey("file"):
        let filePath = node["file"].getStr()
        let fullPath = getCurrentDir() / filePath
        if fileExists(fullPath):
          mdSource = readFile(fullPath)
        else:
          mdSource = "**Error:** Markdown file not found: " & filePath
      elif node.hasKey("source"):
        let sourceNode = node["source"]
        if sourceNode.kind == JString:
          mdSource = sourceNode.getStr()

      if mdSource.len > 0:
        html.add(convertMarkdownToHtml(mdSource))

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
.kryon-markdown { padding: 20px; line-height: 1.6; color: #E6EDF3; }
.kryon-md { padding: 20px; line-height: 1.6; color: #E6EDF3; }
.kryon-md-h1 { font-size: 2em; font-weight: bold; margin: 0.67em 0; border-bottom: 1px solid #30363D; padding-bottom: 0.3em; color: #E6EDF3; }
.kryon-md-h2 { font-size: 1.5em; font-weight: bold; margin: 0.83em 0; border-bottom: 1px solid #30363D; padding-bottom: 0.3em; color: #E6EDF3; }
.kryon-md-h3 { font-size: 1.25em; font-weight: bold; margin: 1em 0; color: #E6EDF3; }
.kryon-md-h4 { font-size: 1em; font-weight: bold; margin: 1.33em 0; color: #E6EDF3; }
.kryon-md-h5 { font-size: 0.9em; font-weight: bold; margin: 1.5em 0; color: #E6EDF3; }
.kryon-md-h6 { font-size: 0.85em; font-weight: bold; margin: 1.67em 0; color: #8B949E; }
.kryon-md-p { margin: 1em 0; color: #C9D1D9; }
.kryon-md-code { background: #161B22; padding: 0.2em 0.4em; border-radius: 4px; font-family: monospace; font-size: 0.9em; color: #79C0FF; }
.kryon-md-pre { background: #161B22; padding: 16px; border-radius: 8px; overflow-x: auto; margin: 1em 0; border: 1px solid #30363D; }
.kryon-md-pre code { background: none; padding: 0; font-size: 0.9em; color: #C9D1D9; }
.kryon-md-ul, .kryon-md-ol { padding-left: 2em; margin: 1em 0; color: #C9D1D9; }
.kryon-md-li { margin: 0.25em 0; color: #C9D1D9; }
.kryon-md-blockquote { border-left: 4px solid #00A8FF; padding-left: 16px; margin: 1em 0; color: #8B949E; }
.kryon-md-hr { border: none; border-top: 1px solid #30363D; margin: 2em 0; }
.kryon-md-link { color: #58A6FF; text-decoration: none; }
.kryon-md-link:hover { text-decoration: underline; }
.kryon-md-table { border-collapse: collapse; width: 100%; margin: 1em 0; }
.kryon-md-th, .kryon-md-td { border: 1px solid #30363D; padding: 8px 12px; text-align: left; color: #C9D1D9; }
.kryon-md-th { background: #161B22; font-weight: bold; color: #E6EDF3; }
.kryon-md strong { color: #E6EDF3; }
.kryon-md em { color: #C9D1D9; font-style: italic; }
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