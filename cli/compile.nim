## Kryon IR Compilation Pipeline
##
## Advanced compilation with caching, validation, and inspection

import os, osproc, strutils, times, hashes, json, tables
import ../bindings/nim/ir_core
import ../bindings/nim/ir_serialization

type
  CompileFormat* = enum
    FormatBinary,  # .kirb - efficient binary format
    FormatJSON     # .kir - human-readable JSON format

  CompileOptions* = object
    sourceFile*: string
    outputFile*: string
    frontend*: string  # "nim", "lua", "c"
    format*: CompileFormat  # Output format
    enableCache*: bool
    enableValidation*: bool
    enableOptimization*: bool
    verboseOutput*: bool
    includeManifest*: bool  # Include reactive manifest in output

  CompilationResult* = object
    success*: bool
    irFile*: string
    errors*: seq[string]
    warnings*: seq[string]
    componentCount*: int
    manifestVars*: int
    compileTime*: float
    cacheHit*: bool

  CacheEntry = object
    sourceHash: string
    irFilePath: string
    timestamp: Time
    componentCount: int

# Cache management
const CACHE_DIR = ".kryon_cache"
const CACHE_INDEX = ".kryon_cache/index.json"

var compilationCache: Table[string, CacheEntry]

proc initCache*() =
  ## Initialize the compilation cache
  createDir(CACHE_DIR)

  if fileExists(CACHE_INDEX):
    try:
      let indexData = readFile(CACHE_INDEX)
      let jsonIndex = parseJson(indexData)

      for key, val in jsonIndex.pairs:
        compilationCache[key] = CacheEntry(
          sourceHash: val["hash"].getStr(),
          irFilePath: val["irFile"].getStr(),
          timestamp: fromUnix(val["timestamp"].getInt()),
          componentCount: val["componentCount"].getInt()
        )
    except:
      echo "[cache] Warning: Failed to load cache index, starting fresh"
      compilationCache = initTable[string, CacheEntry]()
  else:
    compilationCache = initTable[string, CacheEntry]()

proc saveCache*() =
  ## Save cache index to disk
  var jsonIndex = newJObject()

  for key, entry in compilationCache.pairs:
    jsonIndex[key] = %* {
      "hash": entry.sourceHash,
      "irFile": entry.irFilePath,
      "timestamp": entry.timestamp.toUnix(),
      "componentCount": entry.componentCount
    }

  writeFile(CACHE_INDEX, $jsonIndex)

proc computeFileHash(path: string): string =
  ## Compute hash of source file for cache invalidation
  if not fileExists(path):
    return ""

  let content = readFile(path)
  result = $hash(content)

proc getCachedIR*(sourceFile: string): tuple[found: bool, irPath: string] =
  ## Check if compiled IR exists in cache and is up-to-date
  let sourceHash = computeFileHash(sourceFile)
  let cacheKey = absolutePath(sourceFile)

  if compilationCache.hasKey(cacheKey):
    let entry = compilationCache[cacheKey]

    # Validate cache entry
    if entry.sourceHash == sourceHash and fileExists(entry.irFilePath):
      let irModTime = getLastModificationTime(entry.irFilePath)
      let sourceModTime = getLastModificationTime(sourceFile)

      if irModTime >= sourceModTime:
        return (true, entry.irFilePath)

  return (false, "")

proc addToCache*(sourceFile: string, irFile: string, componentCount: int) =
  ## Add compilation result to cache
  let sourceHash = computeFileHash(sourceFile)
  let cacheKey = absolutePath(sourceFile)

  compilationCache[cacheKey] = CacheEntry(
    sourceHash: sourceHash,
    irFilePath: absolutePath(irFile),
    timestamp: getTime(),
    componentCount: componentCount
  )

  saveCache()

proc detectFrontend*(sourceFile: string): string =
  ## Detect frontend language from file extension
  let ext = splitFile(sourceFile).ext.toLowerAscii()

  case ext
  of ".nim": return "nim"
  of ".lua": return "lua"
  of ".c", ".cpp", ".cc": return "c"
  else: return "unknown"

