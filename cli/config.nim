##[
  Kryon Configuration System

  Provides project configuration support via kryon.toml files.
  Supports hierarchical configuration with smart defaults.
]##

import parsetoml, json, os, strutils, tables, sequtils

type
  KryonConfig* = object
    # Project metadata
    projectName*: string
    projectVersion*: string
    projectAuthor*: string
    projectDescription*: string

    # Build settings
    buildTarget*: string           # "web", "desktop", "terminal", "framebuffer"
    buildOutputDir*: string
    buildEntry*: string            # Entry file (auto-detect if empty)
    buildFrontend*: string         # "js", "ts", "nim", "lua", "c"

    # Optimization settings
    optimizationEnabled*: bool
    optimizationMinifyCss*: bool
    optimizationMinifyJs*: bool
    optimizationTreeShake*: bool

    # Web target settings
    webGenerateSeparateFiles*: bool
    webIncludeJsRuntime*: bool
    webIncludeWasmModules*: bool

    # Development settings
    devHotReload*: bool
    devPort*: int
    devWatchPaths*: seq[string]
    devOpenBrowser*: bool

    # Desktop target settings
    desktopWindowWidth*: int
    desktopWindowHeight*: int
    desktopWindowTitle*: string
    desktopResizable*: bool
    desktopVsync*: bool
    desktopTargetFps*: int
    desktopBackend*: string        # "sdl3", "terminal", "framebuffer"

    # Metadata
    metadataCreated*: string
    metadataKryonVersion*: string

proc defaultConfig*(): KryonConfig =
  ## Return default configuration with sensible values
  result.projectName = getCurrentDir().lastPathPart()
  result.projectVersion = "1.0.0"
  result.projectAuthor = ""
  result.projectDescription = ""

  result.buildTarget = "desktop"
  result.buildOutputDir = "dist"
  result.buildEntry = ""  # Auto-detect
  result.buildFrontend = ""  # Auto-detect

  result.optimizationEnabled = false
  result.optimizationMinifyCss = false
  result.optimizationMinifyJs = false
  result.optimizationTreeShake = false

  result.webGenerateSeparateFiles = true
  result.webIncludeJsRuntime = false
  result.webIncludeWasmModules = false

  result.devHotReload = true
  result.devPort = 3000
  result.devWatchPaths = @[]
  result.devOpenBrowser = false

  result.desktopWindowWidth = 800
  result.desktopWindowHeight = 600
  result.desktopWindowTitle = "Kryon App"
  result.desktopResizable = true
  result.desktopVsync = true
  result.desktopTargetFps = 60
  result.desktopBackend = "sdl3"

  result.metadataCreated = ""
  result.metadataKryonVersion = "1.2.0"

proc loadTomlConfig*(path: string): KryonConfig =
  ## Load and parse TOML configuration file
  if not fileExists(path):
    raise newException(IOError, "Config file not found: " & path)

  let toml = parseFile(path)
  result = defaultConfig()

  # Parse [project] section
  if toml.hasKey("project"):
    let proj = toml["project"]
    if proj.hasKey("name"):
      result.projectName = proj["name"].getStr()
    if proj.hasKey("version"):
      result.projectVersion = proj["version"].getStr()
    if proj.hasKey("author"):
      result.projectAuthor = proj["author"].getStr()
    if proj.hasKey("description"):
      result.projectDescription = proj["description"].getStr()

  # Parse [build] section
  if toml.hasKey("build"):
    let build = toml["build"]
    if build.hasKey("target"):
      result.buildTarget = build["target"].getStr()
    if build.hasKey("output_dir"):
      result.buildOutputDir = build["output_dir"].getStr()
    if build.hasKey("entry"):
      result.buildEntry = build["entry"].getStr()
    if build.hasKey("frontend"):
      result.buildFrontend = build["frontend"].getStr()

    # Parse [build.optimization] subsection
    if build.hasKey("optimization"):
      let opt = build["optimization"]
      if opt.hasKey("enabled"):
        result.optimizationEnabled = opt["enabled"].getBool()
      if opt.hasKey("minify_css"):
        result.optimizationMinifyCss = opt["minify_css"].getBool()
      if opt.hasKey("minify_js"):
        result.optimizationMinifyJs = opt["minify_js"].getBool()
      if opt.hasKey("tree_shake"):
        result.optimizationTreeShake = opt["tree_shake"].getBool()

  # Parse [web] section
  if toml.hasKey("web"):
    let web = toml["web"]
    if web.hasKey("generate_separate_files"):
      result.webGenerateSeparateFiles = web["generate_separate_files"].getBool()
    if web.hasKey("include_js_runtime"):
      result.webIncludeJsRuntime = web["include_js_runtime"].getBool()
    if web.hasKey("include_wasm_modules"):
      result.webIncludeWasmModules = web["include_wasm_modules"].getBool()
    if web.hasKey("minify_css"):
      result.optimizationMinifyCss = web["minify_css"].getBool()
    if web.hasKey("minify_js"):
      result.optimizationMinifyJs = web["minify_js"].getBool()

  # Parse [dev] section
  if toml.hasKey("dev"):
    let dev = toml["dev"]
    if dev.hasKey("hot_reload"):
      result.devHotReload = dev["hot_reload"].getBool()
    if dev.hasKey("port"):
      result.devPort = int(dev["port"].getInt())
    if dev.hasKey("watch_paths"):
      let paths = dev["watch_paths"]
      if paths.kind == TomlValueKind.Array:
        result.devWatchPaths = @[]
        for item in paths.getElems():
          result.devWatchPaths.add(item.getStr())
    if dev.hasKey("open_browser"):
      result.devOpenBrowser = dev["open_browser"].getBool()

  # Parse [desktop] section
  if toml.hasKey("desktop"):
    let desktop = toml["desktop"]
    if desktop.hasKey("window_width"):
      result.desktopWindowWidth = int(desktop["window_width"].getInt())
    if desktop.hasKey("window_height"):
      result.desktopWindowHeight = int(desktop["window_height"].getInt())
    if desktop.hasKey("window_title"):
      result.desktopWindowTitle = desktop["window_title"].getStr()
    if desktop.hasKey("resizable"):
      result.desktopResizable = desktop["resizable"].getBool()
    if desktop.hasKey("vsync"):
      result.desktopVsync = desktop["vsync"].getBool()
    if desktop.hasKey("target_fps"):
      result.desktopTargetFps = int(desktop["target_fps"].getInt())
    if desktop.hasKey("backend"):
      result.desktopBackend = desktop["backend"].getStr()

  # Parse [metadata] section
  if toml.hasKey("metadata"):
    let metadata = toml["metadata"]
    if metadata.hasKey("created"):
      result.metadataCreated = metadata["created"].getStr()
    if metadata.hasKey("kryon_version"):
      result.metadataKryonVersion = metadata["kryon_version"].getStr()

