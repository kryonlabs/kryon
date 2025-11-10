## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os, sequtils
import ./runtime, ./core_kryon, ./reactive_system

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
  result = quote do:
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
  result.add(appName)

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

macro Container*(props: untyped): untyped =
  ## Container component macro
  var
    containerName = genSym(nskLet, "container")
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
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
    flexGrowVal: NimNode = nil
    flexShrinkVal: NimNode = nil
    justifyNode: NimNode = alignmentNode("start")
    alignNode: NimNode = alignmentNode("start")
    layoutDirectionVal: NimNode = nil
    gapVal: NimNode = nil
    childNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "backgroundcolor", "bg":  # Added 'bg' shorthand
        bgColorVal = colorNode(value)
      of "color", "textcolor":  # Text color for inheritance
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width", "w":  # Added 'w' shorthand
        widthVal = value
      of "height", "h":  # Added 'h' shorthand
        heightVal = value
      of "padding", "p":  # Added 'p' shorthand
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "margin", "m":  # Added 'm' shorthand
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
      of "contentalignment":
        if value.kind == nnkStrLit:
          let alignStr = value.strVal
          justifyNode = alignmentNode(alignStr)
          alignNode = alignmentNode(alignStr)
      of "justifycontent":
        if value.kind == nnkStrLit:
          justifyNode = alignmentNode(value.strVal)
      of "mainaxisalignment":
        if value.kind == nnkStrLit:
          justifyNode = alignmentNode(value.strVal)
        else:
          # Non-literal value - call runtime parser
          justifyNode = newCall(ident("parseAlignmentString"), value)
      of "alignitems":
        if value.kind == nnkStrLit:
          alignNode = alignmentNode(value.strVal)
        else:
          alignNode = newCall(ident("parseAlignmentString"), value)
      of "crossaxisalignment":  # Alias for alignItems (React-like API)
        if value.kind == nnkStrLit:
          alignNode = alignmentNode(value.strVal)
        else:
          alignNode = newCall(ident("parseAlignmentString"), value)
      of "justify":  # Shorthand for justifyContent
        if value.kind == nnkStrLit:
          justifyNode = alignmentNode(value.strVal)
      of "align":  # Shorthand for alignItems
        if value.kind == nnkStrLit:
          alignNode = alignmentNode(value.strVal)
      of "gap":  # Gap between children
        gapVal = value
      of "layoutdirection":
        layoutDirectionVal = value
      else:
        discard
    else:
      childNodes.add(node)

  var initStmts = newTree(nnkStmtList)

  # Set layout direction FIRST - immediately after component creation
  if layoutDirectionVal != nil:
    initStmts.add quote do:
      echo "[kryon][nim] Setting layout direction EARLY for component to ", uint8(`layoutDirectionVal`)
      kryon_component_set_layout_direction(`containerName`, uint8(`layoutDirectionVal`))

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

  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  initStmts.add quote do:
    kryon_component_set_bounds_mask(`containerName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`widthExpr`),
      toFixed(`heightExpr`),
      `maskCast`)

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

  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`containerName`,
        uint8(`padTopExpr`),
        uint8(`padRightExpr`),
        uint8(`padBottomExpr`),
        uint8(`padLeftExpr`))

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

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`containerName`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

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

  if flexGrowVal != nil or flexShrinkVal != nil:
    let growExpr = if flexGrowVal != nil: flexGrowVal else: newIntLitNode(0)
    let shrinkExpr = if flexShrinkVal != nil: flexShrinkVal else: newIntLitNode(1)
    initStmts.add quote do:
      kryon_component_set_flex(`containerName`, uint8(`growExpr`), uint8(`shrinkExpr`))

  initStmts.add quote do:
    kryon_component_set_layout_alignment(`containerName`, `justifyNode`, `alignNode`)

  # Set gap if specified
  if gapVal != nil:
    initStmts.add quote do:
      kryon_component_set_gap(`containerName`, uint8(`gapVal`))

  # Layout direction already set early above

  # Process child nodes - handle static blocks specially
  for node in childNodes:
    # Check if this is a static block that generates components
    if node.kind == nnkStaticStmt:
      # Static block - we need to handle this differently
      # The static block contains code that will execute at compile-time
      # We need to collect the results and add them as children

      # Actually, we need a different approach - static blocks don't return values
      # We need to unroll the static block's for loop at macro expansion time

      # Extract the for loop from the static block
      # The static block has structure: nnkStaticStmt -> nnkStmtList -> nnkForStmt
      if node.len > 0 and node[0].kind == nnkStmtList and
         node[0].len > 0 and node[0][0].kind == nnkForStmt:
        let forStmt = node[0][0]  # Get the actual for statement
        # forStmt structure: [loopVar, collection, body]
        let loopVar = forStmt[0]
        let collection = forStmt[1]
        let body = forStmt[2]

        # Helper to replace identifiers in AST
        # This needs to handle dotted expressions like alignment.name -> alignments[i].name
        proc replaceInAst(n: NimNode, target: NimNode, replacement: NimNode): NimNode =
          case n.kind
          of nnkIdent:
            if n.eqIdent(target):
              return copyNimTree(replacement)
            else:
              return n
          of nnkEmpty, nnkNilLit:
            return n
          of nnkCharLit..nnkUInt64Lit, nnkFloatLit..nnkFloat128Lit, nnkStrLit..nnkTripleStrLit:
            return n
          of nnkDotExpr:
            # Handle dotted expressions like alignment.name
            # If the left side is our target, replace it
            if n.len >= 2 and n[0].kind == nnkIdent and n[0].eqIdent(target):
              result = nnkDotExpr.newTree(
                copyNimTree(replacement),
                n[1]  # Keep the field name
              )
            else:
              result = copyNimNode(n)
              for child in n:
                result.add replaceInAst(child, target, replacement)
          of nnkBracketExpr:
            # Handle bracket expressions like alignment.colors[0]
            # Recursively replace in all children
            result = copyNimNode(n)
            for child in n:
              result.add replaceInAst(child, target, replacement)
          else:
            result = copyNimNode(n)
            for child in n:
              result.add replaceInAst(child, target, replacement)

        # BREAKTHROUGH: Don't use when at all. Use static evaluation at the OUTER level
        # to determine how many items to generate, then generate ONLY that many blocks

        # Wrap the unrolling in a static block that evaluates the collection length
        let staticBlock = quote do:
          static:
            const numItems = `collection`.len
            echo "Static for loop: unrolling ", numItems, " iterations"

        initStmts.add(staticBlock)

        # Generate unrolled code with independent AST copies for each iteration
        # Use copyNimTree to prevent AST node reuse across iterations

        for i in 0 ..< 20:  # Support up to 20 items
          let iLit = newLit(i)
          let itemAccess = nnkBracketExpr.newTree(collection, iLit)
          let expandedBody = replaceInAst(body, loopVar, itemAccess)
          # CRITICAL: Create independent copy to prevent AST reuse
          let expandedBodyCopy = copyNimTree(expandedBody)
          let childSym = genSym(nskLet, "staticChild")

          # Use when to guard against out-of-bounds access
          initStmts.add(quote do:
            when `iLit` < `collection`.len:
              block:
                let `childSym` = `expandedBodyCopy`
                if `childSym` != nil:
                  kryon_component_mark_dirty(`childSym`)
                  discard kryon_component_add_child(`containerName`, `childSym`)
          )
      else:
        # Static block doesn't contain a for loop - just execute it
        initStmts.add node
    else:
      # Regular child node
      let childSym = genSym(nskLet, "child")
      initStmts.add quote do:
        let `childSym` = `node`
        if `childSym` != nil:
          kryon_component_mark_dirty(`childSym`)
          discard kryon_component_add_child(`containerName`, `childSym`)

  initStmts.add quote do:
    kryon_component_mark_dirty(`containerName`)

  result = quote do:
    block:
      let `containerName` = newKryonContainer()
      `initStmts`
      `containerName`

  # TODO: Add background color handling once basic compilation works
  # For now, skip background color to get compilation working

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
      else:
        discard

  # Add defaults first (before user properties)
  if not posXSet:
    bodyStmt.add newTree(nnkAsgn, ident("posX"), newIntLitNode(0))
  if not posYSet:
    bodyStmt.add newTree(nnkAsgn, ident("posY"), newIntLitNode(0))
  if not backgroundSet:
    bodyStmt.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("#101820FF"))

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
      # Create closure at runtime in the actual scope where variables exist
      let evalProc = proc (): string {.closure.} =
        `textContent`  # This captures variables from the runtime scope
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

  var initStmts = newTree(nnkStmtList)
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

  result = quote do:
    block:
      let `buttonName` = `buttonCall`
      `initStmts`
      `buttonName`

macro Checkbox*(props: untyped): untyped =
  ## Checkbox component macro
  var
    checkboxName = genSym(nskLet, "checkbox")
    clickHandler = newNilLit()
    labelVal: NimNode = newNilLit()
    checkedVal: NimNode = newNilLit()
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
        if node[1].kind == nnkIdent or node[1].kind == nnkStrLit:
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

  # Label, checked state, and default dimensions are handled by the C core creation function

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

macro Canvas*(props: untyped): untyped =
  ## Canvas component macro
  var
    canvasName = genSym(nskLet, "canvas")

  discard props

  result = quote do:
    block:
      let `canvasName` = newKryonCanvas()
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
    sourceVal: NimNode = newNilLit()
    fileVal: NimNode = newNilLit()
    themeVal: NimNode = newNilLit()
    widthVal: NimNode = newIntLitNode(800)
    heightVal: NimNode = newIntLitNode(600)
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

  # First pass: detect which properties user has set
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName == "width": hasWidth = true
      if propName == "height": hasHeight = true
      if propName == "mainAxisAlignment": hasMainAxisAlignment = true
      if propName == "alignItems": hasAlignItems = true

  # Add defaults first (only if not specified by user)
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

  # Add defaults first (only if not specified by user)
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
  var body = newTree(nnkStmtList)
  body.add newTree(nnkAsgn, ident("justifyContent"), newStrLitNode("center"))
  body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  body.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))
  for node in props.children:
    body.add(node)
  result = newTree(nnkCall, ident("Container"), body)

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

    # Add a when branch for this index
    result.add quote do:
      when `iLit` < `lenSym`:
        `expandedBody`

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
