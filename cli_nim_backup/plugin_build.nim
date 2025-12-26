## Plugin Build System
##
## Handles building plugins and parsing plugin.toml metadata

import std/[os, osproc, strformat, strutils, sequtils, algorithm]
import parsetoml
import binding_generator

type
  PluginMetadata* = object
    name*: string
    version*: string
    description*: string
    license*: string
    repository*: string
    kryon_version_min*: string
    commandIds*: seq[int]
    backends*: seq[string]
    features*: seq[string]

# =============================================================================
# Plugin.toml Parsing
# =============================================================================

proc parsePluginToml*(path: string): PluginMetadata =
  ## Parse plugin.toml file
  result = PluginMetadata()

  if not fileExists(path):
    raise newException(IOError, &"Plugin manifest not found: {path}")

  try:
    let toml = parseFile(path)

    # Parse [plugin] section
    if toml.hasKey("plugin"):
      let pluginSection = toml["plugin"]

      if pluginSection.hasKey("name"):
        result.name = pluginSection["name"].stringVal

      if pluginSection.hasKey("version"):
        result.version = pluginSection["version"].stringVal

      if pluginSection.hasKey("description"):
        result.description = pluginSection["description"].stringVal

      if pluginSection.hasKey("license"):
        result.license = pluginSection["license"].stringVal

      if pluginSection.hasKey("repository"):
        result.repository = pluginSection["repository"].stringVal

    # Parse [compatibility] section
    if toml.hasKey("compatibility"):
      let compatSection = toml["compatibility"]

      if compatSection.hasKey("kryon_version"):
        let versionStr = compatSection["kryon_version"].stringVal
        # Parse >=0.3.0 format
        if versionStr.startsWith(">="):
          result.kryon_version_min = versionStr[2..^1]
        else:
          result.kryon_version_min = versionStr

    # Parse [capabilities] section
    if toml.hasKey("capabilities"):
      let capsSection = toml["capabilities"]

      if capsSection.hasKey("command_ids"):
        let idsNode = capsSection["command_ids"]
        if idsNode.kind == TomlValueKind.Array:
          result.commandIds = @[]
          for idNode in idsNode.arrayVal:
            result.commandIds.add(int(idNode.intVal))

      if capsSection.hasKey("backends"):
        let backendsNode = capsSection["backends"]
        if backendsNode.kind == TomlValueKind.Array:
          result.backends = @[]
          for backendNode in backendsNode.arrayVal:
            result.backends.add(backendNode.stringVal)

      if capsSection.hasKey("features"):
        let featuresNode = capsSection["features"]
        if featuresNode.kind == TomlValueKind.Array:
          result.features = @[]
          for featureNode in featuresNode.arrayVal:
            result.features.add(featureNode.stringVal)

  except CatchableError as e:
    raise newException(IOError, &"Failed to parse {path}: {e.msg}")

proc validatePluginMetadata*(metadata: PluginMetadata): tuple[valid: bool, errors: seq[string]] =
  ## Validate plugin metadata
  var errors: seq[string] = @[]

  if metadata.name.len == 0:
    errors.add("Plugin name is required")

  if metadata.version.len == 0:
    errors.add("Plugin version is required")

  if metadata.commandIds.len == 0:
    errors.add("Plugin must define at least one command ID")

  # Validate command ID range
  for id in metadata.commandIds:
    if id < 100 or id > 255:
      errors.add(&"Invalid command ID {id} (must be in range 100-255)")

  return (errors.len == 0, errors)

# =============================================================================
# Plugin Building
# =============================================================================

proc buildPlugin*(pluginDir: string): bool =
  ## Build a plugin using its Makefile
  ##
  ## Returns true on success, false on failure

  if not dirExists(pluginDir):
    echo &"✗ Plugin directory not found: {pluginDir}"
    return false

  # Check for Makefile
  let makefilePath = pluginDir / "Makefile"
  if not fileExists(makefilePath):
    echo &"✗ No Makefile found in {pluginDir}"
    echo "  Plugins must provide a Makefile for building"
    return false

  # Check for plugin.toml
  let pluginTomlPath = pluginDir / "plugin.toml"
  if not fileExists(pluginTomlPath):
    echo &"✗ No plugin.toml found in {pluginDir}"
    return false

  # Set KRYON_ROOT environment variable
  # The plugin's Makefile should use this to find Kryon headers/libs
  var kryonRoot = getEnv("KRYON_ROOT")
  if kryonRoot.len == 0:
    # Try to find Kryon root
    kryonRoot = getCurrentDir()  # Assume we're running from Kryon root
    # TODO: Better detection logic

  let envCmd = &"KRYON_ROOT=\"{kryonRoot}\" "

  # Run make clean
  echo "  Cleaning previous build..."
  let (cleanOutput, cleanExitCode) = execCmdEx(&"{envCmd}make -C \"{pluginDir}\" clean")
  if cleanExitCode != 0:
    echo "  ⚠ Clean failed (continuing anyway)"

  # Run make
  echo "  Compiling plugin..."
  let (buildOutput, buildExitCode) = execCmdEx(&"{envCmd}make -C \"{pluginDir}\"")

  if buildExitCode != 0:
    echo &"✗ Build failed:"
    echo buildOutput
    return false

  # Check for build artifacts
  let buildDir = pluginDir / "build"
  if not dirExists(buildDir):
    echo &"✗ Build directory not created: {buildDir}"
    echo "  Build may have failed silently"
    return false

  return true

proc generateBindings*(pluginDir: string): bool =
  ## Generate language bindings for a plugin
  ##
  ## Returns true on success, false on failure

  if not dirExists(pluginDir):
    echo &"✗ Plugin directory not found: {pluginDir}"
    return false

  # Check for bindings specification
  let bindingsJsonPath = pluginDir / "bindings" / "bindings.json"
  if not fileExists(bindingsJsonPath):
    echo "  ⚠ No bindings.json found - skipping binding generation"
    return true  # Not an error

  # Call binding generator
  return generateNimBindingsForPlugin(pluginDir)
