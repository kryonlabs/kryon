
# ============================================================================
# Helper Functions
# ============================================================================

proc colorNode(value: NimNode): NimNode =
  if value.kind == nnkStrLit:
    let text = value.strVal
    if text.startsWith("#"):
      result = newCall(ident("parseHexColor"), value)
    else:
      result = newCall(ident("parseNamedColor"), value)
  elif value.kind == nnkBracketExpr:
    # Handle array access like alignment.colors[0]
    # Assume it returns a string that needs color parsing
    result = newCall(ident("parseHexColor"), value)
  else:
    # For other node types, try to parse as hex color
    result = newCall(ident("parseHexColor"), value)


proc alignmentNode(name: string): NimNode =
  let normalized = name.toLowerAscii()
  case normalized
  of "center", "middle": result = bindSym("kaCenter")
  of "end", "bottom", "right": result = bindSym("kaEnd")
  of "stretch": result = bindSym("kaStretch")
  of "spaceevenly": result = bindSym("kaSpaceEvenly")
  of "spacearound": result = bindSym("kaSpaceAround")
  of "spacebetween": result = bindSym("kaSpaceBetween")
  else: result = bindSym("kaStart")

proc parseAlignmentValue(value: NimNode): NimNode =
  ## Parse alignment value and return KryonAlignment enum value for C API
  if value.kind == nnkStrLit:
    # Convert string to enum value at compile time using alignmentNode
    result = alignmentNode(value.strVal)
  else:
    # Runtime parsing for non-literal values
    result = newCall(ident("parseAlignmentString"), value)

proc isEchoStatement(node: NimNode): bool =
  ## Detect Nim echo statements (command or call form)
  if node.len == 0:
    return false
  let head = node[0]
  if head.kind == nnkIdent and head.strVal == "echo":
    if node.kind == nnkCommand or node.kind == nnkCall:
      return true
  result = false

proc isStaticCollection(node: NimNode): bool =
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

# ============================================================================
# Reactive Expression Detection
# ============================================================================

proc isReactiveExpression*(node: NimNode): bool =
  ## Check if a node contains a reactive variable reference
  ## Detects patterns like `$value` where `value` could be a ReactiveVar
  result = false

  case node.kind
  of nnkPrefix:
    # Check for `$` prefix (string interpolation)
    if node.len >= 1:
      let operator = node[0]
      if operator.kind == nnkIdent and operator.strVal == "$":
        if node.len == 2 and node[1].kind == nnkIdent:
          # Pattern: `$variableName`
          result = true
        elif node.len >= 2:
          # Recursively check the operand
          result = isReactiveExpression(node[1])
  of nnkIdent:
    # Direct variable reference - could be a ReactiveVar
    # We'll be conservative and treat all idents as potentially reactive
    result = true
  of nnkDotExpr:
    # Field access like `counter.value` - check if base is reactive
    if node.len >= 1:
      result = isReactiveExpression(node[0])
  of nnkInfix:
    # Binary operations - check both operands
    if node.len >= 3:
      result = isReactiveExpression(node[1]) or isReactiveExpression(node[2])
  of nnkCall:
    # Function calls - check arguments
    for child in node.children:
      if isReactiveExpression(child):
        result = true
        break
  else:
    # For other node types, recursively check children
    for child in node.children:
      if isReactiveExpression(child):
        result = true
        break

proc extractReactiveVars*(node: NimNode): seq[NimNode] =
  ## Extract all reactive variable references from a node
  result = @[]

  case node.kind
  of nnkPrefix:
    if node.len >= 1:
      let operator = node[0]
      if operator.kind == nnkIdent and operator.strVal == "$":
        if node.len == 2 and node[1].kind == nnkIdent:
          # Pattern: `$variableName` - extract the variable
          result.add(node[1])
        elif node.len >= 2:
          # Recursively check the operand
          result.add(extractReactiveVars(node[1]))
      else:
        # Not a $ prefix, check if len == 1
        if node.len == 1:
          result.add(extractReactiveVars(node[0]))
  of nnkIdent:
    # Direct variable reference
    result.add(node)
  of nnkDotExpr:
    # Field access - extract the base variable
    if node.len >= 1:
      result.add(extractReactiveVars(node[0]))
  of nnkInfix:
    # Binary operations - extract from both operands
    if node.len >= 3:
      result.add(extractReactiveVars(node[1]))
      result.add(extractReactiveVars(node[2]))
  of nnkCall:
    # Function calls - check arguments
    for child in node.children:
      result.add(extractReactiveVars(child))
  else:
    # For other node types, recursively check children
    for child in node.children:
      result.add(extractReactiveVars(child))

# createReactiveBinding function removed - now using live expression evaluation

# ============================================================================
# Style Macro Implementation
# ============================================================================

macro style*(styleCall: untyped, body: untyped): untyped =
  ## Define a reusable style function
  ## Usage:
  ##   style calendarDayStyle(day: CalendarDay):
  ##     backgroundColor = "#3d3d3d"
  ##     textColor = "#ffffff"
  ##     if day.isCompleted:
  ##       backgroundColor = "#4a90e2"

  # Parse the style call: calendarDayStyle(day: CalendarDay)
  var styleName: NimNode
  var params: seq[NimNode]

  if styleCall.kind == nnkCall:
    # Function call syntax: calendarDayStyle(day: CalendarDay)
    styleName = styleCall[0]  # The function name

    # Extract individual parameter definitions
    var userParams: seq[NimNode] = @[]

    for i in 1..<styleCall.len:
      let arg = styleCall[i]
      if arg.kind == nnkExprEqExpr:
        # Parameter with type: day: CalendarDay
        userParams.add(nnkIdentDefs.newTree(arg[0], arg[1], newEmptyNode()))
      else:
        # Just a parameter name, infer type as auto
        userParams.add(nnkIdentDefs.newTree(arg, ident("auto"), newEmptyNode()))

    params = userParams

  elif styleCall.kind == nnkObjConstr:
    # Object constructor syntax: calendarDayStyle(day: CalendarDay)
    styleName = styleCall[0]  # The function name

    # Extract individual parameter definitions (not as FormalParams yet)
    var userParams: seq[NimNode] = @[]

    for i in 1..<styleCall.len:
      let arg = styleCall[i]
      if arg.kind == nnkExprColonExpr:
        # Parameter with type: day: CalendarDay
        userParams.add(nnkIdentDefs.newTree(arg[0], arg[1], newEmptyNode()))
      else:
        # Just a parameter name, infer type as auto
        userParams.add(nnkIdentDefs.newTree(arg, ident("auto"), newEmptyNode()))

    # Convert userParams to a simpler format for later processing
    params = userParams

  elif styleCall.kind == nnkIdent:
    # Simple name: myStyle
    styleName = styleCall
    params = @[]  # No user parameters
  else:
    echo "Style call kind: ", styleCall.kind
    echo "Style call repr: ", styleCall.treeRepr
    error("Invalid style function call format", styleCall)

  # Generate the style function name
  let styleFuncName = ident($styleName)

  # Parse the body to extract property assignments
  var propertyStmts: seq[NimNode] = @[]

  for stmt in body.children:
    if stmt.kind == nnkAsgn:
      let propName = $stmt[0]
      let value = stmt[1]

      case propName.toLowerAscii():
      of "backgroundcolor":
        propertyStmts.add newCall(
          ident("kryon_component_set_background_color"),
          ident("component"),
          colorNode(value)
        )
      of "textcolor", "color":
        propertyStmts.add newCall(
          ident("kryon_component_set_text_color"),
          ident("component"),
          colorNode(value)
        )
      of "bordercolor":
        propertyStmts.add newCall(
          ident("kryon_component_set_border_color"),
          ident("component"),
          colorNode(value)
        )
      of "borderwidth":
        propertyStmts.add newCall(
          ident("kryon_component_set_border_width"),
          ident("component"),
          newCall("uint8", value)
        )
      of "textfade":
        # Parse: textFade = horizontal(15) or textFade = none
        if value.kind == nnkCall:
          let fadeTypeStr = ($value[0]).toLowerAscii()
          var fadeType = ident("IR_TEXT_FADE_NONE")
          var fadeLength = newFloatLitNode(0.0)
          case fadeTypeStr:
          of "horizontal":
            fadeType = ident("IR_TEXT_FADE_HORIZONTAL")
            if value.len > 1:
              fadeLength = value[1]
          of "vertical":
            fadeType = ident("IR_TEXT_FADE_VERTICAL")
            if value.len > 1:
              fadeLength = value[1]
          of "radial":
            fadeType = ident("IR_TEXT_FADE_RADIAL")
            if value.len > 1:
              fadeLength = value[1]
          of "none":
            fadeType = ident("IR_TEXT_FADE_NONE")
          else:
            discard
          propertyStmts.add newCall(
            ident("kryon_component_set_text_fade"),
            ident("component"),
            fadeType,
            newCall("float", fadeLength)
          )
        elif value.kind == nnkIdent and ($value).toLowerAscii() == "none":
          propertyStmts.add newCall(
            ident("kryon_component_set_text_fade"),
            ident("component"),
            ident("IR_TEXT_FADE_NONE"),
            newFloatLitNode(0.0)
          )
      of "textoverflow":
        # Parse: textOverflow = ellipsis | clip | fade | visible
        let overflowStr = if value.kind == nnkIdent: ($value).toLowerAscii() else: ""
        var overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
        case overflowStr:
        of "visible": overflowType = ident("IR_TEXT_OVERFLOW_VISIBLE")
        of "clip": overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
        of "ellipsis": overflowType = ident("IR_TEXT_OVERFLOW_ELLIPSIS")
        of "fade": overflowType = ident("IR_TEXT_OVERFLOW_FADE")
        else: discard
        propertyStmts.add newCall(
          ident("kryon_component_set_text_overflow"),
          ident("component"),
          overflowType
        )
      of "opacity":
        propertyStmts.add newCall(
          ident("kryon_component_set_opacity"),
          ident("component"),
          newCall("float", value)
        )
      else:
        # Unknown property - skip for now
        discard
    elif stmt.kind == nnkIfStmt:
      # Handle conditional styling
      var ifBranches: seq[NimNode] = @[]
      for branch in stmt.children:
        if branch.len >= 2:
          let condition = branch[0]
          let branchStmts = branch[1]

          var branchProperties: seq[NimNode] = @[]
          for branchStmt in branchStmts:
            if branchStmt.kind == nnkAsgn:
              let propName = $branchStmt[0]
              let value = branchStmt[1]

              case propName.toLowerAscii():
              of "backgroundcolor":
                branchProperties.add newCall(
                  ident("kryon_component_set_background_color"),
                  ident("component"),
                  colorNode(value)
                )
              of "textcolor", "color":
                branchProperties.add newCall(
                  ident("kryon_component_set_text_color"),
                  ident("component"),
                  colorNode(value)
                )
              of "bordercolor":
                branchProperties.add newCall(
                  ident("kryon_component_set_border_color"),
                  ident("component"),
                  colorNode(value)
                )
              of "textfade":
                if value.kind == nnkCall:
                  let fadeTypeStr = ($value[0]).toLowerAscii()
                  var fadeType = ident("IR_TEXT_FADE_NONE")
                  var fadeLength = newFloatLitNode(0.0)
                  case fadeTypeStr:
                  of "horizontal":
                    fadeType = ident("IR_TEXT_FADE_HORIZONTAL")
                    if value.len > 1: fadeLength = value[1]
                  of "vertical":
                    fadeType = ident("IR_TEXT_FADE_VERTICAL")
                    if value.len > 1: fadeLength = value[1]
                  of "radial":
                    fadeType = ident("IR_TEXT_FADE_RADIAL")
                    if value.len > 1: fadeLength = value[1]
                  of "none":
                    fadeType = ident("IR_TEXT_FADE_NONE")
                  else: discard
                  branchProperties.add newCall(
                    ident("kryon_component_set_text_fade"),
                    ident("component"),
                    fadeType,
                    newCall("float", fadeLength)
                  )
                elif value.kind == nnkIdent and ($value).toLowerAscii() == "none":
                  branchProperties.add newCall(
                    ident("kryon_component_set_text_fade"),
                    ident("component"),
                    ident("IR_TEXT_FADE_NONE"),
                    newFloatLitNode(0.0)
                  )
              of "textoverflow":
                let overflowStr = if value.kind == nnkIdent: ($value).toLowerAscii() else: ""
                var overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
                case overflowStr:
                of "visible": overflowType = ident("IR_TEXT_OVERFLOW_VISIBLE")
                of "clip": overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
                of "ellipsis": overflowType = ident("IR_TEXT_OVERFLOW_ELLIPSIS")
                of "fade": overflowType = ident("IR_TEXT_OVERFLOW_FADE")
                else: discard
                branchProperties.add newCall(
                  ident("kryon_component_set_text_overflow"),
                  ident("component"),
                  overflowType
                )
              of "opacity":
                branchProperties.add newCall(
                  ident("kryon_component_set_opacity"),
                  ident("component"),
                  newCall("float", value)
                )
              else:
                discard

          if branchProperties.len > 0:
            ifBranches.add(newTree(nnkElifBranch, condition, newStmtList(branchProperties)))

      if ifBranches.len > 0:
        # Build a single if statement with all branches
        var ifStmt = newTree(nnkIfStmt, ifBranches[0])
        for i in 1..<ifBranches.len:
          ifStmt.add(ifBranches[i])
        propertyStmts.add(ifStmt)

  # Generate the style function with the component and any parameters
  let componentParam = ident("component")

  # Start with the return type
  var paramDecl = nnkFormalParams.newTree(ident("void"))

  # Always include component parameter first
  paramDecl.add(nnkIdentDefs.newTree(componentParam, ident("KryonComponent"), newEmptyNode()))

  # Add user parameters (params is now a seq of individual parameter definitions)
  for paramDef in params:
    paramDecl.add(paramDef)

  # Extract complete parameter list for newProc (skip return type, include component)
  var paramList: seq[NimNode] = @[]
  for i in 1..<paramDecl.len:  # Skip return type at index 0
    paramList.add(paramDecl[i])

  # Use the parsed parameters and body to create the complete style function
  # Convert paramDecl to a sequence for newProc
  var paramSeq: seq[NimNode] = @[]
  for param in paramDecl:
    paramSeq.add(param)

  result = newProc(
    styleFuncName,
    paramSeq,
    newStmtList(propertyStmts)
  )

  
