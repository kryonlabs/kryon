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
  ## Convert a property value node to a Value (wraps dynamic expressions in getters)
  case node.kind:
  of nnkIntLit:
    # Static integer literal
    result = quote do: val(`node`)
  of nnkFloatLit:
    # Static float literal
    result = quote do: val(`node`)
  of nnkStrLit:
    # Static string literal (check if it's a color)
    let strVal = node.strVal
    if strVal.startsWith("#"):
      result = quote do: val(parseColor(`node`))
    else:
      result = quote do: val(`node`)
  of nnkIdent:
    # Could be true/false (static) or a variable reference (dynamic)
    let identStr = node.strVal
    if identStr == "true":
      result = quote do: val(true)
    elif identStr == "false":
      result = quote do: val(false)
    else:
      # Variable reference - wrap in getter for reactivity!
      result = quote do: valGetter(proc(): Value = val(`node`))
  of nnkInfix, nnkPrefix, nnkCall, nnkDotExpr:
    # Complex expression - wrap in getter for reactivity!
    result = quote do: valGetter(proc(): Value = val(`node`))
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
        # This might be a function call that returns Element (custom component)
        # Check if it's a function call like Counter(0)
        if stmt.len > 1 or (stmt.len == 1 and stmt[0].kind == nnkIdent):
          # Treat as a function call that returns Element
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

    of nnkExprColonExpr:
      # Colon syntax: width: 800 (same as width = 800)
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

    of nnkIfStmt:
      # Handle if statements for conditional rendering
      # Convert if statement to a conditional element
      let ifStmt = stmt

      # Extract condition and branches
      var conditionExpr: NimNode = nil
      var trueBranchContent: NimNode = nil
      var falseBranchContent: NimNode = nil

      # Process the if statement branches
      for branch in ifStmt:
        if branch.kind == nnkElifBranch:
          # elif branch - take the condition and body
          conditionExpr = branch[0]
          trueBranchContent = branch[1]
          # For simplicity, we'll treat elif as if for now
          break
        elif branch.kind == nnkElse:
          # else branch
          falseBranchContent = branch[0]
        elif branch.kind == nnkElifExpr:
          # This shouldn't happen in a properly formed if statement
          discard
        else:
          # Regular if branch
          conditionExpr = branch[0]
          trueBranchContent = branch[1]

      if conditionExpr != nil and trueBranchContent != nil:
        # Wrap the condition in a getter function for reactivity
        let conditionGetter = quote do:
          proc(): bool = `conditionExpr`

        # Create a Container element for the true branch
        # We'll manually process the true branch content by calling processElementBody
        let trueContainerCode = processElementBody(ekContainer, trueBranchContent)

        # Same for false branch if it exists
        var falseContainerCode: NimNode
        if falseBranchContent != nil:
          falseContainerCode = processElementBody(ekContainer, falseBranchContent)
        else:
          falseContainerCode = quote do:
            nil

        # Create the conditional element and add it as a child
        result.add quote do:
          `elemVar`.addChild(newConditionalElement(`conditionGetter`, `trueContainerCode`, `falseContainerCode`))

        # Debug: Print that we found an if statement
        echo "=== DSL: Found if statement, creating conditional element ==="

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

macro Header*(body: untyped): Element =
  ## Create a Header element (window metadata)
  result = processElementBody(ekHeader, body)

macro Body*(body: untyped): Element =
  ## Create a Body element (UI content wrapper)
  result = processElementBody(ekBody, body)

# ============================================================================
# Application macro
# ============================================================================

macro kryonApp*(body: untyped): Element =
  ## Main application macro with smart Header/Body detection
  ##
  ## Examples:
  ## ```nim
  ## # Full syntax:
  ## kryonApp:
  ##   Header:
  ##     windowWidth = 800
  ##     windowTitle = "My App"
  ##   Body:
  ##     Container:
  ##       Text:
  ##         text = "Hello"
  ##
  ## # Header optional (uses defaults):
  ## kryonApp:
  ##   Body:
  ##     Container:
  ##       Text:
  ##         text = "Hello"
  ##
  ## # Body optional (auto-wraps children):
  ## kryonApp:
  ##   Header:
  ##     windowTitle = "My App"
  ##   Container:  # Auto-wrapped in Body
  ##     Text:
  ##       text = "Hello"
  ##
  ## # Both optional (minimal):
  ## kryonApp:
  ##   Container:  # Auto-wrapped in Body, default Header created
  ##     Text:
  ##       text = "Hello"
  ## ```

  result = newStmtList()

  var headerNode: NimNode = nil
  var bodyChildren = newSeq[NimNode]()

  # Parse body to find Header and/or Body elements
  for stmt in body:
    if stmt.kind == nnkCall:
      let name = stmt[0].strVal
      if name == "Header":
        headerNode = stmt
      elif name == "Body":
        # Body found - collect its children
        if stmt.len > 1 and stmt[1].kind == nnkStmtList:
          for child in stmt[1]:
            bodyChildren.add(child)
      else:
        # Regular UI element - add to body children
        bodyChildren.add(stmt)
    else:
      # Other statement - add to body children
      bodyChildren.add(stmt)

  # Create default Header if not provided
  if headerNode == nil:
    headerNode = quote do:
      Header:
        windowWidth = 800
        windowHeight = 600
        windowTitle = "Kryon App"

  # Wrap body children in Body if needed
  var bodyNode: NimNode
  if bodyChildren.len > 0:
    let bodyStmtList = newStmtList()
    for child in bodyChildren:
      bodyStmtList.add(child)
    # Manually construct nnkCall for Body with proper children
    bodyNode = nnkCall.newTree(
      newIdentNode("Body"),
      bodyStmtList
    )
  else:
    # No body children - create empty Body
    bodyNode = quote do:
      Body:
        Container:
          width = 800
          height = 600

  # Create app structure with Header and Body
  let appVar = genSym(nskVar, "app")
  result.add quote do:
    var `appVar` = newElement(ekBody)
    `appVar`.addChild(`headerNode`)
    `appVar`.addChild(`bodyNode`)
    `appVar`

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
