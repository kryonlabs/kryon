## Kryon Configuration System
##
## Provides project configuration support via kryon.toml or kryon.json files.
## Supports hierarchical configuration with smart defaults.
## Uses simple built-in TOML parser (no external dependencies)

import json, os, strutils, tables, times

type
  PageEntry* = object
    ## Represents a page in a multi-page build
    file*: string      # Source file (e.g., "index.tsx", "docs.tsx")
    path*: string      # Output path (e.g., "/", "/docs")
    docsFile*: string  # Markdown file for auto-generated docs pages (e.g., "docs/architecture.md")

  DocsConfig* = object
    ## Configuration for auto-generated documentation pages
    enabled*: bool           # Enable docs auto-generation
    directory*: string       # Directory with .md files (default: "docs")
    templateFile*: string    # TSX template file (default: "docs.tsx")
    basePath*: string        # URL base path (default: "/docs")
    indexFile*: string       # File for index page (default: "getting-started.md")

  PluginSource* = enum
    ## How a plugin dependency is specified
    psPath,     # Local filesystem path
    psGit,      # Git repository URL
    psRegistry  # Version from registry

  PluginDependency* = object
    ## A plugin dependency declaration
    name*: string       # Plugin name (e.g., "markdown")
    source*: PluginSource
    path*: string       # For psPath: local path to plugin
    gitUrl*: string     # For psGit: git repository URL
    version*: string    # For psRegistry: version string

  KryonConfig* = object
    # Project metadata
    projectName*: string
    projectVersion*: string
    projectAuthor*: string
    projectDescription*: string

    # Build settings
    buildTarget*: string           # "web", "desktop", "terminal", "framebuffer"
    buildTargets*: seq[string]     # Multiple targets: ["web", "desktop"]
    buildOutputDir*: string
    buildEntry*: string            # Entry file (auto-detect if empty)
    buildFrontend*: string         # "js", "ts", "tsx", "nim", "lua", "c"
    buildPages*: seq[PageEntry]    # Multiple pages for multi-page builds

    # Optimization settings
    optimizationEnabled*: bool
    optimizationMinifyCss*: bool
    optimizationMinifyJs*: bool
    optimizationTreeShake*: bool

    # Web-specific settings
    webGenerateSeparateFiles*: bool  # Generate separate HTML/CSS/JS files
    webIncludeJsRuntime*: bool       # Include Kryon JS runtime
    webMinifyCss*: bool
    webMinifyJs*: bool

    # Development settings
    devHotReload*: bool
    devPort*: int
    devAutoOpen*: bool
    devWatchPaths*: seq[string]

    # Metadata
    metadataCreated*: string
    metadataModified*: string

    # Docs auto-generation
    docsConfig*: DocsConfig

    # Plugin dependencies
    plugins*: seq[PluginDependency]

proc defaultConfig*(): KryonConfig =
  ## Create a config with sensible defaults
  result.projectName = "kryon-app"
  result.projectVersion = "0.1.0"
  result.projectAuthor = ""
  result.projectDescription = ""

  result.buildTarget = "desktop"
  result.buildTargets = @[]
  result.buildOutputDir = "build"
  result.buildEntry = ""
  result.buildFrontend = "nim"
  result.buildPages = @[]

  result.optimizationEnabled = true
  result.optimizationMinifyCss = true
  result.optimizationMinifyJs = true
  result.optimizationTreeShake = true

  result.webGenerateSeparateFiles = false
  result.webIncludeJsRuntime = false
  result.webMinifyCss = true
  result.webMinifyJs = true

  result.devHotReload = true
  result.devPort = 3000
  result.devAutoOpen = false
  result.devWatchPaths = @[]

  result.metadataCreated = now().format("yyyy-MM-dd")
  result.metadataModified = now().format("yyyy-MM-dd")

  # Docs auto-generation defaults
  result.docsConfig.enabled = false
  result.docsConfig.directory = "docs"
  result.docsConfig.templateFile = "docs.tsx"
  result.docsConfig.basePath = "/docs"
  result.docsConfig.indexFile = "getting-started.md"

  # Plugin dependencies default
  result.plugins = @[]

proc parseSimpleArray(value: string): seq[string] =
  ## Parse simple TOML array like ["web", "desktop"]
  result = @[]
  var inner = value.strip()
  if inner.startsWith("[") and inner.endsWith("]"):
    inner = inner[1..^2]
    for item in inner.split(","):
      var cleaned = item.strip()
      if cleaned.startsWith("\"") and cleaned.endsWith("\""):
        cleaned = cleaned[1..^2]
      elif cleaned.startsWith("'") and cleaned.endsWith("'"):
        cleaned = cleaned[1..^2]
      if cleaned.len > 0:
        result.add(cleaned)