# ============================================================================
# DSL Macros for Declarative UI
# ============================================================================

macro kryonApp*(body: untyped): untyped =
  ## Top-level application macro
  ## Creates root container with window configuration

  # Parse the body for configuration and components
  var windowWidth = newIntLitNode(800)
  var windowHeight = newIntLitNode(600)
  var windowTitle = newStrLitNode("Kryon Application")
  var pendingChildren: seq[NimNode] = @[]
  var bodyNode: NimNode = nil
  var resourcesNode: NimNode = nil

  # Extract window configuration and component tree
  for node in body.children:
    if node.kind == nnkCall and node[0].kind == nnkIdent:
      let nodeName = $node[0]
      case nodeName.toLowerAscii():
      of "header":
        # Extract window config from Header block
        for child in node[1].children:
          if child.kind == nnkAsgn:
            let propName = $child[0]
            let value = child[1]
            case propName.toLowerAscii():
            of "width", "windowwidth":
              windowWidth = value
            of "height", "windowheight":
              windowHeight = value
            of "title", "windowtitle":
              windowTitle = value
            else:
              discard
      of "resources":
        # Process resources (fonts, etc.)
        resourcesNode = node
      of "body":
        if bodyNode.isNil:
          bodyNode = node
        else:
          # Merge additional Body blocks into the first one
          if node.len >= 2 and node[1].kind == nnkStmtList:
            for child in node[1].children:
              bodyNode[1].add(child)
      else:
        pendingChildren.add(node)
    else:
      pendingChildren.add(node)

  proc ensureBodyDimensions(bodyCall: var NimNode) =
    if bodyCall.kind == nnkCall and bodyCall.len >= 2:
      var widthSet = false
      var heightSet = false
      let section = bodyCall[1]
      if section.kind == nnkStmtList:
        for stmt in section.children:
          if stmt.kind == nnkAsgn:
            let propName = $stmt[0]
            case propName.toLowerAscii():
            of "width":
              widthSet = true
            of "height":
              heightSet = true
            else:
              discard
        if not heightSet:
          section.insert(0, newTree(nnkAsgn, ident("height"), copyNimTree(windowHeight)))
        if not widthSet:
          section.insert(0, newTree(nnkAsgn, ident("width"), copyNimTree(windowWidth)))

  if bodyNode.isNil:
    var synthesizedBody = newTree(nnkStmtList)
    for child in pendingChildren:
      synthesizedBody.add(child)
    bodyNode = newTree(nnkCall, ident("Body"), synthesizedBody, copyNimTree(windowWidth), copyNimTree(windowHeight))
  elif pendingChildren.len > 0:
    if bodyNode.len >= 2 and bodyNode[1].kind == nnkStmtList:
      for child in pendingChildren:
        bodyNode[1].add(child)

  ensureBodyDimensions(bodyNode)

  # Generate proper application code
  var appName = genSym(nskLet, "app")

  # Build the final code structure
  var appCode = newStmtList()

  # Add initialization
  let createContextSym = bindSym("ir_create_context")
  let setContextSym = bindSym("ir_set_context")

  appCode.add quote do:
    # Initialize application
    let `appName` = newKryonApp()

    # Initialize IR context BEFORE creating any components
    # This ensures all components get unique auto-incrementing IDs
    let irContext = `createContextSym`()
    `setContextSym`(irContext)
    echo "[kryon][dsl] Initialized IR context"

    # Set window properties
    var window = KryonWindow(
      width: `windowWidth`,
      height: `windowHeight`,
      title: `windowTitle`
    )

    # Set up renderer with window config
    # Note: IR architecture defers renderer creation to run()
    # initRenderer() just sets up config, returns nil placeholder
    let renderer = initRenderer(window.width, window.height, window.title)

    `appName`.setWindow(window)
    `appName`.setRenderer(renderer)

  # Add resources loading if present
  if not resourcesNode.isNil:
    appCode.add(resourcesNode)

  # Add component tree creation and run
  appCode.add quote do:
    # Create component tree
    let rootComponent: KryonComponent = block:
      `bodyNode`

    if rootComponent != nil:
      `appName`.setRoot(rootComponent)
    else:
      echo "Warning: No root component defined; application will exit."

    # Run the application
    `appName`.run()

    # Return the app
    `appName`

  result = appCode

macro component*(name: untyped, props: untyped): untyped =
  ## Generic component creation macro

  # Extract component name
  let compName = $name

  # Generate component creation code
  result = quote do:
    let `name` = createComponent(`compName`, `props`)

# ============================================================================
# Built-in Component Macros
# ============================================================================

macro Header*(props: untyped): untyped =
  ## Header configuration macro - sets window properties
  # Return a tuple with window configuration values
  var
    widthVal = newIntLitNode(800)
    heightVal = newIntLitNode(600)
    titleVal = newStrLitNode("Kryon Application")

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "width", "windowwidth":
        widthVal = value
      of "height", "windowheight":
        heightVal = value
      of "title", "windowtitle":
        titleVal = value
      else:
        discard

  # Generate window configuration assignments
  result = quote do:
    window.width = `widthVal`
    window.height = `heightVal`
    window.title = `titleVal`

macro Resources*(props: untyped): untyped =
  ## Resources configuration macro - loads fonts and other assets
  ## Returns nothing - this is a statement block
  var loadStatements = newStmtList()

  for node in props.children:
    if node.kind == nnkCall and $node[0] == "Font":
      # Extract font properties
      var fontName: NimNode = nil
      var fontSources: seq[NimNode] = @[]

      for child in node[1].children:
        if child.kind == nnkAsgn:
          let propName = $child[0]
          case propName.toLowerAscii():
          of "name":
            fontName = child[1]
          of "sources", "source":
            # Handle both single source and array of sources
            if child[1].kind == nnkBracket:
              for source in child[1].children:
                fontSources.add(source)
            else:
              fontSources.add(child[1])
          else:
            discard

      # Generate font loading code
      if fontName != nil and fontSources.len > 0:
        # Load the font from the first source and register with provided name
        let firstSource = fontSources[0]
        loadStatements.add quote do:
          when defined(KRYON_SDL3):
            discard registerFont(`fontName`, `firstSource`, 16)

  # If no statements were added, add a pass statement
  if loadStatements.len == 0:
    loadStatements.add(newNilLit())

  result = loadStatements

# Helper function to transform loop variable references in component expressions
proc transformLoopVariableReferences(node: NimNode, originalVar: NimNode, capturedVar: NimNode): NimNode =
  ## Recursively replace all ident nodes that match the original loop variable with captured copy
  result = node.copyNimTree()
  if result.kind == nnkIdent and result.eqIdent(originalVar):
    # Replace with captured variable reference
    result = capturedVar
  else:
    # Recursively process child nodes
    for i in 0..<result.len:
      result[i] = transformLoopVariableReferences(result[i], originalVar, capturedVar)

proc convertIfStmtToReactiveConditional*(ifStmt: NimNode, windowWidth: NimNode, windowHeight: NimNode): NimNode =
  ## Convert an if statement to reactive component expression
  ## Creates a reactive container that evaluates the if statement and returns the appropriate component
  # Process all branches: if, elif, else
  var reactiveCode = newStmtList()
  let containerSym = genSym(nskLet, "ifContainer")

  proc buildComponentArray(body: NimNode): NimNode =
    var elements = newNimNode(nnkBracket)
    if body.kind == nnkStmtList:
      for child in body:
        elements.add(quote do:
          block:
            `child`)
    else:
      elements.add(quote do:
        block:
          `body`)
    newCall(ident("@"), elements)

  # Create the main container with window-filling dimensions using Container DSL
  reactiveCode.add quote do:
    let `containerSym` = Container:
      width = `windowWidth`
      height = `windowHeight`

  # For each branch in the if statement, create a reactive conditional
  for i in 0..<ifStmt.len:
    let branch = ifStmt[i]

    if branch.kind == nnkElifBranch:
      # elif branch: condition and body
      let condition = branch[0]
      let branchBody = branch[1]

      # Create reactive conditional for this elif condition
      # CRITICAL FIX: Use lambda expressions for proper closure capture
      let conditionProc = genSym(nskLet, "elifConditionProc")
      let thenProc = genSym(nskLet, "elifThenProc")
      let thenBody = buildComponentArray(branchBody)

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = `condition`
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil)

    elif branch.kind == nnkElse:
      # else branch: only body
      let elseBody = branch[0]

      # Create reactive conditional for else (always true condition)
      # CRITICAL FIX: Use lambda expressions for proper closure capture
      let conditionProc = genSym(nskLet, "elseConditionProc")
      let thenProc = genSym(nskLet, "elseThenProc")
      let thenBody = buildComponentArray(elseBody)

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = true  # else is always true if reached
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil)

    elif branch.kind == nnkIfExpr:
      # This is the main 'if' branch (first element in ifStmt)
      # The actual structure is: ifStmt[0] = condition, ifStmt[1] = first branch body
      # But we're iterating, so this case handles the condition separately
      discard

  # Handle the main 'if' branch separately (it's structured differently)
  if ifStmt.len >= 2:
    let ifCondition = ifStmt[0]  # The condition
    let ifBranch = ifStmt[1]     # The first branch body

    if ifBranch.kind == nnkStmtList and ifBranch.len > 0:
      let branchBody = ifBranch    # Full branch body (may contain multiple statements)

      # Create reactive conditional for the main if condition
      # CRITICAL FIX: Use lambda expressions for proper closure capture
      let conditionProc = genSym(nskLet, "ifConditionProc")
      let thenProc = genSym(nskLet, "ifThenProc")
      let thenBody = buildComponentArray(branchBody)

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = `ifCondition`
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil)

  # Return the container
  reactiveCode.add(containerSym)
  result = reactiveCode

