## HTML Transpilation Module
##
## FFI bindings for HTML generator with transpilation mode support
## Enables `.kir → HTML` with metadata preservation for roundtrip validation

import os

type
  IRComponent* = ptr object
  HTMLGenerator* = ptr object

  HtmlGeneratorMode* {.pure.} = enum
    Display = 0      ## Runtime rendering with JS (current behavior)
    Transpile = 1    ## Static transpilation for roundtrip (new)

  # FFI struct must match C layout exactly
  HtmlGeneratorOptions* {.packed.} = object
    mode*: cint              ## Use cint to match C enum (4 bytes)
    minify*: bool            ## Minify output (remove whitespace)
    inline_css*: bool        ## Inline CSS vs external file
    preserve_ids*: bool      ## Preserve component IDs for debugging

const libIrPath = when defined(release):
  getHomeDir() & ".local/lib/libkryon_ir.so"
else:
  currentSourcePath().parentDir() & "/../build/libkryon_ir.so"

const libWebPath = when defined(release):
  getHomeDir() & ".local/lib/libkryon_web.so"
else:
  currentSourcePath().parentDir() & "/../build/libkryon_web.so"

# IR deserialization
proc ir_read_json_v2_file*(filename: cstring): IRComponent {.
  importc, dynlib: libIrPath.}

proc ir_destroy_component*(component: IRComponent) {.
  importc, dynlib: libIrPath.}

# HTML generation with options
proc html_generator_create_with_options*(options: HtmlGeneratorOptions): HTMLGenerator {.
  importc, dynlib: libWebPath.}

proc html_generator_destroy*(generator: HTMLGenerator) {.
  importc, dynlib: libWebPath.}

proc html_generator_generate*(generator: HTMLGenerator, root: IRComponent): cstring {.
  importc, dynlib: libWebPath.}

proc html_generator_default_options*(): HtmlGeneratorOptions {.
  importc, dynlib: libWebPath.}

# High-level API for Nim
proc defaultTranspilationOptions*(): HtmlGeneratorOptions =
  ## Get default options for transpilation mode
  result = HtmlGeneratorOptions(
    mode: cint(HtmlGeneratorMode.Transpile),
    minify: false,
    inline_css: true,
    preserve_ids: false
  )

proc defaultDisplayOptions*(): HtmlGeneratorOptions =
  ## Get default options for display mode (runtime with JS)
  result = HtmlGeneratorOptions(
    mode: cint(HtmlGeneratorMode.Display),
    minify: false,
    inline_css: true,
    preserve_ids: false
  )

proc transpileKirToHTML*(kirPath: string, options: HtmlGeneratorOptions): string =
  ## Transpile a .kir file to HTML with given options
  ##
  ## Args:
  ##   kirPath: Path to .kir (JSON IR) file
  ##   options: HTML generation options (mode, minify, etc.)
  ##
  ## Returns:
  ##   Generated HTML as string
  ##
  ## Example:
  ##   let opts = defaultTranspilationOptions()
  ##   let html = transpileKirToHTML("app.kir", opts)
  ##   writeFile("app.html", html)

  if not fileExists(kirPath):
    raise newException(IOError, "KIR file not found: " & kirPath)

  # Load IR from file
  let root = ir_read_json_v2_file(kirPath.cstring)
  if root.isNil:
    raise newException(ValueError, "Failed to load IR from: " & kirPath)

  # Create generator with options
  let generator = html_generator_create_with_options(options)
  if generator.isNil:
    ir_destroy_component(root)
    raise newException(ValueError, "Failed to create HTML generator")

  # Generate HTML
  let htmlCStr = html_generator_generate(generator, root)
  result = $htmlCStr

  # Cleanup
  html_generator_destroy(generator)
  ir_destroy_component(root)

proc transpileKirToHTMLFile*(kirPath: string, outputPath: string, options: HtmlGeneratorOptions) =
  ## Transpile a .kir file to HTML and write to file
  ##
  ## Args:
  ##   kirPath: Path to .kir (JSON IR) file
  ##   outputPath: Path to output HTML file
  ##   options: HTML generation options
  ##
  ## Example:
  ##   let opts = defaultTranspilationOptions()
  ##   transpileKirToHTMLFile("app.kir", "build/index.html", opts)

  let html = transpileKirToHTML(kirPath, options)

  # Ensure output directory exists
  let outputDir = outputPath.parentDir()
  if not dirExists(outputDir):
    createDir(outputDir)

  writeFile(outputPath, html)
  echo "✅ Generated HTML: " & outputPath

proc transpileKirToHTMLDefault*(kirPath: string, outputPath: string) =
  ## Convenience function: transpile with default transpilation options
  let opts = defaultTranspilationOptions()
  transpileKirToHTMLFile(kirPath, outputPath, opts)

proc displayKirToHTML*(kirPath: string, outputPath: string) =
  ## Convenience function: generate display HTML (with JS) from .kir
  let opts = defaultDisplayOptions()
  transpileKirToHTMLFile(kirPath, outputPath, opts)

# Testing/debugging helpers
when isMainModule:
  import strutils

  proc testTranspilation() =
    echo "Testing HTML transpilation..."

    # Test with sample .kir file (if exists)
    let testKir = "../build/generated/kir/hello_world.kir"
    if fileExists(testKir):
      echo "\nTest 1: Transpilation mode"
      let opts1 = defaultTranspilationOptions()
      let html1 = transpileKirToHTML(testKir, opts1)
      echo "Output length: " & $html1.len
      echo "Has data-ir-type: " & $(html1.contains("data-ir-type"))
      echo "Has onclick: " & $(html1.contains("onclick"))

      echo "\nTest 2: Display mode"
      let opts2 = defaultDisplayOptions()
      let html2 = transpileKirToHTML(testKir, opts2)
      echo "Output length: " & $html2.len
      echo "Has data-ir-type: " & $(html2.contains("data-ir-type"))
      echo "Has onclick: " & $(html2.contains("onclick"))

      echo "\n✅ Tests completed"
    else:
      echo "⚠️  Test .kir file not found: " & testKir

  testTranspilation()
