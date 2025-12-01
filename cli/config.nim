## Kryon Configuration System
##
## Provides project configuration support via kryon.toml or kryon.json files.
## Supports hierarchical configuration with smart defaults.
## Uses simple built-in TOML parser (no external dependencies)

import json, os, strutils, tables, times

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

    # Development settings
    devHotReload*: bool
    devPort*: int
    devAutoOpen*: bool

    # Metadata
    metadataCreated*: string
    metadataModified*: string

proc defaultConfig*(): KryonConfig =
  ## Create a config with sensible defaults
  result.projectName = "kryon-app"
  result.projectVersion = "0.1.0"
  result.projectAuthor = ""
  result.projectDescription = ""

  result.buildTarget = "desktop"
  result.buildOutputDir = "build"
  result.buildEntry = ""
  result.buildFrontend = "nim"

  result.optimizationEnabled = true
  result.optimizationMinifyCss = true
  result.optimizationMinifyJs = true
  result.optimizationTreeShake = true

  result.devHotReload = true
  result.devPort = 3000
  result.devAutoOpen = false

  result.metadataCreated = now().format("yyyy-MM-dd")
  result.metadataModified = now().format("yyyy-MM-dd")

proc parseSimpleToml(content: string): Table[string, string] =
  ## Simple TOML parser for basic key = "value" pairs
  ## Supports sections like [project], [build], etc.
  result = initTable[string, string]()
  var currentSection = ""

  for rawLine in content.splitLines():
    let line = rawLine.strip()

    # Skip empty lines and comments
    if line.len == 0 or line.startsWith("#"):
      continue

    # Section headers [name]
    if line.startsWith("[") and line.endsWith("]"):
      currentSection = line[1..^2]
      continue

    # Key = value pairs
    if "=" in line:
      let parts = line.split("=", maxsplit=1)
      if parts.len == 2:
        var key = parts[0].strip()
        var value = parts[1].strip()

        # Remove quotes from value
        if value.startsWith("\"") and value.endsWith("\""):
          value = value[1..^2]
        elif value.startsWith("'") and value.endsWith("'"):
          value = value[1..^2]

        # Prefix key with section
        if currentSection.len > 0:
          key = currentSection & "." & key

        result[key] = value

proc tomlToConfig(toml: Table[string, string]): KryonConfig =
  ## Convert parsed TOML table to KryonConfig
  result = defaultConfig()

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
  if toml.hasKey("build.output_dir"):
    result.buildOutputDir = toml["build.output_dir"]
  if toml.hasKey("build.entry"):
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

  # Optimization settings
  if jsonNode.hasKey("optimization"):
    let opt = jsonNode["optimization"]
    if opt.hasKey("enabled"): result.optimizationEnabled = opt["enabled"].getBool()
    if opt.hasKey("minify_css"): result.optimizationMinifyCss = opt["minify_css"].getBool()
    if opt.hasKey("minify_js"): result.optimizationMinifyJs = opt["minify_js"].getBool()
    if opt.hasKey("tree_shake"): result.optimizationTreeShake = opt["tree_shake"].getBool()

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
  const validFrontends = ["nim", "lua", "c", "js", "ts"]
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