macro Container*(props: untyped): untyped =
  ## Simplified Container component using C core layout API
  var
    containerName = genSym(nskLet, "container")
    childNodes: seq[NimNode] = @[]
    sideEffectStmts: seq[NimNode] = @[]
    deferredIfStmts: seq[NimNode] = @[]  # If statements to process with parent context
    # Layout properties - default to HTML-like behavior (column layout)
    layoutDirectionVal = newIntLitNode(0)  # Default to column layout
    justifyContentVal = alignmentNode("start")  # Start alignment
    alignItemsVal = alignmentNode("start")      # Start alignment
    gapVal = newIntLitNode(0)
    # Visual properties
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    # Position properties
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    minWidthVal: NimNode = nil
    maxWidthVal: NimNode = nil
    minHeightVal: NimNode = nil
    maxHeightVal: NimNode = nil
    zIndexVal: NimNode = nil
    # Spacing properties
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    # Flex properties
    flexGrowVal: NimNode = nil
    flexShrinkVal: NimNode = nil
    # Style properties
    styleName: NimNode = nil
    styleData: NimNode = nil

  # Parse properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "backgroundcolor", "bg", "background":
        bgColorVal = colorNode(value)
      of "color", "textcolor":
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width", "w":
        widthVal = value
      of "height", "h":
        heightVal = value
      of "minwidth":
        minWidthVal = value
      of "maxwidth":
        maxWidthVal = value
      of "minheight":
        minHeightVal = value
      of "maxheight":
        maxHeightVal = value
      of "zindex", "z":
        zIndexVal = value
      of "padding", "p":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "margin", "m":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "flex", "flexgrow":
        flexGrowVal = value
      of "flexshrink":
        flexShrinkVal = value
      of "justifycontent", "mainaxisalignment", "justify":
        justifyContentVal = parseAlignmentValue(value)
      of "alignitems", "crossaxisalignment", "align":
        alignItemsVal = parseAlignmentValue(value)
      of "contentalignment":
        justifyContentVal = parseAlignmentValue(value)
        alignItemsVal = parseAlignmentValue(value)
      of "gap":
        gapVal = value
      of "layoutdirection":
        layoutDirectionVal = value
      of "style":
        if value.kind == nnkCall:
          styleName = newStrLitNode($value[0])
          if value.len > 1:
            styleData = value[1]
        else:
          styleName = newStrLitNode($value)
      else:
        discard
    else:
      # Handle static blocks specially using the same pattern as staticFor
      if isEchoStatement(node):
        # Allow echo/logging statements inside templates without treating them as children
        sideEffectStmts.add(node)
      elif node.kind in {nnkLetSection, nnkVarSection, nnkConstSection}:
        # Local bindings inside DSL blocks should execute but not become children
        sideEffectStmts.add(node)
      elif node.kind == nnkStaticStmt:
        # Static blocks will be processed later after initStmts is initialized
        # Don't add them to childNodes - they'll be handled separately
        discard
      elif node.kind == nnkIfStmt:
        # Defer if statement processing - will be handled with parent context
        # This avoids creating wrapper containers and respects parent layout
        deferredIfStmts.add(node)
      else:
        childNodes.add(node)

  # Generate initialization statements using C core API
  var initStmts = newTree(nnkStmtList)

  # Add standalone side-effect statements (e.g., echo) before component setup
  for stmt in sideEffectStmts:
    initStmts.add(stmt)

  # Process static blocks from the original props after initStmts is available
  for node in props.children:
    if node.kind == nnkStaticStmt:
      for staticChild in node.children:
        # Handle static block children appropriately based on their type
        if staticChild.kind == nnkStmtList:
          # Static block content is wrapped in a statement list - iterate through it
          for stmt in staticChild.children:
            if stmt.kind == nnkForStmt:
              # Handle for loop from statement list - force value capture for reactive expressions
              # CRITICAL FIX: Use immediately-invoked closure to force value capture
              let loopVar = stmt[0]  # The loop variable (e.g., "alignment")
              let capturedParam = genSym(nskParam, "capturedIdx")
              var innerBody = newTree(nnkStmtList)

              for bodyNode in stmt[^1].children:
                # Transform the body node to replace loop variable references with captured parameter
                let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, capturedParam)

                if transformedBodyNode.kind == nnkDiscardStmt and transformedBodyNode.len > 0:
                  let componentExpr = transformedBodyNode[0]
                  let childSym = genSym(nskLet, "loopChild")
                  innerBody.add(newLetStmt(childSym, componentExpr))
                  innerBody.add quote do:
                    discard kryon_component_add_child(`containerName`, `childSym`)
                elif transformedBodyNode.kind == nnkCall:
                  let childSym = genSym(nskLet, "loopChild")
                  innerBody.add(newLetStmt(childSym, transformedBodyNode))
                  innerBody.add quote do:
                    discard kryon_component_add_child(`containerName`, `childSym`)
                else:
                  # For other nodes, just add them directly
                  innerBody.add(transformedBodyNode)

              if innerBody.len > 0:
                # Create immediately-invoked closure: (proc(capturedIdx: typeof(loopVar)) = body)(loopVar)
                let closureProc = newProc(
                  name = newEmptyNode(),
                  params = [newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
                  body = innerBody,
                  procType = nnkLambda
                )
                closureProc.addPragma(ident("closure"))
                let closureCall = newCall(closureProc, loopVar)
                let newForLoop = newTree(nnkForStmt, loopVar, stmt[1], newStmtList(closureCall))
                initStmts.add(newForLoop)
            else:
              # Handle other statements from statement list
              initStmts.add(stmt)
        elif staticChild.kind == nnkDiscardStmt:
          # Static child already has discard - extract the component and add to container
          if staticChild.len > 0:
            let childExpr = staticChild[0]
            let childSym = genSym(nskLet, "staticChild")
            initStmts.add(newLetStmt(childSym, childExpr))
            initStmts.add quote do:
              discard kryon_component_add_child(`containerName`, `childSym`)
          else:
            # Discard without expression - just execute as-is
            initStmts.add(staticChild)
        elif staticChild.kind == nnkCall:
          # Direct component call in static block (e.g., Container(...))
          let childSym = genSym(nskLet, "staticChild")
          initStmts.add(newLetStmt(childSym, staticChild))
          initStmts.add quote do:
            discard kryon_component_add_child(`containerName`, `childSym`)
        else:
          # For control flow statements (for loops, if statements, etc.),
          # transform them to capture component results and add to parent
          if staticChild.kind == nnkForStmt:
            # Transform for loops to capture component results and add them to parent
            # CRITICAL FIX: Wrap loop body in immediately-invoked closure to force value capture
            # Without this, all closures share the same loop variable and see the final value
            let loopVar = staticChild[0]
            let capturedParam = genSym(nskParam, "capturedIdx")
            var innerBody = newTree(nnkStmtList)

            # Process the body of the for loop
            for bodyNode in staticChild[^1].children:
              # Transform the body node to replace loop variable references with captured parameter
              let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, capturedParam)

              if transformedBodyNode.kind == nnkDiscardStmt and transformedBodyNode.len > 0:
                # Extract component from discard statement
                let componentExpr = transformedBodyNode[0]
                let childSym = genSym(nskLet, "loopChild")
                innerBody.add(newLetStmt(childSym, componentExpr))
                innerBody.add quote do:
                  let added = kryon_component_add_child(`containerName`, `childSym`)
                  echo "[kryon][static] Added child to container: ", `containerName`, " -> ", `childSym`, " success: ", added
              elif transformedBodyNode.kind == nnkCall:
                # Direct component call
                let childSym = genSym(nskLet, "loopChild")
                innerBody.add(newLetStmt(childSym, transformedBodyNode))
                innerBody.add quote do:
                  let added = kryon_component_add_child(`containerName`, `childSym`)
                  echo "[kryon][static] Added direct child to container: ", `containerName`, " -> ", `childSym`, " success: ", added
              else:
                # Other statements - just add as-is
                innerBody.add(transformedBodyNode)

            # Create immediately-invoked closure: (proc(capturedIdx: typeof(i)) = body)(i)
            # This ensures each iteration captures its own copy of the loop variable
            if innerBody.len > 0:
              let loopRange = staticChild[1]
              # Create closure proc type with parameter matching the loop variable type
              let closureProc = newProc(
                name = newEmptyNode(),
                params = [newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
                body = innerBody,
                procType = nnkLambda
              )
              closureProc.addPragma(ident("closure"))

              # Create call to closure with loop variable as argument
              let closureCall = newCall(closureProc, loopVar)

              # Recreate the for loop with closure call as body
              let newForLoop = newTree(nnkForStmt, loopVar, loopRange, newStmtList(closureCall))
              initStmts.add(newForLoop)
            else:
              # If no transformable content, add original
              initStmts.add(staticChild)
          else:
            # Other control flow statements - add as-is for now
            initStmts.add(staticChild)

  # Set layout direction first (most important)
  initStmts.add quote do:
    kryon_component_set_layout_direction(`containerName`, int(`layoutDirectionVal`))

  # Set layout alignment
  initStmts.add quote do:
    kryon_component_set_layout_alignment(`containerName`,
      `justifyContentVal`, `alignItemsVal`)

  # Set gap
  initStmts.add quote do:
    kryon_component_set_gap(`containerName`, uint8(`gapVal`))

  # Apply style if specified
  if styleName != nil:
    if styleData != nil:
      initStmts.add quote do:
        `styleName`(`containerName`, `styleData`)
    else:
      initStmts.add quote do:
        `styleName`(`containerName`)

  # Set position and size
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let wExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let hExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08

  initStmts.add quote do:
    kryon_component_set_bounds_mask(`containerName`,
      cfloat(`xExpr`), cfloat(`yExpr`), cfloat(`wExpr`), cfloat(`hExpr`), uint8(`maskVal`))

  # Set padding
  let padTop = if paddingTopVal != nil: paddingTopVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)
  let padRight = if paddingRightVal != nil: paddingRightVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)
  let padBottom = if paddingBottomVal != nil: paddingBottomVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)
  let padLeft = if paddingLeftVal != nil: paddingLeftVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)

  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`containerName`,
        uint8(`padTop`), uint8(`padRight`), uint8(`padBottom`), uint8(`padLeft`))

  # Set margin
  let marginTop = if marginTopVal != nil: marginTopVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginRight = if marginRightVal != nil: marginRightVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginBottom = if marginBottomVal != nil: marginBottomVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginLeft = if marginLeftVal != nil: marginLeftVal elif marginAll != nil: marginAll else: newIntLitNode(0)

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`containerName`,
        uint8(`marginTop`), uint8(`marginRight`), uint8(`marginBottom`), uint8(`marginLeft`))

  # Set visual properties
  if bgColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`containerName`, `bgColorVal`)

  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`containerName`, `textColorVal`)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`containerName`, `borderColorVal`)

  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`containerName`, uint8(`borderWidthVal`))

  if zIndexVal != nil:
    initStmts.add quote do:
      kryon_component_set_z_index(`containerName`, uint16(`zIndexVal`))

  # Set flex properties
  if flexGrowVal != nil or flexShrinkVal != nil:
    let growExpr = if flexGrowVal != nil: flexGrowVal else: newIntLitNode(0)
    let shrinkExpr = if flexShrinkVal != nil: flexShrinkVal else: newIntLitNode(1)
    initStmts.add quote do:
      kryon_component_set_flex(`containerName`, uint8(`growExpr`), uint8(`shrinkExpr`))

  # Apply min/max width/height constraints via layout
  if minWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_min_width(layout, `dimType`, cfloat(`minWidthVal`))

  if maxWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_max_width(layout, `dimType`, cfloat(`maxWidthVal`))

  if minHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_min_height(layout, `dimType`, cfloat(`minHeightVal`))

  if maxHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_max_height(layout, `dimType`, cfloat(`maxHeightVal`))

  # Add child components
  for child in childNodes:
    # For-loops: distinguish static vs dynamic collections
    if child.kind == nnkForStmt:
      let loopCollection = child[1]

      # Static collections (array literals, seq literals, ranges) get compile-time unrolled
      if isStaticCollection(loopCollection):
        proc buildStaticForLoop(forNode: NimNode): NimNode =
          ## Generate a compile-time for loop that directly adds children to container
          let loopVar = forNode[0]
          let collection = forNode[1]
          let loopBody = forNode[2]

          # Use immediately-invoked closure to capture loop variable value
          let capturedParam = genSym(nskParam, "captured")

          var innerBody = newTree(nnkStmtList)
          for bodyNode in loopBody.children:
            let transformed = transformLoopVariableReferences(bodyNode, loopVar, capturedParam)
            case transformed.kind
            of nnkLetSection, nnkVarSection, nnkConstSection:
              innerBody.add(transformed)
            of nnkDiscardStmt:
              # Discarded component - still add to container
              if transformed.len > 0:
                let childSym = genSym(nskLet, "child")
                innerBody.add(newLetStmt(childSym, transformed[0]))
                innerBody.add quote do:
                  discard kryon_component_add_child(`containerName`, `childSym`)
              else:
                innerBody.add(transformed)
            else:
              # Component expression - add to container
              let childSym = genSym(nskLet, "child")
              innerBody.add(newLetStmt(childSym, transformed))
              innerBody.add quote do:
                discard kryon_component_add_child(`containerName`, `childSym`)

          # Create closure and call it with loop variable
          let closureProc = newProc(
            name = newEmptyNode(),
            params = @[newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
            body = innerBody,
            procType = nnkLambda
          )
          closureProc.addPragma(ident("closure"))
          let closureCall = newCall(closureProc, loopVar)

          # Return the for loop with closure call as body
          result = newTree(nnkForStmt, loopVar, collection, newStmtList(closureCall))

        initStmts.add(buildStaticForLoop(child))
        continue

      # Dynamic collections use reactive for-loop system
      proc buildReactiveForLoop(forNode: NimNode): NimNode =
        let loopVar = forNode[0]
        let collection = forNode[1]
        let loopBody = forNode[2]

        # Use a parameter for captured value in the item proc
        let itemParam = genSym(nskParam, "item")
        let indexParam = genSym(nskParam, "idx")

        # Build the item creation proc body - must return a component
        var itemBody = newTree(nnkStmtList)
        var lastComponentExpr: NimNode = nil

        for bodyNode in loopBody.children:
          # Transform the body node to replace loop variable references with item parameter
          let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, itemParam)

          case transformedBodyNode.kind
          of nnkLetSection, nnkVarSection, nnkConstSection:
            itemBody.add(transformedBodyNode)
          of nnkDiscardStmt:
            if transformedBodyNode.len > 0:
              lastComponentExpr = transformedBodyNode[0]
            else:
              itemBody.add(transformedBodyNode)
          else:
            # This should be a component expression - save it as the return value
            lastComponentExpr = transformedBodyNode

        # Add the return expression (the component)
        if lastComponentExpr != nil:
          itemBody.add(lastComponentExpr)
        else:
          # Fallback - return nil if no component found
          itemBody.add(newNilLit())

        # Generate the createReactiveForLoop call
        # collectionProc: returns the current collection
        # itemProc: creates a component from an item
        let collectionProcSym = genSym(nskProc, "collectionProc")
        let itemProcSym = genSym(nskProc, "itemProc")

        # Determine element type from collection
        let elemType = newCall(ident("typeof"), newTree(nnkBracketExpr, collection, newLit(0)))

        result = quote do:
          block:
            proc `collectionProcSym`(): seq[`elemType`] {.closure.} =
              result = `collection`
            proc `itemProcSym`(`itemParam`: `elemType`, `indexParam`: int): ptr IRComponent {.closure.} =
              `itemBody`
            discard createReactiveForLoop(`containerName`, `collectionProcSym`, `itemProcSym`)

      initStmts.add(buildReactiveForLoop(child))
      continue

    # Handle both discarded and non-discarded children
    if child.kind == nnkDiscardStmt:
      # Child already has discard - just execute it directly
      # The user took care of discarding, so we just need to add it to container
      # But first, we need to extract the actual component from the discard
      if child.len > 0:
        let childExpr = child[0]
        let childSym = genSym(nskLet, "child")
        initStmts.add(newLetStmt(childSym, childExpr))
        initStmts.add quote do:
          discard kryon_component_add_child(`containerName`, `childSym`)
      else:
        # Discard without expression - just execute as-is
        initStmts.add(child)
    else:
      # Regular child component - create symbol and add to container
      let childSym = genSym(nskLet, "child")
      initStmts.add quote do:
        let `childSym` = block:
          `child`
        discard kryon_component_add_child(`containerName`, `childSym`)

  # Process deferred if statements - create reactive conditionals with parent context
  for ifStmt in deferredIfStmts:
    # Extract condition and branches from if statement
    # nnkIfStmt structure: [condition, thenBranch, (elifBranch | elseBranch)...]
    var conditionExpr: NimNode
    var thenBody: NimNode
    var elseBody: NimNode = nil

    # Process the if statement branches
    for i in 0..<ifStmt.len:
      let branch = ifStmt[i]
      if branch.kind == nnkElifBranch:
        if i == 0:
          # First elif is the main if condition
          conditionExpr = branch[0]
          thenBody = branch[1]
        else:
          # Subsequent elif branches - chain into else
          if elseBody == nil:
            elseBody = newTree(nnkIfStmt)
          elseBody.add(branch)
      elif branch.kind == nnkElse:
        if elseBody != nil and elseBody.kind == nnkIfStmt:
          elseBody.add(branch)
        else:
          elseBody = branch[0]

    # Build closures for condition and branches
    # CRITICAL: Use nskLet since we create lambda expressions with let bindings
    let condProc = genSym(nskLet, "conditionProc")
    let thenProc = genSym(nskLet, "thenProc")
    let elseProc = genSym(nskLet, "elseProc")

    # Helper to build component array from branch body
    proc buildComponentSeq(body: NimNode): NimNode =
      var elements = newNimNode(nnkBracket)
      if body.kind == nnkStmtList:
        for child in body:
          if child.kind notin {nnkCommentStmt, nnkDiscardStmt}:
            elements.add(quote do:
              block:
                `child`)
      else:
        elements.add(quote do:
          block:
            `body`)
      newCall(ident("@"), elements)

    # CRITICAL FIX: Use let bindings with lambdas instead of proc definitions
    # This ensures proper closure capture of loop variables in reactive conditionals
    # With nnkProcDef, Nim may not properly capture the closure environment
    # when variables like capturedLoopVar are referenced inside the proc body

    let thenBodySeq = buildComponentSeq(thenBody)

    # Generate condition and then procs as lambda expressions
    initStmts.add quote do:
      let `condProc` = proc(): bool {.closure.} = `conditionExpr`
      let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBodySeq`

    # Generate else proc (may be nil)
    if elseBody != nil:
      let elseBodySeq = if elseBody.kind == nnkIfStmt:
        # Nested if-elif chain - wrap in closure
        quote do:
          @[block:
            `elseBody`]
      else:
        buildComponentSeq(elseBody)

      initStmts.add quote do:
        let `elseProc` = proc(): seq[KryonComponent] {.closure.} = `elseBodySeq`

      # Register reactive conditional with else
      initStmts.add quote do:
        createReactiveConditional(`containerName`, `condProc`, `thenProc`, `elseProc`)
    else:
      # Register reactive conditional without else
      initStmts.add quote do:
        createReactiveConditional(`containerName`, `condProc`, `thenProc`, nil)

  # Mark container as dirty
  initStmts.add quote do:
    kryon_component_mark_dirty(`containerName`)

  # Generate the final result
  # Always return the component - the calling context should handle it properly
  # Use IR_COMPONENT_ROW or IR_COMPONENT_COLUMN instead of CONTAINER based on layoutDirection
  let componentCreation =
    if layoutDirectionVal.kind == nnkIntLit:
      if layoutDirectionVal.intVal == 0:
        # Column layout (vertical)
        quote do: newKryonColumn()
      elif layoutDirectionVal.intVal == 1:
        # Row layout (horizontal)
        quote do: newKryonRow()
      else:
        # Default to container
        quote do: newKryonContainer()
    else:
      # Runtime value - default to container
      quote do: newKryonContainer()

  result = quote do:
    block:
      let `containerName` = `componentCreation`
      `initStmts`
      `containerName`

proc convertCaseStmtToReactiveConditional*(caseStmt: NimNode, windowWidth: NimNode, windowHeight: NimNode): NimNode =
  ## Convert a case statement to reactive component expression
  ## Creates a reactive container that evaluates the case statement and returns the appropriate component
  let caseExpr = caseStmt[0]

  # Create a container for the reactive expression
  let containerSym = genSym(nskLet, "caseContainer")

  # Build if-elif chain for component evaluation
  var ifChain = newNimNode(nnkIfStmt)

  # Process each branch
  for i in 1..<caseStmt.len:
    let branch = caseStmt[i]
    if branch.kind == nnkOfBranch:
      # Extract the value(s) and the body
      let ofValues = branch[0]
      let branchBody = branch[1]

      # Create condition for this branch
      var condition: NimNode
      if ofValues.kind == nnkCurly:  # Multiple values: {val1, val2, val3}
        # Create (caseExpr == val1 or caseExpr == val2 or caseExpr == val3)
        condition = newNimNode(nnkInfix).add(ident("or"))
        for j in 0..<ofValues.len:
          let eqCheck = newNimNode(nnkInfix).add(ident("=="), caseExpr, ofValues[j])
          if j == 0:
            condition[0] = eqCheck
          elif j == 1:
            condition[1] = eqCheck
          else:
            # Nest or operations for more than 2 values
            let newOr = newNimNode(nnkInfix).add(ident("or"), condition, eqCheck)
            condition = newOr
      else:
        # Single value: caseExpr == value
        condition = newNimNode(nnkInfix).add(ident("=="), caseExpr, ofValues)

      # Add branch to if-elif chain
      ifChain.add(newNimNode(nnkElifBranch).add(condition, branchBody))

  # Create a modified if-elif chain where each branch returns the component
  var returnIfChain = newNimNode(nnkIfStmt)

  # Process each branch and add return statements
  for i in 1..<caseStmt.len:
    let branch = caseStmt[i]
    if branch.kind == nnkOfBranch:
      # Extract the value(s) and the body
      let ofValues = branch[0]
      let branchBody = branch[1]

      # Create condition for this branch
      var condition: NimNode
      if ofValues.kind == nnkCurly:  # Multiple values: {val1, val2, val3}
        condition = newNimNode(nnkInfix).add(ident("or"))
        for j in 0..<ofValues.len:
          let eqCheck = newNimNode(nnkInfix).add(ident("=="), caseExpr, ofValues[j])
          if j == 0:
            condition[0] = eqCheck
          elif j == 1:
            condition[1] = eqCheck
          else:
            let newOr = newNimNode(nnkInfix).add(ident("or"), condition, eqCheck)
            condition = newOr
      else:
        condition = newNimNode(nnkInfix).add(ident("=="), caseExpr, ofValues)

      # Wrap the branch body in a return statement
      let returnStmt = newNimNode(nnkReturnStmt).add(branchBody)

      # Add branch with return
      returnIfChain.add(newNimNode(nnkElifBranch).add(condition, returnStmt))

  # Handle else branch if present
  let lastBranch = caseStmt[caseStmt.len - 1]
  if lastBranch.kind == nnkElse:
    let elseBody = lastBranch[0]
    let returnStmt = newNimNode(nnkReturnStmt).add(elseBody)
    returnIfChain.add(newNimNode(nnkElse).add(returnStmt))

  # Generate code that uses the existing reactive conditional system
  # Convert case statement to if-elif chain with reactive conditionals for each branch
  var reactiveCode = newStmtList()

  # Create the main container with window-filling dimensions using Container DSL
  reactiveCode.add quote do:
    let `containerSym` = Container:
      width = `windowWidth`
      height = `windowHeight`

  # For each case branch, create a reactive conditional
  for i in 1..<caseStmt.len:
    let branch = caseStmt[i]
    if branch.kind == nnkOfBranch:
      # Extract the value(s) and the body
      let ofValues = branch[0]
      let branchBody = branch[1]

      # Create condition for this branch
      var condition: NimNode
      if ofValues.kind == nnkCurly:  # Multiple values: {val1, val2, val3}
        # Create (caseExpr == val1 or caseExpr == val2 or caseExpr == val3)
        condition = newNimNode(nnkInfix).add(ident("or"))
        for j in 0..<ofValues.len:
          let eqCheck = newNimNode(nnkInfix).add(ident("=="), caseExpr, ofValues[j])
          if j == 0:
            condition[0] = eqCheck
          elif j == 1:
            condition[1] = eqCheck
          else:
            let newOr = newNimNode(nnkInfix).add(ident("or"), condition, eqCheck)
            condition = newOr
      else:
        # Single value: caseExpr == value
        condition = newNimNode(nnkInfix).add(ident("=="), caseExpr, ofValues)

      # Create reactive conditional for this specific condition using quote
      # CRITICAL FIX: Use lambda expressions for proper closure capture
      let conditionProc = genSym(nskLet, "caseConditionProc")
      let thenProc = genSym(nskLet, "caseThenProc")
      let thenBody = newCall(ident("@"), newNimNode(nnkBracket).add(branchBody))

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = `condition`
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil)

  # Return the container
  reactiveCode.add(containerSym)

  result = reactiveCode

macro Body*(props: untyped, windowWidth: untyped = nil, windowHeight: untyped = nil): untyped =
  ## Body macro - top-level container that fills the window by default
  var
    bodyStmt = newTree(nnkStmtList)
    backgroundSet = false
    posXSet = false
    posYSet = false
    flexGrowSet = false
    widthSet = false
    heightSet = false
    layoutDirectionSet = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "backgroundcolor":
        backgroundSet = true
      of "posx", "x":
        posXSet = true
      of "posy", "y":
        posYSet = true
      of "flexgrow":
        flexGrowSet = true
      of "width":
        widthSet = true
      of "height":
        heightSet = true
      of "layoutdirection":
        layoutDirectionSet = true
      else:
        discard

  # Add defaults first (before user properties)
  if not posXSet:
    bodyStmt.add newTree(nnkAsgn, ident("posX"), newIntLitNode(0))
  if not posYSet:
    bodyStmt.add newTree(nnkAsgn, ident("posY"), newIntLitNode(0))
  if not backgroundSet:
    bodyStmt.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("#101820FF"))
  if not layoutDirectionSet:
    # Default to LAYOUT_COLUMN (value 0) to support contentAlignment and proper centering
    bodyStmt.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))

  # Then add user properties (including width/height from ensureBodyDimensions)
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "backgroundcolor":
        backgroundSet = true
      of "posx", "x":
        posXSet = true
      of "posy", "y":
        posYSet = true
      of "flexgrow":
        flexGrowSet = true
      of "width":
        widthSet = true
      of "height":
        heightSet = true
      of "layoutdirection":
        layoutDirectionSet = true
      bodyStmt.add(node)
    elif node.kind == nnkCaseStmt:
      # Convert case statement to reactive conditional using existing infrastructure
      echo "[kryon][case] Converting case statement to reactive conditional"
      # Use provided window dimensions or defaults
      let bodyWidth = if windowWidth != nil: windowWidth else: newIntLitNode(800)
      let bodyHeight = if windowHeight != nil: windowHeight else: newIntLitNode(600)
      let reactiveResult = convertCaseStmtToReactiveConditional(node, bodyWidth, bodyHeight)
      bodyStmt.add(reactiveResult)
    elif node.kind == nnkIfStmt:
      # Convert if statement to reactive conditional using existing infrastructure
      echo "[kryon][if] Converting if statement to reactive conditional"
      # Use provided window dimensions or defaults
      let bodyWidth = if windowWidth != nil: windowWidth else: newIntLitNode(800)
      let bodyHeight = if windowHeight != nil: windowHeight else: newIntLitNode(600)
      let reactiveResult = convertIfStmtToReactiveConditional(node, bodyWidth, bodyHeight)
      bodyStmt.add(reactiveResult)
    else:
      # Regular node - add as-is
      bodyStmt.add(node)

  result = newTree(nnkCall, ident("Container"), bodyStmt)

macro Text*(props: untyped): untyped =
  ## Text component macro
  var
    textContent = newStrLitNode("Hello")
    colorVal: NimNode = nil
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    fontSizeVal: NimNode = nil
    fontWeightVal: NimNode = nil
    fontFamilyVal: NimNode = nil
    textAlignVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    zIndexVal: NimNode = nil
    isReactive = false
    reactiveVars: seq[NimNode] = @[]
  let textSym = genSym(nskLet, "text")

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "content", "text":
        textContent = value
        # Check if the text content contains reactive expressions
        isReactive = isReactiveExpression(value)
        if isReactive:
          reactiveVars = extractReactiveVars(value)
          # Text generated inside runtime loops uses loopValCopy_*; treat those as static
          if value.repr.contains("loopValCopy"):
            isReactive = false
            reactiveVars = @[]
      of "color", "colour":  # Added 'colour' alias
        colorVal = colorNode(value)
      of "fontsize":
        fontSizeVal = value
      of "fontweight":
        fontWeightVal = value
      of "fontfamily":
        fontFamilyVal = value
      of "textalign":
        textAlignVal = value
      of "margin":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width", "w":
        widthVal = value
      of "height", "h":
        heightVal = value
      of "zindex", "z":
        zIndexVal = value
      else:
        discard

  let marginTopExpr = if marginTopVal != nil: marginTopVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginRightExpr = if marginRightVal != nil: marginRightVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginBottomExpr = if marginBottomVal != nil: marginBottomVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginLeftExpr = if marginLeftVal != nil: marginLeftVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)

  var initStmts = newTree(nnkStmtList)

  # Handle font family (load font if specified)
  if fontFamilyVal != nil:
    initStmts.add quote do:
      let fontId = getFontId(`fontFamilyVal`, 16)
      if fontId > 0:
        # Set font ID on the text component
        let state = cast[ptr KryonTextState](kryon_component_get_state(`textSym`))
        if not state.isNil:
          state.font_id = fontId

  if colorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`textSym`, `colorVal`)

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`textSym`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

  # Set bounds if specified
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let widthExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let heightExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil:
    maskVal = maskVal or 0x01
  if posYVal != nil:
    maskVal = maskVal or 0x02
  if widthVal != nil:
    maskVal = maskVal or 0x04
  if heightVal != nil:
    maskVal = maskVal or 0x08

  if maskVal != 0:
    let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))
    initStmts.add quote do:
      kryon_component_set_bounds_mask(`textSym`,
        toFixed(`xExpr`),
        toFixed(`yExpr`),
        toFixed(`widthExpr`),
        toFixed(`heightExpr`),
        `maskCast`)

  if zIndexVal != nil:
    initStmts.add quote do:
      kryon_component_set_z_index(`textSym`, uint16(`zIndexVal`))

  # Add reactive bindings if detected
  if isReactive and reactiveVars.len > 0:
    # Create live expression evaluation instead of static reactive binding
    let varNames = newLit(reactiveVars.map(proc(x: NimNode): string = x.strVal))
    let expressionStr = newLit(textContent.repr)

    # Add code to create reactive text expression
    # CRITICAL: Create the closure at RUNTIME, not at compile-time, so Nim's
    # closure mechanism properly captures variables and heap-allocates them
    let createExprSym = bindSym("createReactiveTextExpression")
    # FIX: Embed the expression AST directly in the closure body (not pre-evaluated)
    # This allows the closure to re-evaluate the expression each time it's called,
    # reading the current variable values (not capturing evaluated results)
    initStmts.add quote do:
      # Create reactive expression for text
      # The closure body contains the expression AST, which will be evaluated
      # each time the closure is called, capturing variables by reference
      let evalProc = proc (): string {.closure.} =
        `textContent`  # Re-evaluate expression each call
      `createExprSym`(`textSym`, `expressionStr`, evalProc, `varNames`)
      echo "[kryon][reactive] Created live reactive expression: ", `expressionStr`

  result = quote do:
    block:
      let `textSym` = newKryonText(`textContent`)
      `initStmts`
      `textSym`

macro Button*(props: untyped): untyped =
  ## Button component macro
  var
    buttonName = genSym(nskLet, "button")
    buttonText = newStrLitNode("Button")
    childNodes: seq[NimNode] = @[]
    clickHandler = newNilLit()
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    minWidthVal: NimNode = nil
    maxWidthVal: NimNode = nil
    minHeightVal: NimNode = nil
    maxHeightVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    styleName: NimNode = nil
    styleData: NimNode = nil
    textFadeVal: NimNode = nil
    textOverflowVal: NimNode = nil
    opacityVal: NimNode = nil

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "text":
        buttonText = value
      of "minwidth":
        minWidthVal = value
      of "maxwidth":
        maxWidthVal = value
      of "minheight":
        minHeightVal = value
      of "maxheight":
        maxHeightVal = value
      of "onclick":
        # Wrap the click handler to automatically trigger reactive updates
        # For Counter demo, we need to intercept variable changes in increment/decrement
        clickHandler = value
      of "margin":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "padding":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "backgroundcolor":
        bgColorVal = colorNode(value)
      of "color", "textcolor":  # Allow "color" as alias for "textcolor"
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "style":
        # Parse style assignment: style = calendarDayStyle(day)
        if value.kind == nnkCall:
          # Function call: calendarDayStyle(day)
          styleName = value[0]  # The function identifier
          if value.len > 1:
            styleData = value[1]
        elif value.kind == nnkIdent:
          # Simple identifier: myStyle
          styleName = value
        else:
          # Other expression, try to convert to identifier
          styleName = value
      of "textfade":
        textFadeVal = value
      of "textoverflow":
        textOverflowVal = value
      of "opacity":
        opacityVal = value
      else:
        discard
    else:
      # Handle static blocks specially - don't add them to childNodes
      if node.kind == nnkStaticStmt:
        discard  # Static blocks will be handled by the Container macros they contain
      else:
        childNodes.add(node)

  let marginTopExpr = if marginTopVal != nil: marginTopVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginRightExpr = if marginRightVal != nil: marginRightVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginBottomExpr = if marginBottomVal != nil: marginBottomVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginLeftExpr = if marginLeftVal != nil: marginLeftVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)

  let padTopExpr = if paddingTopVal != nil: paddingTopVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)
  let padRightExpr = if paddingRightVal != nil: paddingRightVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)
  let padBottomExpr = if paddingBottomVal != nil: paddingBottomVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)
  let padLeftExpr = if paddingLeftVal != nil: paddingLeftVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)

  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let widthExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let heightExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08
  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  # Normalize click handlers: allow plain calls to be lifted into procs
  if clickHandler.kind notin {nnkNilLit, nnkProcDef, nnkLambda} and clickHandler.kind != nnkIdent:
    clickHandler = quote do:
      proc() =
        `clickHandler`

  var initStmts = newTree(nnkStmtList)

  # Apply style first (before individual property overrides)
  if styleName != nil:
    if styleData != nil:
      initStmts.add quote do:
        `styleName`(`buttonName`, `styleData`)
    else:
      initStmts.add quote do:
        `styleName`(`buttonName`)

  initStmts.add quote do:
    kryon_component_set_bounds_mask(`buttonName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`widthExpr`),
      toFixed(`heightExpr`),
      `maskCast`)

  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`buttonName`,
        uint8(`padTopExpr`),
        uint8(`padRightExpr`),
        uint8(`padBottomExpr`),
        uint8(`padLeftExpr`))

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`buttonName`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

  if bgColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`buttonName`, `bgColorVal`)

  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`buttonName`, `textColorVal`)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`buttonName`, `borderColorVal`)

  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`buttonName`, uint8(`borderWidthVal`))
  else:
    initStmts.add quote do:
      kryon_component_set_border_width(`buttonName`, 1)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`buttonName`, `borderColorVal`)
  else:
    initStmts.add quote do:
      # Default border: white for good contrast with dark button backgrounds
      kryon_component_set_border_color(`buttonName`, rgba(200, 200, 200, 255))

  # Text effect properties
  if textFadeVal != nil:
    # Parse: textFade = horizontal(15) or textFade = none
    if textFadeVal.kind == nnkCall:
      let fadeTypeStr = ($textFadeVal[0]).toLowerAscii()
      var fadeType = ident("IR_TEXT_FADE_NONE")
      var fadeLength = newFloatLitNode(0.0)
      case fadeTypeStr:
      of "horizontal":
        fadeType = ident("IR_TEXT_FADE_HORIZONTAL")
        if textFadeVal.len > 1:
          fadeLength = textFadeVal[1]
      of "vertical":
        fadeType = ident("IR_TEXT_FADE_VERTICAL")
        if textFadeVal.len > 1:
          fadeLength = textFadeVal[1]
      of "radial":
        fadeType = ident("IR_TEXT_FADE_RADIAL")
        if textFadeVal.len > 1:
          fadeLength = textFadeVal[1]
      of "none":
        fadeType = ident("IR_TEXT_FADE_NONE")
      else:
        discard
      initStmts.add quote do:
        kryon_component_set_text_fade(`buttonName`, `fadeType`, float(`fadeLength`))
    elif textFadeVal.kind == nnkIdent and ($textFadeVal).toLowerAscii() == "none":
      let fadeNone = ident("IR_TEXT_FADE_NONE")
      initStmts.add quote do:
        kryon_component_set_text_fade(`buttonName`, `fadeNone`, 0.0)

  if textOverflowVal != nil:
    let overflowStr = if textOverflowVal.kind == nnkIdent: ($textOverflowVal).toLowerAscii() else: ""
    var overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
    case overflowStr:
    of "visible": overflowType = ident("IR_TEXT_OVERFLOW_VISIBLE")
    of "clip": overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
    of "ellipsis": overflowType = ident("IR_TEXT_OVERFLOW_ELLIPSIS")
    of "fade": overflowType = ident("IR_TEXT_OVERFLOW_FADE")
    else: discard
    initStmts.add quote do:
      kryon_component_set_text_overflow(`buttonName`, `overflowType`)

  if opacityVal != nil:
    initStmts.add quote do:
      kryon_component_set_opacity(`buttonName`, float(`opacityVal`))

  # Apply min/max width/height constraints via layout
  if minWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_min_width(layout, `dimType`, cfloat(`minWidthVal`))

  if maxWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_max_width(layout, `dimType`, cfloat(`maxWidthVal`))

  if minHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_min_height(layout, `dimType`, cfloat(`minHeightVal`))

  if maxHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_max_height(layout, `dimType`, cfloat(`maxHeightVal`))

  let newButtonSym = bindSym("newKryonButton")
  let buttonBridgeSym = bindSym("nimButtonBridge")
  let registerHandlerSym = bindSym("registerButtonHandler")

  # Create button (IR system handles events separately via registerButtonHandler)
  var buttonCall = newCall(newButtonSym, buttonText)
  if clickHandler.kind != nnkNilLit:
    # Register handler separately using IR event system
    initStmts.add quote do:
      `registerHandlerSym`(`buttonName`, `clickHandler`)

  initStmts.add quote do:
    kryon_component_mark_dirty(`buttonName`)

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComp = block:
        `child`
      if childComp != nil:
        discard kryon_component_add_child(`buttonName`, childComp)

  result = quote do:
    block:
      let `buttonName` = `buttonCall`
      `initStmts`
      `childStmtList`
      `buttonName`

