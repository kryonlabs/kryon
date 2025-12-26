## TSX Parser for Kryon
## Parses TSX files to .kir JSON format using Bun/Babel

import os, osproc, json, strutils

# Embed the parser script at compile time
const TsxParserScript = staticRead("../scripts/tsx/parse_tsx.ts")

var cachedParserPath: string = ""

proc findBun(): string =
  ## Find bun executable
  result = findExe("bun")
  if result == "":
    # Try common locations
    let paths = [
      expandTilde("~/.bun/bin/bun"),
      "/usr/local/bin/bun",
      "/usr/bin/bun"
    ]
    for p in paths:
      if fileExists(p):
        return p
    raise newException(IOError, "bun not found. Please install bun: https://bun.sh")

proc getTsxParserScript(): string =
  ## Get path to TSX parser script (creates temp file from embedded script)

  # Return cached path if still valid
  if cachedParserPath != "" and fileExists(cachedParserPath):
    return cachedParserPath

  # First try to find installed script in known locations
  let searchPaths = [
    getAppDir() / "scripts" / "tsx" / "parse_tsx.ts",
    getAppDir().parentDir() / "scripts" / "tsx" / "parse_tsx.ts",
    getAppDir().parentDir() / "share" / "kryon" / "scripts" / "tsx" / "parse_tsx.ts",
    expandTilde("~/.local/share/kryon/scripts/tsx/parse_tsx.ts"),
    getEnv("KRYON_ROOT", "") / "scripts" / "tsx" / "parse_tsx.ts"
  ]

  for path in searchPaths:
    if path != "" and fileExists(path):
      cachedParserPath = path
      return path

  # If not found, write embedded script to cache directory
  let cacheDir = getHomeDir() / ".cache" / "kryon" / "scripts"
  createDir(cacheDir)

  let scriptPath = cacheDir / "parse_tsx.ts"

  # Always write the embedded script (in case it was updated)
  writeFile(scriptPath, TsxParserScript)

  cachedParserPath = scriptPath
  return scriptPath

proc parseTsxToKir*(tsxPath: string, outputPath: string = ""): string =
  ## Parse a .tsx file and convert to .kir JSON
  ## Returns the generated .kir content
  if not fileExists(tsxPath):
    raise newException(IOError, "File not found: " & tsxPath)

  let bun = findBun()
  let parserScript = getTsxParserScript()

  # Determine output path
  let kirPath = if outputPath != "":
                  outputPath
                else:
                  tsxPath.changeFileExt(".kir")

  # Run the parser
  let (output, exitCode) = execCmdEx(bun & " " & parserScript & " " & tsxPath & " " & kirPath)

  if exitCode != 0:
    raise newException(IOError, "TSX parser failed: " & output)

  # Read and return the generated .kir content
  if fileExists(kirPath):
    return readFile(kirPath)
  else:
    raise newException(IOError, "TSX parser did not generate output file: " & kirPath)

proc parseTsxToKirJson*(tsxPath: string): JsonNode =
  ## Parse a .tsx file and return as JSON
  if not fileExists(tsxPath):
    raise newException(IOError, "File not found: " & tsxPath)

  let bun = findBun()
  let parserScript = getTsxParserScript()

  # Run the parser with stdout output
  let (output, exitCode) = execCmdEx(bun & " " & parserScript & " " & tsxPath & " -")

  if exitCode != 0:
    raise newException(IOError, "TSX parser failed: " & output)

  # Parse and return JSON
  return parseJson(output)

proc isTsxFile*(path: string): bool =
  ## Check if a file is a TSX/JSX file
  let ext = path.splitFile().ext.toLowerAscii()
  return ext in [".tsx", ".jsx"]

when isMainModule:
  import os

  if paramCount() < 1:
    echo "Usage: tsx_parser <input.tsx> [output.kir]"
    quit(1)

  let inputPath = paramStr(1)
  let outputPath = if paramCount() >= 2: paramStr(2) else: inputPath.changeFileExt(".kir")

  try:
    let kirContent = parseTsxToKir(inputPath, outputPath)
    echo "Generated: ", outputPath
  except IOError as e:
    echo "Error: ", e.msg
    quit(1)
