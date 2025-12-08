# Kryon Plugin Architecture

**Version:** 1.0.0
**Date:** December 8, 2025
**Status:** Production Ready

## Overview

Kryon now features a complete plugin architecture that allows markdown, canvas, and other features to be developed as optional, installable plugins. This document describes the architecture, usage, and implementation details.

## Architecture

### Decentralized Package System

Kryon uses a **Go-style decentralized** package system:
- No central plugin registry
- Plugins can be hosted on **any git platform** (GitHub, GitLab, Bitbucket, self-hosted)
- Plugins can be installed from **local paths** for development
- Plugins stored in `~/Projects/` directory
- Registry maintained in `~/.config/kryon/plugins.toml`

### Plugin Structure

```
kryon-plugin-{name}/
├── plugin.toml           # Plugin manifest
├── Makefile              # Build system
├── include/
│   └── kryon_{name}.h    # Public API
├── src/                  # Core, backend-agnostic code
│   ├── {name}_api.c      # Public API implementation
│   ├── {name}_parser.c   # Backend-agnostic logic
│   └── {name}_plugin.c   # Plugin registration
├── renderers/            # Backend-specific renderers
│   ├── sdl3/
│   │   └── {name}_renderer_sdl3.c
│   └── terminal/
│       └── {name}_renderer_terminal.c
├── bindings/
│   ├── bindings.json     # Binding specification
│   └── nim/              # Generated language bindings
├── examples/
└── tests/
```

### Command ID System

- **Core Kryon**: Command IDs 0-99
- **Plugins**: Command IDs 100-255
- Each plugin reserves a range (e.g., markdown: 200-202)
- Conflict detection at installation time

### IR Plugin System

**C API** (`ir/ir_plugin.h`, `ir/ir_plugin.c`):
```c
typedef struct {
    char name[IR_PLUGIN_NAME_MAX];
    char version[IR_PLUGIN_VERSION_MAX];
    char description[IR_PLUGIN_DESC_MAX];
    char kryon_version_min[IR_PLUGIN_VERSION_MAX];
    uint32_t command_id_start;
    uint32_t command_id_end;
    const char** required_capabilities;
    uint32_t capability_count;
} IRPluginMetadata;

bool ir_plugin_register(const IRPluginMetadata* metadata);
bool ir_plugin_check_command_conflict(uint32_t start, end, ...);
```

**Features:**
- Version compatibility checking
- Command ID conflict detection
- Capability negotiation
- Up to 32 simultaneous plugins

## CLI Plugin Management

### Installation

```bash
# From git repository (any host)
kryon plugin add github.com/user/kryon-plugin-markdown
kryon plugin add gitlab.com/other/kryon-plugin-canvas
kryon plugin add git.example.com/custom/plugin.git

# From local path (development)
kryon plugin add /path/to/plugin
kryon plugin add ../kryon-plugin-markdown
```

### Management Commands

```bash
# List installed plugins
kryon plugin list

# Show plugin details
kryon plugin info markdown

# Update plugin from git
kryon plugin update markdown

# Unregister plugin (keeps source)
kryon plugin remove markdown
```

### Implementation

**CLI Modules:**
- `cli/plugin_manager.nim` - High-level commands
- `cli/plugin_registry.nim` - `~/.config/kryon/plugins.toml` management
- `cli/plugin_git.nim` - Git operations
- `cli/plugin_build.nim` - Plugin building and validation
- `cli/binding_generator.nim` - Automatic binding generation

## Plugin Development

### 1. Plugin Manifest (`plugin.toml`)

```toml
[plugin]
name = "markdown"
version = "1.0.0"
description = "CommonMark markdown rendering"
license = "0BSD"
repository = "https://github.com/kryon-plugins/markdown"

[compatibility]
kryon_version = ">=0.3.0"
api_version = "1.0"

[capabilities]
command_ids = [200, 201, 202]
backends = ["sdl3", "terminal", "web"]
features = ["rich_text", "inline_formatting"]

[build]
c_sources = ["src/*.c"]
c_headers = ["include/*.h"]
dependencies = ["harfbuzz", "fribidi"]

[bindings]
nim = true
lua = true
```

