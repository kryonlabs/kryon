## Core Application and Component Infrastructure
##
## This module provides the fundamental DSL macros for creating Kryon applications:
## - kryonApp: Top-level application macro with window configuration
## - component: Generic component creation macro
## - Header: Window property configuration
## - Resources: Asset loading (fonts, etc.)

import macros, strutils
import ./properties
import ./reactive

export properties, reactive

# Re-export runtime symbols needed by macros
import ../runtime
import ../ir_core

export runtime, ir_core

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

      # FIX: Re-propagate animation flags after tree is fully constructed
      # Animations are attached before components are added to parents,
      # so the flag propagation in ir_component_add_animation doesn't work
      ir_animation_propagate_flags(rootComponent)
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
# Built-in Configuration Macros
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
