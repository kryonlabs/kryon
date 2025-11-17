## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os, sequtils, random
import ./runtime, ./core_kryon, ./reactive_system, ./markdown

# Export font management functions from runtime
export addFontSearchDir, loadFont, getFontId

# Export core component functions for styling
export kryon_component_set_background_color, kryon_component_set_border_color
export kryon_component_set_border_width, kryon_component_set_text_color
export KryonComponent

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

proc alignmentNode(name: string): NimNode =
  let normalized = name.toLowerAscii()
  let variant =
    case normalized
    of "center", "middle": "kaCenter"
    of "end", "bottom", "right": "kaEnd"
    of "stretch": "kaStretch"
    of "spaceevenly": "kaSpaceEvenly"
    of "spacearound": "kaSpaceAround"
    of "spacebetween": "kaSpaceBetween"
    else: "kaStart"
  let alignSym = bindSym("KryonAlignment")
  result = newTree(nnkDotExpr, alignSym, ident(variant))

proc parseAlignmentValue(value: NimNode): NimNode =
  ## Parse alignment value and return KryonAlignment enum value for C API
  if value.kind == nnkStrLit:
    # Convert string to enum value at compile time using alignmentNode
    result = alignmentNode(value.strVal)
  else:
    # Runtime parsing for non-literal values
    result = newCall(ident("parseAlignmentString"), value)

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
    bodyNode = newTree(nnkCall, ident("Body"), synthesizedBody)
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
  appCode.add quote do:
    # Initialize application
    let `appName` = newKryonApp()

    # Set window properties
    var window = KryonWindow(
      width: `windowWidth`,
      height: `windowHeight`,
      title: `windowTitle`
    )

    # Set up renderer with window config
    let renderer = initRenderer(window.width, window.height, window.title)
    if renderer == nil:
      echo "Failed to initialize renderer"
      quit(1)

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
        # Just load the font by name - the C backend will find it in search paths
        loadStatements.add quote do:
          when defined(KRYON_SDL3):
            discard loadFont(`fontName`, 16)  # Load at default size

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

