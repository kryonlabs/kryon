## Plugin Manager - High-level plugin management commands
##
## Handles: kryon plugin add/remove/list/update/info

import std/[os, strformat, strutils, tables, algorithm, options]
import plugin_registry, plugin_git, plugin_build, config

# =============================================================================
# Plugin Add Command
# =============================================================================

proc handlePluginAddCommand*(args: seq[string]) =
  ## Handle 'kryon plugin add <url|path>' command
  if args.len == 0:
    echo "Error: Git URL or path required"
    echo "Usage: kryon plugin add <url|path>"
    echo ""
    echo "Examples:"
    echo "  kryon plugin add github.com/user/kryon-plugin-markdown"
    echo "  kryon plugin add https://gitlab.com/user/plugin.git"
    echo "  kryon plugin add ../kryon-plugin-markdown"
    echo "  kryon plugin add /absolute/path/to/plugin"
    quit(1)

  let urlOrPath = args[0]
  var registry = loadRegistry()

  # Determine if it's a git URL or local path
  if isGitUrl(urlOrPath):
    # Git URL - clone to ~/Projects/
    let pluginName = extractPluginName(urlOrPath)
    let targetDir = getHomeDir() / "Projects" / pluginName

    echo &"Cloning plugin '{pluginName}' from {urlOrPath}..."

    # Clone the plugin
    if not gitClone(urlOrPath, targetDir):
      echo "✗ Failed to clone plugin"
      quit(1)

    echo &"✓ Cloned to {targetDir}"

    # Read plugin.toml from cloned directory
    let pluginTomlPath = targetDir / "plugin.toml"
    if not fileExists(pluginTomlPath):
      echo &"✗ No plugin.toml found in {targetDir}"
      echo "  This doesn't appear to be a valid Kryon plugin"
      quit(1)

    # Parse plugin metadata
    let metadata = parsePluginToml(pluginTomlPath)

    # Validate metadata
    if metadata.name != pluginName:
      echo &"⚠ Warning: plugin.toml name '{metadata.name}' doesn't match directory '{pluginName}'"

    # Check for command ID conflicts
    if metadata.commandIds.len > 0:
      let conflict = registry.checkCommandIdConflict(
        metadata.commandIds.min(), metadata.commandIds.max())
      if conflict.isSome():
        echo &"✗ Command ID conflict with plugin '{conflict.get()}'"
        echo &"  Plugin '{metadata.name}' uses IDs: {metadata.commandIds}"
        quit(1)

    # Build the plugin
    echo &"Building plugin '{metadata.name}'..."
    if not buildPlugin(targetDir):
      echo "✗ Failed to build plugin"
      quit(1)

    echo "✓ Plugin built successfully"

    # Generate bindings
    echo "  Generating bindings..."
    if not generateBindings(targetDir):
      echo "⚠ Binding generation failed (continuing anyway)"

    # Add to registry
    var entry = PluginEntry(
      name: metadata.name,
      version: metadata.version,
      gitUrl: urlOrPath,
      localPath: targetDir,
      commandIds: metadata.commandIds,
      kryon_version_min: metadata.kryon_version_min
    )

    if not registry.addPlugin(entry):
      quit(1)

    echo ""
    echo &"✓ Plugin '{metadata.name}' installed successfully"
    echo &"  Path: {targetDir}"
    echo &"  Version: {metadata.version}"

  else:
    # Local path
    let absPath = expandPath(urlOrPath)

    if not dirExists(absPath):
      echo &"✗ Directory not found: {absPath}"
      quit(1)

    # Read plugin.toml
    let pluginTomlPath = absPath / "plugin.toml"
    if not fileExists(pluginTomlPath):
      echo &"✗ No plugin.toml found in {absPath}"
      echo "  This doesn't appear to be a valid Kryon plugin"
      quit(1)

    # Parse plugin metadata
    let metadata = parsePluginToml(pluginTomlPath)

    # Check for command ID conflicts
    if metadata.commandIds.len > 0:
      let conflict = registry.checkCommandIdConflict(
        metadata.commandIds.min(), metadata.commandIds.max())
      if conflict.isSome():
        echo &"✗ Command ID conflict with plugin '{conflict.get()}'"
        echo &"  Plugin '{metadata.name}' uses IDs: {metadata.commandIds}"
        quit(1)

    # Build the plugin
    echo &"Building plugin '{metadata.name}'..."
    if not buildPlugin(absPath):
      echo "✗ Failed to build plugin"
      quit(1)

    echo "✓ Plugin built successfully"

    # Generate bindings
    echo "  Generating bindings..."
    if not generateBindings(absPath):
      echo "⚠ Binding generation failed (continuing anyway)"

    # Add to registry
    var entry = PluginEntry(
      name: metadata.name,
      version: metadata.version,
      gitUrl: "",  # No git URL for local paths
      localPath: absPath,
      commandIds: metadata.commandIds,
      kryon_version_min: metadata.kryon_version_min
    )

    if not registry.addPlugin(entry):
      quit(1)

    echo ""
    echo &"✓ Plugin '{metadata.name}' registered successfully"
    echo &"  Path: {absPath}"
    echo &"  Version: {metadata.version}"