proc loadJsonConfig*(path: string): KryonConfig =
  ## Load legacy kryon.json configuration for backward compatibility
  if not fileExists(path):
    raise newException(IOError, "Config file not found: " & path)

  let jsonData = parseFile(path)
  result = defaultConfig()

  # Parse basic fields from legacy format
  if jsonData.hasKey("name"):
    result.projectName = jsonData["name"].getStr()
  if jsonData.hasKey("version"):
    result.projectVersion = jsonData["version"].getStr()
  if jsonData.hasKey("template"):
    result.buildFrontend = jsonData["template"].getStr()

  # Parse targets array
  if jsonData.hasKey("targets"):
    let targets = jsonData["targets"].getElems()
    if targets.len > 0:
      result.buildTarget = targets[0].getStr()

  # Parse output configuration if present
  if jsonData.hasKey("output"):
    let output = jsonData["output"]
    if output.hasKey("dir"):
      result.buildOutputDir = output["dir"].getStr()

  # Parse dev configuration if present
  if jsonData.hasKey("dev"):
    let dev = jsonData["dev"]
    if dev.hasKey("hotReload"):
      result.devHotReload = dev["hotReload"].getBool()
    if dev.hasKey("watchPaths"):
      let paths = dev["watchPaths"].getElems()
      result.devWatchPaths = @[]
      for path in paths:
        result.devWatchPaths.add(path.getStr())

proc findProjectConfig*(): string =
  ## Search for kryon.toml or kryon.json in current directory or ancestors
  var dir = getCurrentDir()
  while true:
    # Prefer TOML over JSON
    let tomlPath = dir / "kryon.toml"
    if fileExists(tomlPath):
      return tomlPath

    let jsonPath = dir / "kryon.json"
    if fileExists(jsonPath):
      return jsonPath

    # Move up to parent directory
    let parent = parentDir(dir)
    if parent == dir or parent.len == 0:
      break
    dir = parent

  return ""

proc loadProjectConfig*(): KryonConfig =
  ## Load project configuration from file or return defaults
  let configPath = findProjectConfig()

  if configPath.len == 0:
    return defaultConfig()

  if configPath.endsWith(".toml"):
    return loadTomlConfig(configPath)
  elif configPath.endsWith(".json"):
    return loadJsonConfig(configPath)
  else:
    return defaultConfig()

proc validateConfig*(config: KryonConfig): tuple[valid: bool, errors: seq[string]] =
  ## Validate configuration and return any errors
  result.valid = true
  result.errors = @[]

  # Validate target
  const validTargets = ["web", "desktop", "terminal", "framebuffer"]
  if config.buildTarget notin validTargets:
    result.valid = false
    result.errors.add("Invalid target: " & config.buildTarget & ". Must be one of: " & validTargets.join(", "))

  # Validate port range
  if config.devPort < 1 or config.devPort > 65535:
    result.valid = false
    result.errors.add("Invalid port: " & $config.devPort & ". Must be between 1 and 65535")

  # Validate window dimensions
  if config.desktopWindowWidth < 1 or config.desktopWindowHeight < 1:
    result.valid = false
    result.errors.add("Invalid window dimensions: " & $config.desktopWindowWidth & "x" & $config.desktopWindowHeight)

  # Validate backend
  const validBackends = ["sdl3", "terminal", "framebuffer"]
  if config.desktopBackend notin validBackends:
    result.valid = false
    result.errors.add("Invalid desktop backend: " & config.desktopBackend & ". Must be one of: " & validBackends.join(", "))

  # Validate output directory
  if config.buildOutputDir.len == 0:
    result.valid = false
    result.errors.add("Output directory cannot be empty")

