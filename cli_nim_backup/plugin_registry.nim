## Plugin Registry System
##
## Manages the ~/.config/kryon/plugins.toml file that tracks installed plugins
## with their git URLs, local paths, and command ID allocations.

import std/[os, tables, strformat, strutils, sequtils, options, algorithm]
import parsetoml

type
  PluginEntry* = object
    name*: string
    version*: string
    gitUrl*: string              # Git URL (e.g., "github.com/user/plugin")
    localPath*: string           # Absolute path (e.g., "/home/user/Projects/plugin")
    commandIds*: seq[int]        # Allocated command IDs
    kryon_version_min*: string   # Minimum Kryon version required

  PluginRegistry* = object
    plugins*: Table[string, PluginEntry]
    configPath*: string

# =============================================================================
# Registry File Management
# =============================================================================

proc getRegistryPath*(): string =
  ## Get the path to plugins.toml
  let configDir = getConfigDir() / "kryon"
  result = configDir / "plugins.toml"

proc ensureConfigDir*(): string =
  ## Ensure ~/.config/kryon/ exists and return the path
  let configDir = getConfigDir() / "kryon"
  if not dirExists(configDir):
    createDir(configDir)
  result = configDir

proc loadRegistry*(): PluginRegistry =
  ## Load plugin registry from plugins.toml
  result.configPath = getRegistryPath()
  result.plugins = initTable[string, PluginEntry]()

  if not fileExists(result.configPath):
    return

  try:
    let toml = parseFile(result.configPath)

    # Parse [[plugin]] sections
    if toml.hasKey("plugin"):
      let pluginArray = toml["plugin"]
      if pluginArray.kind == TomlValueKind.Array:
        for pluginNode in pluginArray.arrayVal:
          var entry: PluginEntry

          # Required fields
          if pluginNode.hasKey("name"):
            entry.name = pluginNode["name"].stringVal
          else:
            continue

          # Optional fields
          if pluginNode.hasKey("version"):
            entry.version = pluginNode["version"].stringVal

          if pluginNode.hasKey("git_url"):
            entry.gitUrl = pluginNode["git_url"].stringVal

          if pluginNode.hasKey("local_path"):
            entry.localPath = pluginNode["local_path"].stringVal

          if pluginNode.hasKey("kryon_version_min"):
            entry.kryon_version_min = pluginNode["kryon_version_min"].stringVal

          # Parse command_ids array
          if pluginNode.hasKey("command_ids"):
            let idsNode = pluginNode["command_ids"]
            if idsNode.kind == TomlValueKind.Array:
              entry.commandIds = @[]
              for idNode in idsNode.arrayVal:
                entry.commandIds.add(int(idNode.intVal))

          result.plugins[entry.name] = entry

  except CatchableError as e:
    echo "[kryon][plugin] Error loading registry: ", e.msg

proc saveRegistry*(registry: PluginRegistry) =
  ## Save plugin registry to plugins.toml
  discard ensureConfigDir()

  var tomlContent = ""

  # Write each plugin as [[plugin]] section
  for name, entry in registry.plugins:
    tomlContent.add("[[plugin]]\n")
    tomlContent.add(&"name = \"{entry.name}\"\n")

    if entry.version.len > 0:
      tomlContent.add(&"version = \"{entry.version}\"\n")

    if entry.gitUrl.len > 0:
      tomlContent.add(&"git_url = \"{entry.gitUrl}\"\n")

    if entry.localPath.len > 0:
      tomlContent.add(&"local_path = \"{entry.localPath}\"\n")

    if entry.kryon_version_min.len > 0:
      tomlContent.add(&"kryon_version_min = \"{entry.kryon_version_min}\"\n")

    if entry.commandIds.len > 0:
      let idsStr = entry.commandIds.mapIt($it).join(", ")
      tomlContent.add(&"command_ids = [{idsStr}]\n")

    tomlContent.add("\n")

  writeFile(registry.configPath, tomlContent)

# =============================================================================
# Plugin Management
# =============================================================================

proc addPlugin*(registry: var PluginRegistry, entry: PluginEntry): bool =
  ## Add a plugin to the registry
  ## Returns false if plugin already exists
  if registry.plugins.hasKey(entry.name):
    echo &"[kryon][plugin] Plugin '{entry.name}' is already in registry"
    return false

  registry.plugins[entry.name] = entry
  registry.saveRegistry()
  echo &"[kryon][plugin] Added '{entry.name}' to registry"
  true

proc removePlugin*(registry: var PluginRegistry, name: string): bool =
  ## Remove a plugin from the registry
  ## Returns false if plugin not found
  if not registry.plugins.hasKey(name):
    echo &"[kryon][plugin] Plugin '{name}' not found in registry"
    return false

  registry.plugins.del(name)
  registry.saveRegistry()
  echo &"[kryon][plugin] Removed '{name}' from registry"
  true