# =============================================================================
# Plugin Remove Command
# =============================================================================

proc handlePluginRemoveCommand*(args: seq[string]) =
  ## Handle 'kryon plugin remove <name>' command
  if args.len == 0:
    echo "Error: Plugin name required"
    echo "Usage: kryon plugin remove <name>"
    quit(1)

  let pluginName = args[0]
  var registry = loadRegistry()

  if not registry.removePlugin(pluginName):
    quit(1)

  echo ""
  echo &"✓ Plugin '{pluginName}' unregistered"
  echo "  Note: Plugin source files were not deleted"
  echo "  To delete manually, remove the plugin directory"

# =============================================================================
# Plugin List Command
# =============================================================================

proc handlePluginListCommand*(args: seq[string]) =
  ## Handle 'kryon plugin list' command
  let registry = loadRegistry()

  printRegistry(registry)

# =============================================================================
# Plugin Update Command
# =============================================================================

proc handlePluginUpdateCommand*(args: seq[string]) =
  ## Handle 'kryon plugin update <name>' command
  if args.len == 0:
    echo "Error: Plugin name required"
    echo "Usage: kryon plugin update <name>"
    quit(1)

  let pluginName = args[0]
  var registry = loadRegistry()

  let entryOpt = registry.getPlugin(pluginName)
  if entryOpt.isNone():
    echo &"✗ Plugin '{pluginName}' not found in registry"
    quit(1)

  let entry = entryOpt.get()

  # Update from git if it has a git URL
  if entry.gitUrl.len > 0:
    echo &"Updating plugin '{pluginName}' from {entry.gitUrl}..."
    if not gitPull(entry.localPath):
      echo "✗ Failed to update plugin"
      quit(1)

    echo "✓ Git pull successful"

  # Rebuild the plugin
  echo &"Rebuilding plugin '{pluginName}'..."
  if not buildPlugin(entry.localPath):
    echo "✗ Failed to rebuild plugin"
    quit(1)

  echo "✓ Plugin rebuilt successfully"

  # Re-read plugin.toml in case version changed
  let pluginTomlPath = entry.localPath / "plugin.toml"
  if fileExists(pluginTomlPath):
    let metadata = parsePluginToml(pluginTomlPath)
    var updatedEntry = entry
    updatedEntry.version = metadata.version

    if not registry.updatePlugin(pluginName, updatedEntry):
      quit(1)

  echo ""
  echo &"✓ Plugin '{pluginName}' updated successfully"

# =============================================================================
# Plugin Info Command
# =============================================================================