macro Container*(props: untyped): untyped =
  ## Simplified Container component using C core layout API
  var
    containerName = genSym(nskLet, "container")
    childNodes: seq[NimNode] = @[]
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
      of "backgroundcolor", "bg":
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
      if node.kind == nnkStaticStmt:
        # Static blocks will be processed later after initStmts is initialized
        # Don't add them to childNodes - they'll be handled separately
        discard
      else:
        childNodes.add(node)

  # Generate initialization statements using C core API
  var initStmts = newTree(nnkStmtList)

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
              var transformedBody = newTree(nnkStmtList)

              # Force value capture by creating a let copy of the loop variable for each iteration
              let loopVar = stmt[0]  # The loop variable (e.g., "alignment")
              let loopIterVar = genSym(nskLet, "iteration")

              # Add let declaration to capture the current loop variable value
              transformedBody.add(newLetStmt(loopIterVar, loopVar))

              for bodyNode in stmt[^1].children:
                # Transform the body node to replace loop variable references
                let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, loopIterVar)

                if transformedBodyNode.kind == nnkDiscardStmt and transformedBodyNode.len > 0:
                  let componentExpr = transformedBodyNode[0]
                  let childSym = genSym(nskLet, "loopChild")
                  transformedBody.add(newLetStmt(childSym, componentExpr))
                  transformedBody.add quote do:
                    discard kryon_component_add_child(`containerName`, `childSym`)
                elif transformedBodyNode.kind == nnkCall:
                  let childSym = genSym(nskLet, "loopChild")
                  transformedBody.add(newLetStmt(childSym, transformedBodyNode))
                  transformedBody.add quote do:
                    discard kryon_component_add_child(`containerName`, `childSym`)
                else:
                  # For other nodes, just add them directly
                  transformedBody.add(transformedBodyNode)
              if transformedBody.len > 0:
                let newForLoop = newTree(nnkForStmt, stmt[0], stmt[1], transformedBody)
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
            var transformedBody = newTree(nnkStmtList)

            # Process the body of the for loop
            for bodyNode in staticChild[^1].children:
              if bodyNode.kind == nnkDiscardStmt and bodyNode.len > 0:
                # Extract component from discard statement
                let componentExpr = bodyNode[0]
                let childSym = genSym(nskLet, "loopChild")
                transformedBody.add(newLetStmt(childSym, componentExpr))
                transformedBody.add quote do:
                  let added = kryon_component_add_child(`containerName`, `childSym`)
                  echo "[kryon][static] Added child to container: ", `containerName`, " -> ", `childSym`, " success: ", added
              elif bodyNode.kind == nnkCall:
                # Direct component call
                let childSym = genSym(nskLet, "loopChild")
                transformedBody.add(newLetStmt(childSym, bodyNode))
                transformedBody.add quote do:
                  let added = kryon_component_add_child(`containerName`, `childSym`)
                  echo "[kryon][static] Added direct child to container: ", `containerName`, " -> ", `childSym`, " success: ", added
              else:
                # Other statements - just add as-is
                transformedBody.add(bodyNode)

            # Recreate the for loop with transformed body
            if transformedBody.len > 0:
              let newForLoop = newTree(nnkForStmt, staticChild[0], staticChild[1], transformedBody)
              initStmts.add(newForLoop)
            else:
              # If no transformable content, add original
              initStmts.add(staticChild)
          else:
            # Other control flow statements - add as-is for now
            initStmts.add(staticChild)

  # Set layout direction first (most important)
  initStmts.add quote do:
    kryon_component_set_layout_direction(`containerName`, uint8(`layoutDirectionVal`))

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
      toFixed(`xExpr`), toFixed(`yExpr`), toFixed(`wExpr`), toFixed(`hExpr`), uint8(`maskVal`))

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

  # Add child components
  for child in childNodes:
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
      initStmts.add(newLetStmt(childSym, child))
      initStmts.add quote do:
        discard kryon_component_add_child(`containerName`, `childSym`)

  # Mark container as dirty
  initStmts.add quote do:
    kryon_component_mark_dirty(`containerName`)

  # Generate the final result
  # Always return the component - the calling context should handle it properly
  result = quote do:
    block:
      let `containerName` = newKryonContainer()
      `initStmts`
      `containerName`

macro Body*(props: untyped): untyped =
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
    initStmts.add quote do:
      # Create reactive expression for text
      # The closure will capture the current runtime value of variables in `textContent`
      let evalProc = proc (): string {.closure.} =
        `textContent`  # This will be evaluated with captured runtime values
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
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    styleName: NimNode = nil
    styleData: NimNode = nil

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "text":
        buttonText = value
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

  let newButtonSym = bindSym("newKryonButton")
  let buttonBridgeSym = bindSym("nimButtonBridge")
  let registerHandlerSym = bindSym("registerButtonHandler")

  var buttonCall = newCall(newButtonSym, buttonText)
  if clickHandler.kind != nnkNilLit:
    buttonCall = newCall(newButtonSym, buttonText, buttonBridgeSym)
    initStmts.add quote do:
      `registerHandlerSym`(`buttonName`, `clickHandler`)

  initStmts.add quote do:
    kryon_component_mark_dirty(`buttonName`)

  var childStmtList = newStmtList()
  for child in childNodes:
    childStmtList.add quote do:
      let childComp = `child`
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

  # Don't set explicit bounds or layout direction - let layout system handle it completely
  # Checkboxes should be positioned naturally in the flow so Body centering works

  # Set text color if provided
  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`checkboxName`, `textColorVal`)

  # Set background color if provided
  if backgroundColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`checkboxName`, `backgroundColorVal`)

  # Set up click handler with proper signature
  var checkboxHandler = clickHandler
  if clickHandler.kind != nnkNilLit:
    # Create a wrapper function with correct signature
    checkboxHandler = quote do:
      proc(checkbox: KryonComponent, checked: bool) {.cdecl.} =
        `clickHandler`()
  else:
    checkboxHandler = newNilLit()

  initStmts.add quote do:
    kryon_component_mark_dirty(`checkboxName`)

  result = quote do:
    block:
      let `checkboxName` = newKryonCheckbox(`labelVal`, `checkedVal`, `checkboxHandler`)
      `initStmts`
      kryon_component_mark_dirty(`checkboxName`)
      `checkboxName`

