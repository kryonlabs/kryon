## Core Application and Component Infrastructure
##
## This module provides the fundamental DSL macros for creating Kryon applications:
## - kryonApp: Top-level application macro with window configuration
## - component: Generic component creation macro
## - Header: Window property configuration
## - Resources: Asset loading (fonts, etc.)

import macros, strutils, tables
import ./properties
import ./reactive

export properties, reactive

# Re-export runtime symbols needed by macros
import ../runtime
import ../ir_core

export runtime, ir_core

# ============================================================================
# Custom Component with Automatic Reactive Detection
# ============================================================================

type ReactiveVarInfo = object
  name: string
  initialValue: NimNode
  usedInHandlers: bool
  usedInText: bool

type ReactiveVarTable = ref object
  data: Table[string, ReactiveVarInfo]

proc detectReactiveVars(body: NimNode): Table[string, ReactiveVarInfo] =
  ## Scan a component proc body for reactive variables
  var vars = ReactiveVarTable(data: initTable[string, ReactiveVarInfo]())

  proc scanForVars(n: NimNode, vars: ReactiveVarTable) =
    case n.kind:
    of nnkVarSection:
      # Found variable declaration
      for identDef in n:
        if identDef.len >= 3:
          let varName = $identDef[0]
          vars.data[varName] = ReactiveVarInfo(
            name: varName,
            initialValue: identDef[2],
            usedInHandlers: false,
            usedInText: false
          )

    of nnkCall:
      # Check if it's a component with properties
      if n.len >= 2:
        let props = n[1]
        if props.kind == nnkStmtList:
          for prop in props:
            if prop.kind == nnkAsgn:
              let propName = $prop[0]
              let value = prop[1]

              # Check for onClick handlers
              if propName.toLowerAscii() == "onclick":
                let mutated = analyzeHandlerMutations(value)
                for varName in mutated:
                  if varName in vars.data:
                    vars.data[varName].usedInHandlers = true

              # Check for text expressions
              elif propName.toLowerAscii() in ["text", "content", "title"]:
                let referenced = extractVariableReferences(value)
                for varName in referenced:
                  if varName in vars.data:
                    vars.data[varName].usedInText = true

      # Also recursively scan child components
      for child in n:
        scanForVars(child, vars)

    else:
      for child in n: scanForVars(child, vars)

  scanForVars(body, vars)

  # Filter to only variables used reactively
  result = initTable[string, ReactiveVarInfo]()
  for name, info in vars.data:
    if info.usedInHandlers or info.usedInText:
      result[name] = info

proc transformToReactive(body: NimNode, reactiveVars: Table[string, ReactiveVarInfo], componentName: string): NimNode =
  ## Transform variable declarations and access to use namedReactiveVar
  result = body.copyNimTree()

  proc transform(n: NimNode): NimNode =
    case n.kind:
    of nnkVarSection:
      result = newNimNode(nnkVarSection)
      for identDef in n:
        let varName = $identDef[0]
        if varName in reactiveVars:
          # Transform: var value = init
          # Into: var value = namedReactiveVar("Component:value", init)
          let scopedName = componentName & ":" & varName
          let initVal = identDef[2]
          let wrappedInit = quote do:
            namedReactiveVar(`scopedName`, `initVal`)
          result.add(newIdentDefs(identDef[0], identDef[1], wrappedInit))
        else:
          result.add(identDef)

    of nnkIdent:
      # Transform variable access: value → value.value
      let varName = $n
      if varName in reactiveVars:
        result = newDotExpr(n, ident("value"))
      else:
        result = n

    else:
      result = n.copyNimNode()
      for child in n:
        result.add(transform(child))

  result = transform(body)

