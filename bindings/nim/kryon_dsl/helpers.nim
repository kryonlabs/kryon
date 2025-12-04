## Kryon DSL - Helper Functions
##
## Shared helper functions for macro expansion and color parsing

import macros, strutils, random
import ../runtime
import ../ir_core
# Removed core_kryon - using IR system now

# ============================================================================
# Style Variable and Color Helpers
# ============================================================================

proc isStyleVarRef*(value: NimNode): bool =
  ## Check if the value is a style variable reference (@varName)
  if value.kind == nnkPrefix and value.len == 2:
    let op = value[0]
    if op.kind == nnkIdent and op.strVal == "@":
      return true
  return false

proc styleVarNode*(value: NimNode): NimNode =
  ## Convert @varName to colorVar(varName) call
  ## Maps common variable names to their builtin IDs
  let varName = value[1]  # Get the identifier after @
  let varNameStr = if varName.kind == nnkIdent: varName.strVal else: ""
  # Map semantic names to builtin variable IDs
  case varNameStr.toLowerAscii()
  of "primary": result = newCall(ident("colorVar"), ident("varPrimary"))
  of "primaryhover": result = newCall(ident("colorVar"), ident("varPrimaryHover"))
  of "secondary": result = newCall(ident("colorVar"), ident("varSecondary"))
  of "accent": result = newCall(ident("colorVar"), ident("varAccent"))
  of "background", "bg": result = newCall(ident("colorVar"), ident("varBackground"))
  of "surface": result = newCall(ident("colorVar"), ident("varSurface"))
  of "surfacehover": result = newCall(ident("colorVar"), ident("varSurfaceHover"))
  of "card": result = newCall(ident("colorVar"), ident("varCard"))
  of "text": result = newCall(ident("colorVar"), ident("varText"))
  of "textmuted", "muted": result = newCall(ident("colorVar"), ident("varTextMuted"))
  of "textonprimary": result = newCall(ident("colorVar"), ident("varTextOnPrimary"))
  of "success": result = newCall(ident("colorVar"), ident("varSuccess"))
  of "warning": result = newCall(ident("colorVar"), ident("varWarning"))
  of "error": result = newCall(ident("colorVar"), ident("varError"))
  of "border": result = newCall(ident("colorVar"), ident("varBorder"))
  of "divider": result = newCall(ident("colorVar"), ident("varDivider"))
  else:
    # Pass through as identifier (custom variable)
    result = newCall(ident("colorVar"), varName)

proc colorNode*(value: NimNode): NimNode =
  # Check for style variable reference (@varName)
  if isStyleVarRef(value):
    return styleVarNode(value)
  if value.kind == nnkStrLit:
    let text = value.strVal
    if text.startsWith("#"):
      result = newCall(ident("parseHexColor"), value)
    else:
      result = newCall(ident("parseNamedColor"), value)
  elif value.kind == nnkCall:
    # Function call (like rgb(), rgba()) - return as-is, already returns uint32
    result = value
  elif value.kind == nnkBracketExpr:
    # Handle array access like alignment.colors[0]
    # Assume it returns a string that needs color parsing
    result = newCall(ident("parseHexColor"), value)
  else:
    # For other node types, try to parse as hex color
    result = newCall(ident("parseHexColor"), value)

# ============================================================================
# Dimension Helpers for DSL
# ============================================================================

# CSS-style percentage suffix
# Usage: width = 50.pct means 50% of parent width
func pct*(value: int): int = value
func pct*(value: float): float = value

# Alias: width = 50.percent
func percent*(value: int): int = value
func percent*(value: float): float = value

# CSS-style pixel suffix
# Usage: fontSize = 24.px explicitly marks value as pixels
func px*(value: int): int = value
func px*(value: float): float = value

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
  of "center", "middle": kaCenter
  of "end", "bottom", "right": kaEnd
  of "stretch": kaStretch
  of "spaceevenly": kaSpaceEvenly
  of "spacearound": kaSpaceAround
  of "spacebetween": kaSpaceBetween
  else: kaStart

# ============================================================================
# Color Parsing Utilities
# ============================================================================

proc rgb*(r, g, b: int): uint32 =
  ## Create RGB color from components (0-255) with full opacity
  result = (uint32(r) shl 24) or (uint32(g) shl 16) or (uint32(b) shl 8) or 255'u32

proc rgba*(r, g, b, a: int): uint32 =
  ## Create RGBA color from components (0-255)
  result = (uint32(r) shl 24) or (uint32(g) shl 16) or (uint32(b) shl 8) or uint32(a)

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

# ============================================================================
# Typography Helpers
# ============================================================================

