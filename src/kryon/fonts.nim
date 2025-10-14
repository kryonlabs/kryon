## Font Management for Kryon
##
## This module provides centralized font management for all backends.
## Ensures consistent font usage while allowing optional custom fonts.

import os, strutils, options
import core

# ============================================================================
# Font Configuration
# ============================================================================

const
  DEFAULT_FONT_SIZE* = 20
  SYSTEM_FONT_STACK = "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
  FALLBACK_FONTS* = [
    # System fonts for different platforms (used by native backends)
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/System/Library/Fonts/Arial.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "DejaVuSans.ttf",
    "arial.ttf"
  ]

type
  FontInfo* = object
    name*: string
    path*: string
    format*: string  # "ttf", "otf", "woff2"
    size*: int

  FontConfig* = object
    name*: string
    files*: seq[string]
    size*: int

var
  customFontConfig*: Option[FontConfig] = none(FontConfig)
  fontCacheInitialized = false
  cachedFontPath = ""
  fontSearchDirs*: seq[string] = @[]

proc resetFontCache() =
  fontCacheInitialized = false
  cachedFontPath = ""

proc normalizeDir(path: string): string =
  ## Normalize a directory path for consistent comparisons.
  if path.len == 0:
    return ""

  var normalized = normalizedPath(path)
  # Remove trailing separator (if any) to avoid duplicates like /foo and /foo/
  if normalized.len > 1 and normalized[^1] in {'\\', '/'}:
    normalized.setLen(normalized.len - 1)
  normalized

proc addFontSearchDir*(path: string) =
  ## Register an additional directory to search for font assets.
  let normalized = normalizeDir(path)
  if normalized.len == 0:
    return
  if normalized notin fontSearchDirs:
    fontSearchDirs.add(normalized)
    resetFontCache()

proc getFontSearchDirs*(): seq[string] =
  ## Return the list of registered font search directories.
  fontSearchDirs

proc useCustomFont*(name: string, files: openArray[string], size: int = DEFAULT_FONT_SIZE) =
  ## Configure an optional custom font that will be used across backends.
  customFontConfig = some(FontConfig(name: name, files: @files, size: size))
  resetFontCache()

proc clearCustomFont*() =
  ## Clear any configured custom font and revert to system defaults.
  customFontConfig = none(FontConfig)
  resetFontCache()

# ============================================================================
# Font Path Resolution
# ============================================================================

proc addCandidate(candidates: var seq[string], candidate: string) =
  ## Add a candidate path if it hasn't already been recorded.
  let normalized = normalizedPath(candidate)
  if normalized.len == 0:
    return
  if normalized notin candidates:
    candidates.add(normalized)

proc gatherFontCandidates(fontFile: string): seq[string] =
  ## Produce candidate file paths for a given font entry.
  
  if fontFile.isAbsolute():
    addCandidate(result, fontFile)
    return

  # Prioritized search roots: registered dirs, env override, current dir, app dir
  var searchRoots: seq[string] = @[]

  let envDir = getEnv("KRYON_APP_DIR")
  if envDir.len > 0:
    searchRoots.add(envDir)

  for dir in fontSearchDirs:
    searchRoots.add(dir)

  let currentDir = getCurrentDir()
  if currentDir.len > 0:
    searchRoots.add(currentDir)

  let appDir = getAppDir()
  if appDir.len > 0:
    searchRoots.add(appDir)

  for root in searchRoots:
    addCandidate(result, root / fontFile)

  # Finally, attempt the raw relative path in case caller already handled context
  addCandidate(result, fontFile)

proc findDefaultFont*(): string =
  ## Resolve the configured custom font file if available.
  if not fontCacheInitialized:
    fontCacheInitialized = true
    cachedFontPath = ""

    if customFontConfig.isSome:
      let cfg = customFontConfig.get
      var checkedCandidates: seq[string] = @[]

      for fontFile in cfg.files:
        for candidate in gatherFontCandidates(fontFile):
          checkedCandidates.add(candidate)

          if fileExists(candidate):
            cachedFontPath = candidate
            echo "Found custom font: " & cachedFontPath
            break

        if cachedFontPath.len > 0:
          break

      if cachedFontPath.len == 0:
        echo "Warning: Custom font configured but not found."
        echo "  Requested files: " & cfg.files.join(", ")
        if fontSearchDirs.len > 0:
          echo "  Registered font directories: " & fontSearchDirs.join(", ")
        if getEnv("KRYON_APP_DIR").len > 0:
          echo "  KRYON_APP_DIR: " & getEnv("KRYON_APP_DIR")
        if checkedCandidates.len > 0:
          echo "  Checked paths:"
          var unique: seq[string] = @[]
          for candidate in checkedCandidates:
            if candidate notin unique:
              unique.add(candidate)
          for candidate in unique:
            echo "    - " & candidate

  cachedFontPath