macro kryonComponent*(procDef: untyped): untyped =
  ## Wrap a component proc to automatically detect and transform reactive variables
  ## Usage:
  ##   proc Counter*(init: int = 0): Element {.kryonComponent.} =
  ##     var value = init
  ##     result = Row:
  ##       Button: onClick = proc() = value += 1
  ##       Text: text = $value
  ##
  ## This will automatically:
  ## 1. Detect that `value` is used reactively (in onClick and text)
  ## 2. Transform `var value = init` to `var value = namedReactiveVar("Counter:value", init)`
  ## 3. Transform all `value` access to `value.value`
  ## 4. Register the component definition for .kir serialization

  expectKind(procDef, nnkProcDef)

  # Get proc name, handling both exported (nnkPostfix) and non-exported (nnkIdent) procs
  var procName: string
  if procDef[0].kind == nnkPostfix:
    # Exported proc: postfix(`*`, ident("Counter"))
    procName = $procDef[0][1]
  else:
    procName = $procDef[0]

  let procNameLit = newLit(procName)
  let procBody = procDef[6]

  # Extract proc parameters (props)
  var propsSeq = newNimNode(nnkBracket)
  let params = procDef[3]  # nnkFormalParams
  if params.len > 1:  # First child is return type
    for i in 1..<params.len:
      let identDefs = params[i]
      if identDefs.kind == nnkIdentDefs:
        let paramName = $identDefs[0]
        var paramType = "any"
        if identDefs[1].kind == nnkIdent:
          paramType = $identDefs[1]
        elif identDefs[1].kind == nnkSym:
          paramType = $identDefs[1]
        var defaultVal = ""
        if identDefs[2].kind != nnkEmpty:
          defaultVal = repr(identDefs[2])

        propsSeq.add quote do:
          ComponentProp(name: `paramName`, propType: `paramType`, defaultValue: `defaultVal`)

  # Detect reactive variables
  let reactiveVars = detectReactiveVars(procBody)

  # Build state vars sequence
  var stateVarsSeq = newNimNode(nnkBracket)
  for name, info in reactiveVars:
    let nameLit = newLit(name)
    # Format initialExpr as JSON var reference: {"var":"propName"}
    let initExprStr = repr(info.initialValue)
    let initExprLit = newLit("{\"var\":\"" & initExprStr & "\"}")
    stateVarsSeq.add quote do:
      ComponentStateVar(name: `nameLit`, varType: "any", initialExpr: `initExprLit`)

  if reactiveVars.len > 0:
    # Transform the proc body
    let transformedBody = transformToReactive(procBody, reactiveVars, procName)
    procDef[6] = transformedBody

    # Log what we transformed (for debugging)
    echo "[kryonComponent] ", procName, ": detected ", reactiveVars.len, " reactive variables"
    for name, info in reactiveVars:
      echo "  - ", name, " (handlers: ", info.usedInHandlers, ", text: ", info.usedInText, ")"

  # Wrap the proc body to register the component definition and set the template
  let registerComponentDefSym = bindSym("registerComponentDefinition")
  let setComponentTemplateSym = bindSym("setComponentTemplate")
  let setComponentRefSym = bindSym("ir_set_component_ref")
  let setComponentPropsSym = bindSym("ir_set_component_props")

  let originalBody = procDef[6]

  # Build code to serialize props to JSON
  var paramNames: seq[NimNode] = @[]
  if params.len > 1:
    for i in 1..<params.len:
      let identDefs = params[i]
      if identDefs.kind == nnkIdentDefs:
        let paramName = identDefs[0]
        paramNames.add(paramName)

  # Generate props JSON expression
  var propsJsonExpr: NimNode
  if paramNames.len > 0:
    # Build JSON object from params: {"param1": value1, "param2": value2}
    var jsonItems: seq[NimNode] = @[]
    for paramName in paramNames:
      let paramNameStr = newLit($paramName)
      jsonItems.add quote do:
        "\"" & `paramNameStr` & "\": " & (when `paramName` is string: "\"" & `paramName` & "\"" else: $`paramName`)

    let jsonItemsNode = newNimNode(nnkBracket)
    for item in jsonItems:
      jsonItemsNode.add(item)

    propsJsonExpr = quote do:
      "{" & @`jsonItemsNode`.join(", ") & "}"
  else:
    propsJsonExpr = newLit("{}")

  # Create new body that registers on first call and captures template
  # Note: The originalBody assigns to `result`, so we execute it directly
  # and then use `result` for registration
  procDef[6] = quote do:
    # Register component definition (only registers once)
    `registerComponentDefSym`(`procNameLit`, @`propsSeq`, @`stateVarsSeq`)

    # Execute original body (which assigns to result)
    `originalBody`

    # Set template root (only sets once)
    `setComponentTemplateSym`(`procNameLit`, result)

    # Set component_ref on EVERY instance
    `setComponentRefSym`(result, `procNameLit`)

    # Set component_props on EVERY instance
    `setComponentPropsSym`(result, cstring(`propsJsonExpr`))

    # Return result (implicit)

  result = procDef

macro kryonApp*(body: untyped): untyped =
  ## Top-level application macro
  ## Creates root container with window configuration

  # Parse the body for configuration and components
  var windowWidth = newIntLitNode(800)
  var windowHeight = newIntLitNode(600)
  var windowTitle = newStrLitNode("Kryon Application")
  var stateCallbackNode: NimNode = nil  # Phase 5: State callback
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
            of "statecallback":
              stateCallbackNode = value  # Phase 5: State change callback
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

  # Phase 5: Set state callback BEFORE running
  if stateCallbackNode != nil:
    appCode.add quote do:
      `appName`.registerStateCallback(`stateCallbackNode`)

  # Run the application
  appCode.add quote do:
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
