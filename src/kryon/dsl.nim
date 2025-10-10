## Kryon DSL - Declarative UI macro system
##
## This module provides the macros that enable writing declarative UI in Nim syntax.
## Matches the original Kryon syntax using `=` for properties.
##
## Example:
## ```nim
## Container:
##   width = 200
##   height = 100
##   backgroundColor = "#191970FF"
##
##   Text:
##     text = "Hello World"
##     color = "yellow"
## ```

import macros, strutils
import core

# ============================================================================
# Helper procs for macro processing
# ============================================================================

proc parsePropertyValue(node: NimNode): NimNode =
  ## Convert a property value node to a Value
  case node.kind:
  of nnkIntLit:
    result = quote do: val(`node`)
  of nnkFloatLit:
    result = quote do: val(`node`)
  of nnkStrLit:
    # Check if it's a color
    let strVal = node.strVal
    if strVal.startsWith("#"):
      result = quote do: val(parseColor(`node`))
    else:
      result = quote do: val(`node`)
  of nnkIdent:
    # Could be true/false or a variable reference
    let identStr = node.strVal
    if identStr == "true":
      result = quote do: val(true)
    elif identStr == "false":
      result = quote do: val(false)
    else:
      # Variable reference - evaluate at runtime
      result = quote do: val(`node`)
  of nnkInfix, nnkPrefix, nnkCall, nnkDotExpr:
    # Complex expression - evaluate at runtime
    result = quote do: val(`node`)
  else:
    # Default: try to convert to Value
    result = quote do: val(`node`)

proc processElementBody(kind: ElementKind, body: NimNode): NimNode =
  ## Process the body of an element definition
  ## Returns code that creates the element with properties and children

  result = newStmtList()

  let elemVar = genSym(nskVar, "elem")
  let kindLit = newLit(kind)

  # Create element
  result.add quote do:
    var `elemVar` = newElement(`kindLit`)

  # Process body statements
  for stmt in body:
    case stmt.kind:
    of nnkExprEqExpr:
      # Property or event handler assignment: width = 200 or onClick = handler
      let propName = stmt[0].strVal

      # Check if this is an event handler (starts with "on")
      if propName.startsWith("on"):
        # Event handler: onClick = myHandler
        let handler = stmt[1]
        result.add quote do:
          `elemVar`.setEventHandler(`propName`, `handler`)
      else:
        # Regular property
        let propValue = parsePropertyValue(stmt[1])
        result.add quote do:
          `elemVar`.setProp(`propName`, `propValue`)

    of nnkCall:
      # This is either an event handler or child element (Text: ...)
      let name = stmt[0]
      let nameStr = name.strVal

      # Check if this looks like an event handler
      if nameStr.startsWith("on"):
        # Event handler: onClick = myHandler or onClick: <body>
        if stmt.len > 1 and stmt[1].kind == nnkStmtList:
          let handler = stmt[1]
          result.add quote do:
            `elemVar`.setEventHandler(`nameStr`, `handler`)
        elif stmt.len > 1:
          let handler = stmt[1]
          result.add quote do:
            `elemVar`.setEventHandler(`nameStr`, `handler`)
      elif stmt.len > 1 and stmt[1].kind == nnkStmtList:
        # Child element with body: Text: <body>
        result.add quote do:
          `elemVar`.addChild(`stmt`)
      else:
        # Unknown - skip
        discard

    of nnkCommand:
      # Command syntax: might be a child element
      if stmt.len >= 2:
        let childStmt = stmt
        result.add quote do:
          `elemVar`.addChild(`childStmt`)

    of nnkAsgn:
      # Also handle nnkAsgn (rare, but valid)
      let propName = stmt[0].strVal

      # Check if this is an event handler
      if propName.startsWith("on"):
        let handler = stmt[1]
        result.add quote do:
          `elemVar`.setEventHandler(`propName`, `handler`)
      else:
        let propValue = parsePropertyValue(stmt[1])
        result.add quote do:
          `elemVar`.setProp(`propName`, `propValue`)

    else:
      # Skip other node types
      discard

  # Return the element
  result.add quote do:
    `elemVar`

# ============================================================================
# Element macros
# ============================================================================

macro Container*(body: untyped): Element =
  ## Create a Container element
  result = processElementBody(ekContainer, body)

macro Text*(body: untyped): Element =
  ## Create a Text element
  result = processElementBody(ekText, body)

macro Button*(body: untyped): Element =
  ## Create a Button element
  result = processElementBody(ekButton, body)

macro Column*(body: untyped): Element =
  ## Create a Column layout element
  result = processElementBody(ekColumn, body)

macro Row*(body: untyped): Element =
  ## Create a Row layout element
  result = processElementBody(ekRow, body)

macro Input*(body: untyped): Element =
  ## Create an Input element
  result = processElementBody(ekInput, body)

macro Checkbox*(body: untyped): Element =
  ## Create a Checkbox element
  result = processElementBody(ekCheckbox, body)

macro Dropdown*(body: untyped): Element =
  ## Create a Dropdown element
  result = processElementBody(ekDropdown, body)

macro Grid*(body: untyped): Element =
  ## Create a Grid layout element
  result = processElementBody(ekGrid, body)

macro Image*(body: untyped): Element =
  ## Create an Image element
  result = processElementBody(ekImage, body)

macro Center*(body: untyped): Element =
  ## Create a Center layout element
  result = processElementBody(ekCenter, body)

macro ScrollView*(body: untyped): Element =
  ## Create a ScrollView element
  result = processElementBody(ekScrollView, body)

# ============================================================================
# Application macro
# ============================================================================

macro kryonApp*(body: untyped): Element =
  ## Main application macro
  ##
  ## Example:
  ## ```nim
  ## let app = kryonApp:
  ##   Container:
  ##     Text:
  ##       text: "Hello"
  ## ```

  # The body should be a single root element
  result = body

# ============================================================================
# Component helpers
# ============================================================================

template component*(definition: untyped): untyped =
  ## Define a reusable component
  ##
  ## Example:
  ## ```nim
  ## component:
  ##   proc MyButton(text: string, onClick: EventHandler): Element =
  ##     Button:
  ##       text: text
  ##       onClick: onClick
  ##       backgroundColor: "#4A90E2"
  ## ```
  definition

# ============================================================================
# Debugging
# ============================================================================

when isMainModule:
  # Test the DSL
  let ui = Container:
    width = 200
    height = 100
    backgroundColor = "#191970FF"

    Text:
      text = "Hello World"
      color = "yellow"

  echo "Created UI element:"
  ui.printTree()
