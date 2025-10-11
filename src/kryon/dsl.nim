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

import macros, strutils, options
import core

# ============================================================================
# Helper procs for macro processing
# ============================================================================

proc resolvePropertyAlias*(propName: string): string =
  ## Resolve property aliases to canonical names
  ## Add new aliases here without touching other code
  case propName:
  of "background": "backgroundColor"
  # Future aliases can be added here:
  # of "color": "textColor"
  # of "size": "fontSize"
  else: propName

proc parsePropertyValue(node: NimNode): tuple[value: NimNode, boundVar: Option[NimNode]] =
  ## Convert a property value node to a Value (wraps dynamic expressions in getters)
  ## Returns both the value code and optionally the bound variable name
  case node.kind:
  of nnkIntLit:
    # Static integer literal
    result.value = quote do: val(`node`)
    result.boundVar = none(NimNode)
  of nnkFloatLit:
    # Static float literal
    result.value = quote do: val(`node`)
    result.boundVar = none(NimNode)
  of nnkStrLit:
    # Static string literal (check if it's a color)
    let strVal = node.strVal
    if strVal.startsWith("#"):
      result.value = quote do: val(parseColor(`node`))
    else:
      result.value = quote do: val(`node`)
    result.boundVar = none(NimNode)
  of nnkIdent:
    # Could be true/false (static) or a variable reference (dynamic)
    let identStr = node.strVal
    if identStr == "true":
      result.value = quote do: val(true)
      result.boundVar = none(NimNode)
    elif identStr == "false":
      result.value = quote do: val(false)
      result.boundVar = none(NimNode)
    else:
      # Variable reference - wrap in getter for reactivity!
      let varName = node.strVal
      result.value = quote do: valGetter(proc(): Value =
        # Register dependency using variable name as identifier
        registerDependency(`varName`)
        val(`node`)
      )
      result.boundVar = some(node)  # Return the variable name for binding
  of nnkCall:
    # Check if this is a UI element creation call
    if node.len > 0:
      let nodeName = node[0]
      if nodeName.kind == nnkIdent:
        let nameStr = nodeName.strVal
        if nameStr in ["Container", "Text", "Button", "Column", "Row", "Input", "Checkbox",
                      "Dropdown", "Grid", "Image", "Center", "ScrollView", "Header", "Body"]:
          # This is a UI element creation call, return it directly
          result.value = node
          result.boundVar = none(NimNode)
          return
    # Complex expression - wrap in getter for reactivity!
    result.value = quote do: valGetter(proc(): Value = val(`node`))
    result.boundVar = none(NimNode)
  of nnkInfix, nnkPrefix, nnkDotExpr:
    # Complex expression - wrap in getter for reactivity!
    result.value = quote do: valGetter(proc(): Value = val(`node`))
    result.boundVar = none(NimNode)
  else:
    # Default: try to convert to Value
    result.value = quote do: val(`node`)
    result.boundVar = none(NimNode)