macro Dropdown*(props: untyped): untyped =
  ## Dropdown component macro
  var
    dropdownName = genSym(nskLet, "dropdown")
    placeholderVal = newStrLitNode("Select...")
    optionsVal: NimNode = nil
    selectedIndexVal: NimNode = newIntLitNode(-1)
    clickHandler = newNilLit()
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    hoverColorVal: NimNode = nil
    fontSizeVal: NimNode = nil

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = value
      of "options":
        optionsVal = value
      of "selectedindex":
        selectedIndexVal = value
      of "onchange":
        clickHandler = value
      of "margin":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "padding":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "backgroundcolor":
        bgColorVal = colorNode(value)
      of "color", "textcolor":
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "hovercolor":
        hoverColorVal = colorNode(value)
      of "fontsize":
        fontSizeVal = value
      else:
        discard

  let marginTopExpr = if marginTopVal != nil: marginTopVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginRightExpr = if marginRightVal != nil: marginRightVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginBottomExpr = if marginBottomVal != nil: marginBottomVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginLeftExpr = if marginLeftVal != nil: marginLeftVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)

  let padTopExpr = if paddingTopVal != nil: paddingTopVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)
  let padRightExpr = if paddingRightVal != nil: paddingRightVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)
  let padBottomExpr = if paddingBottomVal != nil: paddingBottomVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)
  let padLeftExpr = if paddingLeftVal != nil: paddingLeftVal
    elif paddingAll != nil: paddingAll
    else: newIntLitNode(0)

  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let widthExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let heightExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08
  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  # Build initialization statements
  var initStmts = newTree(nnkStmtList)

  # Set bounds
  initStmts.add quote do:
    kryon_component_set_bounds_mask(`dropdownName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`widthExpr`),
      toFixed(`heightExpr`),
      `maskCast`)

  # Set padding if specified
  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`dropdownName`,
        uint8(`padTopExpr`),
        uint8(`padRightExpr`),
        uint8(`padBottomExpr`),
        uint8(`padLeftExpr`))

  # Set margin if specified
  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`dropdownName`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

  # Note: Colors and styles are set through the state during creation,
  # so they don't need to be set after component creation

  initStmts.add quote do:
    kryon_component_mark_dirty(`dropdownName`)

  # Build the dropdown creation call with all parameters
  let newDropdownSym = bindSym("newKryonDropdown")
  let optionsExpr = if optionsVal != nil: optionsVal
    else: newNimNode(nnkPrefix).add(ident("@"), newNimNode(nnkBracket))

  # Build named arguments for the creation function
  var creationStmt = newTree(nnkCall, newDropdownSym)
  creationStmt.add(newTree(nnkExprEqExpr, ident("placeholder"), placeholderVal))
  creationStmt.add(newTree(nnkExprEqExpr, ident("options"), optionsExpr))
  creationStmt.add(newTree(nnkExprEqExpr, ident("selectedIndex"), selectedIndexVal))

  # Add optional parameters if they were specified
  if fontSizeVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("fontSize"), newCall(ident("uint8"), fontSizeVal)))
  if textColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("textColor"), textColorVal))
  if bgColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("backgroundColor"), bgColorVal))
  if borderColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("borderColor"), borderColorVal))
  if borderWidthVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("borderWidth"), newCall(ident("uint8"), borderWidthVal)))
  if hoverColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("hoverColor"), hoverColorVal))
  if clickHandler.kind != nnkNilLit:
    creationStmt.add(newTree(nnkExprEqExpr, ident("onChangeCallback"), clickHandler))

  result = quote do:
    block:
      let `dropdownName` = `creationStmt`
      `initStmts`
      `dropdownName`

macro Checkbox*(props: untyped): untyped =
  ## Checkbox component macro
  var
    checkboxName = genSym(nskLet, "checkbox")
    clickHandler = newNilLit()
    labelVal: NimNode = newStrLitNode("")
    checkedVal: NimNode = newLit(false)
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    fontSizeVal: NimNode = nil
    textColorVal: NimNode = nil
    backgroundColorVal: NimNode = nil

  # Parse Checkbox properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "label":
        if node[1].kind == nnkStrLit:
          labelVal = node[1]
      of "checked":
        if node[1].kind == nnkIdent or node[1].kind in {nnkIntLit, nnkIdent}:
          checkedVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "fontsize":
        fontSizeVal = node[1]
      of "textcolor":
        textColorVal = colorNode(node[1])
      of "backgroundcolor":
        backgroundColorVal = colorNode(node[1])
      of "onclick", "onchange":
        if node[1].kind == nnkIdent:
          clickHandler = node[1]
      else:
        discard

  var initStmts = newTree(nnkStmtList)

  # Set width and height using IR style system
  if widthVal != nil:
    initStmts.add quote do:
      setWidth(`checkboxName`, `widthVal`)
  if heightVal != nil:
    initStmts.add quote do:
      setHeight(`checkboxName`, `heightVal`)

  # Set font size if provided
  if fontSizeVal != nil:
    initStmts.add quote do:
      setFontSize(`checkboxName`, `fontSizeVal`)

  # Set text color if provided (for the label text)
  if textColorVal != nil:
    initStmts.add quote do:
      setTextColor(`checkboxName`, runtime.parseColor(`textColorVal`))

  # Set background color if provided
  if backgroundColorVal != nil:
    initStmts.add quote do:
      setBgColor(`checkboxName`, runtime.parseColor(`backgroundColorVal`))

  # Store checked state in custom_data
  initStmts.add quote do:
    setCheckboxState(`checkboxName`, `checkedVal`)

  # Register click handler if provided
  if clickHandler.kind != nnkNilLit:
    initStmts.add quote do:
      registerCheckboxHandler(`checkboxName`, proc() = `clickHandler`())

  result = quote do:
    block:
      let `checkboxName` = newKryonCheckbox(`labelVal`)
      `initStmts`
      `checkboxName`

macro Input*(props: untyped): untyped =
  ## Input component macro
  ## Note: Text editing not yet supported by C core, shows as styled container
  var
    inputName = genSym(nskLet, "input")
    placeholderVal: NimNode = newStrLitNode("")
    valueVal: NimNode = newStrLitNode("")
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    backgroundColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    fontSizeVal: NimNode = newIntLitNode(14)
    onTextChangeVal: NimNode = nil
    childNodes: seq[NimNode] = @[]

  # Parse Input properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = node[1]
      of "value":
        valueVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "posx", "x":
        posXVal = node[1]
      of "posy", "y":
        posYVal = node[1]
      of "backgroundcolor":
        backgroundColorVal = colorNode(node[1])
      of "textcolor", "color":
        textColorVal = colorNode(node[1])
      of "bordercolor":
        borderColorVal = colorNode(node[1])
      of "borderwidth":
        borderWidthVal = node[1]
      of "fontsize":
        fontSizeVal = node[1]
      of "ontextchange":
        onTextChangeVal = node[1]
      of "inputtype", "min", "max", "step", "accept":
        # Store these for future implementation
        discard
      else:
        discard
    else:
      childNodes.add(node)

  var initStmts = newTree(nnkStmtList)

  # Use bounds_mask to allow layout system to position the input
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let wExpr = if widthVal != nil: widthVal else: newIntLitNode(200)
  let hExpr = if heightVal != nil: heightVal else: newIntLitNode(40)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08
  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  initStmts.add quote do:
    kryon_component_set_bounds_mask(`inputName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`wExpr`),
      toFixed(`hExpr`),
      `maskCast`)

  # Add padding
  initStmts.add quote do:
    kryon_component_set_padding(`inputName`, 8, 8, 8, 8)

  # Apply colors if provided (accept string or uint32)
  if backgroundColorVal != nil:
    let bgExpr =
      if backgroundColorVal.kind == nnkStrLit: quote do: runtime.parseColor(`backgroundColorVal`)
      else: backgroundColorVal
    initStmts.add quote do:
      setBackgroundColor(`inputName`, `bgExpr`)

  if textColorVal != nil:
    let tcExpr =
      if textColorVal.kind == nnkStrLit: quote do: runtime.parseColor(`textColorVal`)
      else: textColorVal
    initStmts.add quote do:
      setTextColor(`inputName`, `tcExpr`)

  # Store placeholder separately; only set real value as text content
  initStmts.add quote do:
    setCustomData(`inputName`, `placeholderVal`)
    if `valueVal`.len > 0:
      setText(`inputName`, `valueVal`)
    else:
      setText(`inputName`, "")

  # Wire onChange/value binding if provided
  let hasValueBinding = valueVal.kind == nnkIdent
  if hasValueBinding and onTextChangeVal != nil:
    initStmts.add quote do:
      registerInputHandler(`inputName`, proc(t: string) =
        `valueVal` = t
        `onTextChangeVal`(t)
      )
  elif hasValueBinding:
    initStmts.add quote do:
      registerInputHandler(`inputName`, proc(t: string) =
        `valueVal` = t
      )
  elif onTextChangeVal != nil:
    initStmts.add quote do:
      registerInputHandler(`inputName`, proc(t: string) =
        `onTextChangeVal`(t)
      )

  # Keep the component value in sync with the bound variable (e.g., when cleared in code)
  if hasValueBinding:
    let syncProcSym = genSym(nskProc, "syncInputValue")
    initStmts.add quote do:
      proc `syncProcSym`() =
        setText(`inputName`, `valueVal`)
      registerReactiveRebuild(`syncProcSym`)
      `syncProcSym`()

  # Add any child components (though Input usually doesn't have children)
  for child in childNodes:
    if isEchoStatement(child):
      initStmts.add(child)
    else:
      initStmts.add quote do:
        let childComponent = block:
          `child`
        if childComponent != nil:
          discard kryon_component_add_child(`inputName`, childComponent)

  let onTextChangeExpr = if onTextChangeVal != nil: onTextChangeVal else: newNilLit()
  
  # Prepare color expressions
  let textColorExpr = if textColorVal != nil:
    if textColorVal.kind == nnkStrLit: newCall(ident("parseColor"), textColorVal) else: textColorVal
  else: newIntLitNode(0)
  
  let bgColorExpr = if backgroundColorVal != nil:
    if backgroundColorVal.kind == nnkStrLit: newCall(ident("parseColor"), backgroundColorVal) else: backgroundColorVal
  else: newIntLitNode(0)
  
  let borderColorExpr = if borderColorVal != nil:
    if borderColorVal.kind == nnkStrLit: newCall(ident("parseColor"), borderColorVal) else: borderColorVal
  else: newIntLitNode(0)

  result = quote do:
    block:
      # Use the new IR-based Input component (placeholder/value currently unused)
      let `inputName` = newKryonInput()
      `initStmts`
      kryon_component_mark_dirty(`inputName`)
      `inputName`

