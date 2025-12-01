## Property Parsing and Value Detection Utilities
##
## This module contains helper functions for parsing CSS-like values,
## detecting special syntax (.pct, .px), and handling alignment enums.

import macros, strutils, sequtils
import ../ir_core  # For KryonAlignment enum constants

# ============================================================================
# Percent and Pixel Expression Detection
# ============================================================================

proc isPercentExpression*(value: NimNode): bool =
  ## Check if the value is a percent expression like `50.pct`
  ## Returns true for DotExpr with ident "pct"
  if value.kind == nnkDotExpr and value.len == 2:
    let suffix = value[1]
    if suffix.kind == nnkIdent and suffix.strVal == "pct":
      return true
  # Also handle call syntax: pct(50)
  if value.kind == nnkCall and value.len >= 1:
    let fn = value[0]
    if fn.kind == nnkIdent and fn.strVal == "pct":
      return true
  return false

proc extractPercentValue*(value: NimNode): NimNode =
  ## Extract the numeric value from a percent expression
  ## For `50.pct`, returns 50
  if value.kind == nnkDotExpr and value.len == 2:
    return value[0]  # The numeric part before .pct
  elif value.kind == nnkCall and value.len >= 2:
    return value[1]  # The argument to pct()
  return value

proc isPxExpression*(value: NimNode): bool =
  ## Check if the value is a px expression like `24.px`
  ## Returns true for DotExpr with ident "px"
  if value.kind == nnkDotExpr and value.len == 2:
    let suffix = value[1]
    if suffix.kind == nnkIdent and suffix.strVal == "px":
      return true
  # Also handle call syntax: px(24)
  if value.kind == nnkCall and value.len >= 1:
    let fn = value[0]
    if fn.kind == nnkIdent and fn.strVal == "px":
      return true
  return false

proc extractPxValue*(value: NimNode): NimNode =
  ## Extract the numeric value from a px expression
  ## For `24.px`, returns 24
  if value.kind == nnkDotExpr and value.len == 2:
    return value[0]  # The numeric part before .px
  elif value.kind == nnkCall and value.len >= 2:
    return value[1]  # The argument to px()
  return value

# ============================================================================
# Unit String Parsing
# ============================================================================

proc parseUnitString*(value: NimNode): tuple[numericValue: NimNode, isPercent: bool, isPx: bool] =
  ## Parse strings like "100%", "10px", "150" and extract value + unit
  ## Returns (value, isPercent, isPx)

  if value.kind == nnkStrLit:
    let str = value.strVal.strip()

    # Check for percentage: "100%"
    if str.endsWith("%"):
      let numStr = str[0 .. ^2].strip()  # Remove '%'
      return (newLit(parseFloat(numStr)), true, false)

    # Check for pixels: "10px"
    elif str.endsWith("px"):
      let numStr = str[0 .. ^3].strip()  # Remove 'px'
      return (newLit(parseFloat(numStr)), false, true)

    # Plain number as string: "150"
    else:
      return (newLit(parseFloat(str)), false, false)

  # Not a string literal - return as-is
  return (value, false, false)

proc parseQuadString*(value: NimNode): tuple[top, right, bottom, left: NimNode] =
  ## Parse strings like "20px, 20px, 20px, 20px" or "10px, 20px"
  ## Supports 1-4 values (CSS-style)

  if value.kind != nnkStrLit:
    return (value, value, value, value)  # Not a string, use as-is for all sides

  let str = value.strVal.strip()
  let parts = str.split(',').mapIt(it.strip())

  var values: seq[NimNode]
  for part in parts:
    let parsed = parseUnitString(newStrLitNode(part))
    values.add(parsed.numericValue)

  # CSS-style value expansion
  case values.len:
  of 1:  # All sides same
    return (values[0], values[0], values[0], values[0])
  of 2:  # top/bottom, left/right
    return (values[0], values[1], values[0], values[1])
  of 3:  # top, left/right, bottom
    return (values[0], values[1], values[2], values[1])
  of 4:  # top, right, bottom, left
    return (values[0], values[1], values[2], values[3])
  else:
    return (values[0], values[0], values[0], values[0])

# ============================================================================
# Constant Expression Evaluation
# ============================================================================

proc evalConstExpr*(node: NimNode): NimNode =
  ## Pass const expressions through as-is
  ## Within static blocks, const field access works correctly
  ## This is a placeholder for potential future enhancements
  return node

# ============================================================================
# Animation String Parsing
# ============================================================================