### 2. Plugin Registration

```c
// src/markdown_plugin.c
#include "ir_plugin.h"

bool kryon_markdown_plugin_init(void) {
    IRPluginMetadata metadata = {
        .name = "markdown",
        .version = "1.0.0",
        .description = "CommonMark markdown rendering",
        .kryon_version_min = "0.3.0",
        .command_id_start = 200,
        .command_id_end = 202,
        .required_capabilities = (const char*[]){"text_rendering", NULL},
        .capability_count = 1
    };

    return ir_plugin_register(&metadata);
}
```

### 3. Binding Specification (`bindings/bindings.json`)

```json
{
  "plugin": {
    "name": "markdown",
    "version": "1.0.0",
    "description": "CommonMark markdown rendering"
  },
  "functions": [
    {
      "name": "render_markdown",
      "c_name": "kryon_render_markdown",
      "description": "Render markdown content as a component",
      "returns": {
        "type": "IRComponent*",
        "nim_type": "KryonComponent"
      },
      "parameters": [
        {
          "name": "source",
          "type": "const char*",
          "nim_type": "cstring",
          "description": "Markdown source text"
        },
        {
          "name": "theme",
          "type": "const char*",
          "nim_type": "cstring",
          "default": "light",
          "description": "Theme name (light or dark)"
        }
      ]
    }
  ],
  "types": {
    "MarkdownTheme": {
      "kind": "enum",
      "values": ["light", "dark"]
    }
  },
  "dsl_macros": {
    "nim": {
      "Markdown": {
        "properties": [
          {
            "name": "source",
            "type": "string",
            "required": true,
            "description": "Markdown content to render"
          },
          {
            "name": "theme",
            "type": "string",
            "required": false,
            "default": "light",
            "values": ["light", "dark"],
            "description": "Color theme"
          }
        ],
        "example": "Markdown:\\n  source = \"# Hello **World**\"\\n  theme = \"dark\""
      }
    }
  }
}
```

### 4. Automatic Binding Generation

When a plugin is built, bindings are automatically generated:

```bash
$ kryon plugin add /path/to/plugin
Building plugin 'markdown'...
✓ Plugin built successfully
  Generating bindings...
  ✓ Generated Nim bindings: bindings/nim/markdown.nim
✓ Plugin 'markdown' registered successfully
```

Generated Nim binding:
```nim
# Auto-generated from bindings.json
import ../../../bindings/nim/ir_core
import ../../../bindings/nim/runtime

const pluginPath = currentSourcePath().parentDir() / ".." / ".." / "build" / "libmarkdown.so"
{.pragma: pluginImport, importc, dynlib: pluginPath.}

proc kryon_render_markdown(source: cstring, theme: cstring): KryonComponent {.pluginImport.}

proc render_markdown*(source: cstring, theme: cstring = "light"): KryonComponent =
  result = kryon_render_markdown(source, theme)

macro Markdown*(props: untyped): untyped =
  # Auto-generated DSL macro
  # ...
```

### 5. Backend-Specific Rendering

Plugins separate backend-agnostic code from renderer-specific code:

```
src/
  markdown_parser.c       # Backend-agnostic parsing

renderers/
  sdl3/
    markdown_renderer_sdl3.c   # SDL3-specific rendering
  terminal/
    markdown_renderer_terminal.c  # Terminal rendering
  web/
    markdown_renderer_web.c      # Web/CSS rendering
```

## Example: Markdown Plugin

### Installation

```bash
# Install from GitHub
kryon plugin add github.com/kryon-plugins/markdown

# Or from local development
kryon plugin add ~/Projects/kryon-plugin-markdown
```

### Usage in Nim