proc compileNimToIR*(sourceFile: string, outputFile: string, format: CompileFormat, verbose: bool): CompilationResult =
  ## Compile Nim source to Kryon IR
  result.success = false
  result.errors = @[]
  result.warnings = @[]
  result.irFile = outputFile

  let startTime = cpuTime()

  if verbose:
    echo "[compile] Compiling Nim to IR: ", sourceFile
    echo "[compile] Output format: ", if format == FormatBinary: "binary (.kirb)" else: "JSON (.kir)"

  # Get project root directory (where the CLI is located)
  let projectRoot = getCurrentDir()

  # Build temporary executable path
  let tempExe = ".kryon_cache" / "temp_compile_" & sourceFile.extractFilename().changeFileExt("")

  # Build Nim compiler command with proper paths
  var compileCmd = "nim c"
  compileCmd &= " --path:bindings/nim"
  compileCmd &= " --passC:\"-Iir\""
  compileCmd &= " --passC:\"-I" & projectRoot & "\""
  compileCmd &= " --passC:\"-DKRYON_TARGET_PLATFORM=0\""
  compileCmd &= " --passC:\"-DKRYON_NO_FLOAT=0\""
  compileCmd &= " --passL:\"-Lbuild -lkryon_ir -lkryon_desktop -lSDL3 -lSDL3_ttf -lm\""
  compileCmd &= " --threads:on --mm:arc"
  compileCmd &= " --nimcache:.kryon_cache/nimcache"
  compileCmd &= " --out:" & tempExe
  compileCmd &= " " & sourceFile

  if verbose:
    echo "[compile] Running: ", compileCmd

  let (compileOutput, compileExit) = execCmdEx(compileCmd)

  if compileExit != 0:
    result.errors.add("Nim compilation failed")
    result.errors.add(compileOutput)
    result.compileTime = cpuTime() - startTime
    return

  if verbose:
    echo "[compile] Executable built successfully"
    echo "[compile] Running with KRYON_SERIALIZE_IR=", outputFile

  # Run the executable with KRYON_SERIALIZE_IR set
  let runCmd = "LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH KRYON_SERIALIZE_IR=" & outputFile & " " & tempExe
  let (runOutput, runExit) = execCmdEx(runCmd)

  if verbose:
    echo "[compile] Run output:"
    echo runOutput

  if runExit != 0:
    result.errors.add("IR serialization failed")
    result.errors.add(runOutput)
    result.compileTime = cpuTime() - startTime
    return

  # Check if the IR file was created
  if not fileExists(outputFile):
    result.errors.add("IR file was not created: " & outputFile)
    result.compileTime = cpuTime() - startTime
    return

  # Clean up temporary executable
  try:
    removeFile(tempExe)
  except:
    discard

  result.success = true
  result.componentCount = 0  # TODO: Parse from output
  result.compileTime = cpuTime() - startTime

  if verbose:
    echo "[compile] IR compilation completed in ", result.compileTime, "s"

proc validateIR*(irFile: string): tuple[valid: bool, errors: seq[string]] =
  ## Validate IR file format and structure
  result.valid = false
  result.errors = @[]

  if not fileExists(irFile):
    result.errors.add("IR file not found: " & irFile)
    return

  # Use IR validation functions
  let buffer = ir_buffer_create_from_file(cstring(irFile))
  if buffer == nil:
    result.errors.add("Failed to read IR file")
    return

  # Validate format
  if not ir_validate_binary_format(buffer):
    result.errors.add("Invalid IR binary format")
    ir_buffer_destroy(buffer)
    return

  # Deserialize to validate structure
  let root = ir_deserialize_binary(buffer)
  ir_buffer_destroy(buffer)

  if root == nil:
    result.errors.add("Failed to deserialize IR - corrupt or invalid structure")
    return

  # TODO: Add semantic validation
  # - Check for orphaned components
  # - Validate parent-child relationships
  # - Check for circular dependencies
  # - Validate style/layout constraints

  ir_destroy_component(root)
  result.valid = true

proc inspectIR*(irFile: string): JsonNode =
  ## Inspect IR file and return metadata
  result = newJObject()

  if not fileExists(irFile):
    result["error"] = %"IR file not found"
    return

  # Detect format by extension
  let (_, _, ext) = splitFile(irFile)
  let isJson = ext == ".kir"  # .kir is JSON, .kirb is binary

  var root: ptr IRComponent = nil

  if isJson:
    # JSON format - just load the component tree
    root = ir_read_json_v2_file(cstring(irFile))
  else:
    # Binary format
    let buffer = ir_buffer_create_from_file(cstring(irFile))
    if buffer == nil:
      result["error"] = %"Failed to read IR file"
      return
    root = ir_deserialize_binary(buffer)
    ir_buffer_destroy(buffer)

  if root == nil:
    result["error"] = %"Failed to deserialize IR"
    return

  # Count components recursively
  proc countComponents(comp: ptr IRComponent): int =
    result = 1
    if comp.child_count > 0:
      let childArray = cast[ptr UncheckedArray[ptr IRComponent]](comp.children)
      for i in 0..<comp.child_count:
        result += countComponents(childArray[i])

  let componentCount = countComponents(root)

  result["componentCount"] = %componentCount
  result["rootType"] = %($root.`type`)

  ir_destroy_component(root)