proc getFontFormat*(path: string): string =
  ## Get font format from file extension
  let ext = path.splitFile().ext.toLowerAscii()
  case ext:
  of ".ttf": return "ttf"
  of ".otf": return "otf"
  of ".woff2": return "woff2"
  else: return "unknown"

proc getDefaultFontInfo*(): FontInfo =
  ## Get information about the active font (custom if configured).
  if customFontConfig.isSome:
    let fontPath = findDefaultFont()
    if fontPath.len > 0:
      let cfg = customFontConfig.get
      return FontInfo(
        name: cfg.name,
        path: fontPath,
        format: getFontFormat(fontPath),
        size: cfg.size
      )

  FontInfo(
    name: "System Default",
    path: "",
    format: "system",
    size: DEFAULT_FONT_SIZE
  )

# ============================================================================
# CSS Font Generation
# ============================================================================

proc generateFontFaceCSS*(fontName: string, fontPath: string): string =
  ## Generate @font-face CSS for the given font
  if fontPath.len == 0:
    return ""

  let format = getFontFormat(fontPath)
  let fontFormat = case format:
    of "ttf": "truetype"
    of "otf": "opentype"
    of "woff2": "woff2"
    else: "truetype"

  # For HTML backend, we need to reference the font relative to the HTML file
  let relativePath = fontPath.replace(getCurrentDir() / "", "")

  result = """
@font-face {
  font-family: '""" & fontName & """';
  src: url('""" & relativePath & """') format('""" & fontFormat & """');
  font-weight: normal;
  font-style: normal;
  font-display: swap;
}
"""

# ============================================================================
# Font Utilities
# ============================================================================

proc isFontAvailable*(): bool =
  ## Check if a custom font is available
  customFontConfig.isSome and findDefaultFont().len > 0

proc getFontStack*(includeDefault: bool = true): string =
  ## Get CSS font stack with or without our default font
  if includeDefault and customFontConfig.isSome:
    let fontPath = findDefaultFont()
    if fontPath.len > 0:
      return "'" & customFontConfig.get.name & "', " & SYSTEM_FONT_STACK
  SYSTEM_FONT_STACK

proc getFontStackFor*(fontName: string): string =
  ## Build a font stack preferring the provided font name.
  if fontName.len == 0:
    return SYSTEM_FONT_STACK
  "'" & fontName & "', " & SYSTEM_FONT_STACK

# ============================================================================
# Resource-based Font Management
# ============================================================================

proc findResourceFont*(fontName: string): string =
  ## Find a font resource by name using the global app resources
  let fontResource = getFontResource(fontName)
  if fontResource.isSome:
    let resource = fontResource.get()
    for source in resource.sources:
      for candidate in gatherFontCandidates(source):
        if fileExists(candidate):
          return candidate

  ""

proc getResourceFontInfo*(fontName: string): FontInfo =
  ## Get information about a resource font by name
  let fontPath = findResourceFont(fontName)
  if fontPath.len > 0:
    return FontInfo(
      name: fontName,
      path: fontPath,
      format: getFontFormat(fontPath),
      size: DEFAULT_FONT_SIZE
    )

  FontInfo(
    name: fontName,
    path: "",
    format: "not-found",
    size: DEFAULT_FONT_SIZE
  )

proc isResourceFontAvailable*(fontName: string): bool =
  ## Check if a resource font is available
  findResourceFont(fontName).len > 0

proc generateResourceFontCSS*(): string =
  ## Generate CSS for all registered resource fonts
  result = ""
  for fontResource in appResources.fonts:
    let fontPath = findResourceFont(fontResource.name)
    if fontPath.len > 0:
      result.add(generateFontFaceCSS(fontResource.name, fontPath))
      result.add("\n")

proc findFontByName*(fontName: string): string =
  ## Find a font by name, checking resources first, then custom fonts
  # First check resource fonts
  let resourcePath = findResourceFont(fontName)
  if resourcePath.len > 0:
    return resourcePath

  # Fall back to custom font if it matches the name
  if customFontConfig.isSome and customFontConfig.get.name == fontName:
    return findDefaultFont()

  # Not found
  ""

proc getFontInfoByName*(fontName: string): FontInfo =
  ## Get font information by name, checking resources first
  let fontPath = findFontByName(fontName)
  if fontPath.len > 0:
    return FontInfo(
      name: fontName,
      path: fontPath,
      format: getFontFormat(fontPath),
      size: DEFAULT_FONT_SIZE
    )

  FontInfo(
    name: fontName,
    path: "",
    format: "not-found",
    size: DEFAULT_FONT_SIZE
  )

proc resolvePreferredFont*(root: Element): string =
  ## Resolve the preferred font family defined on the nearest Body element.
  proc findInTree(elem: Element): string =
    if elem.kind == ekBody:
      let fontProp = elem.getProp("fontFamily")
      if fontProp.isSome:
        let value = fontProp.get.getString()
        if value.len > 0:
          return value

    for child in elem.children:
      let found = findInTree(child)
      if found.len > 0:
        return found

    ""

  findInTree(root)