proc getPlugin*(registry: PluginRegistry, name: string): Option[PluginEntry] =
  ## Get plugin entry by name
  if registry.plugins.hasKey(name):
    result = some(registry.plugins[name])
  else:
    result = none(PluginEntry)

proc updatePlugin*(registry: var PluginRegistry, name: string, entry: PluginEntry): bool =
  ## Update an existing plugin entry
  if not registry.plugins.hasKey(name):
    echo &"[kryon][plugin] Plugin '{name}' not found in registry"
    return false

  registry.plugins[name] = entry
  registry.saveRegistry()
  echo &"[kryon][plugin] Updated '{name}' in registry"
  true

proc listPlugins*(registry: PluginRegistry): seq[PluginEntry] =
  ## List all registered plugins
  result = @[]
  for name, entry in registry.plugins:
    result.add(entry)

# =============================================================================
# Command ID Allocation
# =============================================================================

const
  PLUGIN_CMD_START* = 100
  PLUGIN_CMD_END* = 255

proc checkCommandIdConflict*(registry: PluginRegistry, startId, endId: int,
                             excludeName: string = ""): Option[string] =
  ## Check if command ID range conflicts with existing plugins
  ## Returns the name of the conflicting plugin, if any
  ## excludeName: plugin name to exclude from conflict check (for updates)

  for name, entry in registry.plugins:
    if name == excludeName:
      continue

    # Check if ranges overlap
    if entry.commandIds.len > 0:
      let entryMin = entry.commandIds.min()
      let entryMax = entry.commandIds.max()

      if startId <= entryMax and endId >= entryMin:
        return some(name)

  return none(string)

proc allocateCommandIds*(registry: PluginRegistry, count: int): Option[seq[int]] =
  ## Automatically allocate command IDs for a plugin
  ## Returns a sequence of allocated IDs, or none if not enough space

  if count <= 0 or count > (PLUGIN_CMD_END - PLUGIN_CMD_START + 1):
    return none(seq[int])

  # Build set of used IDs
  var usedIds: set[0..255] = {}
  for name, entry in registry.plugins:
    for id in entry.commandIds:
      usedIds.incl(id)

  # Find contiguous block of free IDs
  var allocated: seq[int] = @[]
  for startId in PLUGIN_CMD_START..PLUGIN_CMD_END:
    allocated = @[]
    var canAllocate = true

    for offset in 0..<count:
      let id = startId + offset
      if id > PLUGIN_CMD_END or id in usedIds:
        canAllocate = false
        break
      allocated.add(id)

    if canAllocate and allocated.len == count:
      return some(allocated)

  return none(seq[int])

proc validateCommandIds*(ids: seq[int]): bool =
  ## Validate that command IDs are in valid range
  for id in ids:
    if id < PLUGIN_CMD_START or id > PLUGIN_CMD_END:
      return false
  true

# =============================================================================
# Utility Functions
# =============================================================================

proc expandPath*(path: string): string =
  ## Expand ~ and relative paths to absolute paths
  if path.startsWith("~"):
    result = getHomeDir() / path[1..^1]
  elif path.isAbsolute():
    result = path
  else:
    result = getCurrentDir() / path

  result = expandFilename(result)

proc isGitUrl*(url: string): bool =
  ## Check if string looks like a git URL
  url.contains("github.com") or url.contains("gitlab.com") or
  url.contains("bitbucket.org") or url.endsWith(".git") or
  url.startsWith("http://") or url.startsWith("https://") or
  url.startsWith("git@")

proc extractPluginName*(url: string): string =
  ## Extract plugin name from git URL
  ## Examples:
  ##   github.com/user/kryon-plugin-markdown -> kryon-plugin-markdown
  ##   https://github.com/user/plugin.git -> plugin
  ##   /path/to/kryon-plugin-foo -> kryon-plugin-foo

  var name = url

  # Remove .git suffix
  if name.endsWith(".git"):
    name = name[0..^5]

  # Extract last path component
  if '/' in name:
    name = name.split('/')[^1]

  result = name

# =============================================================================
# Display Functions
# =============================================================================

proc printPlugin*(entry: PluginEntry, verbose: bool = false) =
  ## Pretty-print plugin entry
  echo &"  • {entry.name}"

  if entry.version.len > 0:
    echo &"    Version: {entry.version}"

  if entry.localPath.len > 0:
    echo &"    Path: {entry.localPath}"

  if verbose:
    if entry.gitUrl.len > 0:
      echo &"    Git: {entry.gitUrl}"

    if entry.kryon_version_min.len > 0:
      echo &"    Requires Kryon: >={entry.kryon_version_min}"

    if entry.commandIds.len > 0:
      let idsStr = entry.commandIds.mapIt($it).join(", ")
      echo &"    Command IDs: {idsStr}"

proc printRegistry*(registry: PluginRegistry) =
  ## Print all plugins in registry
  let count = registry.plugins.len

  if count == 0:
    echo "No plugins installed."
    return

  echo &"Installed plugins ({count}):"
  for name, entry in registry.plugins:
    printPlugin(entry, verbose = false)
    echo ""