type
  TomlParseResult = object
    values: Table[string, string]
    pages: seq[PageEntry]
    plugins: seq[PluginDependency]

proc parseInlineTable(value: string): Table[string, string] =
  ## Parse TOML inline table like { path = "...", git = "..." }
  result = initTable[string, string]()
  var inner = value.strip()
  if inner.startsWith("{") and inner.endsWith("}"):
    inner = inner[1..^2].strip()
    if inner.len == 0:
      return
    for pair in inner.split(","):
      let parts = pair.split("=", maxsplit=1)
      if parts.len == 2:
        var key = parts[0].strip()
        var val = parts[1].strip()
        # Remove quotes from value
        if val.startsWith("\"") and val.endsWith("\""):
          val = val[1..^2]
        elif val.startsWith("'") and val.endsWith("'"):
          val = val[1..^2]
        result[key] = val

proc parsePluginDependency(name: string, value: string): PluginDependency =
  ## Parse a plugin dependency from TOML value
  result.name = name
  let trimmed = value.strip()

  # Check if it's an inline table { path = "..." } or { git = "..." }
  if trimmed.startsWith("{") and trimmed.endsWith("}"):
    let table = parseInlineTable(trimmed)
    if table.hasKey("path"):
      result.source = psPath
      result.path = table["path"]
    elif table.hasKey("git"):
      result.source = psGit
      result.gitUrl = table["git"]
    else:
      # Unknown inline table, treat as registry
      result.source = psRegistry
      result.version = "latest"
  else:
    # Simple string value - could be path or version
    var val = trimmed
    if val.startsWith("\"") and val.endsWith("\""):
      val = val[1..^2]
    # If it looks like a path, treat as path; otherwise treat as version
    if val.startsWith(".") or val.startsWith("/") or val.startsWith("~"):
      result.source = psPath
      result.path = val
    else:
      result.source = psRegistry
      result.version = val

proc parseSimpleToml(content: string): TomlParseResult =
  ## Simple TOML parser for basic key = "value" pairs
  ## Supports sections like [project], [build], etc.
  ## Supports array tables like [[build.pages]]
  ## Supports [plugins] section with inline tables
  ## Arrays are stored as raw strings and parsed later
  result.values = initTable[string, string]()
  result.pages = @[]
  result.plugins = @[]
  var currentSection = ""
  var inPagesArray = false
  var inPluginsSection = false
  var currentPage: PageEntry

  for rawLine in content.splitLines():
    let line = rawLine.strip()

    # Skip empty lines and comments
    if line.len == 0 or line.startsWith("#"):
      continue

    # Array table headers [[name]]
    if line.startsWith("[[") and line.endsWith("]]"):
      # If we were building a page entry, save it
      if inPagesArray and (currentPage.file.len > 0 or currentPage.path.len > 0):
        result.pages.add(currentPage)
        currentPage = PageEntry()

      let section = line[2..^3]  # Remove [[ and ]]
      if section == "build.pages":
        inPagesArray = true
        inPluginsSection = false
        currentSection = section
      else:
        inPagesArray = false
        inPluginsSection = false
        currentSection = section
      continue

    # Section headers [name]
    if line.startsWith("[") and line.endsWith("]"):
      # If we were building a page entry, save it
      if inPagesArray and (currentPage.file.len > 0 or currentPage.path.len > 0):
        result.pages.add(currentPage)
        currentPage = PageEntry()

      currentSection = line[1..^2]
      inPagesArray = false
      inPluginsSection = (currentSection == "plugins")
      continue

    # Key = value pairs
    if "=" in line:
      let parts = line.split("=", maxsplit=1)
      if parts.len == 2:
        var key = parts[0].strip()
        var value = parts[1].strip()

        # Handle plugins section specially - parse inline tables
        if inPluginsSection:
          let plugin = parsePluginDependency(key, value)
          result.plugins.add(plugin)
          continue

        # Keep arrays and inline tables as-is (don't remove brackets/braces)
        if not (value.startsWith("[") and value.endsWith("]")) and
           not (value.startsWith("{") and value.endsWith("}")):
          # Remove quotes from scalar value
          if value.startsWith("\"") and value.endsWith("\""):
            value = value[1..^2]
          elif value.startsWith("'") and value.endsWith("'"):
            value = value[1..^2]

        # Handle page entries specially
        if inPagesArray:
          case key:
            of "file": currentPage.file = value
            of "path": currentPage.path = value
            else: discard
        else:
          # Prefix key with section
          if currentSection.len > 0:
            key = currentSection & "." & key
          result.values[key] = value

  # Don't forget the last page entry
  if inPagesArray and (currentPage.file.len > 0 or currentPage.path.len > 0):
    result.pages.add(currentPage)

