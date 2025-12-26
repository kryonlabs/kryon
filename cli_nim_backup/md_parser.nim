## Markdown Parser for Kryon CLI
## Wraps core C parser ir_markdown_parse()

import os

# C stdlib free function for freeing C-allocated strings
proc c_free(p: pointer) {.importc: "free", header: "<stdlib.h>".}

# FFI bindings to core markdown parser
type
  IRComponent = ptr object
  SVGTheme* {.pure.} = enum
    Default = 0
    Dark = 1
    Light = 2
    HighContrast = 3

  SVGOptions = object
    width: cint
    height: cint
    theme: SVGTheme
    interactive: bool
    accessibility: bool
    use_intrinsic_size: bool
    title: cstring
    description: cstring

const libIrPath = when defined(release):
  getHomeDir() & ".local/lib/libkryon_ir.so"  # Use installed path for release builds
else:
  currentSourcePath().parentDir() & "/../build/libkryon_ir.so"  # Use build path for development

# DISABLED: Requires libkryon_web.so which is not built by default
# const libWebPath = when defined(release):
#   getHomeDir() & ".local/lib/libkryon_web.so"  # Use installed path for release builds
# else:
#   currentSourcePath().parentDir() & "/../build/libkryon_web.so"  # Use build path for development

proc ir_markdown_parse(source: cstring, length: csize_t): IRComponent {.
  importc, dynlib: libIrPath.}

proc ir_markdown_to_kir(source: cstring, length: csize_t): cstring {.
  importc, dynlib: libIrPath.}

proc ir_destroy_component(root: IRComponent) {.
  importc, dynlib: libIrPath.}

# Web backend SVG generation
# DISABLED: Requires libkryon_web.so which is not built by default
# proc flowchart_to_svg(flowchart: IRComponent, options: ptr SVGOptions): cstring {.
#   importc, dynlib: libWebPath.}
#
# proc svg_options_default(): SVGOptions {.
#   importc, dynlib: libWebPath.}

proc parseMdToKir*(mdPath: string, outputPath: string = ""): string =
  ## Parse .md file and convert to .kir JSON
  ##
  ## Args:
  ##   mdPath: Path to .md file to parse
  ##   outputPath: Optional path to write .kir JSON output
  ##
  ## Returns:
  ##   KIR JSON string
  ##
  ## Raises:
  ##   IOError: If file not found or parsing failed

  if not fileExists(mdPath):
    raise newException(IOError, "Markdown file not found: " & mdPath)

  let source = readFile(mdPath)

  # Convert to KIR JSON
  stderr.writeLine "=== Nim: Calling ir_markdown_to_kir..."
  let kirJson = ir_markdown_to_kir(source.cstring, source.len.csize_t)
  stderr.writeLine "=== Nim: ir_markdown_to_kir returned, kirJson=" & (if kirJson == nil: "NULL" else: "valid")

  if kirJson == nil:
    raise newException(IOError, "Markdown parsing failed for: " & mdPath)

  stderr.writeLine "=== Nim: Converting cstring to Nim string..."
  result = $kirJson
  stderr.writeLine "=== Nim: Conversion complete, length=" & $result.len

  # CRITICAL: Free the C-allocated string to prevent memory leak
  stderr.writeLine "=== Nim: Freeing C string..."
  c_free(kirJson)
  stderr.writeLine "=== Nim: C string freed"

  # Write to file if output path specified
  if outputPath != "":
    writeFile(outputPath, result)
    echo "✅ Markdown compiled to KIR: " & outputPath

proc parseMdSource*(source: string): string =
  ## Parse markdown source string to KIR JSON
  ##
  ## Args:
  ##   source: Markdown source text
  ##
  ## Returns:
  ##   KIR JSON string
  ##
  ## Raises:
  ##   IOError: If parsing failed

  let kirJson = ir_markdown_to_kir(source.cstring, source.len.csize_t)
  if kirJson == nil:
    raise newException(IOError, "Markdown parsing failed")

  result = $kirJson
  # Free the C-allocated string to prevent memory leak
  c_free(kirJson)

proc convertMermaidToSvg*(mermaidCode: string, theme: SVGTheme = SVGTheme.Dark): string =
  ## Convert a Mermaid code block to SVG
  ##
  ## Args:
  ##   mermaidCode: The mermaid diagram code (without the ```mermaid fence)
  ##   theme: SVG theme to use (default: dark)
  ##
  ## Returns:
  ##   SVG string
  ##
  ## Raises:
  ##   IOError: If parsing or conversion failed

  # SVG conversion requires libkryon_web.so which is not built by default
  raise newException(IOError,
    "SVG conversion is currently disabled. " &
    "This feature requires libkryon_web.so which is not built by default. " &
    "To enable SVG conversion, build the web backend first.")