proc parseAnimationString*(spec: string): NimNode =
  ## Parse animation spec string and return the call node
  ## Examples: "pulse(2.0, -1)" -> pulse(2.0, -1)
  ##           "fadeInOut(3.0)" -> fadeInOut(3.0, 1)
  ##           "slideInLeft(2.0)" -> slideInLeft(2.0)

  # Parse function name and args from "funcName(arg1, arg2)"
  let openParen = spec.find('(')
  let closeParen = spec.find(')')

  if openParen == -1 or closeParen == -1:
    error("Invalid animation spec: " & spec & ". Expected format: 'funcName(args)'")

  let funcName = spec[0..<openParen].strip()
  let argsStr = spec[openParen+1..<closeParen].strip()

  # Split arguments
  var args: seq[NimNode]
  if argsStr.len > 0:
    for arg in argsStr.split(','):
      let trimmed = arg.strip()
      # Parse as float or int
      if trimmed.contains('.') or trimmed.contains('e') or trimmed.contains('E'):
        args.add(newLit(parseFloat(trimmed)))
      else:
        args.add(newLit(parseInt(trimmed)))

  # Generate call based on animation type
  case funcName.toLowerAscii()
  of "pulse":
    if args.len == 1:
      result = newCall(ident("pulse"), args[0], newLit(-1))
    elif args.len == 2:
      result = newCall(ident("pulse"), args[0], args[1])
    else:
      error("pulse() expects 1-2 args: duration, [iterations]. Got: " & spec)

  of "fadeinout", "fade_in_out":
    if args.len == 1:
      result = newCall(ident("fadeInOut"), args[0], newLit(1))
    elif args.len == 2:
      result = newCall(ident("fadeInOut"), args[0], args[1])
    else:
      error("fadeInOut() expects 1-2 args: duration, [iterations]. Got: " & spec)

  of "slideinleft", "slide_in_left":
    if args.len == 1:
      result = newCall(ident("slideInLeft"), args[0])
    else:
      error("slideInLeft() expects 1 arg: duration. Got: " & spec)

  else:
    error("Unknown animation: " & funcName & ". Available: pulse, fadeInOut, slideInLeft")

# ============================================================================
# Alignment Helpers
# ============================================================================

proc alignmentNode*(name: string): NimNode =
  ## Convert alignment string to corresponding Kryon alignment enum value
  let normalized = name.toLowerAscii()
  case normalized
  of "center", "middle": result = bindSym("IR_ALIGNMENT_CENTER")
  of "end", "bottom", "right": result = bindSym("IR_ALIGNMENT_END")
  of "stretch": result = bindSym("IR_ALIGNMENT_STRETCH")
  of "spaceevenly": result = bindSym("IR_ALIGNMENT_SPACE_EVENLY")
  of "spacearound": result = bindSym("IR_ALIGNMENT_SPACE_AROUND")
  of "spacebetween": result = bindSym("IR_ALIGNMENT_SPACE_BETWEEN")
  else: result = bindSym("IR_ALIGNMENT_START")

proc parseAlignmentValue*(value: NimNode): NimNode =
  ## Parse alignment value and return KryonAlignment enum value for C API
  if value.kind == nnkStrLit:
    # Convert string to enum value at compile time using alignmentNode
    result = alignmentNode(value.strVal)
  else:
    # Runtime parsing for non-literal values
    result = newCall(ident("parseAlignmentString"), value)

# ============================================================================
# Detection Utilities
# ============================================================================

proc isEchoStatement*(node: NimNode): bool =
  ## Detect Nim echo statements (command or call form)
  if node.len == 0:
    return false
  let head = node[0]
  if head.kind == nnkIdent and head.strVal == "echo":
    if node.kind == nnkCommand or node.kind == nnkCall:
      return true
  result = false

proc isStaticCollection*(node: NimNode): bool =
  ## Check if a for-loop collection is static (can be unrolled at compile time)
  ## Static collections: array literals, seq literals, range literals
  case node.kind
  of nnkBracket:
    # Array literal: [1, 2, 3]
    return true
  of nnkPrefix:
    # Seq literal: @[1, 2, 3]
    if node.len == 2 and node[0].eqIdent("@") and node[1].kind == nnkBracket:
      return true
  of nnkInfix:
    # Range: 0..6 or 0..<7
    if node[0].eqIdent("..") or node[0].eqIdent("..<"):
      return true
  else:
    discard
  return false