proc processElementBody(kind: ElementKind, body: NimNode): NimNode =
  ## Process the body of an element definition
  ## Returns code that creates the element with properties and children

  result = newStmtList()

  let elemVar = genSym(nskVar, "elem")
  let kindLit = newLit(kind)

  # Debug: print the kind
  when defined(debugDropdown):
    result.add quote do:
      echo "Debug: Creating element with kind = ", `kindLit`

  # Debug: print all statements in body
  when defined(debugDropdown):
    echo "Debug: Processing body with ", body.len, " statements"
    for i, stmt in body:
      echo "Debug: Statement ", i, " kind: ", stmt.kind, " repr: ", stmt.repr

  # Create element
  result.add quote do:
    var `elemVar` = newElement(`kindLit`)

  # Process body statements
  for stmt in body:
    case stmt.kind:
    of nnkExprEqExpr:
      # Property or event handler assignment: width = 200 or onClick = handler
      when defined(debugDropdown):
        echo "Debug: nnkExprEqExpr: stmt[0] = ", stmt[0].repr, " kind = ", stmt[0].kind
      let propName = stmt[0].strVal
      when defined(debugDropdown):
        echo "Debug: nnkExprEqExpr: propName = ", propName

      # Check if this is an event handler (starts with "on")
      if propName.startsWith("on"):
        # Event handler: onClick = myHandler
        let handler = stmt[1]
        result.add quote do:
          `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), proc(data: string = "") {.closure.} =
            `handler`())
      else:
        # Handle dropdown-specific properties
        if kind == ekDropdown:
          if propName == "options":
            # Handle options array for dropdown
            if stmt[1].kind == nnkBracket:
              # Direct array syntax: options = ["Red", "Green", "Blue"]
              var optionsCode = newNimNode(nnkBracket)
              for child in stmt[1]:
                optionsCode.add(child)
              result.add quote do:
                `elemVar`.dropdownOptions = `optionsCode`
            elif stmt[1].kind == nnkPrefix and stmt[1][0].strVal == "@":
              # Array constructor syntax: options = @["Red", "Green", "Blue"]
              let optionsValue = stmt[1]
              result.add quote do:
                `elemVar`.dropdownOptions = `optionsValue`
          elif propName == "selectedIndex":
            # Handle selectedIndex for dropdown
            when defined(debugDropdown):
              echo "Debug: Handling selectedIndex, stmt[1] = ", stmt[1].repr, " kind = ", stmt[1].kind
            let indexValue = stmt[1]
            result.add quote do:
              `elemVar`.dropdownSelectedIndex = `indexValue`
          else:
            # Regular property
            let parseResult = parsePropertyValue(stmt[1])
            let propValue = parseResult.value
            result.add quote do:
              `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)
        else:
          # Regular property
          let parseResult = parsePropertyValue(stmt[1])
          let propValue = parseResult.value
          result.add quote do:
            `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)

          # If this is an Input element and we have a bound variable, store it and create setter
          if kind == ekInput and parseResult.boundVar.isSome:
            let varNode = parseResult.boundVar.get()
            let varName = varNode.strVal
            result.add quote do:
              `elemVar`.setBoundVarName(`varName`)
              # Create a setter function for two-way binding that invalidates dependent elements
              `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
                `varNode` = data
                # Invalidate all elements that depend on this variable
                invalidateReactiveValue(`varName`)
              )

    of nnkCall:
      # This could be:
      # 1. Property assignment with colon syntax: selectedIndex: 0
      # 2. Event handler with colon syntax: onClick: <body>
      # 3. Child element with body: Text: <body>
      # 4. Function call that returns Element (custom component)
      let name = stmt[0]
      let nameStr = name.strVal

      # Check if this is a UI element creation call
      if nameStr in ["Container", "Text", "Button", "Column", "Row", "Input", "Checkbox",
                     "Dropdown", "Grid", "Image", "Center", "ScrollView", "Header", "Body"]:
        result.add quote do:
          `elemVar`.addChild(`stmt`)
      # Check if this looks like an event handler
      elif nameStr.startsWith("on"):
        # Event handler: onClick = myHandler or onClick: <body>
        if stmt.len > 1 and stmt[1].kind == nnkStmtList:
          let handler = stmt[1]
          result.add quote do:
            `elemVar`.setEventHandler(resolvePropertyAlias(`nameStr`), `handler`)
        elif stmt.len > 1:
          let handler = stmt[1]
          result.add quote do:
            `elemVar`.setEventHandler(resolvePropertyAlias(`nameStr`), proc(data: string = "") {.closure.} =
              `handler`())
      elif stmt.len > 1 and stmt[1].kind == nnkStmtList:
        # Check if this is a property assignment (colon syntax)
        # Property: selectedIndex: 0 -> StmtList contains just the value
        let stmtList = stmt[1]
        if stmtList.len == 1:
          # This looks like a property assignment with colon syntax
          let propValue = stmtList[0]

          # Handle dropdown-specific properties
          if kind == ekDropdown:
            if nameStr == "options":
              # Handle options array for dropdown
              if propValue.kind == nnkBracket:
                # Direct array syntax: options: ["Red", "Green", "Blue"]
                var optionsCode = newNimNode(nnkBracket)
                for child in propValue:
                  optionsCode.add(child)
                result.add quote do:
                  `elemVar`.dropdownOptions = `optionsCode`
              elif propValue.kind == nnkPrefix and propValue[0].strVal == "@":
                # Array constructor syntax: options: @["Red", "Green", "Blue"]
                result.add quote do:
                  `elemVar`.dropdownOptions = `propValue`
              else:
                # Regular property
                let parseResult = parsePropertyValue(propValue)
                let propValueNode = parseResult.value
                result.add quote do:
                  `elemVar`.setProp(resolvePropertyAlias(`nameStr`), `propValueNode`)
            elif nameStr == "selectedIndex":
              # Handle selectedIndex for dropdown
              when defined(debugDropdown):
                echo "Debug: Handling selectedIndex property, propValue = ", propValue.repr, " kind = ", propValue.kind
              let indexValue = propValue
              result.add quote do:
                `elemVar`.dropdownSelectedIndex = `indexValue`
            else:
              # Regular property
              let parseResult = parsePropertyValue(propValue)
              let propValueNode = parseResult.value
              result.add quote do:
                `elemVar`.setProp(resolvePropertyAlias(`nameStr`), `propValueNode`)

              # If this is an Input element and we have a bound variable, store it and create setter
              if kind == ekInput and parseResult.boundVar.isSome:
                let varNode = parseResult.boundVar.get()
                let varName = varNode.strVal
                result.add quote do:
                  `elemVar`.setBoundVarName(`varName`)
                  # Create a setter function for two-way binding that invalidates dependent elements
                  `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
                    `varNode` = data
                    # Invalidate all elements that depend on this variable
                    invalidateReactiveValue(`varName`)
                  )
        else:
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
      when defined(debugDropdown):
        echo "Debug: nnkAsgn: stmt[0] = ", stmt[0].repr, " kind = ", stmt[0].kind
      let propName = stmt[0].strVal
      when defined(debugDropdown):
        echo "Debug: nnkAsgn: propName = ", propName

      # Check if this is an event handler
      if propName.startsWith("on"):
        let handler = stmt[1]
        result.add quote do:
          `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), proc(data: string = "") {.closure.} =
            `handler`())
      else:
        # Handle dropdown-specific properties
        if kind == ekDropdown:
          if propName == "options":
            # Handle options array for dropdown
            if stmt[1].kind == nnkBracket:
              # Direct array syntax: options = ["Red", "Green", "Blue"]
              var optionsCode = newNimNode(nnkBracket)
              for child in stmt[1]:
                optionsCode.add(child)
              result.add quote do:
                `elemVar`.dropdownOptions = `optionsCode`
            elif stmt[1].kind == nnkPrefix and stmt[1][0].strVal == "@":
              # Array constructor syntax: options = @["Red", "Green", "Blue"]
              let optionsValue = stmt[1]
              result.add quote do:
                `elemVar`.dropdownOptions = `optionsValue`
          elif propName == "selectedIndex":
            # Handle selectedIndex for dropdown
            when defined(debugDropdown):
              echo "Debug: Handling selectedIndex, stmt[1] = ", stmt[1].repr, " kind = ", stmt[1].kind
            let indexValue = stmt[1]
            result.add quote do:
              `elemVar`.dropdownSelectedIndex = `indexValue`
          else:
            # Regular property
            let parseResult = parsePropertyValue(stmt[1])
            let propValue = parseResult.value
            result.add quote do:
              `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)

            # If this is an Input element and we have a bound variable, store it and create setter
            if kind == ekInput and parseResult.boundVar.isSome:
              let varNode = parseResult.boundVar.get()
              let varName = varNode.strVal
              result.add quote do:
                `elemVar`.setBoundVarName(`varName`)
                # Create a setter function for two-way binding that invalidates dependent elements
                `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
                  `varNode` = data
                  # Invalidate all elements that depend on this variable
                  invalidateReactiveValue(`varName`)
                )
        else:
          let parseResult = parsePropertyValue(stmt[1])
          let propValue = parseResult.value
          result.add quote do:
            `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)

          # If this is an Input element and we have a bound variable, store it and create setter
          if kind == ekInput and parseResult.boundVar.isSome:
            let varNode = parseResult.boundVar.get()
            let varName = varNode.strVal
            result.add quote do:
              `elemVar`.setBoundVarName(`varName`)
              # Create a setter function for two-way binding that invalidates dependent elements
              `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
                `varNode` = data
                # Invalidate all elements that depend on this variable
                invalidateReactiveValue(`varName`)
              )

    of nnkExprColonExpr:
      # Colon syntax: width: 800 (same as width = 800)
      let propName = stmt[0].strVal

      # Check if this is an event handler
      if propName.startsWith("on"):
        let handler = stmt[1]
        result.add quote do:
          `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), proc(data: string = "") {.closure.} =
            `handler`())
      else:
        # Handle dropdown-specific properties
        if kind == ekDropdown:
          if propName == "options":
            # Handle options array for dropdown
            if stmt[1].kind == nnkBracket:
              # Direct array syntax: options = ["Red", "Green", "Blue"]
              var optionsCode = newNimNode(nnkBracket)
              for child in stmt[1]:
                optionsCode.add(child)
              result.add quote do:
                `elemVar`.dropdownOptions = `optionsCode`
            elif stmt[1].kind == nnkPrefix and stmt[1][0].strVal == "@":
              # Array constructor syntax: options = @["Red", "Green", "Blue"]
              let optionsValue = stmt[1]
              result.add quote do:
                `elemVar`.dropdownOptions = `optionsValue`
          elif propName == "selectedIndex":
            # Handle selectedIndex for dropdown
            when defined(debugDropdown):
              echo "Debug: Handling selectedIndex, stmt[1] = ", stmt[1].repr, " kind = ", stmt[1].kind
            let indexValue = stmt[1]
            result.add quote do:
              `elemVar`.dropdownSelectedIndex = `indexValue`
          else:
            # Regular property
            let parseResult = parsePropertyValue(stmt[1])
            let propValue = parseResult.value
            result.add quote do:
              `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)

            # If this is an Input element and we have a bound variable, store it and create setter
            if kind == ekInput and parseResult.boundVar.isSome:
              let varNode = parseResult.boundVar.get()
              let varName = varNode.strVal
              result.add quote do:
                `elemVar`.setBoundVarName(`varName`)
                # Create a setter function for two-way binding that invalidates dependent elements
                `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
                  `varNode` = data
                  # Invalidate all elements that depend on this variable
                  invalidateReactiveValue(`varName`)
                )
        else:
          let parseResult = parsePropertyValue(stmt[1])
          let propValue = parseResult.value
          result.add quote do:
            `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)

          # If this is an Input element and we have a bound variable, store it and create setter
          if kind == ekInput and parseResult.boundVar.isSome:
            let varNode = parseResult.boundVar.get()
            let varName = varNode.strVal
            result.add quote do:
              `elemVar`.setBoundVarName(`varName`)
              # Create a setter function for two-way binding that invalidates dependent elements
              `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
                `varNode` = data
                # Invalidate all elements that depend on this variable
                invalidateReactiveValue(`varName`)
              )

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
        # Use valGetter to make it reactive like other properties
        let conditionGetter = quote do:
          proc(): bool =
            `conditionExpr`

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

    of nnkForStmt:
      # Handle for loops for generating repeated elements
      # Extract loop variable and iterable
      let loopVar = stmt[0]
      let iterable = stmt[1]
      let loopBody = stmt[2]

      # Check if iterable is a simple variable reference that needs reactive binding
      let iterableParseResult = parsePropertyValue(iterable)
      let iterableGetter = if iterableParseResult.boundVar.isSome:
        # Variable reference - create reactive getter that calls on every frame
        # For seq[string], we need to access it directly
        let varName = iterable.strVal
        quote do:
          proc(): seq[string] =
            # Register dependency on the iterable variable
            registerDependency(`varName`)
            # Access the current state of the iterable variable reactively
            `iterable`
      else:
        # Complex expression - use getter
        quote do:
          proc(): seq[string] =
            # Access the current state of the iterable variable
            `iterable`

      # Process the loop body to create a template function
      # We need to replace all instances of the loop variable with the parameter
      var loopBodyForTemplate = copy(loopBody)

      # Replace all occurrences of the loop variable with 'item' parameter
      # This is a simple replacement approach
      let bodyTemplate = quote do:
        proc(item: string): Element =
          # Create the elements from the processed loop body
          # Note: loopVar is replaced with 'item' parameter
          let `loopVar` = item  # Create local variable substitution
          `loopBodyForTemplate`

      # Create the for loop element and add it as a child
      result.add quote do:
        `elemVar`.addChild(newForLoopElement(`iterableGetter`, `bodyTemplate`))

    of nnkStaticStmt:
      # Handle static blocks for compile-time evaluation
      # The body is in stmt[0], which should be a StmtList
      let staticBody = stmt[0]

      # Process each statement in the static block
      for staticStmt in staticBody:
        case staticStmt.kind:
        of nnkForStmt:
          # For loop inside static block
          let loopVar = staticStmt[0]
          let iterable = staticStmt[1]
          let loopBody = staticStmt[2]

          # Use immediately-invoked proc to create new scope for each iteration
          # This ensures each iteration captures its own copy of the loop variable
          let indexVar = genSym(nskForVar, "i")
          result.add quote do:
            for `indexVar` in 0..<`iterable`.len:
              # Immediately invoke a proc to create a new scope
              # This captures the loop variable by value, not reference
              (proc() =
                let `loopVar` = `iterable`[`indexVar`]
                `elemVar`.addChild:
                  `loopBody`
              )()
        else:
          # Other statement in static block - just add as child
          result.add quote do:
            `elemVar`.addChild(`staticStmt`)

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