```nim
import kryon_dsl
# Configure Nim path in nim.cfg:
# --path:"$HOME/Projects/kryon-plugin-markdown/bindings/nim"
import markdown

kryonApp:
  Container:
    Markdown:
      source = """
        # Hello World

        This is **bold** and this is *italic*.

        ```nim
        echo "Code block!"
        ```
      """
      theme = "light"
```

### Migration from Embedded

**Old (embedded markdown):**
```nim
import kryon_dsl  # Markdown was built-in

kryonApp:
  Markdown: source = "# Title"
```

**New (plugin):**
```nim
import kryon_dsl
import markdown  # Import from plugin

kryonApp:
  Markdown: source = "# Title"  # Same API!
```

Only change needed: Add import and configure Nim path.

## Plugin Registry

Located at `~/.config/kryon/plugins.toml`:

```toml
[[plugin]]
name = "markdown"
version = "1.0.0"
local_path = "/home/user/Projects/kryon-plugin-markdown"
git_url = "github.com/kryon-plugins/markdown"
kryon_version_min = "0.3.0"
command_ids = [200, 201, 202]

[[plugin]]
name = "canvas"
version = "2.0.0"
local_path = "/home/user/Projects/kryon-plugin-canvas"
kryon_version_min = "0.3.0"
command_ids = [100, 101, 102]
```

## Core Changes

### Removed from Core

**Deleted:**
- `backends/desktop/desktop_markdown.c` (775 lines)
- `core/include/kryon_markdown.h`
- Markdown types from `desktop_internal.h`
- Markdown globals from `desktop_globals.c`

**Modified:**
- `backends/desktop/Makefile` - Removed markdown compilation
- `backends/desktop/desktop_rendering.c` - Markdown case delegated to plugin
- `backends/desktop/desktop_input.c` - Removed markdown scrolling
- `bindings/nim/kryon_dsl/components.nim` - Removed Markdown macro
- `bindings/nim/runtime.nim` - Removed newKryonMarkdown()

### Added to Core

**New Files:**
- `ir/ir_plugin.h` - Plugin API
- `ir/ir_plugin.c` - Plugin registration and management
- `cli/plugin_manager.nim` - CLI plugin commands
- `cli/plugin_registry.nim` - Plugin registry management
- `cli/plugin_git.nim` - Git operations
- `cli/plugin_build.nim` - Build orchestration
- `cli/binding_generator.nim` - Automatic binding generation

## Benefits

### For Users
✓ Install only plugins you need
✓ Smaller core download
✓ Plugin updates independent of core
✓ Works with any git host
✓ Local development friendly

### For Developers
✓ Separate repositories for each plugin
✓ Independent release cycles
✓ Automatic binding generation
✓ Backend separation enforced
✓ Clear plugin API

### For the Project
✓ Cleaner core codebase
✓ Modular architecture
✓ Third-party plugin ecosystem
✓ Easier maintenance
✓ Scalable design

## Supported Platforms

- **Git Hosts**: GitHub, GitLab, Bitbucket, Gitea, self-hosted
- **Backends**: SDL3, terminal (web planned)
- **Languages**: Nim (Lua planned)
- **OS**: Linux, macOS (Windows planned)

## Future Plugins

Planned plugins using this architecture:
- ✓ **markdown** - CommonMark rendering (complete)
- ⏳ **canvas** - 2D drawing (exists, needs standardization)
- ⏳ **tilemap** - Tile-based games (exists, needs standardization)
- 📋 **charts** - Data visualization
- 📋 **video** - Video playback
- 📋 **3d** - 3D rendering
- 📋 **physics** - Physics simulation

## References

- **Plugin Development Guide**: `docs/PLUGIN_DEVELOPMENT.md`
- **API Reference**: `ir/ir_plugin.h`
- **Example Plugin**: `~/Projects/kryon-plugin-markdown`
- **CLI Help**: `kryon plugin --help`

---

**Note**: This architecture represents a major milestone in Kryon's evolution, transforming it from a monolithic framework into a modular, extensible platform.