macro Input*(props: untyped): untyped =
  ## Input component macro (placeholder implementation)
  ## TODO: Implement proper text input once C core supports it
  var
    inputName = genSym(nskLet, "input")
    placeholderVal: NimNode = newStrLitNode("")
    valueVal: NimNode = newStrLitNode("")
    widthVal: NimNode = newIntLitNode(200)
    heightVal: NimNode = newIntLitNode(40)
    backgroundColorVal: NimNode = nil
    textColorVal: NimNode = nil
    fontSizeVal: NimNode = newIntLitNode(14)
    onTextChangeVal: NimNode = nil
    childNodes: seq[NimNode] = @[]

  # Parse Input properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        if node[1].kind == nnkStrLit:
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
      of "fontsize":
        fontSizeVal = node[1]
      of "ontextchange":
        onTextChangeVal = node[1]
      else:
        discard
    else:
      childNodes.add(node)

  var initStmts = newTree(nnkStmtList)

  # Set background color (default light gray for input field)
  if backgroundColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`inputName`, `backgroundColorVal`)
  else:
    initStmts.add quote do:
      kryon_component_set_background_color(`inputName`, 0xF5F5F5FF'u32)

  # Set border to indicate it's an input field
  initStmts.add quote do:
    kryon_component_set_border_width(`inputName`, 1)
    kryon_component_set_border_color(`inputName`, 0xCCCCCCFF'u32)

  # Set size
  initStmts.add quote do:
    kryon_component_set_bounds(`inputName`, toFixed(0), toFixed(0), toFixed(`widthVal`), toFixed(`heightVal`))

  # Add padding
  initStmts.add quote do:
    kryon_component_set_padding(`inputName`, 8, 8, 8, 8)

  # Create text component for input value (use value, not placeholder)
  let textSym = genSym(nskLet, "inputText")
  # Use the actual value, fallback to placeholder if empty
  let displayText = quote do:
    if `valueVal`.len > 0: `valueVal` else: `placeholderVal`

  if textColorVal != nil:
    initStmts.add quote do:
      let `textSym` = newKryonText(`displayText`, `fontSizeVal`, 0, 0)
      kryon_component_set_text_color(`textSym`, `textColorVal`)
      discard kryon_component_add_child(`inputName`, `textSym`)
  else:
    initStmts.add quote do:
      let `textSym` = newKryonText(`displayText`, `fontSizeVal`, 0, 0)
      kryon_component_set_text_color(`textSym`, 0x333333FF'u32)  # Darker text for input
      discard kryon_component_add_child(`inputName`, `textSym`)

  # Add any child components
  for child in childNodes:
    initStmts.add quote do:
      let childComponent = `child`
      if childComponent != nil:
        discard kryon_component_add_child(`inputName`, childComponent)

  result = quote do:
    block:
      let `inputName` = newKryonContainer()
      `initStmts`
      kryon_component_mark_dirty(`inputName`)
      `inputName`

macro Canvas*(props: untyped): untyped =
  ## Canvas component macro
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

  if hasOnDraw:
    result = quote do:
      block:
        let `canvasName` = newKryonCanvas()
        # Set canvas size
        kryon_canvas_set_size(`canvasName`, uint16(`widthVal`), uint16(`heightVal`))
        # Set canvas background color
        kryon_canvas_component_set_background_color(`canvasName`, runtime.parseColor(`bgColorVal`))
        # Set bounds using the bounds API
        kryon_component_set_bounds(`canvasName`,
          kryon_fp_from_float(0),
          kryon_fp_from_float(0),
          kryon_fp_from_float(float32(`widthVal`)),
          kryon_fp_from_float(float32(`heightVal`)))
        registerCanvasHandler(`canvasName`, `onDrawVal`)
        `canvasName`
  else:
    result = quote do:
      block:
        let `canvasName` = newKryonCanvas()
        # Set canvas size
        kryon_canvas_set_size(`canvasName`, uint16(`widthVal`), uint16(`heightVal`))
        # Set canvas background color
        kryon_canvas_component_set_background_color(`canvasName`, runtime.parseColor(`bgColorVal`))
        kryon_component_set_bounds(`canvasName`,
          kryon_fp_from_float(0),
          kryon_fp_from_float(0),
          kryon_fp_from_float(float32(`widthVal`)),
          kryon_fp_from_float(float32(`heightVal`)))
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
  var
    markdownName = genSym(nskLet, "markdown")
    sourceVal: NimNode = newStrLitNode("")
    fileVal: NimNode = newStrLitNode("")
    themeVal: NimNode = newNilLit()
    widthVal: NimNode = newIntLitNode(800)
    heightVal: NimNode = newIntLitNode(600)
    scrollableVal: NimNode = newLit(true)  # Default to true (bool literal)
    onLinkClickVal: NimNode = newNilLit()
    onImageClickVal: NimNode = newNilLit()

  # Extract properties from props
  for node in props.children:
    if node.kind == nnkAsgn:
      let identName = node[0]
      let value = node[1]

      case $identName
      of "source":
        sourceVal = value
      of "file":
        fileVal = value
      of "theme":
        themeVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      of "scrollable":
        scrollableVal = value
      of "onLinkClick":
        onLinkClickVal = value
      of "onImageClick":
        onImageClickVal = value
      else:
        # Unknown property, skip
        discard

  # Generate component creation code
  result = quote do:
    let `markdownName` = MarkdownProps(
      source: `sourceVal`,
      file: `fileVal`,
      theme: `themeVal`,
      width: `widthVal`,
      height: `heightVal`,
      scrollable: `scrollableVal`,
      onLinkClick: `onLinkClickVal`,
      onImageClick: `onImageClickVal`
    )
    createMarkdownComponent(`markdownName`)

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
  ## Uses explicit flex layout for proper centering regardless of parent layout.
  var body = newTree(nnkStmtList)
  var hasFlexGrow = false

  # Check user properties for flexGrow override
  for node in props.children:
    if node.kind == nnkAsgn and $node[0] == "flexGrow":
      hasFlexGrow = true

  # Set explicit layout direction to column flex layout for proper centering
  body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))  # KRYON_LAYOUT_COLUMN

  # Set flex properties for centering
  body.add newTree(nnkAsgn, ident("justifyContent"), newStrLitNode("center"))
  body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))

  # Only set flexGrow if not provided by user - prevents layout conflicts with siblings
  if not hasFlexGrow:
    body.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))

  # Add user properties (allowing overrides)
  for node in props.children:
    body.add(node)

  result = newTree(nnkCall, ident("Container"), body)

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
  var hasLayout = false
  var hasGap = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "selectedindex":
        selectedIndexVal = node[1]
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "gap":
        hasGap = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))
  if not hasGap:
    body.add newTree(nnkAsgn, ident("gap"), newIntLitNode(8))
  for node in propertyNodes:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let groupSym = genSym(nskLet, "tabGroup")
  let stateSym = genSym(nskLet, "tabGroupState")

  var childStmtList = newStmtList()
  for child in childNodes:
    childStmtList.add quote do:
      let childComponent = `child`
      if childComponent != nil:
        discard `addChildSym`(`groupSym`, childComponent)

  result = quote do:
    block:
      let `groupSym` = `containerCall`
      let `stateSym` = `createSym`(`groupSym`, `selectedIndexVal`)
      let `ctxSym` = `stateSym`
      block:
        `childStmtList`
      `finalizeSym`(`stateSym`)
      `groupSym`