proc parseTextDecoration*(value: NimNode): NimNode =
  ## Parse text decoration value into bitfield constant
  ## Supports:
  ## - String literals: "underline", "line-through", "overline", "none"
  ## - Arrays: ["underline", "line-through"]
  ## Returns a NimNode representing the bitfield value

  if value.kind == nnkStrLit:
    # Single decoration string
    let decorStr = value.strVal.toLowerAscii().strip()
    case decorStr
    of "underline":
      return ident("IR_TEXT_DECORATION_UNDERLINE")
    of "line-through", "linethrough":
      return ident("IR_TEXT_DECORATION_LINE_THROUGH")
    of "overline":
      return ident("IR_TEXT_DECORATION_OVERLINE")
    of "none", "":
      return ident("IR_TEXT_DECORATION_NONE")
    else:
      error("Invalid text decoration: " & decorStr & ". Valid values: underline, line-through, overline, none")
      return ident("IR_TEXT_DECORATION_NONE")

  elif value.kind == nnkBracket:
    # Array of decorations - combine with bitwise OR
    if value.len == 0:
      return ident("IR_TEXT_DECORATION_NONE")

    var result = nnkInfix.newTree(
      ident("or"),
      parseTextDecoration(value[0]),
      ident("IR_TEXT_DECORATION_NONE")
    )

    for i in 1..<value.len:
      result = nnkInfix.newTree(
        ident("or"),
        result,
        parseTextDecoration(value[i])
      )

    return result

  else:
    # For other node types (identifiers, etc.), return as-is
    return value

proc parseTextAlign*(value: NimNode): NimNode =
  ## Parse text alignment value into IRTextAlign enum
  ## Supports: "left", "right", "center", "justify"

  if value.kind == nnkStrLit:
    let alignStr = value.strVal.toLowerAscii().strip()
    case alignStr
    of "left":
      return ident("IR_TEXT_ALIGN_LEFT")
    of "right":
      return ident("IR_TEXT_ALIGN_RIGHT")
    of "center":
      return ident("IR_TEXT_ALIGN_CENTER")
    of "justify":
      return ident("IR_TEXT_ALIGN_JUSTIFY")
    else:
      error("Invalid text alignment: " & alignStr & ". Valid values: left, right, center, justify")
      return ident("IR_TEXT_ALIGN_LEFT")
  else:
    # Return as-is for enum values
    return value

proc parseFontWeight*(value: NimNode): NimNode =
  ## Parse font weight value into uint16 (100-900 range)
  ## Supports:
  ## - String literals: "bold" (700), "normal" (400), "light" (300), etc.
  ## - Numeric strings: "600" -> 600
  ## - Numeric values: 700 -> 700 (pass through)
  ## Returns a NimNode representing the uint16 weight value

  if value.kind == nnkStrLit:
    # String literal - convert name to weight value
    let weightStr = value.strVal.toLowerAscii().strip()
    case weightStr
    of "thin", "hairline":
      return newLit(100)
    of "extralight", "extra-light", "ultralight", "ultra-light":
      return newLit(200)
    of "light":
      return newLit(300)
    of "normal", "regular":
      return newLit(400)
    of "medium":
      return newLit(500)
    of "semibold", "semi-bold", "demibold", "demi-bold":
      return newLit(600)
    of "bold":
      return newLit(700)
    of "extrabold", "extra-bold", "ultrabold", "ultra-bold":
      return newLit(800)
    of "black", "heavy":
      return newLit(900)
    else:
      # Try parsing as numeric string
      try:
        let weight = parseInt(weightStr)
        if weight >= 100 and weight <= 900:
          return newLit(weight)
        else:
          error("Font weight must be between 100 and 900, got: " & weightStr)
          return newLit(400)
      except ValueError:
        error("Invalid font weight: " & weightStr & ". Valid: thin, extralight, light, normal, medium, semibold, bold, extrabold, black, or 100-900")
        return newLit(400)

  elif value.kind in {nnkIntLit, nnkInt8Lit, nnkInt16Lit, nnkInt32Lit, nnkInt64Lit,
                       nnkUIntLit, nnkUInt8Lit, nnkUInt16Lit, nnkUInt32Lit, nnkUInt64Lit}:
    # Numeric literal - validate range
    let weight = value.intVal.int
    if weight >= 100 and weight <= 900:
      return value
    else:
      error("Font weight must be between 100 and 900, got: " & $weight)
      return newLit(400)

  else:
    # For other node types (identifiers, expressions) - pass through
    # This allows runtime values like variables
    return value

# ============================================================================
# Grid Layout Helpers
# ============================================================================