proc tomlToConfig(tomlResult: TomlParseResult): KryonConfig =
  ## Convert parsed TOML to KryonConfig
  result = defaultConfig()
  let toml = tomlResult.values
  result.buildPages = tomlResult.pages
  result.plugins = tomlResult.plugins

  # Project metadata
  if toml.hasKey("project.name"):
    result.projectName = toml["project.name"]
  if toml.hasKey("project.version"):
    result.projectVersion = toml["project.version"]
  if toml.hasKey("project.author"):
    result.projectAuthor = toml["project.author"]
  if toml.hasKey("project.description"):
    result.projectDescription = toml["project.description"]

  # Build settings
  if toml.hasKey("build.target"):
    result.buildTarget = toml["build.target"]
  if toml.hasKey("build.targets"):
    result.buildTargets = parseSimpleArray(toml["build.targets"])
  if toml.hasKey("build.output_dir"):
    result.buildOutputDir = toml["build.output_dir"]
  if toml.hasKey("build.entry_point"):
    result.buildEntry = toml["build.entry_point"]
  elif toml.hasKey("build.entry"):
    result.buildEntry = toml["build.entry"]
  if toml.hasKey("build.frontend"):
    result.buildFrontend = toml["build.frontend"]

  # Optimization settings
  if toml.hasKey("optimization.enabled"):
    result.optimizationEnabled = toml["optimization.enabled"] == "true"
  if toml.hasKey("optimization.minify_css"):
    result.optimizationMinifyCss = toml["optimization.minify_css"] == "true"
  if toml.hasKey("optimization.minify_js"):
    result.optimizationMinifyJs = toml["optimization.minify_js"] == "true"
  if toml.hasKey("optimization.tree_shake"):
    result.optimizationTreeShake = toml["optimization.tree_shake"] == "true"

  # Web settings
  if toml.hasKey("web.generate_separate_files"):
    result.webGenerateSeparateFiles = toml["web.generate_separate_files"] == "true"
  if toml.hasKey("web.include_js_runtime"):
    result.webIncludeJsRuntime = toml["web.include_js_runtime"] == "true"
  if toml.hasKey("web.minify_css"):
    result.webMinifyCss = toml["web.minify_css"] == "true"
  if toml.hasKey("web.minify_js"):
    result.webMinifyJs = toml["web.minify_js"] == "true"

  # Dev settings
  if toml.hasKey("dev.hot_reload"):
    result.devHotReload = toml["dev.hot_reload"] == "true"
  if toml.hasKey("dev.port"):
    result.devPort = parseInt(toml["dev.port"])
  if toml.hasKey("dev.auto_open"):
    result.devAutoOpen = toml["dev.auto_open"] == "true"

  # Metadata
  if toml.hasKey("metadata.created"):
    result.metadataCreated = toml["metadata.created"]
  if toml.hasKey("metadata.modified"):
    result.metadataModified = toml["metadata.modified"]

  # Docs auto-generation settings
  if toml.hasKey("build.docs.enabled"):
    result.docsConfig.enabled = toml["build.docs.enabled"] == "true"
  if toml.hasKey("build.docs.directory"):
    result.docsConfig.directory = toml["build.docs.directory"]
  if toml.hasKey("build.docs.template"):
    result.docsConfig.templateFile = toml["build.docs.template"]
  if toml.hasKey("build.docs.base_path"):
    result.docsConfig.basePath = toml["build.docs.base_path"]
  if toml.hasKey("build.docs.index_file"):
    result.docsConfig.indexFile = toml["build.docs.index_file"]