proc compile*(opts: CompileOptions): CompilationResult =
  ## Main compilation entry point with caching
  result.success = false
  result.errors = @[]
  result.warnings = @[]
  result.cacheHit = false

  # Validate source file exists
  if not fileExists(opts.sourceFile):
    result.errors.add("Source file not found: " & opts.sourceFile)
    return

  # Check cache if enabled
  if opts.enableCache:
    initCache()
    let (found, cachedPath) = getCachedIR(opts.sourceFile)

    if found:
      if opts.verboseOutput:
        echo "[compile] Cache hit: ", cachedPath

      result.success = true
      result.irFile = cachedPath
      result.cacheHit = true
      result.compileTime = 0.0
      result.componentCount = 0  # Component count not tracked in cache

      return

  # Detect frontend if not specified
  var frontend = opts.frontend
  if frontend.len == 0:
    frontend = detectFrontend(opts.sourceFile)

  if opts.verboseOutput:
    echo "[compile] Frontend: ", frontend
    echo "[compile] Source: ", opts.sourceFile
    echo "[compile] Output: ", opts.outputFile

  # Compile based on frontend
  case frontend
  of "nim":
    result = compileNimToIR(opts.sourceFile, opts.outputFile, opts.format, opts.verboseOutput)
  of "lua":
    result.errors.add("Lua frontend not yet implemented")
    return
  of "c":
    result.errors.add("C frontend not yet implemented")
    return
  else:
    result.errors.add("Unknown frontend: " & frontend)
    return

  if not result.success:
    return

  # Validate if requested
  if opts.enableValidation:
    if opts.verboseOutput:
      echo "[compile] Validating IR..."

    let (valid, errors) = validateIR(result.irFile)
    if not valid:
      result.success = false
      result.errors.add("IR validation failed:")
      result.errors.add(errors)
      return

  # Add to cache if enabled and compilation succeeded
  if opts.enableCache and result.success:
    addToCache(opts.sourceFile, result.irFile, result.componentCount)
    if opts.verboseOutput:
      echo "[compile] Added to cache"

proc clearCache*() =
  ## Clear compilation cache
  if dirExists(CACHE_DIR):
    removeDir(CACHE_DIR)
    echo "[cache] Cache cleared"
  else:
    echo "[cache] No cache to clear"

# CLI command handlers
proc handleCompileCommand*(args: seq[string]) =
  ## Handle 'kryon compile' command
  if args.len == 0:
    echo "Error: Source file required"
    echo "Usage: kryon compile <source> [--output=<file>] [--format=binary|json] [--no-cache] [--validate]"
    return

  var opts = CompileOptions(
    sourceFile: args[0],
    outputFile: "",
    frontend: "",
    format: FormatJSON,  # Default to JSON (.kir) format
    enableCache: true,
    enableValidation: false,
    enableOptimization: true,
    verboseOutput: false,
    includeManifest: false
  )

  # Parse options
  for i in 1..<args.len:
    let arg = args[i]
    if arg.startsWith("--output="):
      opts.outputFile = arg[9..^1]
    elif arg.startsWith("--format="):
      let formatStr = arg[9..^1].toLowerAscii()
      case formatStr
      of "binary", "kirb":
        opts.format = FormatBinary
      of "json", "kir":
        opts.format = FormatJSON
      else:
        echo "Error: Unknown format '", formatStr, "'. Use 'binary' or 'json'."
        return
    elif arg == "--no-cache":
      opts.enableCache = false
    elif arg == "--validate":
      opts.enableValidation = true
    elif arg == "--verbose" or arg == "-v":
      opts.verboseOutput = true
    elif arg == "--with-manifest":
      opts.includeManifest = true

  # Default output file based on format
  if opts.outputFile.len == 0:
    # Create build/ir directory if it doesn't exist
    createDir("build/ir")

    let (dir, name, ext) = splitFile(opts.sourceFile)
    let extension = if opts.format == FormatBinary: ".kirb" else: ".kir"
    opts.outputFile = "build/ir" / name & extension

  # Compile
  echo "🔨 Compiling ", opts.sourceFile
  let result = compile(opts)

  if result.success:
    echo "✅ Compilation successful"
    if result.cacheHit:
      echo "   📦 Cache hit"
    else:
      echo "   ⏱️  ", result.compileTime.formatFloat(ffDecimal, 3), "s"
    echo "   📄 Output: ", result.irFile
    echo "   🧩 Components: ", result.componentCount

    if result.manifestVars > 0:
      echo "   📊 Reactive vars: ", result.manifestVars
  else:
    echo "❌ Compilation failed"
    for err in result.errors:
      echo "   ", err

