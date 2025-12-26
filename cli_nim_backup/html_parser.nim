## HTML Parser Module
##
## FFI bindings for HTML to KIR conversion
## Enables `.html → .kir` parsing for roundtrip validation

import os

const libIrPath = when defined(release):
  getHomeDir() & ".local/lib/libkryon_ir.so"
else:
  currentSourcePath().parentDir() & "/../build/libkryon_ir.so"

# FFI: Parse HTML file to KIR JSON string
proc ir_html_file_to_kir*(filepath: cstring): cstring {.
  importc, dynlib: libIrPath.}

# FFI: Parse HTML string to KIR JSON string
proc ir_html_to_kir*(html: cstring, length: csize_t): cstring {.
  importc, dynlib: libIrPath.}

# High-level API for Nim

proc parseHTMLToKir*(htmlPath: string): string =
  ## Parse HTML file to KIR JSON
  ##
  ## Args:
  ##   htmlPath: Path to HTML file
  ##
  ## Returns:
  ##   KIR JSON as string
  ##
  ## Raises:
  ##   IOError: If file doesn't exist
  ##   ValueError: If HTML parsing fails
  ##
  ## Example:
  ##   let kir = parseHTMLToKir("build/app.html")
  ##   writeFile("build/app.kir", kir)

  if not fileExists(htmlPath):
    raise newException(IOError, "HTML file not found: " & htmlPath)

  let kirCStr = ir_html_file_to_kir(htmlPath.cstring)

  if kirCStr.isNil:
    raise newException(ValueError, "Failed to parse HTML file: " & htmlPath)

  result = $kirCStr
  # Note: C function allocates, we don't free here (TODO: check memory management)

proc parseHTMLStringToKir*(html: string): string =
  ## Parse HTML string to KIR JSON
  ##
  ## Args:
  ##   html: HTML content as string
  ##
  ## Returns:
  ##   KIR JSON as string
  ##
  ## Raises:
  ##   ValueError: If HTML parsing fails

  let kirCStr = ir_html_to_kir(html.cstring, html.len.csize_t)

  if kirCStr.isNil:
    raise newException(ValueError, "Failed to parse HTML string")

  result = $kirCStr
  # Note: C function allocates, we don't free here (TODO: check memory management)

proc parseHTMLToKirFile*(htmlPath: string, outputPath: string) =
  ## Parse HTML file to KIR and write to file
  ##
  ## Args:
  ##   htmlPath: Path to HTML file
  ##   outputPath: Path to output KIR file
  ##
  ## Example:
  ##   parseHTMLToKirFile("build/app.html", "build/app_roundtrip.kir")

  let kir = parseHTMLToKir(htmlPath)

  # Ensure output directory exists
  let outputDir = outputPath.parentDir()
  if not dirExists(outputDir):
    createDir(outputDir)

  writeFile(outputPath, kir)
  echo "✅ Generated KIR: " & outputPath

# Testing/debugging
when isMainModule:
  import strutils

  proc testHTMLParser() =
    echo "Testing HTML Parser..."

    # Test with sample HTML file (if exists)
    let testHTML = "../examples/kry/build/kryon-app.html"
    if fileExists(testHTML):
      echo "\nParsing: " & testHTML
      let kir = parseHTMLToKir(testHTML)
      echo "KIR length: " & $kir.len
      echo "First 500 chars:"
      echo kir[0..min(500, kir.len-1)]
      echo "\n✅ HTML parsing successful"
    else:
      echo "⚠️  Test HTML file not found: " & testHTML

  testHTMLParser()