macro TabBar*(props: untyped): untyped =
  let registerSym = bindSym("registerTabBar")
  let ctxSym = ident("__kryonCurrentTabGroup")

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
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(2))
  if not hasAlign:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  if not hasGap:
    body.add newTree(nnkAsgn, ident("gap"), newIntLitNode(6))
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
  for node in propertyNodes:
    if node.kind == nnkAsgn and $node[0] == "height":
      hasHeight = true
    body.add(node)

  # Set default height if not provided
  if not hasHeight:
    body.add newTree(nnkAsgn, ident("height"), newIntLitNode(40))

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let barSym = genSym(nskLet, "tabBar")

  var childStmtList = newStmtList()
  for node in childNodes:
    if node.kind == nnkForStmt:
      # Handle runtime for loops with Tab components
      # Structure: nnkForStmt[loopVar, collection, body]
      let loopVar = node[0]
      let collection = node[1]
      let body = node[2]

      # Generate code that iterates at runtime and processes Tab components
      # Since Tab components self-register, we just execute the body without capturing results
      childStmtList.add newTree(nnkForStmt, loopVar, collection, body)
    else:
      # Handle regular child nodes
      if node.kind == nnkCall and node[0].kind == nnkIdent and node[0].eqIdent("Tab"):
        # Tab components are statements that self-register, add them directly
        childStmtList.add(node)
      else:
        # Other components return values, add normally
        childStmtList.add(node)

  result = quote do:
    block:
      let `barSym` = `containerCall`
      `registerSym`(`ctxSym`, `barSym`, `reorderableVal`)
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
    if node.kind == nnkForStmt:
      # Handle runtime for loops with TabPanel components
      # Structure: nnkForStmt[loopVar, collection, body]
      let loopVar = node[0]
      let collection = node[1]
      let body = node[2]

      # Generate code that iterates at runtime and processes TabPanel components
      # TabPanel components return values that need to be added as children
      childStmtList.add quote do:
        for `loopVar` in `collection`:
          let panelChild = `body`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)
    else:
      # Handle regular child nodes
      if node.kind == nnkCall and node[0].kind == nnkIdent and node[0].eqIdent("TabPanel"):
        # TabPanel components self-register and are added as children, so we need to capture the result
        childStmtList.add quote do:
          let panelChild = `node`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)
      else:
        # Other components
        childStmtList.add quote do:
          let panelChild = `node`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)

  result = quote do:
    block:
      let `contentSym` = `containerCall`
      `registerSym`(`ctxSym`, `contentSym`)
      block:
        `childStmtList`
      `contentSym`