macro Canvas*(props: untyped): untyped =
  ## Canvas component macro - IR version
  var
    canvasName = genSym(nskLet, "canvas")
    widthVal: NimNode = newIntLitNode(800)
    heightVal: NimNode = newIntLitNode(600)
    bgColorVal: NimNode = newStrLitNode("#000000")
    onDrawVal: NimNode = newNilLit()
    hasOnDraw = false

  # Extract properties from props
  for node in props.children:
    if node.kind == nnkAsgn:
      let identName = node[0]
      let value = node[1]

      case $identName
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      of "backgroundColor":
        bgColorVal = value
      of "onDraw":
        onDrawVal = value
        hasOnDraw = true
      else:
        discard

  let registerHandlerSym = bindSym("registerCanvasHandler")
  let newCanvasSym = bindSym("newKryonCanvas")
  let setBgColorSym = bindSym("setBackgroundColor")
  let setWidthSym = bindSym("setWidth")
  let setHeightSym = bindSym("setHeight")

  if hasOnDraw:
    result = quote do:
      block:
        let `canvasName` = `newCanvasSym`()
        # Set canvas size using IR style system
        `setWidthSym`(`canvasName`, `widthVal`)
        `setHeightSym`(`canvasName`, `heightVal`)
        # Set canvas background color
        `setBgColorSym`(`canvasName`, runtime.parseColor(`bgColorVal`))
        # Register the onDraw handler
        `registerHandlerSym`(`canvasName`, `onDrawVal`)
        `canvasName`
  else:
    result = quote do:
      block:
        let `canvasName` = `newCanvasSym`()
        # Set canvas size using IR style system
        `setWidthSym`(`canvasName`, `widthVal`)
        `setHeightSym`(`canvasName`, `heightVal`)
        # Set canvas background color
        `setBgColorSym`(`canvasName`, runtime.parseColor(`bgColorVal`))
        `canvasName`

