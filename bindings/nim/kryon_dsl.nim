## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os
import ./runtime, ./core_kryon

proc colorNode(value: NimNode): NimNode =
  if value.kind == nnkStrLit:
    let text = value.strVal
    if text.startsWith("#"):
      result = newCall(ident("parseHexColor"), value)
    else:
      result = newCall(ident("parseNamedColor"), value)
  else:
    result = value

proc alignmentNode(name: string): NimNode =
  let normalized = name.toLowerAscii()
  let variant =
    case normalized
    of "center", "middle": "kaCenter"
    of "end", "bottom", "right": "kaEnd"
    of "stretch": "kaStretch"
    else: "kaStart"
  let alignSym = bindSym("KryonAlignment")
  result = newTree(nnkDotExpr, alignSym, ident(variant))

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
    childNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "backgroundcolor":
        bgColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
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
      of "alignitems":
        if value.kind == nnkStrLit:
          alignNode = alignmentNode(value.strVal)
      else:
        discard
    else:
      childNodes.add(node)

  var initStmts = newTree(nnkStmtList)

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

  for node in childNodes:
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
      else:
        discard

  if not posXSet:
    bodyStmt.add newTree(nnkAsgn, ident("posX"), newIntLitNode(0))
  if not posYSet:
    bodyStmt.add newTree(nnkAsgn, ident("posY"), newIntLitNode(0))
  if not flexGrowSet:
    bodyStmt.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))
  if not backgroundSet:
    bodyStmt.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("#101820FF"))

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
  let textSym = genSym(nskLet, "text")

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "content", "text":
        textContent = value
      of "color":
        colorVal = colorNode(value)
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
      of "textcolor":
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
  elif hex.len == 9 and hex.startsWith("#"):
    let r = parseHexInt(hex[1..2])
    let g = parseHexInt(hex[3..4])
    let b = parseHexInt(hex[5..6])
    let a = parseHexInt(hex[7..8])
    result = rgba(r, g, b, a)
  else:
    result = 0xFF000000'u32  # Default to black

proc parseNamedColor*(name: string): uint32 =
  ## Parse named colors
  case name.toLowerAscii():
  of "red":
    result = rgba(255, 0, 0, 255)
  of "green":
    result = rgba(0, 255, 0, 255)
  of "blue":
    result = rgba(0, 0, 255, 255)
  of "white":
    result = rgba(255, 255, 255, 255)
  of "black":
    result = rgba(0, 0, 0, 255)
  of "yellow":
    result = rgba(255, 255, 0, 255)
  of "cyan":
    result = rgba(0, 255, 255, 255)
  of "magenta":
    result = rgba(255, 0, 255, 255)
  else:
    result = rgba(128, 128, 128, 255)  # Default to gray