proc parseGridTrack*(trackStr: string): NimNode =
  ## Parse a single grid track value into an IRGridTrack constructor call
  ## Supports: "1fr", "100px", "25%", "auto", "min-content", "max-content"
  let track = trackStr.strip().toLowerAscii()

  if track == "auto":
    return newCall(ident("ir_grid_track_auto"))
  elif track == "min-content":
    return newCall(ident("ir_grid_track_min_content"))
  elif track == "max-content":
    return newCall(ident("ir_grid_track_max_content"))
  elif track.endsWith("fr"):
    # Fractional unit: "1fr", "2.5fr"
    let valueStr = track[0 ..< track.len - 2].strip()
    let value = try: parseFloat(valueStr) except: 1.0
    return newCall(ident("ir_grid_track_fr"), newLit(value))
  elif track.endsWith("px"):
    # Pixels: "100px"
    let valueStr = track[0 ..< track.len - 2].strip()
    let value = try: parseFloat(valueStr) except: 100.0
    return newCall(ident("ir_grid_track_px"), newLit(value))
  elif track.endsWith("%"):
    # Percentage: "25%"
    let valueStr = track[0 ..< track.len - 1].strip()
    let value = try: parseFloat(valueStr) except: 100.0
    return newCall(ident("ir_grid_track_percent"), newLit(value))
  else:
    # Try to parse as a number (assume pixels)
    let value = try: parseFloat(track) except: 100.0
    return newCall(ident("ir_grid_track_px"), newLit(value))

proc parseGridTemplate*(value: NimNode): NimNode =
  ## Parse grid template string into an array of IRGridTrack objects
  ## Example: "1fr 2fr auto" -> @[ir_grid_track_fr(1), ir_grid_track_fr(2), ir_grid_track_auto()]

  if value.kind == nnkStrLit:
    let templateStr = value.strVal
    let tracks = templateStr.split(' ')
    var trackArray = nnkBracket.newTree()

    for track in tracks:
      if track.strip().len > 0:
        trackArray.add(parseGridTrack(track))

    return prefix(trackArray, "@")
  else:
    # Return as-is for non-string values
    return value

proc parseGridLine*(value: NimNode): tuple[start: NimNode, endVal: NimNode] =
  ## Parse grid line value into start and end positions
  ## Supports: "1 / 3" (string), (1, 3) (tuple), or single int (auto-span)

  if value.kind == nnkStrLit:
    # Parse "start / end" format
    let lineStr = value.strVal
    if lineStr.contains('/'):
      let parts = lineStr.split('/')
      let start = try: parseInt(parts[0].strip()) except: 1
      let endVal = try: parseInt(parts[1].strip()) except: -1  # -1 = auto
      return (newLit(int16(start)), newLit(int16(endVal)))
    else:
      # Single value - use as start, auto-span 1
      let start = try: parseInt(lineStr.strip()) except: 1
      return (newLit(int16(start)), newLit(int16(-1)))

  elif value.kind == nnkTupleConstr or value.kind == nnkPar:
    # Tuple format (start, end)
    if value.len >= 2:
      return (value[0], value[1])
    else:
      return (value[0], newLit(int16(-1)))

  elif value.kind == nnkIntLit:
    # Single int - use as start, auto-span 1
    return (value, newLit(int16(-1)))

  else:
    # Unknown format - return defaults
    return (newLit(int16(1)), newLit(int16(-1)))

# ============================================================================
# Gradient Helpers (Phase 5)
# ============================================================================

type GradientSpec* = object
  ## Parsed gradient specification
  gradientType*: NimNode  # IR_GRADIENT_LINEAR, IR_GRADIENT_RADIAL, or IR_GRADIENT_CONIC
  angle*: NimNode         # For linear gradients
  centerX*: NimNode       # For radial/conic gradients
  centerY*: NimNode       # For radial/conic gradients
  stops*: seq[tuple[position: NimNode, color: NimNode]]