macro TabPanel*(props: untyped): untyped =
  let registerSym = bindSym("registerTabPanel")
  let ctxSym = ident("__kryonCurrentTabGroup")

  var body = newTree(nnkStmtList)
  for node in props.children:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let panelSym = genSym(nskLet, "tabPanel")

  result = quote do:
    block:
      let `panelSym` = `containerCall`
      `registerSym`(`ctxSym`, `panelSym`)
      `panelSym`

macro Tab*(props: untyped): untyped =
  let registerSym = bindSym("registerTabComponent")
  let visualType = bindSym("TabVisualState")
  let ctxSym = ident("__kryonCurrentTabGroup")

  var buttonProps = newTree(nnkStmtList)
  var extraChildren: seq[NimNode] = @[]
  var titleVal: NimNode = newStrLitNode("Tab")
  var backgroundColorExpr: NimNode = newStrLitNode("#292C30")
  var activeBackgroundExpr: NimNode = newStrLitNode("#3C4047")
  var textColorExpr: NimNode = newStrLitNode("#C7C9CC")
  var activeTextExpr: NimNode = newStrLitNode("#FFFFFF")
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
  if not hasWidth:
    buttonProps.add newTree(nnkAsgn, ident("width"), newIntLitNode(168))
  buttonProps.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  buttonProps.add newTree(nnkAsgn, ident("justifyContent"), newStrLitNode("center"))

  buttonProps.add newTree(nnkAsgn, ident("text"), titleVal)

  for child in extraChildren:
    buttonProps.add(child)

  let buttonCall = newTree(nnkCall, ident("Button"), buttonProps)
  let tabSym = genSym(nskLet, "tabComponent")
  let centerSetter = bindSym("kryon_button_set_center_text")
  let ellipsizeSetter = bindSym("kryon_button_set_ellipsize")
  var afterCreate = newStmtList()

  if not hasPadding:
    afterCreate.add quote do:
      kryon_component_set_padding(`tabSym`, 10'u8, 20'u8, 10'u8, 20'u8)

  if not hasMargin:
    afterCreate.add quote do:
      kryon_component_set_margin(`tabSym`, 2'u8, 6'u8, 0'u8, 6'u8)

  afterCreate.add quote do:
    kryon_component_set_layout_alignment(`tabSym`, kaCenter, kaCenter)
    kryon_component_set_flex(`tabSym`, 0'u8, 0'u8)
    kryon_component_set_layout_direction(`tabSym`, 1'u8)
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
      let `tabSym` = `buttonCall`
      `afterCreate`
      `registerSym`(`ctxSym`, `tabSym`, `visualNode`)
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
    when defined(debug):
      echo "[kryon][color] parseHexColor: ", hex, " -> rgba(", r, ",", g, ",", b, ",255) = 0x", result.toHex(8)
  elif hex.len == 9 and hex.startsWith("#"):
    let r = parseHexInt(hex[1..2])
    let g = parseHexInt(hex[3..4])
    let b = parseHexInt(hex[5..6])
    let a = parseHexInt(hex[7..8])
    result = rgba(r, g, b, a)
    when defined(debug):
      echo "[kryon][color] parseHexColor: ", hex, " -> rgba(", r, ",", g, ",", b, ",", a, ") = 0x", result.toHex(8)
  else:
    result = 0xFF000000'u32  # Default to black
    when defined(debug):
      echo "[kryon][color] parseHexColor: invalid format '", hex, "' -> default black 0x", result.toHex(8)

proc parseNamedColor*(name: string): uint32 =
  ## Parse named colors - supports common CSS color names
  case name.toLowerAscii():
  # Primary colors
  of "red":
    result = rgba(255, 0, 0, 255)
  of "green":
    result = rgba(0, 255, 0, 255)
  of "blue":
    result = rgba(0, 0, 255, 255)
  of "yellow":
    result = rgba(255, 255, 0, 255)

  # Grayscale
  of "white":
    result = rgba(255, 255, 255, 255)
  of "black":
    result = rgba(0, 0, 0, 255)
  of "gray", "grey":
    result = rgba(128, 128, 128, 255)
  of "lightgray", "lightgrey":
    result = rgba(211, 211, 211, 255)
  of "darkgray", "darkgrey":
    result = rgba(169, 169, 169, 255)
  of "silver":
    result = rgba(192, 192, 192, 255)

  # Common colors
  of "orange":
    result = rgba(255, 165, 0, 255)
  of "purple":
    result = rgba(128, 0, 128, 255)
  of "pink":
    result = rgba(255, 192, 203, 255)
  of "brown":
    result = rgba(165, 42, 42, 255)
  of "cyan":
    result = rgba(0, 255, 255, 255)
  of "magenta":
    result = rgba(255, 0, 255, 255)
  of "lime":
    result = rgba(0, 255, 0, 255)
  of "olive":
    result = rgba(128, 128, 0, 255)
  of "navy":
    result = rgba(0, 0, 128, 255)
  of "teal":
    result = rgba(0, 128, 128, 255)
  of "aqua":
    result = rgba(0, 255, 255, 255)
  of "maroon":
    result = rgba(128, 0, 0, 255)
  of "fuchsia":
    result = rgba(255, 0, 255, 255)

  # Light variations
  of "lightblue":
    result = rgba(173, 216, 230, 255)
  of "lightgreen":
    result = rgba(144, 238, 144, 255)
  of "lightpink":
    result = rgba(255, 182, 193, 255)
  of "lightyellow":
    result = rgba(255, 255, 224, 255)
  of "lightcyan":
    result = rgba(224, 255, 255, 255)

  # Dark variations
  of "darkred":
    result = rgba(139, 0, 0, 255)
  of "darkgreen":
    result = rgba(0, 100, 0, 255)
  of "darkblue":
    result = rgba(0, 0, 139, 255)
  of "darkorange":
    result = rgba(255, 140, 0, 255)
  of "darkviolet":
    result = rgba(148, 0, 211, 255)

  # Transparent
  of "transparent":
    result = rgba(0, 0, 0, 0)

  else:
    result = rgba(128, 128, 128, 255)  # Default to gray