macro Spacer*(props: untyped): untyped =
  ## Spacer component macro
  var
    spacerName = genSym(nskLet, "spacer")

  discard props

  result = quote do:
    block:
      let `spacerName` = newKryonSpacer()
      `spacerName`

macro Markdown*(props: untyped): untyped =
  ## Markdown component macro for rich text formatting
  ## Creates an IR_COMPONENT_MARKDOWN that will be rendered by the backend
  var
    markdownName = genSym(nskLet, "markdown")
    sourceVal: NimNode = newStrLitNode("")
    widthVal: NimNode = newIntLitNode(0)
    heightVal: NimNode = newIntLitNode(0)

  # Extract properties from props
  for node in props.children:
    if node.kind == nnkAsgn:
      let identName = node[0]
      let value = node[1]

      case $identName
      of "source":
        sourceVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      else:
        # Unknown properties ignored for now (file, theme, scrollable, onLinkClick, etc.)
        # Backend will handle all markdown rendering
        discard

  # Generate IR component creation code
  result = quote do:
    block:
      let `markdownName` = newKryonMarkdown(`sourceVal`)
      # Apply width/height if specified
      if `widthVal` > 0:
        `markdownName`.setWidth(`widthVal`)
      if `heightVal` > 0:
        `markdownName`.setHeight(`heightVal`)
      `markdownName`

macro Row*(props: untyped): untyped =
  ## Convenience macro for a row layout container
  ## Preserves user-specified properties and sets defaults for missing ones
  var body = newTree(nnkStmtList)

  # Track which properties are set by user
  var hasWidth = false
  var hasHeight = false
  var hasMainAxisAlignment = false
  var hasAlignItems = false
  var hasBackgroundColor = false

  # First pass: detect which properties user has set
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName == "width": hasWidth = true
      if propName == "height": hasHeight = true
      if propName == "mainAxisAlignment": hasMainAxisAlignment = true
      if propName == "alignItems": hasAlignItems = true
      if propName == "backgroundColor": hasBackgroundColor = true

  # Add defaults first (only if not specified by user)
  # Row should have transparent background by default to avoid double-rendering inherited colors
  if not hasBackgroundColor:
    body.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("transparent"))
  # Note: Don't set default width or height for containers - let them size based on content
  # if not hasWidth:
  #   body.add newTree(nnkAsgn, ident("width"), newIntLitNode(800))  # Default width
  # if not hasHeight:
  #   body.add newTree(nnkAsgn, ident("height"), newIntLitNode(500)) # Default height
  if not hasMainAxisAlignment:
    body.add newTree(nnkAsgn, ident("mainAxisAlignment"), newStrLitNode("start"))
  if not hasAlignItems:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))

  # Then add user properties (so they override defaults if there are duplicates)
  for node in props.children:
    body.add(node)


  # Always set layout direction to row (at the end so it always overrides)
  body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(1))  # 1 = KRYON_LAYOUT_ROW

  result = newTree(nnkCall, ident("Container"), body)