proc handlePluginInfoCommand*(args: seq[string]) =
  ## Handle 'kryon plugin info <name>' command
  if args.len == 0:
    echo "Error: Plugin name required"
    echo "Usage: kryon plugin info <name>"
    quit(1)

  let pluginName = args[0]
  let registry = loadRegistry()

  let entryOpt = registry.getPlugin(pluginName)
  if entryOpt.isNone():
    echo &"✗ Plugin '{pluginName}' not found in registry"
    quit(1)

  let entry = entryOpt.get()

  echo &"Plugin: {entry.name}"
  printPlugin(entry, verbose = true)

# =============================================================================
# Main Plugin Command Dispatcher
# =============================================================================

proc handlePluginCommand*(args: seq[string]) =
  ## Handle 'kryon plugin <subcommand>' command
  if args.len == 0:
    echo "Kryon Plugin Manager"
    echo ""
    echo "Usage: kryon plugin <subcommand> [args]"
    echo ""
    echo "Subcommands:"
    echo "  add <url|path>     Install a plugin from git or local path"
    echo "  remove <name>      Unregister a plugin (keeps source files)"
    echo "  list               List all installed plugins"
    echo "  update <name>      Update plugin from git and rebuild"
    echo "  info <name>        Show detailed plugin information"
    echo ""
    echo "Examples:"
    echo "  kryon plugin add github.com/user/kryon-plugin-markdown"
    echo "  kryon plugin add ../kryon-plugin-markdown"
    echo "  kryon plugin list"
    echo "  kryon plugin update markdown"
    echo "  kryon plugin info markdown"
    echo "  kryon plugin remove markdown"
    quit(0)

  let subcommand = args[0].toLowerAscii()
  let subcommandArgs = if args.len > 1: args[1..^1] else: @[]

  case subcommand
  of "add":
    handlePluginAddCommand(subcommandArgs)
  of "remove", "rm":
    handlePluginRemoveCommand(subcommandArgs)
  of "list", "ls":
    handlePluginListCommand(subcommandArgs)
  of "update", "up":
    handlePluginUpdateCommand(subcommandArgs)
  of "info":
    handlePluginInfoCommand(subcommandArgs)
  else:
    echo &"Unknown plugin subcommand: {subcommand}"
    echo "Use 'kryon plugin' for help"
    quit(1)

# =============================================================================
# Runtime Plugin Loading (for Lua/frontend execution)
# =============================================================================

import ../bindings/nim/ir_core

proc loadPluginsForRuntime*(config: KryonConfig): seq[ptr IRPluginHandle] =
  ## Load all plugins from config before Lua execution
  result = @[]

  if config.plugins.len == 0:
    return

  echo &"📦 Loading {config.plugins.len} plugin(s)..."

  for plugin in config.plugins:
    if plugin.source != psPath:
      echo &"  ⚠ Skipping non-path plugin '{plugin.name}'"
      continue

    # Resolve plugin path
    let pluginPath = if plugin.path.startsWith(".") or plugin.path.startsWith(".."):
      getCurrentDir() / plugin.path
    else:
      plugin.path

    # Find .so file
    let soPath = pluginPath / "build" / &"libkryon_{plugin.name}.so"

    if not fileExists(soPath):
      echo &"  ✗ Plugin '{plugin.name}' not built: {soPath}"
      echo &"    Build it with: cd {pluginPath} && make"
      quit(1)

    echo &"  Loading {plugin.name}..."

    # Load plugin with RTLD_GLOBAL
    let handle = ir_plugin_load(cstring(soPath), cstring(plugin.name))

    if handle.isNil:
      echo &"  ✗ Failed to load plugin '{plugin.name}'"
      quit(1)

    # Call init function
    if not handle.init_func.isNil:
      if not handle.init_func(nil):  # Pass nil for backend context
        echo &"  ✗ Plugin '{plugin.name}' initialization failed"
        ir_plugin_unload(handle)
        quit(1)

    result.add(handle)
    echo &"  ✓ {plugin.name} loaded"

proc unloadPluginsForRuntime*(handles: seq[ptr IRPluginHandle]) =
  ## Unload all runtime-loaded plugins
  for handle in handles:
    if not handle.isNil:
      if not handle.shutdown_func.isNil:
        handle.shutdown_func()
      ir_plugin_unload(handle)
