## Kryon DSL - Helper Functions
##
## Shared helper functions for macro expansion and color parsing

import macros, strutils, random
import ../runtime, ../core_kryon

# ============================================================================
# Style System Infrastructure  
# ============================================================================

# Helper to generate unique style function names
proc genStyleFuncName*(styleName: string): string =
  "styleFunc_" & styleName & "_" & $rand(1000..9999)

# ============================================================================
# Type Aliases for Custom Components
# ============================================================================

type Element* = KryonComponent
  ## Type alias for custom component return values.
  ## Use this as the return type for procedures that compose UI components.
  ## Example:
  ##   proc MyCustomComponent(): Element =
  ##     result = Container:
  ##       Button: ...

# ============================================================================
# Runtime Helpers
# ============================================================================

proc parseAlignmentString*(name: string): KryonAlignment =
  ## Runtime function to parse alignment strings
  let normalized = name.toLowerAscii()
  case normalized
  of "center", "middle": KryonAlignment.kaCenter
  of "end", "bottom", "right": KryonAlignment.kaEnd
  of "stretch": KryonAlignment.kaStretch
  of "spaceevenly": KryonAlignment.kaSpaceEvenly
  of "spacearound": KryonAlignment.kaSpaceAround
  of "spacebetween": KryonAlignment.kaSpaceBetween
  else: KryonAlignment.kaStart

# ============================================================================
# Color Parsing Utilities
# ============================================================================

proc parseHexColor*(hex: string): uint32 =
  ## Parse hex color string like "#ff0000" or "#ff0000ff"
  if hex.len == 7 and hex.startsWith("#"):
    let r = parseHexInt(hex[1..2])
    let g = parseHexInt(hex[3..4])
    let b = parseHexInt(hex[5..6])
    result = rgba(r, g, b, 255)
  elif hex.len == 9 and hex.startsWith("#"):
    let r = parseHexInt(hex[1..2])
    let g = parseHexInt(hex[3..4])
    let b = parseHexInt(hex[5..6])
    let a = parseHexInt(hex[7..8])
    result = rgba(r, g, b, a)
  else:
    result = 0xFF000000'u32  # Default to black

proc parseNamedColor*(name: string): uint32 =
  ## Parse named colors - supports common CSS color names
  case name.toLowerAscii():
  # Primary colors
  of "red": rgba(255, 0, 0, 255)
  of "green": rgba(0, 255, 0, 255)
  of "blue": rgba(0, 0, 255, 255)
  of "yellow": rgba(255, 255, 0, 255)
  
  # Grayscale
  of "white": rgba(255, 255, 255, 255)
  of "black": rgba(0, 0, 0, 255)
  of "gray", "grey": rgba(128, 128, 128, 255)
  of "lightgray", "lightgrey": rgba(211, 211, 211, 255)
  of "darkgray", "darkgrey": rgba(169, 169, 169, 255)
  of "silver": rgba(192, 192, 192, 255)
  
  # Common colors
  of "orange": rgba(255, 165, 0, 255)
  of "purple": rgba(128, 0, 128, 255)
  of "pink": rgba(255, 192, 203, 255)
  of "brown": rgba(165, 42, 42, 255)
  of "cyan": rgba(0, 255, 255, 255)
  of "magenta": rgba(255, 0, 255, 255)
  of "lime": rgba(0, 255, 0, 255)
  of "olive": rgba(128, 128, 0, 255)
  of "navy": rgba(0, 0, 128, 255)
  of "teal": rgba(0, 128, 128, 255)
  of "aqua": rgba(0, 255, 255, 255)
  of "maroon": rgba(128, 0, 0, 255)
  of "fuchsia": rgba(255, 0, 255, 255)
  
  # Light variations
  of "lightblue": rgba(173, 216, 230, 255)
  of "lightgreen": rgba(144, 238, 144, 255)
  of "lightpink": rgba(255, 182, 193, 255)
  of "lightyellow": rgba(255, 255, 224, 255)
  of "lightcyan": rgba(224, 255, 255, 255)
  
  # Dark variations
  of "darkred": rgba(139, 0, 0, 255)
  of "darkgreen": rgba(0, 100, 0, 255)
  of "darkblue": rgba(0, 0, 139, 255)
  of "darkorange": rgba(255, 140, 0, 255)
  of "darkviolet": rgba(148, 0, 211, 255)
  
  # Transparent
  of "transparent": rgba(0, 0, 0, 0)
  
  else: rgba(128, 128, 128, 255)  # Default to gray
