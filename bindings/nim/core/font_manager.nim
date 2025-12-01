# 0BSD
## Font Manager - Font Loading and Resolution
## Phase 1, Week 2: Font Management
##
## This module handles:
## - Font loading and caching with hash tables
## - Font fallback chains
## - System font discovery
## - Font metrics calculation
##
## Design Goals:
## - Fast font lookup with hash tables (O(1))
## - Automatic fallback to system fonts
## - Font preloading and caching
## - Memory-efficient font storage

import ../ir_core
import tables, os, strutils

type
  FontCacheKey = tuple[family: string, size: float]

  FontFallbackChain = seq[string]  # Ordered list of font families to try

  FontManager* = ref object
    ## Central font management system
    initialized*: bool
    fontCache: Table[FontCacheKey, ptr IRShapingFont]  # Hash-based font cache
    fontRegistry: Table[string, string]  # Family name -> file path
    fallbackChains: Table[string, FontFallbackChain]  # Family -> fallback list
    defaultFontFamily: string
    preloadedFonts: seq[string]  # List of preloaded font families

# ============================================================================
# Font Manager Creation and Initialization
# ============================================================================

proc createFontManager*(): FontManager =
  ## Creates a new font manager with default configuration
  result = FontManager(
    initialized: false,
    fontCache: initTable[FontCacheKey, ptr IRShapingFont](),
    fontRegistry: initTable[string, string](),
    fallbackChains: initTable[string, FontFallbackChain](),
    defaultFontFamily: "sans-serif",
    preloadedFonts: @[]
  )

  # Register default fallback chains
  result.fallbackChains["sans-serif"] = @["Inter", "DejaVu Sans", "Liberation Sans", "Arial", "Helvetica"]
  result.fallbackChains["serif"] = @["DejaVu Serif", "Liberation Serif", "Times New Roman", "Georgia"]
  result.fallbackChains["monospace"] = @["Fira Code", "DejaVu Sans Mono", "Liberation Mono", "Courier New"]

  result.initialized = true

# ============================================================================
# Font Registration and Discovery
# ============================================================================

proc registerFont*(manager: FontManager, family: string, path: string) =
  ## Registers a font family with its file path
  if not manager.initialized:
    return

  manager.fontRegistry[family] = path

proc discoverSystemFonts*(manager: FontManager) =
  ## Discovers and registers common system fonts
  ## This is a simplified version - production would use fontconfig/similar
  if not manager.initialized:
    return

  const commonFontPaths = [
    # Linux paths
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    # macOS paths
    "/System/Library/Fonts/Arial.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    # Windows paths (if on Windows)
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/times.ttf"
  ]

  for path in commonFontPaths:
    if fileExists(path):
      # Extract family name from filename
      let basename = splitFile(path).name
      let family = basename.replace("-Regular", "").replace("-", " ")
      manager.registerFont(family, path)

# ============================================================================
# Font Loading and Caching
# ============================================================================

proc loadFont*(manager: FontManager,
              fontPath: string,
              size: float): ptr IRShapingFont =
  ## Loads a font from file path (uses cache if available)
  if not manager.initialized or not fileExists(fontPath):
    return nil

  # Check if text shaping is available
  if not ir_text_shaping_available():
    return nil

  # Try to load font using IR text shaping API
  result = ir_font_load(cstring(fontPath), cfloat(size))

proc getCachedFont*(manager: FontManager,
                   family: string,
                   size: float): ptr IRShapingFont =
  ## Gets a font from cache or loads it
  if not manager.initialized:
    return nil

  let key: FontCacheKey = (family, size)

  # Check cache first (O(1) hash lookup)
  if key in manager.fontCache:
    return manager.fontCache[key]

  # Not in cache - try to load it
  if family in manager.fontRegistry:
    let path = manager.fontRegistry[family]
    let font = manager.loadFont(path, size)
    if not font.isNil:
      manager.fontCache[key] = font
      return font

  return nil

# ============================================================================
# Font Resolution with Fallback Chains
# ============================================================================

proc resolveFont*(manager: FontManager,
                 fontFamily: string,
                 size: float): ptr IRShapingFont =
  ## Resolves a font family to a loaded font with fallback support
  if not manager.initialized:
    return nil

  # Try requested family first
  var font = manager.getCachedFont(fontFamily, size)
  if not font.isNil:
    return font

  # Try fallback chain
  if fontFamily in manager.fallbackChains:
    for fallback in manager.fallbackChains[fontFamily]:
      font = manager.getCachedFont(fallback, size)
      if not font.isNil:
        return font

  # Try default fallback chain
  if manager.defaultFontFamily != fontFamily and
     manager.defaultFontFamily in manager.fallbackChains:
    for fallback in manager.fallbackChains[manager.defaultFontFamily]:
      font = manager.getCachedFont(fallback, size)
      if not font.isNil:
        return font

  return nil

# ============================================================================
# Font Preloading
# ============================================================================

proc preloadFonts*(manager: FontManager, fontFamilies: seq[string], sizes: seq[float] = @[12.0, 14.0, 16.0]) =
  ## Preloads commonly used fonts for faster first render
  if not manager.initialized:
    return

  for family in fontFamilies:
    for size in sizes:
      discard manager.resolveFont(family, size)

  manager.preloadedFonts = fontFamilies

# ============================================================================
# Cleanup
# ============================================================================

proc cleanupFonts*(manager: FontManager) =
  ## Cleans up all loaded fonts
  if not manager.initialized:
    return

  # Destroy all cached fonts
  for key, font in manager.fontCache:
    if not font.isNil:
      ir_font_destroy(font)

  manager.fontCache.clear()
  manager.preloadedFonts = @[]
  manager.initialized = false
