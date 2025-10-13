## Font Management for Kryon
##
## This module provides centralized font management for all backends.
## Ensures consistent font usage across SDL2, Raylib, and HTML backends.

import os, strutils

# ============================================================================
# Font Configuration
# ============================================================================

const
  DEFAULT_FONT_NAME* = "Fira Sans"
  DEFAULT_FONT_FILES* = [
    "assets/FiraSans-Regular.ttf",
    "assets/FiraSans-Regular.otf",
    "assets/FiraSans-Regular.woff2"
  ]
  FALLBACK_FONTS* = [
    # System fonts for different platforms
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

# ============================================================================
# Font Path Resolution
# ============================================================================

proc findDefaultFont*(): string =
  ## Find the default font file, trying all available formats
  let basePath = getCurrentDir() / ""

  for fontFile in DEFAULT_FONT_FILES:
    let fullPath = basePath / fontFile
    if fileExists(fullPath):
      echo "Found default font: " & fullPath
      return fullPath

  echo "Warning: Default font not found, trying fallback fonts"
  for fontFile in FALLBACK_FONTS:
    if fileExists(fontFile):
      echo "Using fallback font: " & fontFile
      return fontFile

  echo "Error: No font files found, text rendering may not work"
  return ""

proc getFontFormat*(path: string): string =
  ## Get font format from file extension
  let ext = path.splitFile().ext.toLowerAscii()
  case ext:
  of ".ttf": return "ttf"
  of ".otf": return "otf"
  of ".woff2": return "woff2"
  else: return "unknown"

proc getDefaultFontInfo*(): FontInfo =
  ## Get information about the default font
  let fontPath = findDefaultFont()
  if fontPath.len > 0:
    FontInfo(
      name: DEFAULT_FONT_NAME,
      path: fontPath,
      format: getFontFormat(fontPath),
      size: 20  # Default font size
    )
  else:
    FontInfo(
      name: "System Default",
      path: "",
      format: "system",
      size: 20
    )

# ============================================================================
# CSS Font Generation
# ============================================================================

proc generateFontFaceCSS*(fontPath: string): string =
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
  font-family: '""" & DEFAULT_FONT_NAME & """';
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
  ## Check if the default font is available
  let fontPath = findDefaultFont()
  return fontPath.len > 0

proc getFontStack*(includeDefault: bool = true): string =
  ## Get CSS font stack with or without our default font
  if includeDefault and isFontAvailable():
    return "'" & DEFAULT_FONT_NAME & "', system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
  else:
    return "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"