proc parseGradientSpec*(value: NimNode): GradientSpec =
  ## Parse gradient specification from DSL
  ## Supports tuple format: (linear, 45, [(0.0, "red"), (1.0, "blue")])
  ## Or object construction syntax

  result = GradientSpec()
  result.gradientType = ident("IR_GRADIENT_LINEAR")
  result.angle = newLit(0.0)
  result.centerX = newLit(0.5)
  result.centerY = newLit(0.5)
  result.stops = @[]

  if value.kind == nnkTupleConstr or value.kind == nnkPar:
    if value.len < 3:
      error("Gradient spec requires at least 3 elements: (type, angle/center, stops)")
      return

    # Parse gradient type (first element)
    let typeNode = value[0]
    if typeNode.kind == nnkStrLit:
      let typeStr = typeNode.strVal.toLowerAscii()
      case typeStr
      of "linear":
        result.gradientType = ident("IR_GRADIENT_LINEAR")
        # Second element is angle
        result.angle = value[1]
      of "radial":
        result.gradientType = ident("IR_GRADIENT_RADIAL")
        # Second element is center (can be tuple or single value)
        if value[1].kind == nnkTupleConstr or value[1].kind == nnkPar:
          result.centerX = value[1][0]
          result.centerY = if value[1].len > 1: value[1][1] else: value[1][0]
        else:
          result.centerX = value[1]
          result.centerY = value[1]
      of "conic":
        result.gradientType = ident("IR_GRADIENT_CONIC")
        # Second element is center (can be tuple or single value)
        if value[1].kind == nnkTupleConstr or value[1].kind == nnkPar:
          result.centerX = value[1][0]
          result.centerY = if value[1].len > 1: value[1][1] else: value[1][0]
        else:
          result.centerX = value[1]
          result.centerY = value[1]
      else:
        error("Invalid gradient type: " & typeStr & ". Use 'linear', 'radial', or 'conic'")
    elif typeNode.kind == nnkIdent:
      # Direct enum value
      result.gradientType = typeNode

    # Parse color stops (third element should be an array)
    if value.len >= 3:
      let stopsNode = value[2]
      if stopsNode.kind == nnkBracket:
        for stopNode in stopsNode:
          if stopNode.kind == nnkTupleConstr or stopNode.kind == nnkPar:
            if stopNode.len >= 2:
              let position = stopNode[0]
              let color = stopNode[1]
              result.stops.add((position, color))
          else:
            error("Each gradient stop must be a tuple: (position, color)")

proc buildGradientCreation*(spec: GradientSpec, gradientVar: NimNode): NimNode =
  ## Build code to create and configure a gradient
  var stmts = newStmtList()

  # Create the gradient
  if spec.gradientType.repr.contains("LINEAR"):
    stmts.add quote do:
      let `gradientVar` = kryon_create_linear_gradient(cfloat(`spec.angle`))
  elif spec.gradientType.repr.contains("RADIAL"):
    stmts.add quote do:
      let `gradientVar` = kryon_create_radial_gradient(cfloat(`spec.centerX`), cfloat(`spec.centerY`))
  elif spec.gradientType.repr.contains("CONIC"):
    stmts.add quote do:
      let `gradientVar` = kryon_create_conic_gradient(cfloat(`spec.centerX`), cfloat(`spec.centerY`))
  else:
    stmts.add quote do:
      let `gradientVar` = kryon_create_linear_gradient(0.0)

  # Add color stops
  for stop in spec.stops:
    let position = stop.position
    let color = colorNode(stop.color)
    stmts.add quote do:
      kryon_gradient_add_color_stop(`gradientVar`, cfloat(`position`), `color`)

  return stmts

# ============================================================================
# BoxShadow Parsing (Named Parameter Syntax)
# ============================================================================

type
  BoxShadowSpec* = object
    offsetX*: float
    offsetY*: float
    blurRadius*: float
    spreadRadius*: float
    color*: string
    inset*: bool

proc parseBoxShadowSpec*(node: NimNode): BoxShadowSpec =
  ## Parse boxShadow from named tuple syntax
  ## Example: (offsetY: 8.0, blur: 24.0, color: "#000")
  ## Supports short names: x, y, blur, spread
  result = BoxShadowSpec(
    offsetX: 0.0,
    offsetY: 0.0,
    blurRadius: 0.0,
    spreadRadius: 0.0,
    color: "#00000080",
    inset: false
  )

  # Parse named parameters from tuple or table constructor
  if node.kind == nnkTableConstr or node.kind == nnkTupleConstr or node.kind == nnkPar:
    for pair in node:
      if pair.kind == nnkExprColonExpr:
        let key = $pair[0]
        let value = pair[1]

        case key:
        of "offsetX", "x":
          result.offsetX = value.floatVal
        of "offsetY", "y":
          result.offsetY = value.floatVal
        of "blur", "blurRadius":
          result.blurRadius = value.floatVal
        of "spread", "spreadRadius":
          result.spreadRadius = value.floatVal
        of "color":
          result.color = value.strVal
        of "inset":
          result.inset = value.boolVal
        else:
          error("Unknown boxShadow parameter: " & key, pair)

proc buildBoxShadowCreation*(component: NimNode, spec: BoxShadowSpec): NimNode =
  ## Generate C function call from BoxShadowSpec
  let colorNode = newCall(ident("parseHexColor"), newLit(spec.color))
  let offsetX = newLit(spec.offsetX)
  let offsetY = newLit(spec.offsetY)
  let blur = newLit(spec.blurRadius)
  let spread = newLit(spec.spreadRadius)
  let inset = newLit(spec.inset)

  result = quote do:
    kryon_component_set_box_shadow(
      `component`,
      cfloat(`offsetX`),
      cfloat(`offsetY`),
      cfloat(`blur`),
      cfloat(`spread`),
      `colorNode`,
      `inset`
    )