macro Column*(props: untyped): untyped =
  ## Convenience macro for a column layout container
  ## Preserves user-specified properties and sets defaults for missing ones
  var body = newTree(nnkStmtList)

  # Track which properties are set by user
  var hasWidth = false
  var hasHeight = false
  var hasMainAxisAlignment = false
  var hasAlignItems = false
  var hasFlexGrow = false
  var hasLayoutDirection = false
  var hasBackgroundColor = false

  # First pass: detect which properties user has set
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName == "width": hasWidth = true
      if propName == "height": hasHeight = true
      if propName == "mainAxisAlignment": hasMainAxisAlignment = true
      if propName == "alignItems": hasAlignItems = true
      if propName == "flexGrow": hasFlexGrow = true
      if propName == "layoutDirection": hasLayoutDirection = true
      if propName == "backgroundColor": hasBackgroundColor = true

  # Add defaults first (only if not specified by user)
  # Column should have transparent background by default to avoid double-rendering inherited colors
  if not hasBackgroundColor:
    body.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("transparent"))
  # Column should fill available height by default
  if not hasFlexGrow:
    body.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))  # Fill available height
  # Don't set default width - let Column fill available width naturally
  if not hasMainAxisAlignment:
    body.add newTree(nnkAsgn, ident("mainAxisAlignment"), newStrLitNode("start"))
  if not hasAlignItems:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("stretch"))

  # Then add user properties (so they override defaults if there are duplicates)
  for node in props.children:
    body.add(node)

  
  # Always set layout direction to column (at the end so it always overrides)
  body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))  # 0 = KRYON_LAYOUT_COLUMN

  result = newTree(nnkCall, ident("Container"), body)

macro Center*(props: untyped): untyped =
  ## Convenience macro that centers its children both horizontally and vertically.
  ## Creates an IR_COMPONENT_CENTER which handles centering in the renderer.
  let centerSym = genSym(nskLet, "center")
  let newCenterSym = bindSym("newKryonCenter")
  let addChildSym = bindSym("kryon_component_add_child")

  var initStmts = newStmtList()
  var childNodes: seq[NimNode] = @[]

  # Process properties and children
  for node in props.children:
    if node.kind in {nnkCall, nnkCommand, nnkIdent}:
      # This is a child component
      childNodes.add(node)
    elif node.kind == nnkAsgn:
      # Property assignment - apply to center component
      let propName = $node[0]
      let propValue = node[1]
      initStmts.add quote do:
        when compiles(`centerSym`.`propName` = `propValue`):
          `centerSym`.`propName` = `propValue`

  # Add children to center component
  for child in childNodes:
    if isEchoStatement(child):
      initStmts.add(child)
    else:
      initStmts.add quote do:
        let centerChild = block:
          `child`
        discard `addChildSym`(`centerSym`, centerChild)

  result = quote do:
    block:
      let `centerSym` = `newCenterSym`()
      `initStmts`
      `centerSym`

# ============================================================================
# Tabs
# ============================================================================

macro TabGroup*(props: untyped): untyped =
  let createSym = bindSym("createTabGroupState")
  let finalizeSym = bindSym("finalizeTabGroup")
  let addChildSym = bindSym("kryon_component_add_child")
  let ctxSym = ident("__kryonCurrentTabGroup")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]
  var selectedIndexVal: NimNode = newIntLitNode(0)
  var reorderableVal: NimNode = newLit(false)
  var hasLayout = false
  var hasGap = false
  var hasAlign = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "selectedindex":
        selectedIndexVal = node[1]
      of "reorderable":
        reorderableVal = node[1]
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "gap":
        hasGap = true
        propertyNodes.add(node)
      of "alignitems":
        hasAlign = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))
  if not hasAlign:
    # Stretch children to fill TabGroup width - enables flex_grow on tabs
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("stretch"))
  if not hasGap:
    body.add newTree(nnkAsgn, ident("gap"), newIntLitNode(8))
  for node in propertyNodes:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let groupSym = genSym(nskLet, "tabGroup")
  let stateSym = genSym(nskLet, "tabGroupState")

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`groupSym`, childComponent)

  let panelIndexSym = ident("__kryonTabPanelIndex")

  result = quote do:
    block:
      let `groupSym` = `containerCall`
      # TabGroup state needs group, tab bar, tab content, selected index, reorderable flag
      let `stateSym` = `createSym`(`groupSym`, nil, nil, `selectedIndexVal`, `reorderableVal`)
      let `ctxSym` = `stateSym`
      var `panelIndexSym` = 0  # Counter for panel indices
      block:
        `childStmtList`
      `finalizeSym`(`stateSym`)
      # Register state for runtime access by reactive system
      registerTabGroupState(`groupSym`, `stateSym`)
      `groupSym`

macro TabBar*(props: untyped): untyped =
  let registerSym = bindSym("registerTabBar")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let ctxBarSym = ident("__kryonCurrentTabBar")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]
  var reorderableVal: NimNode = newLit(false)
  var hasLayout = false
  var hasGap = false
  var hasAlign = false
  var hasPadding = false
  var hasBackground = false
  var hasBorderColor = false
  var hasBorderWidth = false
  var hasFlex = false
  var hasWidth = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "reorderable":
        reorderableVal = node[1]
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "gap":
        hasGap = true
        propertyNodes.add(node)
      of "alignitems", "crossaxisalignment":
        hasAlign = true
        propertyNodes.add(node)
      of "padding":
        hasPadding = true
        propertyNodes.add(node)
      of "backgroundcolor":
        hasBackground = true
        propertyNodes.add(node)
      of "bordercolor":
        hasBorderColor = true
        propertyNodes.add(node)
      of "borderwidth":
        hasBorderWidth = true
        propertyNodes.add(node)
      of "flexgrow":
        hasFlex = true
        propertyNodes.add(node)
      of "width":
        hasWidth = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(1))
  if not hasAlign:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  if not hasGap:
    # Chrome-like tabs: minimal gap between tabs
    body.add newTree(nnkAsgn, ident("gap"), newIntLitNode(2))
  if not hasPadding:
    body.add newTree(nnkAsgn, ident("paddingTop"), newIntLitNode(4))
    body.add newTree(nnkAsgn, ident("paddingBottom"), newIntLitNode(4))
    body.add newTree(nnkAsgn, ident("paddingLeft"), newIntLitNode(8))
    body.add newTree(nnkAsgn, ident("paddingRight"), newIntLitNode(8))
  if not hasBackground:
    body.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("#202124"))
  if not hasBorderWidth:
    body.add newTree(nnkAsgn, ident("borderWidth"), newIntLitNode(1))
  if not hasBorderColor:
    body.add newTree(nnkAsgn, ident("borderColor"), newStrLitNode("#2b2f33"))
  # Add explicit height to prevent TabBar visibility issues
  var hasHeight = false
  var otherProperties: seq[NimNode] = @[]

  for node in propertyNodes:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName.toLowerAscii() == "height":
        hasHeight = true
        body.add(node)
      else:
        # Only add properties we haven't already handled
        otherProperties.add(node)

  # Add remaining properties
  for node in otherProperties:
    body.add(node)

  # Set default height if not provided
  if not hasHeight:
    body.add newTree(nnkAsgn, ident("height"), newIntLitNode(40))

  # Remove default flex grow - TabBar should maintain its fixed height
  # TabBar expands horizontally due to width=100% in its parent, not flex grow

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let barSym = genSym(nskLet, "tabBar")

  var childStmtList = newStmtList()
  for node in childNodes:
    if isEchoStatement(node):
      childStmtList.add(node)
      continue
    if node.kind == nnkForStmt:
      # Handle runtime for loops with Tab components
      # Structure: nnkForStmt[loopVar, collection, body]
      let loopVar = node[0]
      let collection = node[1]
      let body = node[2]

      # CRITICAL FIX: Use immediately-invoked closure to force value capture
      # Without this, all closures share the same loop variable and see the final value
      let capturedParam = genSym(nskParam, "capturedIdx")
      let transformedBody = transformLoopVariableReferences(body, loopVar, capturedParam)

      # Create immediately-invoked closure: (proc(capturedIdx: typeof(loopVar)) = body)(loopVar)
      let closureProc = newProc(
        name = newEmptyNode(),
        params = [newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
        body = transformedBody,
        procType = nnkLambda
      )
      closureProc.addPragma(ident("closure"))
      let closureCall = newCall(closureProc, loopVar)

      # Generate code that iterates at runtime and processes Tab components
      childStmtList.add newTree(nnkForStmt, loopVar, collection, newStmtList(closureCall))
    else:
      # Handle regular child nodes
      if node.kind == nnkCall and node[0].kind == nnkIdent and node[0].eqIdent("Tab"):
        # Tab components are statements that self-register, add them directly
        childStmtList.add(node)
      else:
        # Other components (like Button) return values - add them as children to TabBar
        let componentSym = genSym(nskLet, "tabBarChild")
        let addChildSym = bindSym("kryon_component_add_child")
        childStmtList.add quote do:
          let `componentSym` = `node`
          discard `addChildSym`(`ctxBarSym`, `componentSym`)

  result = quote do:
    block:
      let `barSym` = `containerCall`
      `registerSym`(`barSym`, `ctxSym`, `reorderableVal`)
      let `ctxBarSym` = `barSym`
      block:
        `childStmtList`
      `barSym`

macro TabContent*(props: untyped): untyped =
  let registerSym = bindSym("registerTabContent")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let addChildSym = bindSym("kryon_component_add_child")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]
  var hasLayout = false
  var hasFlex = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "flexgrow":
        hasFlex = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))
  if not hasFlex:
    body.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))
  for node in propertyNodes:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let contentSym = genSym(nskLet, "tabContent")

  var childStmtList = newStmtList()
  for node in childNodes:
    if isEchoStatement(node):
      childStmtList.add(node)
      continue
    if node.kind == nnkForStmt:
      # Handle runtime for loops with TabPanel components
      # Structure: nnkForStmt[loopVar, collection, body]
      let loopVar = node[0]
      let collection = node[1]
      let body = node[2]

      # CRITICAL FIX: Use immediately-invoked closure to force value capture
      # Without this, all closures share the same loop variable and see the final value
      let capturedParam = genSym(nskParam, "capturedIdx")
      let transformedBody = transformLoopVariableReferences(body, loopVar, capturedParam)

      # Generate code that iterates at runtime and processes TabPanel components
      # TabPanel components return values that need to be added as children
      # Use immediately-invoked closure to capture the loop variable value
      childStmtList.add quote do:
        for `loopVar` in `collection`:
          (proc(`capturedParam`: typeof(`loopVar`)) {.closure.} =
            let panelChild = block:
              `transformedBody`
            if panelChild != nil:
              discard `addChildSym`(`contentSym`, panelChild)
          )(`loopVar`)
    else:
      # Handle regular child nodes
      if node.kind == nnkCall and node[0].kind == nnkIdent and node[0].eqIdent("TabPanel"):
        # TabPanel components self-register and are added as children, so we need to capture the result
        childStmtList.add quote do:
          let panelChild = block:
            `node`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)
      else:
        # Other components
        childStmtList.add quote do:
          let panelChild = block:
            `node`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)

  result = quote do:
    block:
      let `contentSym` = `containerCall`
      `registerSym`(`contentSym`, `ctxSym`)
      block:
        `childStmtList`
      `contentSym`

macro TabPanel*(props: untyped): untyped =
  let registerSym = bindSym("registerTabPanel")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let panelIndexSym = ident("__kryonTabPanelIndex")

  var body = newTree(nnkStmtList)
  for node in props.children:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let panelSym = genSym(nskLet, "tabPanel")

  result = quote do:
    block:
      let `panelSym` = `containerCall`
      `registerSym`(`panelSym`, `ctxSym`, `panelIndexSym`)
      `panelIndexSym` += 1  # Increment for next panel
      `panelSym`

