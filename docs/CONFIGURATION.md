# Kryon Configuration Reference

Complete specification for `kryon.toml` configuration files.

## Table of Contents

1. [Introduction](#introduction)
2. [Required Sections](#required-sections)
3. [Optional Sections](#optional-sections)
4. [Complete Field Reference](#complete-field-reference)
5. [Examples by Project Type](#examples-by-project-type)
6. [Migration Guide](#migration-guide)
7. [Common Patterns](#common-patterns)

---

## Introduction

Every Kryon project uses a `kryon.toml` configuration file that defines:
- Project metadata (name, version, description)
- Build settings (entry point, targets, frontend language)
- Target-specific configurations (window, web, desktop, terminal)
- Development server settings
- Plugin and dependency management

This document defines the **single, canonical format** for all Kryon projects. There is no backward compatibility with old formats - the config parser enforces this specification strictly.

---

## Required Sections

Every `kryon.toml` file **must** include these two sections:

### [project]

Project metadata and identification.

```toml
[project]
name = "my-app"              # Required: Project name (kebab-case recommended)
version = "1.0.0"            # Required: Semantic version (MAJOR.MINOR.PATCH)
description = "..."          # Optional: Brief project description
author = "Author Name"       # Optional: Project author
license = "0BSD"             # Optional: License identifier (SPDX format)
```

**Field Details**:
- `name` (string, required): Project identifier. Use lowercase with hyphens (kebab-case).
- `version` (string, required): Semantic version following [semver](https://semver.org/).
- `description` (string, optional): One-line description of the project.
- `author` (string, optional): Author name or organization.
- `license` (string, optional): License identifier (e.g., "MIT", "0BSD", "Apache-2.0").

---

### [build]

Core build configuration.

```toml
[build]
entry = "main.tsx"           # Required: Entry point source file
targets = ["web"]            # Required: Build targets (always array)
frontend = "tsx"             # Required: Frontend language
output_dir = "dist"          # Optional: Output directory (default: "dist")
```

**Field Details**:
- `entry` (string, required): Path to the main source file. Relative to project root.
- `targets` (array of strings, required): Build targets. Valid values:
  - `"web"` - HTML/CSS/JS output for browsers
  - `"desktop"` - Native desktop application (SDL3)
  - `"terminal"` - Terminal UI application
  - `"embedded"` - Embedded systems (STM32, ESP32, etc.)
- `frontend` (string, required): Source language/format. Valid values:
  - `"tsx"` - TypeScript with JSX
  - `"jsx"` - JavaScript with JSX
  - `"lua"` - Lua scripts
  - `"nim"` - Nim source files
  - `"md"` - Markdown documents
  - `"kry"` - Kryon DSL
  - `"c"` - C source files
- `output_dir` (string, optional): Directory for build output. Defaults to `"dist"`.

**Important**: The `targets` field is **always an array**, even for a single target. Use `targets = ["web"]`, not `target = "web"`.

---

## Optional Sections

### [[build.pages]]

Multi-page project configuration. Use array-of-tables syntax (`[[build.pages]]`) to define multiple pages.

```toml
[[build.pages]]
name = "home"                # Required: Page identifier
route = "/"                  # Required: URL route
source = "index.tsx"         # Required: Source file for this page

[[build.pages]]
name = "about"
route = "/about"
source = "about.tsx"
```

**Field Details**:
- `name` (string, required): Unique identifier for the page.
- `route` (string, required): URL path (e.g., "/", "/about", "/docs/intro").
- `source` (string, required): Source file path relative to project root.

**When to Use**: Multi-page websites where each page has its own source file. For single-page apps, use the `build.entry` field instead.

---

### [build.docs]

Documentation generation configuration.

```toml
[build.docs]
enabled = true               # Required: Enable docs generation
directory = "docs"           # Required: Docs source directory
template = "docs.tsx"        # Required: Template file for rendering docs
base_path = "/docs"          # Optional: Base URL path (default: "/docs")
index_file = "index.md"      # Optional: Landing page (default: "index.md")
```

**Field Details**:
- `enabled` (boolean, required): Set to `true` to enable docs generation.
- `directory` (string, required): Directory containing markdown documentation files.
- `template` (string, required): Template component for rendering docs (e.g., TSX wrapper).
- `base_path` (string, optional): URL base path for docs. Defaults to `"/docs"`.
- `index_file` (string, optional): Index/landing page file. Defaults to `"index.md"`.

**When to Use**: Projects with extensive documentation that should be rendered alongside the main application.

---

### [build.optimization]

Build optimization settings.

```toml
[build.optimization]
enabled = true               # Required: Enable optimization
minify_css = true            # Optional: Minify CSS output (default: true)
minify_js = true             # Optional: Minify JavaScript output (default: true)
tree_shake = true            # Optional: Remove unused code (default: true)
```

**Field Details**:
- `enabled` (boolean, required): Master switch for build optimization.
- `minify_css` (boolean, optional): Minify CSS output. Defaults to `true`.
- `minify_js` (boolean, optional): Minify JavaScript output. Defaults to `true`.
- `tree_shake` (boolean, optional): Remove unused code (dead code elimination). Defaults to `true`.

---

### [window]

Window configuration for desktop and embedded targets.

```toml
[window]
title = "My Application"     # Required: Window title
width = 1200                 # Required: Window width in pixels
height = 800                 # Required: Window height in pixels
resizable = true             # Optional: Allow window resizing (default: true)
fullscreen = false           # Optional: Start in fullscreen (default: false)
```

**Field Details**:
- `title` (string, required): Window title shown in title bar.
- `width` (integer, required): Initial window width in pixels.
- `height` (integer, required): Initial window height in pixels.
- `resizable` (boolean, optional): Allow user to resize window. Defaults to `true`.
- `fullscreen` (boolean, optional): Start in fullscreen mode. Defaults to `false`.

**When to Use**: Projects with `"desktop"` or `"embedded"` targets that need window configuration.

---

### [web]

Web-specific build configuration.

```toml
[web]
renderer = "static"                  # Optional: Renderer type (default: "static")
generate_separate_files = true      # Optional: Generate separate HTML per page (default: true)
include_js_runtime = false          # Optional: Include JS runtime (default: false)
minify_css = true                   # Optional: Minify CSS (default: true)
minify_js = true                    # Optional: Minify JS (default: true)
```

**Field Details**:
- `renderer` (string, optional): Rendering strategy. Valid values:
  - `"static"` - Pre-render to static HTML (default)
  - `"runtime"` - Client-side rendering with JavaScript
- `generate_separate_files` (boolean, optional): Generate separate HTML file per page. Defaults to `true`.
- `include_js_runtime` (boolean, optional): Include JavaScript runtime in output. Defaults to `false`.
- `minify_css` (boolean, optional): Minify CSS output. Overrides `build.optimization.minify_css` if set.
- `minify_js` (boolean, optional): Minify JS output. Overrides `build.optimization.minify_js` if set.

**When to Use**: Projects with `"web"` target that need web-specific build settings.

---

### [desktop]

Desktop-specific configuration.

```toml
[desktop]
renderer = "sdl3"            # Required: Rendering backend
backend = "gpu"              # Optional: Graphics backend (default: "gpu")
vsync = true                 # Optional: Enable vertical sync (default: true)
```

**Field Details**:
- `renderer` (string, required): Desktop rendering backend. Valid values:
  - `"sdl3"` - SDL3 (Simple DirectMedia Layer 3)
  - `"raylib"` - Raylib game framework
  - More backends planned (GTK, Qt, etc.)
- `backend` (string, optional): Graphics acceleration. Valid values:
  - `"gpu"` - Hardware-accelerated rendering (default)
  - `"cpu"` - CPU rendering
  - `"software"` - Software rendering
- `vsync` (boolean, optional): Enable vertical synchronization. Defaults to `true`.

**When to Use**: Projects with `"desktop"` target.

---

### [terminal]

Terminal UI configuration.

```toml
[terminal]
colors = "256"               # Optional: Color support (default: "256")
mouse = true                 # Optional: Enable mouse support (default: true)
```

**Field Details**:
- `colors` (string, optional): Color mode. Valid values:
  - `"16"` - 16-color mode (basic ANSI)
  - `"256"` - 256-color mode (default)
  - `"truecolor"` - 24-bit true color
- `mouse` (boolean, optional): Enable mouse input in terminal. Defaults to `true`.

**When to Use**: Projects with `"terminal"` target.

---

### [dev]

Development server configuration.

```toml
[dev]
hot_reload = true                        # Optional: Enable hot reload (default: true)
port = 3000                              # Optional: Dev server port (default: 3000)
auto_open = true                         # Optional: Auto-open browser (default: false)
watch_paths = ["src/**/*.tsx"]           # Optional: File patterns to watch
```

**Field Details**:
- `hot_reload` (boolean, optional): Enable hot module reloading. Defaults to `true`.
- `port` (integer, optional): Development server port. Defaults to `3000`.
- `auto_open` (boolean, optional): Automatically open browser on server start. Defaults to `false`.
- `watch_paths` (array of strings, optional): Glob patterns for files to watch. Defaults to watching all source files.

**When to Use**: Projects that use the development server (`kryon dev`).

---

### [plugins]

Plugin dependencies.

```toml
[plugins]
plugin-name = { path = "../plugin-path" }
another-plugin = { path = "/abs/path", version = "1.0.0" }
```

**Format**: Always use inline table syntax with `path` (required) and optional `version`.

**Field Details**:
- `path` (string, required): Path to plugin directory (relative or absolute).
- `version` (string, optional): Version constraint for the plugin.

**Examples**:
```toml
[plugins]
markdown = { path = "../kryon-plugin-markdown" }
storage = { path = "../kryon-plugin-storage", version = "^1.0" }
canvas = { path = "/home/user/kryon-canvas" }
```

---

### [dependencies]

Project dependencies.

```toml
[dependencies]
kryon = { path = "../kryon" }
other-lib = { path = "../other-lib", version = "2.0.0" }
```

**Format**: Same as `[plugins]` - use inline table syntax with `path` (required) and optional `version`.

**Field Details**:
- `path` (string, required): Path to dependency directory (relative or absolute).
- `version` (string, optional): Version constraint for the dependency.

---

## Complete Field Reference

### Naming Conventions

**Field Names**:
- Use `snake_case` for all field names
- Use singular for single values: `entry`, `frontend`, `template`
- Use plural for arrays: `targets`, `watch_paths`

**Section Names**:
- Use lowercase for section names: `[project]`, `[build]`, `[window]`
- Use dot notation for nested sections: `[build.docs]`, `[build.optimization]`
- Use double brackets for array-of-tables: `[[build.pages]]`

---

### Standard Field Names

**Required Fields**:
- `entry` - Entry point file (NOT `entry_point`, NOT `file`)
- `targets` - Build targets array (NOT `target`)
- `frontend` - Frontend language (NOT `language`)
- Window fields: `title`, `width`, `height` (NOT `window_title`, `window_width`, `window_height`)

**Deprecated/Invalid Field Names** (rejected by parser):
- `entry_point` - Use `entry`
- `target` (singular string) - Use `targets` (array)
- `language` - Use `frontend`
- `window_title`, `window_width`, `window_height` - Use `[window]` section with `title`, `width`, `height`
- `file`, `path` in pages - Use `name`, `route`, `source`

---

## Examples by Project Type

### Single-Page Web Application

```toml
[project]
name = "my-web-app"
version = "1.0.0"
description = "A simple web application"
author = "Developer Name"
license = "MIT"

[build]
entry = "index.tsx"
targets = ["web"]
frontend = "tsx"
output_dir = "dist"

[build.optimization]
enabled = true
minify_css = true
minify_js = true
tree_shake = true

[web]
renderer = "static"
generate_separate_files = false
include_js_runtime = false

[dev]
hot_reload = true
port = 3000
auto_open = true
watch_paths = ["index.tsx", "src/**/*.tsx"]
```

---

### Multi-Page Website

```toml
[project]
name = "my-website"
version = "2.0.0"
description = "Multi-page website with documentation"
author = "Company Name"
license = "0BSD"

[build]
entry = "index.tsx"
targets = ["web"]
frontend = "tsx"
output_dir = "build"

[[build.pages]]
name = "home"
route = "/"
source = "index.tsx"

[[build.pages]]
name = "about"
route = "/about"
source = "about.tsx"

[[build.pages]]
name = "contact"
route = "/contact"
source = "contact.tsx"

[build.docs]
enabled = true
directory = "docs"
template = "docs.tsx"
base_path = "/docs"
index_file = "getting-started.md"

[build.optimization]
enabled = true
minify_css = true
minify_js = true
tree_shake = true

[web]
renderer = "static"
generate_separate_files = true
include_js_runtime = false

[dev]
hot_reload = true
port = 3000
auto_open = true
watch_paths = ["*.tsx", "docs/**/*.md"]

[plugins]
markdown = { path = "../kryon-plugin-markdown" }
```

---

### Desktop Application (Lua)

```toml
[project]
name = "my-desktop-app"
version = "1.0.0"
description = "Desktop application written in Lua"
author = "Developer Name"

[build]
entry = "main.lua"
targets = ["desktop"]
frontend = "lua"
output_dir = "dist"

[window]
title = "My Desktop App"
width = 1200
height = 800
resizable = true
fullscreen = false

[desktop]
renderer = "sdl3"
backend = "gpu"
vsync = true

[plugins]
storage = { path = "../kryon-storage" }
canvas = { path = "../kryon-canvas" }

[dependencies]
kryon = { path = "../kryon" }
```

---

### Terminal UI Application

```toml
[project]
name = "my-terminal-app"
version = "1.0.0"
description = "Terminal-based user interface"

[build]
entry = "main.tsx"
targets = ["terminal"]
frontend = "tsx"
output_dir = "dist"

[terminal]
colors = "truecolor"
mouse = true
```

---

### Multi-Target Application

```toml
[project]
name = "multi-target-app"
version = "1.0.0"
description = "Application targeting both web and desktop"

[build]
entry = "main.tsx"
targets = ["web", "desktop"]
frontend = "tsx"
output_dir = "dist"

[window]
title = "Multi-Target App"
width = 1200
height = 900
resizable = true

[web]
renderer = "static"
generate_separate_files = true
include_js_runtime = false
minify_css = true
minify_js = true

[desktop]
renderer = "sdl3"
backend = "gpu"
vsync = true

[build.optimization]
enabled = true
minify_css = true
minify_js = true
tree_shake = true

[dev]
hot_reload = true
port = 3000
auto_open = true
watch_paths = ["main.tsx", "src/**/*.tsx"]

[dependencies]
kryon = { path = "../kryon" }
```

---

## Migration Guide

This section helps migrate from old/inconsistent TOML formats to the new standard.

### Breaking Changes

**NO BACKWARD COMPATIBILITY**: The config parser enforces the new format strictly. Old field names and formats are rejected with clear error messages.

---

### Migration Checklist

For each project, update your `kryon.toml`:

1. **Move `entry` to `[build]` section**
   - Old: `[project] entry = "main.lua"`
   - New: `[build] entry = "main.lua"`

2. **Rename `entry_point` to `entry`**
   - Old: `entry_point = "main.lua"`
   - New: `entry = "main.lua"`

3. **Change `target` (string) to `targets` (array)**
   - Old: `target = "web"`
   - New: `targets = ["web"]`

4. **Move `frontend` to `[build]` section**
   - Old: `[project] frontend = "tsx"`
   - New: `[build] frontend = "tsx"`

5. **Move window config to `[window]` section**
   - Old: `[desktop] window_title = "App"`, `window_width = 1200`, `window_height = 800`
   - New: `[window] title = "App"`, `width = 1200`, `height = 800`

6. **Update pages format**
   - Old: `[[build.pages]] file = "index.tsx"`, `path = "/"`
   - New: `[[build.pages]] name = "home"`, `route = "/"`, `source = "index.tsx"`

7. **Convert plugin strings to inline tables**
   - Old: `[plugins] storage = "../kryon-storage"`
   - New: `[plugins] storage = { path = "../kryon-storage" }`

8. **Convert dependency strings to inline tables**
   - Old: `[dependencies] kryon = "../kryon"`
   - New: `[dependencies] kryon = { path = "../kryon" }`

9. **Remove `[backends]` section**
   - Move backend info to target-specific sections like `[desktop]`

10. **Remove `[metadata]` section**
    - Not needed in new format

---

### Example Migration: Old Format → New Format

**Old Format** (habits project):
```toml
[project]
name = "habits"
version = "1.0.0"
description = "Habit tracking application"
entry = "habits.lua"

[build]
target = "lua"

[plugins]
storage = "../kryon-storage"

[dependencies]
kryon = "../kryon"
```

**New Format**:
```toml
[project]
name = "habits"
version = "1.0.0"
description = "Habit tracking application"

[build]
entry = "habits.lua"
targets = ["desktop"]
frontend = "lua"
output_dir = "dist"

[window]
title = "Habits Tracker"
width = 1200
height = 800
resizable = true

[desktop]
renderer = "sdl3"

[plugins]
storage = { path = "../kryon-storage" }

[dependencies]
kryon = { path = "../kryon" }
```

---

### Example Migration: Complex Project

**Old Format** (kryon-website):
```toml
[project]
name = "kryon-website"
version = "1.0.0"
author = "Kryon Labs"

[build]
targets = ["web", "desktop"]
output_dir = "build"
frontend = "tsx"

[[build.pages]]
file = "index.tsx"
path = "/"

[desktop]
renderer = "sdl3"
window_width = 1200
window_height = 900
window_title = "Kryon Framework"

[metadata]
created = "2025-11-30"
kryon_version = "1.2.0"
```

**New Format**:
```toml
[project]
name = "kryon-website"
version = "1.0.0"
author = "Kryon Labs"
description = "Official Kryon website built with Kryon"

[build]
entry = "index.tsx"
targets = ["web", "desktop"]
frontend = "tsx"
output_dir = "build"

[[build.pages]]
name = "home"
route = "/"
source = "index.tsx"

[build.docs]
enabled = true
directory = "docs"
template = "docs.tsx"
base_path = "/docs"
index_file = "getting-started.md"

[build.optimization]
enabled = true
minify_css = true
minify_js = true
tree_shake = true

[window]
title = "Kryon Framework"
width = 1200
height = 900
resizable = true

[desktop]
renderer = "sdl3"
backend = "gpu"
vsync = true

[web]
renderer = "static"
generate_separate_files = true
include_js_runtime = false
minify_css = true
minify_js = true

[dev]
hot_reload = true
port = 3000
auto_open = true
watch_paths = ["index.tsx", "docs.tsx", "docs/**/*.md", "src/**/*.tsx"]

[plugins]
markdown = { path = "../kryon-plugin-markdown" }
```

---

## Common Patterns

### Pattern: Single Entry Point

Most projects have a single entry point file:

```toml
[build]
entry = "main.tsx"
targets = ["web"]
frontend = "tsx"
```

---

### Pattern: Multi-Page Website

Use `[[build.pages]]` for multi-page sites:

```toml
[build]
entry = "index.tsx"
targets = ["web"]
frontend = "tsx"

[[build.pages]]
name = "home"
route = "/"
source = "index.tsx"

[[build.pages]]
name = "about"
route = "/about"
source = "about.tsx"
```

---

### Pattern: Documentation Generation

Enable docs with `[build.docs]`:

```toml
[build.docs]
enabled = true
directory = "docs"
template = "docs.tsx"
base_path = "/docs"
index_file = "getting-started.md"
```

---

### Pattern: Development Workflow

Configure dev server with `[dev]`:

```toml
[dev]
hot_reload = true
port = 3000
auto_open = true
watch_paths = ["src/**/*.tsx", "docs/**/*.md"]
```

---

### Pattern: Cross-Platform App

Build for multiple targets:

```toml
[build]
targets = ["web", "desktop", "terminal"]
frontend = "tsx"

[window]
title = "Cross-Platform App"
width = 1200
height = 800

[web]
renderer = "static"

[desktop]
renderer = "sdl3"

[terminal]
colors = "truecolor"
```

---

### Pattern: Optimized Production Build

Enable all optimizations:

```toml
[build.optimization]
enabled = true
minify_css = true
minify_js = true
tree_shake = true
```

---

## Validation

The Kryon CLI validates `kryon.toml` strictly:

```bash
# Validate configuration
kryon config validate

# Show current configuration
kryon config show
```

**Validation checks**:
- Required fields are present (`project.name`, `project.version`, `build.entry`, `build.targets`, `build.frontend`)
- Field types are correct (strings, integers, booleans, arrays)
- Unknown fields/sections are rejected
- Old/deprecated field names are rejected
- Array fields use array syntax (not strings)

**Error messages** are clear and actionable:
```
Error: Invalid field 'entry_point' in [build]
       Use 'entry' instead
```

---

## Best Practices

1. **Always use the new format** - No mixing of old and new field names
2. **Use `targets` as an array** - Even for a single target: `targets = ["web"]`
3. **Keep required sections minimal** - Only include optional sections when needed
4. **Use semantic versioning** - Follow [semver](https://semver.org/) for `project.version`
5. **Use relative paths** - For plugins/dependencies within the same workspace
6. **Use kebab-case for names** - Project names should be lowercase with hyphens
7. **Document your config** - Add comments to explain non-obvious settings
8. **Validate before committing** - Run `kryon config validate` before committing changes
9. **Use inline tables for plugins** - Always: `{ path = "..." }`, never simple strings
10. **Specify output_dir** - Be explicit about where build artifacts go

---

## See Also

- [Getting Started Guide](getting-started.md)
- [Build System](architecture.md#build-pipeline)
- [Plugin Development](plugins.md)
- [CLI Commands](developer-guide.md#cli-commands)