proc jsonToConfig(jsonNode: JsonNode): KryonConfig =
  ## Convert JSON to KryonConfig
  result = defaultConfig()

  # Project metadata
  if jsonNode.hasKey("project"):
    let proj = jsonNode["project"]
    if proj.hasKey("name"): result.projectName = proj["name"].getStr()
    if proj.hasKey("version"): result.projectVersion = proj["version"].getStr()
    if proj.hasKey("author"): result.projectAuthor = proj["author"].getStr()
    if proj.hasKey("description"): result.projectDescription = proj["description"].getStr()

  # Build settings
  if jsonNode.hasKey("build"):
    let build = jsonNode["build"]
    if build.hasKey("target"): result.buildTarget = build["target"].getStr()
    if build.hasKey("output_dir"): result.buildOutputDir = build["output_dir"].getStr()
    if build.hasKey("entry"): result.buildEntry = build["entry"].getStr()
    if build.hasKey("frontend"): result.buildFrontend = build["frontend"].getStr()
    if build.hasKey("pages"):
      for pageNode in build["pages"]:
        var page = PageEntry()
        if pageNode.hasKey("file"): page.file = pageNode["file"].getStr()
        if pageNode.hasKey("path"): page.path = pageNode["path"].getStr()
        result.buildPages.add(page)

  # Optimization settings
  if jsonNode.hasKey("optimization"):
    let opt = jsonNode["optimization"]
    if opt.hasKey("enabled"): result.optimizationEnabled = opt["enabled"].getBool()
    if opt.hasKey("minify_css"): result.optimizationMinifyCss = opt["minify_css"].getBool()
    if opt.hasKey("minify_js"): result.optimizationMinifyJs = opt["minify_js"].getBool()
    if opt.hasKey("tree_shake"): result.optimizationTreeShake = opt["tree_shake"].getBool()

  # Web settings
  if jsonNode.hasKey("web"):
    let web = jsonNode["web"]
    if web.hasKey("generate_separate_files"): result.webGenerateSeparateFiles = web["generate_separate_files"].getBool()
    if web.hasKey("include_js_runtime"): result.webIncludeJsRuntime = web["include_js_runtime"].getBool()
    if web.hasKey("minify_css"): result.webMinifyCss = web["minify_css"].getBool()
    if web.hasKey("minify_js"): result.webMinifyJs = web["minify_js"].getBool()

  # Dev settings
  if jsonNode.hasKey("dev"):
    let dev = jsonNode["dev"]
    if dev.hasKey("hot_reload"): result.devHotReload = dev["hot_reload"].getBool()
    if dev.hasKey("port"): result.devPort = dev["port"].getInt()
    if dev.hasKey("auto_open"): result.devAutoOpen = dev["auto_open"].getBool()

  # Metadata
  if jsonNode.hasKey("metadata"):
    let meta = jsonNode["metadata"]
    if meta.hasKey("created"): result.metadataCreated = meta["created"].getStr()
    if meta.hasKey("modified"): result.metadataModified = meta["modified"].getStr()

  # Docs auto-generation settings (nested under build.docs in JSON)
  if jsonNode.hasKey("build") and jsonNode["build"].hasKey("docs"):
    let docs = jsonNode["build"]["docs"]
    if docs.hasKey("enabled"): result.docsConfig.enabled = docs["enabled"].getBool()
    if docs.hasKey("directory"): result.docsConfig.directory = docs["directory"].getStr()
    if docs.hasKey("template"): result.docsConfig.templateFile = docs["template"].getStr()
    if docs.hasKey("base_path"): result.docsConfig.basePath = docs["base_path"].getStr()
    if docs.hasKey("index_file"): result.docsConfig.indexFile = docs["index_file"].getStr()

proc configToJson*(config: KryonConfig): JsonNode =
  ## Convert KryonConfig to JSON
  result = %* {
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
      "frontend": config.buildFrontend
    },
    "optimization": {
      "enabled": config.optimizationEnabled,
      "minify_css": config.optimizationMinifyCss,
      "minify_js": config.optimizationMinifyJs,
      "tree_shake": config.optimizationTreeShake
    },
    "dev": {
      "hot_reload": config.devHotReload,
      "port": config.devPort,
      "auto_open": config.devAutoOpen
    },
    "metadata": {
      "created": config.metadataCreated,
      "modified": config.metadataModified
    }
  }