macro Tab*(props: untyped): untyped =
  let registerSym = bindSym("registerTabComponent")
  let visualType = bindSym("TabVisualState")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let ctxBarSym = ident("__kryonCurrentTabBar")

  var buttonProps = newTree(nnkStmtList)
  var extraChildren: seq[NimNode] = @[]
  var titleVal: NimNode = newStrLitNode("Tab")
  var backgroundColorExpr: NimNode = newStrLitNode("#292C30")
  var activeBackgroundExpr: NimNode = newStrLitNode("#3C4047")
  var textColorExpr: NimNode = newStrLitNode("#C7C9CC")
  var activeTextExpr: NimNode = newStrLitNode("#FFFFFF")
  var onClickExpr: NimNode = nil
  var hasPadding = false
  var hasMargin = false
  var hasBackground = false
  var hasTextColor = false
  var hasBorderColor = false
  var hasBorderWidth = false
  var hasWidth = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "title":
        titleVal = node[1]
      of "backgroundcolor":
        backgroundColorExpr = node[1]
        buttonProps.add(node)
        hasBackground = true
      of "activebackgroundcolor":
        activeBackgroundExpr = node[1]
      of "textcolor":
        textColorExpr = node[1]
        buttonProps.add(node)
        hasTextColor = true
      of "activetextcolor":
        activeTextExpr = node[1]
      of "bordercolor":
        hasBorderColor = true
        buttonProps.add(node)
      of "borderwidth":
        hasBorderWidth = true
        buttonProps.add(node)
      of "padding":
        hasPadding = true
        buttonProps.add(node)
      of "margin":
        hasMargin = true
        buttonProps.add(node)
      of "width":
        hasWidth = true
        buttonProps.add(node)
      of "onclick":
        onClickExpr = node[1]
      else:
        buttonProps.add(node)
    else:
        extraChildren.add(node)

  if not hasBackground:
    buttonProps.add newTree(nnkAsgn, ident("backgroundColor"), copyNimTree(backgroundColorExpr))
  if not hasTextColor:
    buttonProps.add newTree(nnkAsgn, ident("color"), copyNimTree(textColorExpr))
  if not hasBorderColor:
    buttonProps.add newTree(nnkAsgn, ident("borderColor"), newStrLitNode("#4C5057"))
  if not hasBorderWidth:
    buttonProps.add newTree(nnkAsgn, ident("borderWidth"), newIntLitNode(1))
  # Don't set fixed width - tabs will fill available space via flex_grow
  buttonProps.add newTree(nnkAsgn, ident("height"), newIntLitNode(32))
  buttonProps.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  buttonProps.add newTree(nnkAsgn, ident("justifyContent"), newStrLitNode("center"))
  # Chrome-like tabs: min/max width for responsive behavior
  # minWidth=16: allows tabs to shrink very small (text fades out with ellipsis)
  # maxWidth=180: prevents excessive expansion (Chrome-like behavior)
  buttonProps.add newTree(nnkAsgn, ident("minWidth"), newIntLitNode(16))
  buttonProps.add newTree(nnkAsgn, ident("maxWidth"), newIntLitNode(180))
  buttonProps.add newTree(nnkAsgn, ident("minHeight"), newIntLitNode(28))

  # Tab default: text fades when overflowing (Chrome-like behavior)
  buttonProps.add newTree(nnkAsgn, ident("textFade"), newCall(ident("horizontal"), newIntLitNode(15)))
  buttonProps.add newTree(nnkAsgn, ident("textOverflow"), ident("fade"))

  buttonProps.add newTree(nnkAsgn, ident("text"), titleVal)

  # ALWAYS add onClick handler to ensure IR_EVENT_CLICK is created
  # This is required for automatic tab switching to work
  # The C renderer detects tab clicks via IR_EVENT_CLICK with "nim_button_" logic_id
  if onClickExpr != nil:
    # User provided onClick - use it
    buttonProps.add newTree(nnkAsgn, ident("onClick"), onClickExpr)
  else:
    # No onClick - add dummy handler to ensure event is created
    # The C layer handles tab switching, this just triggers event creation
    let dummyHandler = quote do:
      proc() = discard
    buttonProps.add newTree(nnkAsgn, ident("onClick"), dummyHandler)

  for child in extraChildren:
    buttonProps.add(child)

  let buttonCall = newTree(nnkCall, ident("Button"), buttonProps)
  let tabSym = genSym(nskLet, "tabComponent")
  let centerSetter = bindSym("kryon_button_set_center_text")
  let ellipsizeSetter = bindSym("kryon_button_set_ellipsize")
  var afterCreate = newStmtList()

  if not hasPadding:
    # Compact padding for tabs: 6px vertical, 10px horizontal
    afterCreate.add quote do:
      kryon_component_set_padding(`tabSym`, 6'u8, 10'u8, 6'u8, 10'u8)

  if not hasMargin:
    # Chrome-like tabs: minimal margin between tabs
    afterCreate.add quote do:
      kryon_component_set_margin(`tabSym`, 2'u8, 1'u8, 0'u8, 1'u8)

  # Flex behavior: tabs with explicit width should NOT shrink (flexShrink=0)
  # Tabs without explicit width fill available space (flexGrow=1, flexShrink=1)
  if hasWidth:
    afterCreate.add quote do:
      kryon_component_set_layout_alignment(`tabSym`, kaCenter, kaCenter)
      # Tab has fixed width - don't allow shrinking
      kryon_component_set_flex(`tabSym`, 0'u8, 0'u8)
      kryon_component_set_layout_direction(`tabSym`, 1)
      `centerSetter`(`tabSym`, false)
      `ellipsizeSetter`(`tabSym`, true)
  else:
    afterCreate.add quote do:
      kryon_component_set_layout_alignment(`tabSym`, kaCenter, kaCenter)
      # Tabs fill available space evenly (flexGrow=1, flexShrink=1)
      kryon_component_set_flex(`tabSym`, 1'u8, 1'u8)
      kryon_component_set_layout_direction(`tabSym`, 1)
      `centerSetter`(`tabSym`, false)
      `ellipsizeSetter`(`tabSym`, true)

  let visualNode = newTree(nnkObjConstr, visualType,
    newTree(nnkExprColonExpr, ident("backgroundColor"), colorNode(copyNimTree(backgroundColorExpr))),
    newTree(nnkExprColonExpr, ident("activeBackgroundColor"), colorNode(copyNimTree(activeBackgroundExpr))),
    newTree(nnkExprColonExpr, ident("textColor"), colorNode(copyNimTree(textColorExpr))),
    newTree(nnkExprColonExpr, ident("activeTextColor"), colorNode(copyNimTree(activeTextExpr)))
  )

  result = quote do:
    block:
      # Check if TabGroup context is available
      if `ctxSym` == nil:
        echo "[kryon][error] Tab component created outside of TabGroup context: ", `titleVal`
        discard ()

      let `tabSym` = `buttonCall`
      if `tabSym` == nil:
        echo "[kryon][error] Failed to create tab button: ", `titleVal`
        discard ()

      `afterCreate`
      # Ensure the tab component is visible by marking dirty
      kryon_component_mark_dirty(`tabSym`)
      # Register tab with current tab group (index handled in backend; use 0 placeholder)
      `registerSym`(`tabSym`, `ctxSym`, `visualNode`, 0)
      # Attach tab button into current TabBar so it renders
      if `ctxBarSym` != nil:
        discard kryon_component_add_child(`ctxBarSym`, `tabSym`)
      echo "[kryon][debug] Created tab button: ", `titleVal`, " component: ", cast[uint](`tabSym`)
      # Tab components are registered with the tab group, explicitly return void
      discard ()

# ============================================================================
# Static For Loop Macro
# ============================================================================

macro staticFor*(loopStmt, bodyStmt: untyped): untyped =
  ## Static for loop macro that expands at compile time
  ## Usage: staticFor(item in items):
  ##           Component:
  ##             property = item.value
  ## This expands the loop at compile time, generating multiple components
  ##
  ## NOTE: The collection must be a compile-time constant (const or literal)

  # Parse the loop statement (e.g., "item in items")
  expectKind loopStmt, nnkInfix

  let loopVar = loopStmt[1]      # "item" - the loop variable
  let collection = loopStmt[2]   # "items" - the collection to iterate over

  # Create a result statement list that will hold all expanded components
  result = newStmtList()

  # The strategy: use untyped template parameters and macro.getType
  # to evaluate the collection at compile time, then substitute values

  # Helper proc to replace all occurrences of loopVar with a value in the AST
  proc replaceIdent(node: NimNode, oldIdent: NimNode, newNode: NimNode): NimNode =
    case node.kind
    of nnkIdent:
      if node.eqIdent(oldIdent):
        return copyNimTree(newNode)
      else:
        return node
    of nnkEmpty, nnkNilLit:
      return node
    of nnkCharLit..nnkUInt64Lit, nnkFloatLit..nnkFloat128Lit, nnkStrLit..nnkTripleStrLit:
      return node
    else:
      result = copyNimNode(node)
      for child in node:
        result.add replaceIdent(child, oldIdent, newNode)

  # Try to evaluate the collection at compile time
  # This works if collection is a const or can be resolved at compile time
  let collectionValue = collection

  # Generate a unique symbol for storing collection length
  let lenSym = genSym(nskConst, "staticForLen")

  # Build the unrolled loop by generating index-based iteration
  # We create compile-time when branches for each possible index
  result = quote do:
    const `lenSym` = len(`collectionValue`)

  # Add each iteration as a separate when branch
  # This is a workaround since we can't truly iterate at macro expansion time
  # We'll generate code for indices 0..99 (should be enough for most cases)
  for i in 0 ..< 100:  # Maximum 100 iterations supported
    let iLit = newLit(i)
    let itemAccess = quote do:
      `collectionValue`[`iLit`]

    # Replace the loop variable with the item access in a copy of the body
    let expandedBody = replaceIdent(bodyStmt, loopVar, itemAccess)

    # CRITICAL FIX: Apply the same symbol isolation as the static: for construct
    # Use unique symbols and block wrapping to prevent AST reuse
    let resultSym = genSym(nskLet, "staticForResult")
    let iterationBlock = ident("iteration")

    result.add quote do:
      when `iLit` < `lenSym`:
        block `iterationBlock`:
          let `resultSym` = `expandedBody`
          # Add to parent container if this is a component
          when compiles(`resultSym`):
            when compiles(kryon_component_add_child):
              when compiles(staticForParent):
                discard kryon_component_add_child(staticForParent, `resultSym`)

# ============================================================================
# DSL Implementation Complete
# ============================================================================
# New input components to add to impl.nim after the Spacer macro

macro RadioGroup*(props: untyped): untyped =
  ## Radio button group component  
  ## Groups radio buttons and manages single selection
  var
    groupName = genSym(nskLet, "radioGroup")
    nameVal = newStrLitNode("radiogroup")
    selectedIndexVal = newIntLitNode(0)
    childNodes: seq[NimNode] = @[]
    gapVal = newIntLitNode(8)
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "name":
        nameVal = node[1]
      of "selectedindex":
        selectedIndexVal = node[1]
      of "gap":
        gapVal = node[1]
      else:
        discard
    else:
      childNodes.add(node)
  
  result = quote do:
    block:
      let `groupName` = Column:
        gap = `gapVal`
      `groupName`


macro RadioButton*(props: untyped): untyped =
  ## Individual radio button component
  var
    labelVal = newStrLitNode("")
    valueVal = newIntLitNode(0)
    clickHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "label":
        labelVal = node[1]
      of "value":
        valueVal = node[1]
      of "onclick":
        clickHandler = node[1]
      else:
        discard
  
  # For now, render as checkbox until we have proper radio support
  result = quote do:
    Checkbox:
      label = `labelVal`
      checked = false
      onClick = `clickHandler`

macro Slider*(props: untyped): untyped =
  ## Slider/Range input component
  var
    valueVal = newIntLitNode(50)
    minVal = newIntLitNode(0)
    maxVal = newIntLitNode(100)
    stepVal = newIntLitNode(1)
    widthVal = newIntLitNode(200)
    heightVal = newIntLitNode(30)
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "value":
        valueVal = node[1]
      of "min":
        minVal = node[1]
      of "max":
        maxVal = node[1]
      of "step":
        stepVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "onchange":
        changeHandler = node[1]
      else:
        discard
  
  # Placeholder: render as a container with text showing value
  result = quote do:
    Container:
      width = `widthVal`
      height = `heightVal`
      backgroundColor = "#ddd"
      layoutDirection = 1  # Row
      
      # Progress bar
      Container:
        width = (`valueVal` * `widthVal`) div `maxVal`
        height = `heightVal`
        backgroundColor = "#3498db"

macro TextArea*(props: untyped): untyped =
  ## Multi-line text input component
  var
    textareaName = genSym(nskLet, "textarea")
    placeholderVal = newStrLitNode("")
    valueVal = newStrLitNode("")

    widthVal = newIntLitNode(300)
    heightVal = newIntLitNode(100)
    backgroundColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = node[1]
      of "value":
        valueVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "backgroundcolor":
        backgroundColorVal = colorNode(node[1])
      of "textcolor", "color":
        textColorVal = colorNode(node[1])
      of "bordercolor":
        borderColorVal = colorNode(node[1])
      of "borderwidth":
        borderWidthVal = node[1]
      of "ontextchange":
        changeHandler = node[1]
      else:
        discard
  
  var initStmts = newTree(nnkStmtList)
  
  if backgroundColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`textareaName`, `backgroundColorVal`)
  
  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`textareaName`, `textColorVal`)
  
  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`textareaName`, `borderColorVal`)
  
  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`textareaName`, uint8(`borderWidthVal`))
  
  result = quote do:
    block:
      let `textareaName` = newKryonContainer()
      kryon_component_set_bounds(`textareaName`,
        toFixed(0), toFixed(0), toFixed(`widthVal`), toFixed(`heightVal`))
      `initStmts`
      # TODO: Implement actual multiline text editing
      # For now, just a container
      `textareaName`

macro ColorPicker*(props: untyped): untyped =
  ## Color picker component
  var
    valueVal = newStrLitNode("#000000")
    widthVal = newIntLitNode(60)
    heightVal = newIntLitNode(35)
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "value":
        valueVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "onchange":
        changeHandler = node[1]
      else:
        discard
  
  result = quote do:
    Container:
      width = `widthVal`
      height = `heightVal`
      backgroundColor = `valueVal`
      borderColor = "#333"
      borderWidth = 1

macro FileInput*(props: untyped): untyped =
  ## File upload input component
  var
    placeholderVal = newStrLitNode("Choose file...")
    acceptVal = newStrLitNode("*")
    widthVal = newIntLitNode(300)
    heightVal = newIntLitNode(35)
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = node[1]
      of "accept":
        acceptVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "onchange":
        changeHandler = node[1]
      else:
        discard
  
  result = quote do:
    Button:
      text = `placeholderVal`
      width = `widthVal`
      height = `heightVal`
      backgroundColor = "#ecf0f1"
      textColor = "#7f8c8d"
      onClick = proc() =
        echo "File input clicked - not yet implemented"
