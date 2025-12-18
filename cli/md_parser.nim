## Markdown Parser for Kryon CLI
## Wraps core C parser ir_markdown_parse()

import os, strutils

# FFI bindings to core markdown parser
type
  IRComponent = ptr object

const libPath = currentSourcePath().parentDir() & "/../build/libkryon_ir.so"

proc ir_markdown_parse(source: cstring, length: csize_t): IRComponent {.
  importc, dynlib: libPath.}

proc ir_markdown_to_kir(source: cstring, length: csize_t): cstring {.
  importc, dynlib: libPath.}

proc ir_free_component_tree(root: IRComponent) {.
  importc, dynlib: libPath.}

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

  return $kirJson