proc configToToml*(config: KryonConfig): string =
  ## Convert KryonConfig to TOML format
  result = "# Kryon Project Configuration\n\n"

  result &= "[project]\n"
  result &= "name = \"" & config.projectName & "\"\n"
  result &= "version = \"" & config.projectVersion & "\"\n"
  result &= "author = \"" & config.projectAuthor & "\"\n"
  result &= "description = \"" & config.projectDescription & "\"\n\n"

  result &= "[build]\n"
  result &= "target = \"" & config.buildTarget & "\"\n"
  result &= "output_dir = \"" & config.buildOutputDir & "\"\n"
  result &= "entry = \"" & config.buildEntry & "\"\n"
  result &= "frontend = \"" & config.buildFrontend & "\"\n\n"

  result &= "[optimization]\n"
  result &= "enabled = " & $config.optimizationEnabled & "\n"
  result &= "minify_css = " & $config.optimizationMinifyCss & "\n"
  result &= "minify_js = " & $config.optimizationMinifyJs & "\n"
  result &= "tree_shake = " & $config.optimizationTreeShake & "\n\n"

  result &= "[dev]\n"
  result &= "hot_reload = " & $config.devHotReload & "\n"
  result &= "port = " & $config.devPort & "\n"
  result &= "auto_open = " & $config.devAutoOpen & "\n\n"

  result &= "[metadata]\n"
  result &= "created = \"" & config.metadataCreated & "\"\n"
  result &= "modified = \"" & config.metadataModified & "\"\n"

proc findProjectConfig*(): string =
  ## Find project config file in current directory or parents
  ## Searches for kryon.toml or kryon.json
  var currentDir = getCurrentDir()

  while true:
    # Check for TOML config
    let tomlPath = currentDir / "kryon.toml"
    if fileExists(tomlPath):
      return tomlPath

    # Check for JSON config
    let jsonPath = currentDir / "kryon.json"
    if fileExists(jsonPath):
      return jsonPath

    # Move up one directory
    let parent = parentDir(currentDir)
    if parent == currentDir or parent.len == 0:
      break
    currentDir = parent

  return ""

proc loadProjectConfig*(): KryonConfig =
  ## Load project config from file
  let configPath = findProjectConfig()

  if configPath.len == 0:
    raise newException(IOError, "No configuration file found (kryon.toml or kryon.json)")

  let content = readFile(configPath)

  if configPath.endsWith(".toml"):
    let toml = parseSimpleToml(content)
    result = tomlToConfig(toml)
  elif configPath.endsWith(".json"):
    let jsonNode = parseJson(content)
    result = jsonToConfig(jsonNode)
  else:
    raise newException(ValueError, "Unknown config format: " & configPath)

proc loadConfig*(): KryonConfig =
  ## Load project config if available, otherwise return defaults
  ## This is the preferred function for build scripts that want graceful fallback
  let configPath = findProjectConfig()

  if configPath.len == 0:
    return defaultConfig()

  try:
    let content = readFile(configPath)
    if configPath.endsWith(".toml"):
      let toml = parseSimpleToml(content)
      result = tomlToConfig(toml)
    elif configPath.endsWith(".json"):
      let jsonNode = parseJson(content)
      result = jsonToConfig(jsonNode)
    else:
      return defaultConfig()
  except:
    return defaultConfig()

proc writeTomlConfig*(config: KryonConfig, path: string) =
  ## Write config to TOML file
  let toml = configToToml(config)
  writeFile(path, toml)

proc writeJsonConfig*(config: KryonConfig, path: string) =
  ## Write config to JSON file
  let jsonNode = configToJson(config)
  writeFile(path, jsonNode.pretty())

proc validateConfig*(config: KryonConfig): tuple[valid: bool, errors: seq[string]] =
  ## Validate configuration values
  result.valid = true
  result.errors = @[]

  # Validate project name
  if config.projectName.len == 0:
    result.valid = false
    result.errors.add("Project name cannot be empty")

  # Validate version format (basic check)
  if config.projectVersion.len == 0:
    result.valid = false
    result.errors.add("Project version cannot be empty")

  # Validate build target
  const validTargets = ["web", "desktop", "terminal", "framebuffer"]
  if config.buildTarget notin validTargets:
    result.valid = false
    result.errors.add("Invalid build target: " & config.buildTarget &
                     " (must be one of: " & validTargets.join(", ") & ")")

  # Validate frontend
  const validFrontends = ["nim", "lua", "c", "js", "ts", "tsx", "jsx", "kry"]
  if config.buildFrontend notin validFrontends:
    result.valid = false
    result.errors.add("Invalid frontend: " & config.buildFrontend &
                     " (must be one of: " & validFrontends.join(", ") & ")")

  # Validate output directory
  if config.buildOutputDir.len == 0:
    result.valid = false
    result.errors.add("Build output directory cannot be empty")

  # Validate dev port
  if config.devPort < 1024 or config.devPort > 65535:
    result.valid = false
    result.errors.add("Dev port must be between 1024 and 65535")