proc configToJson*(config: KryonConfig): JsonNode =
  ## Convert configuration to JSON for display/export
  result = %*{
    "project": {
      "name": config.projectName,
      "version": config.projectVersion,
      "author": config.projectAuthor,
      "description": config.projectDescription
    },
    "build": {
      "target": config.buildTarget,
      "output_dir": config.buildOutputDir,
      "entry": config.buildEntry,
      "frontend": config.buildFrontend,
      "optimization": {
        "enabled": config.optimizationEnabled,
        "minify_css": config.optimizationMinifyCss,
        "minify_js": config.optimizationMinifyJs,
        "tree_shake": config.optimizationTreeShake
      }
    },
    "web": {
      "generate_separate_files": config.webGenerateSeparateFiles,
      "include_js_runtime": config.webIncludeJsRuntime,
      "include_wasm_modules": config.webIncludeWasmModules
    },
    "dev": {
      "hot_reload": config.devHotReload,
      "port": config.devPort,
      "watch_paths": %config.devWatchPaths,
      "open_browser": config.devOpenBrowser
    },
    "desktop": {
      "window_width": config.desktopWindowWidth,
      "window_height": config.desktopWindowHeight,
      "window_title": config.desktopWindowTitle,
      "resizable": config.desktopResizable,
      "vsync": config.desktopVsync,
      "target_fps": config.desktopTargetFps,
      "backend": config.desktopBackend
    },
    "metadata": {
      "created": config.metadataCreated,
      "kryon_version": config.metadataKryonVersion
    }
  }

proc writeTomlConfig*(config: KryonConfig, path: string) =
  ## Write configuration to TOML file
  var content = ""

  # Project section
  content.add("[project]\n")
  content.add("name = \"" & config.projectName & "\"\n")
  content.add("version = \"" & config.projectVersion & "\"\n")
  if config.projectAuthor.len > 0:
    content.add("author = \"" & config.projectAuthor & "\"\n")
  if config.projectDescription.len > 0:
    content.add("description = \"" & config.projectDescription & "\"\n")
  content.add("\n")

  # Build section
  content.add("[build]\n")
  content.add("target = \"" & config.buildTarget & "\"\n")
  content.add("output_dir = \"" & config.buildOutputDir & "\"\n")
  if config.buildEntry.len > 0:
    content.add("entry = \"" & config.buildEntry & "\"\n")
  if config.buildFrontend.len > 0:
    content.add("frontend = \"" & config.buildFrontend & "\"\n")
  content.add("\n")

  # Optimization subsection
  if config.optimizationEnabled:
    content.add("[build.optimization]\n")
    content.add("enabled = true\n")
    if config.optimizationMinifyCss:
      content.add("minify_css = true\n")
    if config.optimizationMinifyJs:
      content.add("minify_js = true\n")
    if config.optimizationTreeShake:
      content.add("tree_shake = true\n")
    content.add("\n")

  # Web section
  if config.buildTarget == "web":
    content.add("[web]\n")
    content.add("generate_separate_files = " & $config.webGenerateSeparateFiles & "\n")
    content.add("include_js_runtime = " & $config.webIncludeJsRuntime & "\n")
    content.add("include_wasm_modules = " & $config.webIncludeWasmModules & "\n")
    content.add("\n")

  # Dev section
  content.add("[dev]\n")
  content.add("hot_reload = " & $config.devHotReload & "\n")
  content.add("port = " & $config.devPort & "\n")
  if config.devWatchPaths.len > 0:
    content.add("watch_paths = [\"" & config.devWatchPaths.join("\", \"") & "\"]\n")
  if config.devOpenBrowser:
    content.add("open_browser = true\n")
  content.add("\n")

  # Desktop section
  if config.buildTarget == "desktop":
    content.add("[desktop]\n")
    content.add("window_width = " & $config.desktopWindowWidth & "\n")
    content.add("window_height = " & $config.desktopWindowHeight & "\n")
    content.add("window_title = \"" & config.desktopWindowTitle & "\"\n")
    content.add("resizable = " & $config.desktopResizable & "\n")
    content.add("vsync = " & $config.desktopVsync & "\n")
    content.add("target_fps = " & $config.desktopTargetFps & "\n")
    content.add("backend = \"" & config.desktopBackend & "\"\n")
    content.add("\n")

  # Metadata section
  if config.metadataCreated.len > 0 or config.metadataKryonVersion.len > 0:
    content.add("[metadata]\n")
    if config.metadataCreated.len > 0:
      content.add("created = \"" & config.metadataCreated & "\"\n")
    if config.metadataKryonVersion.len > 0:
      content.add("kryon_version = \"" & config.metadataKryonVersion & "\"\n")

  writeFile(path, content)