proc handleConvertCommand*(args: seq[string]) =
  ## Handle 'kryon convert' command - Convert .kir (JSON) to .kirb (binary)
  if args.len < 2:
    echo "Error: Input and output files required"
    echo "Usage: kryon convert <input.kir> <output.kirb>"
    echo ""
    echo "Converts Kryon IR from JSON format (.kir) to binary format (.kirb)"
    echo "Binary format is more efficient for backends to load."
    return

  let inputFile = args[0]
  let outputFile = args[1]

  # Check input file exists
  if not fileExists(inputFile):
    echo "❌ Input file not found: ", inputFile
    quit(1)

  # Check input is .kir format
  if not (inputFile.endsWith(".kir") or inputFile.endsWith(".json")):
    echo "⚠️  Warning: Input file should be .kir (JSON format)"

  # Check output is .kirb format
  if not outputFile.endsWith(".kirb"):
    echo "⚠️  Warning: Output file should be .kirb (binary format)"

  echo "🔄 Converting ", inputFile, " → ", outputFile

  # Load JSON IR
  let startTime = cpuTime()
  let component = ir_read_json_v2_file(cstring(inputFile))
  if component == nil:
    echo "❌ Failed to read JSON IR file"
    quit(1)

  # Write binary IR
  if not ir_serialization.ir_write_binary_file(component, cstring(outputFile)):
    echo "❌ Failed to write binary IR file"
    # Cleanup component
    ir_destroy_component(component)
    quit(1)

  let elapsed = cpuTime() - startTime

  # Get file sizes for comparison
  let inputSize = getFileSize(inputFile)
  let outputSize = getFileSize(outputFile)
  let ratio = (1.0 - (outputSize.float / inputSize.float)) * 100.0

  echo "✅ Conversion successful"
  echo "   ⏱️  ", elapsed.formatFloat(ffDecimal, 3), "s"
  echo "   📄 Input:  ", inputFile, " (", formatSize(inputSize), ")"
  echo "   📄 Output: ", outputFile, " (", formatSize(outputSize), ")"
  echo "   📦 Size reduction: ", ratio.formatFloat(ffDecimal, 1), "%"

  # Cleanup
  ir_destroy_component(component)

proc handleValidateCommand*(args: seq[string]) =
  ## Handle 'kryon validate' command
  if args.len == 0:
    echo "Error: IR file required"
    echo "Usage: kryon validate <file.kirb|file.kir>"
    return

  let irFile = args[0]
  echo "🔍 Validating ", irFile

  let (valid, errors) = validateIR(irFile)

  if valid:
    echo "✅ IR file is valid"

    # Show inspection data
    let metadata = inspectIR(irFile)
    echo "   🧩 Components: ", metadata["componentCount"].getInt()
    if metadata.hasKey("manifest"):
      let manifest = metadata["manifest"]
      echo "   📊 Reactive state:"
      echo "      Variables: ", manifest["variables"].getInt()
      echo "      Bindings: ", manifest["bindings"].getInt()
  else:
    echo "❌ IR file is invalid"
    for err in errors:
      echo "   ", err

proc handleInspectIRCommand*(args: seq[string]) =
  ## Handle 'kryon inspect-ir' command
  if args.len == 0:
    echo "Error: IR file required"
    echo "Usage: kryon inspect-ir <file.kirb|file.kir> [--json]"
    return

  let irFile = args[0]
  let jsonOutput = args.len > 1 and args[1] == "--json"

  let metadata = inspectIR(irFile)

  if jsonOutput:
    echo pretty(metadata)
  else:
    echo "📊 IR File: ", irFile
    echo "─────────────────────────────────────"

    if metadata.hasKey("error"):
      echo "❌ Error: ", metadata["error"].getStr()
      return

    echo "Components: ", metadata["componentCount"].getInt()
    echo "Root type: ", metadata["rootType"].getStr()
    echo "Has manifest: ", metadata["hasManifest"].getBool()

    if metadata.hasKey("manifest"):
      echo ""
      echo "Reactive State:"
      let manifest = metadata["manifest"]
      echo "  Variables: ", manifest["variables"].getInt()
      echo "  Bindings: ", manifest["bindings"].getInt()
      echo "  Conditionals: ", manifest["conditionals"].getInt()
      echo "  For-loops: ", manifest["forLoops"].getInt()
