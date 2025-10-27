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

import macros, strutils, options, unicode, tables
import core

# ============================================================================
# Helper procs for macro processing
# ============================================================================

type
  IterationKind* = enum
    ## Different types of iteration patterns for for loops
    ikIndexRange,      # for i in 0..<seq.len (preserve int type)
    ikDirectSequence,  # for item in habits (preserve element type)
    ikCountUpRange,    # for i in countup(0, 10) (preserve int type)
    ikGeneric          # Fallback to Value-based system

proc detectIterationPattern*(iterable: NimNode): IterationKind =
  ## Detect the type of iteration pattern from the iterable AST
  case iterable.kind:
  of nnkInfix:
    # Handle infix patterns like 0..<habits.len or 0..10
    if iterable.len == 3:
      let op = iterable[0]
      if op.kind == nnkIdent:
        let opStr = op.strVal
        case opStr
        of "..<", "<..": return ikIndexRange      # 0..<seq.len
        of "..": return ikCountUpRange    # 0..10 (treat as countup-style)
        else: return ikGeneric
      else: return ikGeneric
    else: return ikGeneric

  of nnkCall:
    # Handle function calls like countup(0, 10) or items(seq)
    if iterable.len > 0:
      let funcName = iterable[0]
      if funcName.kind == nnkIdent:
        case funcName.strVal:
        of "countup", "countdown": return ikCountUpRange
        of "items", "pairs", "mpairs": return ikDirectSequence
        else: return ikGeneric
      else: return ikGeneric
    else: return ikGeneric

  of nnkIdent:
    # Direct sequence/variable reference: for habit in habits
    return ikDirectSequence

  of nnkDotExpr:
    # Support dotted access like data.habits or habits.items
    return ikDirectSequence

  else:
    # Default to generic for complex expressions
    return ikGeneric

proc extractIterableName*(iterable: NimNode): string =
  ## Extract the base variable name for dependency registration
  case iterable.kind:
  of nnkIdent:
    iterable.strVal
  of nnkInfix:
    # For 0..<habits.len, extract "habits"
    if iterable.len == 3:
      extractIterableName(iterable[2])  # Right side of infix
    else: "iterable"
  of nnkCall:
    # For countup(0, habits.len), extract from second argument
    if iterable.len > 2:
      extractIterableName(iterable[2])
    elif iterable.len > 1:
      extractIterableName(iterable[1])
    else: "iterable"
  of nnkDotExpr:
    # Walk left side of dotted expressions to reach the base identifier
    if iterable.len > 0:
      extractIterableName(iterable[0])
    else:
      "iterable"
  else: "iterable"

proc extractConditionDependencies*(condition: NimNode): seq[string] =
  ## Extract all variable names from a condition expression for dependency tracking
  ## This recursively walks the AST to find all identifiers that are variables
  result = @[]
  case condition.kind:
  of nnkIdent:
    # Single identifier - could be a variable or a boolean literal
    let identStr = condition.strVal
    if identStr notin ["true", "false", "nil"]:
      result.add(identStr)
  of nnkInfix, nnkPrefix:
    # Binary operators (==, !=, <, >, and, or) or unary operators (not)
    # Recursively extract from all operands
    for i in 0..<condition.len:
      result.add(extractConditionDependencies(condition[i]))
  of nnkCall:
    # Function call - extract dependencies from arguments
    for i in 1..<condition.len:  # Skip the function name at index 0
      result.add(extractConditionDependencies(condition[i]))
  of nnkDotExpr:
    # Dotted access like obj.field - extract the base identifier
    if condition.len > 0:
      result.add(extractConditionDependencies(condition[0]))
  of nnkBracketExpr:
    # Array/table access like arr[i] - extract from both parts
    for i in 0..<condition.len:
      result.add(extractConditionDependencies(condition[i]))
  of nnkPar:
    # Parenthesized expression - extract from inside
    if condition.len > 0:
      result.add(extractConditionDependencies(condition[0]))
  else:
    # Literals and other node types - no dependencies
    discard

proc resolvePropertyAlias*(propName: string): string =
  ## Resolve property aliases to canonical names
  ## Add new aliases here without touching other code
  case propName:
  of "background": "backgroundColor"
  of "textColor": "color"
  of "justifyContent": "mainAxisAlignment"
  of "alignItems": "crossAxisAlignment"
  # Future aliases can be added here:
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
        try:
          # Register dependency using variable name as identifier
          registerDependency(`varName`)
          val(`node`)
        except:
          # Return empty string as fallback if reactive expression fails
          val("")
      )
      result.boundVar = some(node)  # Return the variable name for binding
  of nnkCall:
    # Check if this is a UI element creation call
    if node.len > 0:
      let nodeName = node[0]
      if nodeName.kind == nnkIdent:
        let nameStr = nodeName.strVal
        if nameStr in ["Container", "Text", "H1", "H2", "H3", "Button", "Column", "Row", "Input", "Checkbox",
                      "Dropdown", "Grid", "Image", "Center", "ScrollView", "Header", "Body",
                      "TabGroup", "TabBar", "Tab", "TabContent", "TabPanel", "Link",
                      "Draggable", "DropTarget"]:
          # This is a UI element creation call, return it directly
          result.value = node
          result.boundVar = none(NimNode)
          return
        else:
          # Check if this might be a function call that returns an Element
          # We'll treat it as a UI element creation call and let it be resolved at runtime
          result.value = node
          result.boundVar = none(NimNode)
          return

    # Complex expression - wrap in getter for reactivity!
    result.value = quote do: valGetter(proc(): Value =
      try:
        val(`node`)
      except:
        # Return empty string as fallback if reactive expression fails
        val("")
    )
    result.boundVar = none(NimNode)
  of nnkIfExpr:
    # Inline if expression: if condition: true_expr else: false_expr
    # Wrap in getter for reactivity!
    let ifExprCopy = copyNimTree(node)
    result.value = quote do: valGetter(proc(): Value =
      try:
        # Register dependency on the entire if expression
        registerDependency("ifExpr_" & $`ifExprCopy`.repr)

        # Evaluate the if expression
        val(`ifExprCopy`)
      except:
        # Return empty string as fallback if reactive expression fails
        val("")
    )
    result.boundVar = none(NimNode)
  of nnkInfix, nnkPrefix, nnkDotExpr:
    # Complex expression - wrap in getter for reactivity!
    result.value = quote do: valGetter(proc(): Value =
      try:
        val(`node`)
      except:
        # Return empty string as fallback if reactive expression fails
        val("")
    )
    result.boundVar = none(NimNode)
  else:
    # Default: try to convert to Value
    result.value = quote do: val(`node`)
    result.boundVar = none(NimNode)

const builtinElementNames = [
  "Container", "Text", "H1", "H2", "H3", "Button", "Column", "Row", "Input", "Checkbox",
  "Dropdown", "Grid", "Image", "Center", "ScrollView", "Header", "Body",
  "Resources", "Font", "TabGroup", "TabBar", "Tab", "TabContent", "TabPanel", "Link",
  "Draggable", "DropTarget", "Canvas"
]

proc isEventName(name: string): bool {.inline.} =
  ## Determine whether the identifier represents an event handler
  name.startsWith("on")

proc toInvocation(node: NimNode): NimNode =
  ## Ensure the node is an invocation expression (call with parentheses) when needed
  case node.kind
  of nnkCall, nnkCommand:
    copyNimTree(node)
  else:
    newCall(copyNimTree(node))

proc isInteractiveEvent*(eventName: string): bool {.inline.} =
  ## Determine if an event should automatically trigger reactivity
  ## Interactive events typically modify application state
  case eventName:
  of "onClick", "onDrop", "onChange", "onValueChange", "onTextChange",
     "onSelectedIndexChanged", "onHover", "onPress", "onRelease": true
  else: false

proc ensureEventHandler(node: NimNode, eventName: string = ""): NimNode =
  ## Wrap the node in a zero-arg proc to use as an event handler
  ## If it's an identifier, wrap it with a closure that calls it
  ## Automatically adds reactivity invalidation for interactive events
  if node.kind == nnkIdent:
    # This is a function identifier like `handleDrop` or `resetDropZone`
    # We need to wrap it in a closure to match EventHandler signature
    let funcIdent = node

    # For onClick events that don't take data, don't pass the data parameter
    if eventName == "onClick":
      # Check if this is an interactive event that should auto-invalidate
      if isInteractiveEvent(eventName):
        quote do:
          proc(data: string = "") {.closure.} =
            `funcIdent`()
            invalidateAllReactiveValues()  # Auto-add for interactive events
      else:
        quote do:
          proc(data: string = "") {.closure.} =
            `funcIdent`()
    else:
      # For events that expect data (like onDrop), pass the data parameter
      # Check if this is an interactive event that should auto-invalidate
      if isInteractiveEvent(eventName):
        quote do:
          proc(data: string = "") {.closure.} =
            `funcIdent`(data)
            invalidateAllReactiveValues()  # Auto-add for interactive events
      else:
        quote do:
          proc(data: string = "") {.closure.} =
            `funcIdent`(data)
  else:
    let invocation = toInvocation(node)

    # Check if this is an interactive event that should auto-invalidate
    if isInteractiveEvent(eventName):
      quote do:
        proc(data: string = "") {.closure.} =
          `invocation`
          invalidateAllReactiveValues()  # Auto-add for interactive events
    else:
      quote do:
        proc(data: string = "") {.closure.} =
          `invocation`

proc convertInlineProcToEventHandler(procNode: NimNode, eventName: string = ""): NimNode =
  ## Convert an inline proc definition to EventHandler signature
  ## Extracts parameters and body, creates wrapper with expected signature
  ## Automatically adds reactivity invalidation for interactive events

  # Handle both nnkProcDef and nnkLambda
  var params: NimNode
  var body: NimNode

  case procNode.kind:
  of nnkProcDef:
    params = procNode[3]  # Parameter list
    body = procNode[6]    # Procedure body
  of nnkLambda:
    params = procNode[3]  # Parameter list
    body = procNode[6]    # Procedure body
  else:
    error("Expected proc definition or lambda", procNode)

  # Extract parameter names and types (skip first param which is return type)
  var paramNames: seq[NimNode] = @[]
  var paramTypes: seq[NimNode] = @[]

  for i in 1..<params.len:
    let param = params[i]
    if param.kind == nnkIdentDefs:
      let paramName = param[0]
      let paramType = param[1]
      paramNames.add(paramName)
      paramTypes.add(paramType)

  # Create a parameter symbol for the wrapper proc
  let dataParam = genSym(nskParam, "data")

  # Create variable declarations for parameters in the wrapper body
  var paramDecls = newStmtList()
  var paramAssignments = newStmtList()

  # For common event patterns, we can extract values from the data string
  # This is a simple approach - could be enhanced with more sophisticated parsing
  if paramNames.len == 1:
    let paramName = paramNames[0]
    paramAssignments.add quote do:
      let `paramName` = `dataParam`

  elif paramNames.len > 1:
    # For multiple parameters, we'll attempt to parse the data string
    # This is a basic implementation - could be improved
    for i, paramName in paramNames:
      paramAssignments.add quote do:
        let `paramName` = `dataParam`

  # Create the event handler wrapper
  let wrapperBody = newStmtList()
  wrapperBody.add(paramAssignments)
  wrapperBody.add(body)  # Add original proc body

  # Add automatic invalidation for interactive events
  if isInteractiveEvent(eventName):
    wrapperBody.add quote do:
      invalidateAllReactiveValues()  # Auto-add for interactive events

  result = quote do:
    proc(`dataParam`: string = "") {.closure.} =
      `wrapperBody`

proc emitEventAssignment(elemVar: NimNode, propName: string, handlerNode: NimNode): seq[NimNode] =
  ## Generate statements to attach an event handler to an element
  var handlerExpr = handlerNode

  # Special handling for Canvas onDraw event
  if propName == "onDraw":
    let valueCopy = copyNimTree(handlerNode)
    return @[
      quote do:
        `elemVar`.canvasDrawProc = `valueCopy`
    ]

  if handlerExpr.kind == nnkStmtList:
    if handlerExpr.len == 1:
      handlerExpr = handlerExpr[0]
    elif handlerExpr.len > 1:
      let body = copyNimTree(handlerExpr)
      let closureProc = quote do:
        proc(data: string = "") {.closure.} =
          `body`
      return @[
        quote do:
          `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), `closureProc`)
      ]

  # Check for inline proc definition: proc(param: Type) = body
  if handlerExpr.kind == nnkProcDef or handlerExpr.kind == nnkLambda:
    let eventHandler = convertInlineProcToEventHandler(handlerExpr, propName)
    return @[
      quote do:
        `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), `eventHandler`)
    ]

  # Check for proc assignment: onTextChange = proc(newName: string) = ...
  if handlerExpr.kind == nnkCall and handlerExpr.len > 0:
    let funcName = handlerExpr[0]
    if funcName.kind == nnkIdent and funcName.strVal == "proc":
      # This is a proc definition inline - need to convert to proper proc definition
      # For now, we'll handle it as a statement list
      if handlerExpr.len > 1:
        let procContent = handlerExpr[1]
        if procContent.kind == nnkStmtList:
          # Try to extract proc signature and body
          if procContent.len >= 2:
            # Assume format: proc(params): returnType = body
            # We need to handle this more carefully in the DSL processing
            let body = if procContent[^1].kind == nnkStmtList: procContent[^1] else: newStmtList(procContent[^1])
            let closureProc = quote do:
              proc(data: string = "") {.closure.} =
                `body`
            return @[
              quote do:
                `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), `closureProc`)
            ]

  let cleanHandler = ensureEventHandler(copyNimTree(handlerExpr), propName)
  @[
    quote do:
      `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), `cleanHandler`)
  ]

proc handleDropdownProperty(kind: ElementKind, elemVar: NimNode, propName: string, valueNode: NimNode): seq[NimNode] =
  ## Handle dropdown-specific properties (options/selectedIndex)
  result = @[]

  if kind != ekDropdown:
    return

  case propName
  of "options":
    when defined(debugDropdown):
      echo "Debug: Handling dropdown options, value kind = ", valueNode.kind

    if valueNode.kind == nnkBracket:
      var optionsCode = newNimNode(nnkBracket)
      for child in valueNode:
        optionsCode.add(copyNimTree(child))
      result.add quote do:
        `elemVar`.dropdownOptions = `optionsCode`
    elif valueNode.kind == nnkPrefix and valueNode.len >= 2 and
         valueNode[0].kind == nnkIdent and valueNode[0].strVal == "@":
      let optionsValue = copyNimTree(valueNode)
      result.add quote do:
        `elemVar`.dropdownOptions = `optionsValue`
  of "selectedIndex":
    when defined(debugDropdown):
      echo "Debug: Handling dropdown selectedIndex, value kind = ", valueNode.kind
    let indexValue = copyNimTree(valueNode)
    result.add quote do:
      `elemVar`.dropdownSelectedIndex = `indexValue`
  else:
    discard

proc isUiComponentNode(node: NimNode): bool {.compileTime.} =
  ## Helper to determine if a NimNode is a UI component macro call.
  ## This is based on the convention that components start with a capital letter.
  case node.kind
  of nnkCall, nnkCommand:
    if node.len > 0 and node[0].kind == nnkIdent:
      let name = node[0].strVal
      # Check if name is in built-ins or just capitalized (for custom components)
      if name in builtinElementNames or (name.len > 0 and name[0] in 'A'..'Z'):
        return true
  else:
    discard
  return false
proc generateTypePreservingForLoop*(elemVar: NimNode, loopVar: NimNode, iterable: NimNode, loopBody: NimNode, iterationKind: IterationKind, iterableName: string): seq[NimNode] =
  ## Generate reactive for loop code that keeps the loop variable's original type
  ## and supports multiple statements/elements in the loop body.
  result = @[]
  let iterableNameNode = newLit(iterableName)
  let loopBodyCopy = copyNimTree(loopBody)
  let loopVarCopy = copyNimTree(loopVar)
  let iterableCopy = copyNimTree(iterable)
  let forLoopSym = genSym(nskVar, "forLoop")

  # THE FIX: Create a symbol for the 'forElem' parameter BEFORE using it.
  # This ensures the symbol is available in the correct scope for the quote block.
  let forElemParam = genSym(nskParam, "forElem")

  # Process the loop body to wrap UI components in addChild calls.
  let processedLoopBody = newStmtList()
  let bodyStmts = if loopBodyCopy.kind == nnkStmtList: loopBodyCopy else: newStmtList(loopBodyCopy)

  for stmt in bodyStmts:
    if isUiComponentNode(stmt):
      # This is a UI component, so wrap it in `addChild`.
      # We use our new symbol 'forElemParam' here.
      processedLoopBody.add(quote do:
        let childElement = `stmt`
        if childElement != nil:
          `forElemParam`.addChild(childElement)
      )
    else:
      # This is a regular statement (like `echo` or `let`), so add it directly.
      processedLoopBody.add(stmt)

  # Generate the builder proc.
  # This is a procedure that the runtime will call to build or update
  # the contents of the for loop.
  var builderProc: NimNode
  case iterationKind
  of ikDirectSequence: # Optimized for `for item in items`
    builderProc = quote do:
      proc(`forElemParam`: Element) = # We use the SAME symbol here for the parameter name.
        registerDependency("forLoopIterable")
        registerDependency(`iterableNameNode`)
        `forElemParam`.children.setLen(0)
        let seqValue = `iterableCopy`
        for index in 0 ..< len(seqValue):
          # Use an immediately-invoked proc to capture the loop variable correctly.
          (proc(capturedParam = seqValue[index]) =
            let `loopVarCopy` = capturedParam
            `processedLoopBody` # Inject our processed body here.
          )()
  else: # Handles countup, ranges, etc.
    builderProc = quote do:
      proc(`forElemParam`: Element) = # And we use the SAME symbol here too.
        registerDependency("forLoopIterable")
        registerDependency(`iterableNameNode`)
        `forElemParam`.children.setLen(0)
        for loopItem in `iterableCopy`:
          # Use an immediately-invoked proc to capture the loop variable correctly.
          (proc(capturedParam = loopItem) =
            let `loopVarCopy` = capturedParam
            `processedLoopBody` # Inject our processed body here.
          )()

  result.add quote do:
    var `forLoopSym` = newElement(ekForLoop)
    `forLoopSym`.forBuilder = `builderProc`
    `elemVar`.addChild(`forLoopSym`)
    
proc handleTabGroupProperty(kind: ElementKind, elemVar: NimNode, propName: string, valueNode: NimNode): seq[NimNode] =
  ## Handle tab group specific properties, including optional bindings
  result = @[]

  if kind != ekTabGroup or propName != "selectedIndex":
    return

  when defined(debugTabs):
    echo "Debug: Handling TabGroup selectedIndex property, value kind = ", valueNode.kind

  let valueCopy = copyNimTree(valueNode)
  result.add quote do:
    `elemVar`.tabSelectedIndex = `valueCopy`

  let parseResult = parsePropertyValue(valueNode)
  if parseResult.boundVar.isSome:
    let varNode = parseResult.boundVar.get()
    let varName = varNode.strVal
    let varNodeCopy = copyNimTree(varNode)

    result.add quote do:
      `elemVar`.setBoundVarName(`varName`)
      `elemVar`.setEventHandler("onSelectedIndexChanged", proc(data: string = "") =
        `varNodeCopy` = `elemVar`.tabSelectedIndex
        invalidateReactiveValue(`varName`)
        invalidateReactiveValue("selectedHabit")
        echo "SYNC: Updated ", `varName`, " to ", `elemVar`.tabSelectedIndex, " from TabGroup"
      )

proc handleFontProperty(kind: ElementKind, elemVar: NimNode, propName: string, valueNode: NimNode): seq[NimNode] =
  ## Handle Font resource properties (name, sources)
  result = @[]

  if kind != ekFont:
    return

  case propName
  of "name":
    let nameValue = copyNimTree(valueNode)
    result.add quote do:
      `elemVar`.setProp("name", `nameValue`)
  of "sources":
    if valueNode.kind == nnkBracket:
      var sourcesCode = newNimNode(nnkBracket)
      for child in valueNode:
        sourcesCode.add(copyNimTree(child))
      result.add quote do:
        var sourcesSeq: seq[string] = @[]
        for item in `sourcesCode`:
          sourcesSeq.add(item)
        `elemVar`.setProp("sources", val(sourcesSeq))
    else:
      # Handle single source string
      let sourceValue = copyNimTree(valueNode)
      result.add quote do:
        `elemVar`.setProp("sources", val(@[`sourceValue`]))
  else:
    discard

proc emitPropertyAssignment(kind: ElementKind, elemVar: NimNode, propName: string, valueNode: NimNode): seq[NimNode] =
  ## Generate statements to assign a property to an element, handling special cases
  result = handleDropdownProperty(kind, elemVar, propName, valueNode)
  if result.len > 0:
    return

  result = handleTabGroupProperty(kind, elemVar, propName, valueNode)
  if result.len > 0:
    return

  result = handleFontProperty(kind, elemVar, propName, valueNode)
  if result.len > 0:
    return

  # Handle style property
  if propName == "style":
    # Check if this is a function call: style = styleName()
    if valueNode.kind == nnkCall:
      # Function-style: style = containerStyle() or style = buttonStyle(param)
      # The valueNode is already the function call, so we just call it
      let styleCallCopy = copyNimTree(valueNode)

      result.add quote do:
        # Call the style function to get properties
        let styleProperties = `styleCallCopy`
        # Apply all style properties
        for prop in styleProperties:
          `elemVar`.setProp(resolvePropertyAlias(prop.name), prop.value)

    elif valueNode.kind == nnkIdent:
      # Old syntax fallback: style = styleName (without parentheses)
      # Treat it as a zero-argument function call
      let styleName = valueNode.strVal
      let styleCall = newCall(newIdentNode(styleName))

      result.add quote do:
        # Call the style function to get properties
        let styleProperties = `styleCall`
        # Apply all style properties
        for prop in styleProperties:
          `elemVar`.setProp(resolvePropertyAlias(prop.name), prop.value)

    else:
      echo "Warning: Invalid style syntax"
    return

  let parseResult = parsePropertyValue(valueNode)
  var propValue = parseResult.value

  # Special handling for alignment properties with string literals
  # Normalize kebab-case to camelCase at compile time for better performance
  let resolvedPropName = resolvePropertyAlias(propName)
  if resolvedPropName in ["mainAxisAlignment", "crossAxisAlignment"] and valueNode.kind == nnkStrLit:
    let strVal = valueNode.strVal
    let normalizedVal = case strVal:
      of "space-between": "spaceBetween"
      of "space-around": "spaceAround"
      of "space-evenly": "spaceEvenly"
      of "flex-start": "start"
      of "flex-end": "end"
      else: strVal
    if normalizedVal != strVal:
      # Replace with normalized value
      propValue = quote do: val(`normalizedVal`)

  result.add quote do:
    `elemVar`.setProp(resolvePropertyAlias(`propName`), `propValue`)

  if kind == ekInput and parseResult.boundVar.isSome:
    let varNode = parseResult.boundVar.get()
    let varName = varNode.strVal
    let varNodeCopy = copyNimTree(varNode)
    result.add quote do:
      `elemVar`.setBoundVarName(`varName`)
      `elemVar`.setEventHandler("onValueChange", proc(data: string = "") =
        `varNodeCopy` = data
        invalidateReactiveValue(`varName`)
      )

proc combineConditions(condNodes: seq[NimNode]): NimNode =
  ## Combine a sequence of boolean expressions using logical OR
  if condNodes.len == 0:
    return nil

  result = copyNimTree(condNodes[0])
  for i in 1..<condNodes.len:
    let left = copyNimTree(result)
    let right = copyNimTree(condNodes[i])
    result = quote do:
      `left` or `right`
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
      if isEventName(propName):
        for eventStmt in emitEventAssignment(elemVar, propName, stmt[1]):
          result.add(eventStmt)
      else:
        for propStmt in emitPropertyAssignment(kind, elemVar, propName, stmt[1]):
          result.add(propStmt)

    of nnkCall:
      # This could be:
      # 1. Property assignment with colon syntax: selectedIndex: 0
      # 2. Event handler with colon syntax: onClick: <body>
      # 3. Child element with body: Text: <body>
      # 4. Function call that returns Element (custom component)
      let name = stmt[0]
      let nameStr = name.strVal

      # Check if this is a UI element creation call
      if nameStr in builtinElementNames:
        result.add quote do:
          `elemVar`.addChild(`stmt`)
      # Check if this looks like an event handler
      elif isEventName(nameStr) and stmt.len > 1:
        for eventStmt in emitEventAssignment(elemVar, nameStr, stmt[1]):
          result.add(eventStmt)
      elif stmt.len > 1 and stmt[1].kind == nnkStmtList:
        # Check if this is a property assignment (colon syntax)
        # Property: selectedIndex: 0 -> StmtList contains just the value
        let stmtList = stmt[1]
        if stmtList.len == 1:
          # This looks like a property assignment with colon syntax
          let propValue = stmtList[0]
          for propStmt in emitPropertyAssignment(kind, elemVar, nameStr, propValue):
            result.add(propStmt)
        else:
          # Child element with body: Text: <body>
          result.add quote do:
            `elemVar`.addChild(`stmt`)
      else:
        # This might be a function call that returns Element (custom component).
        # We use a heuristic: component names are expected to be capitalized.
        if nameStr.len > 0 and nameStr[0] in 'A'..'Z':
          # Treat as a function call that returns an Element (custom component)
          result.add quote do:
            `elemVar`.addChild(`stmt`)
        else:
          # This is a regular procedure call like `echo`.
          # Execute it directly rather than discarding it.
          result.add(stmt)

    of nnkIdent:
      # Handle "with" keyword for behavior attachment
      let identStr = stmt.strVal
      if identStr == "with":
        # This should be followed by behavior elements in the next iteration
        # The actual behavior elements will be processed as separate statements
        discard
      else:
        # Regular identifier - treat as procedure call
        result.add(stmt)

    of nnkCommand:
      # Command syntax: with Draggable: properties...
      # Check if this is a "with" statement for behavior attachment
      if stmt.len > 0 and stmt[0].kind == nnkIdent and stmt[0].strVal == "with":
        # This is a "with" statement - process the behavior attachment
        if stmt.len > 1:
          let behaviorCall = stmt[1]
          if behaviorCall.kind == nnkIdent:
            let behaviorName = behaviorCall.strVal
            if behaviorName in ["Draggable", "DropTarget"]:
              # Create the behavior element with properties
              let behaviorKind = if behaviorName == "Draggable": ekDraggable else: ekDropTarget
              let behaviorBody = if stmt.len > 2 and stmt[2].kind == nnkStmtList: stmt[2] else: newStmtList()

              # Process the behavior element
              let behaviorCode = processElementBody(behaviorKind, behaviorBody)
              result.add quote do:
                let behaviorElem = `behaviorCode`
                `elemVar`.withBehaviors.add(behaviorElem)
                behaviorElem.parent = `elemVar`
            else:
              error("Unsupported behavior: " & behaviorName, stmt)
          else:
            error("Expected behavior identifier after 'with'", stmt)
        else:
          error("Expected behavior after 'with'", stmt)
      elif stmt.len > 0 and stmt[0].kind == nnkIdent:
        let nameStr = stmt[0].strVal
        if nameStr.len > 0 and nameStr[0] in 'A'..'Z':
          # Assumed to be a UI component, add it as a child.
          result.add quote do:
            `elemVar`.addChild(`stmt`)
        else:
          # This is a regular procedure call, execute it directly
          result.add(stmt)

    of nnkSym:
      # This is likely an element reference (e.g., from Canvas macro)
      # Add it as a child to the parent element
      result.add quote do:
        `elemVar`.addChild(`stmt`)

    of nnkAsgn:
      # Also handle nnkAsgn (rare, but valid)
      when defined(debugDropdown):
        echo "Debug: nnkAsgn: stmt[0] = ", stmt[0].repr, " kind = ", stmt[0].kind
      let propName = if stmt[0].kind == nnkIdent: stmt[0].strVal else: ""
      when defined(debugDropdown):
        echo "Debug: nnkAsgn: propName = ", propName

      # Skip processing for direct field assignments like elem.canvasDrawProc = drawShapes
      if stmt[0].kind == nnkDotExpr:
        result.add(stmt)
      else:
        # Check if this is an event handler
        if isEventName(propName):
          for eventStmt in emitEventAssignment(elemVar, propName, stmt[1]):
            result.add(eventStmt)
        else:
          for propStmt in emitPropertyAssignment(kind, elemVar, propName, stmt[1]):
            result.add(propStmt)

    of nnkExprColonExpr:
      # Colon syntax: width: 800 (same as width = 800)
      let propName = stmt[0].strVal

      # Check if this is an event handler
      if isEventName(propName):
        for eventStmt in emitEventAssignment(elemVar, propName, stmt[1]):
          result.add(eventStmt)
      else:
        for propStmt in emitPropertyAssignment(kind, elemVar, propName, stmt[1]):
          result.add(propStmt)
    of nnkLetSection, nnkVarSection:
          # Handle let/var declarations by adding them directly
          # to the generated code. This makes the variable available
          # in the current scope for subsequent elements and properties.
          result.add(copyNimTree(stmt))
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
          # if or elif branch - take the condition and body
          # For now, we only handle the first if branch and else
          # TODO: Support elif chains by creating nested conditionals
          if conditionExpr == nil:  # Only process the first if/elif
            conditionExpr = branch[0]
            trueBranchContent = branch[1]
        elif branch.kind == nnkElse:
          # else branch
          falseBranchContent = branch[0]
        elif branch.kind == nnkElifExpr:
          # This shouldn't happen in a properly formed if statement
          discard
        else:
          # Regular if branch (should not happen, if branches are nnkElifBranch)
          if conditionExpr == nil:
            conditionExpr = branch[0]
            trueBranchContent = branch[1]

      if conditionExpr != nil and trueBranchContent != nil:
        # Extract all variable dependencies from the condition
        let dependencies = extractConditionDependencies(conditionExpr)

        # Create dependency registration statements
        var depRegistrations = newStmtList()
        for dep in dependencies:
          let depLit = newLit(dep)
          depRegistrations.add quote do:
            registerDependency(`depLit`)

        # Wrap the condition in a getter function with dependency tracking
        let conditionGetter = quote do:
          proc(): bool =
            `depRegistrations`
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
      # Handle for loops for generating repeated elements with type preservation
      # Extract loop variable and iterable
      let loopVar = stmt[0]
      let iterable = stmt[1]
      let loopBody = stmt[2]

      # Debug: Print for loop processing
      when defined(debugTabs):
        echo "Debug: Processing type-aware for loop with iterable: ", iterable.repr, " kind: ", iterable.kind

      # Detect the iteration pattern to determine the best approach
      let iterationKind = detectIterationPattern(iterable)
      let iterableName = extractIterableName(iterable)

      # Generate type-preserving for loop code based on the pattern
      let forLoopCode = generateTypePreservingForLoop(elemVar, loopVar, iterable, loopBody, iterationKind, iterableName)

      # Add all generated statements to the result
      for codeStmt in forLoopCode:
        result.add(codeStmt)

    of nnkCaseStmt:
      # Handle case statements for conditional rendering
      let caseStmt = stmt
      if caseStmt.len < 2:
        continue

      let caseExpression = caseStmt[0]
      let dependencyIdentifier =
        if caseExpression.kind == nnkIdent:
          newLit(caseExpression.strVal)
        else:
          newLit(caseExpression.repr)

      var branchConditions = newSeq[NimNode]()
      var elseBranch: NimNode = nil

      for i in 1..<caseStmt.len:
        let branch = caseStmt[i]
        case branch.kind:
        of nnkOfBranch:
          let lastIndex = branch.len - 1
          if lastIndex < 0:
            continue

          var equalityNodes = newSeq[NimNode]()
          for j in 0..<lastIndex:
            let caseExprCopy = copyNimTree(caseExpression)
            let branchValueCopy = copyNimTree(branch[j])
            let equality = quote do:
              `caseExprCopy` == `branchValueCopy`
            equalityNodes.add(equality)

          if equalityNodes.len == 0:
            continue

          let branchCondition = combineConditions(equalityNodes)
          let branchConditionForProc = copyNimTree(branchCondition)
          branchConditions.add(copyNimTree(branchCondition))

          let branchBodyNode = branch[lastIndex]
          let branchBody =
            if branchBodyNode.kind == nnkStmtList:
              branchBodyNode
            else:
              newStmtList(branchBodyNode)

          let processedBranch = processElementBody(ekContainer, branchBody)
          let conditionProc = quote do:
            proc(): bool =
              registerDependency(`dependencyIdentifier`)
              `branchConditionForProc`

          let conditional = quote do:
            newConditionalElement(`conditionProc`, `processedBranch`, nil)

          result.add quote do:
            `elemVar`.addChild(`conditional`)

        of nnkElse:
          if branch.len >= 1:
            elseBranch = branch[0]
        else:
          discard

      if elseBranch != nil:
        let elseBody =
          if elseBranch.kind == nnkStmtList:
            elseBranch
          else:
            newStmtList(elseBranch)

        let processedElse = processElementBody(ekContainer, elseBody)
        let combinedBranches = combineConditions(branchConditions)

        var elseConditionExpr: NimNode
        if combinedBranches == nil:
          elseConditionExpr = quote do:
            true
        else:
          let combinedCopy = copyNimTree(combinedBranches)
          elseConditionExpr = quote do:
            not (`combinedCopy`)

        let elseConditionProc = quote do:
          proc(): bool =
            registerDependency(`dependencyIdentifier`)
            `elseConditionExpr`

        let elseConditional = quote do:
          newConditionalElement(`elseConditionProc`, `processedElse`, nil)

        result.add quote do:
          `elemVar`.addChild(`elseConditional`)

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

  # Special handling for Font elements - register them as resources
  if kind == ekFont:
    result.add quote do:
      # Extract font properties and register as resource
      let nameProp = `elemVar`.getProp("name")
      let sourcesProp = `elemVar`.getProp("sources")

      if nameProp.isSome() and sourcesProp.isSome():
        let fontName = nameProp.get().getString()
        let fontSources = sourcesProp.get().getStringSeq()

        if fontName.len > 0 and fontSources.len > 0:
          addFontResource(fontName, fontSources)

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

macro H1*(body: untyped): Element =
  ## Create an H1 heading element
  result = processElementBody(ekH1, body)

macro H2*(body: untyped): Element =
  ## Create an H2 heading element
  result = processElementBody(ekH2, body)

macro H3*(body: untyped): Element =
  ## Create an H3 heading element
  result = processElementBody(ekH3, body)

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

func Spacer*(): Element =
  ## Create a Spacer element with default dimensions (16px vertical, 0px horizontal)
  ## Usage: Spacer() for default spacing
  let spacer = newElement(ekSpacer)
  spacer.properties["width"] = val(0.0)
  spacer.properties["height"] = val(16.0)
  spacer

macro Spacer*(body: untyped): Element =
  ## Create a Spacer element for empty space with custom properties
  ## Usage: Spacer: height = 20 or Spacer: width = 0, height = 30
  result = processElementBody(ekSpacer, body)

macro Header*(body: untyped): Element =
  ## Create a Header element (window metadata)
  result = processElementBody(ekHeader, body)

macro Body*(body: untyped): Element =
  ## Create a Body element (UI content wrapper)
  result = processElementBody(ekBody, body)

macro TabGroup*(body: untyped): Element =
  ## Create a TabGroup element
  result = processElementBody(ekTabGroup, body)

macro TabBar*(body: untyped): Element =
  ## Create a TabBar element
  result = processElementBody(ekTabBar, body)

macro Tab*(body: untyped): Element =
  ## Create a Tab element
  result = processElementBody(ekTab, body)

macro TabContent*(body: untyped): Element =
  ## Create a TabContent element
  result = processElementBody(ekTabContent, body)

macro TabPanel*(body: untyped): Element =
  ## Create a TabPanel element
  result = processElementBody(ekTabPanel, body)

macro Link*(body: untyped): Element =
  ## Create a Link element for navigation
  result = processElementBody(ekLink, body)

macro Resources*(body: untyped): Element =
  ## Create a Resources element (application resources)
  result = processElementBody(ekResources, body)

macro Font*(body: untyped): Element =
  ## Create a Font resource element
  result = processElementBody(ekFont, body)

macro Draggable*(body: untyped): Element =
  ## Create a Draggable behavior element (used within "with" blocks)
  result = processElementBody(ekDraggable, body)

macro DropTarget*(body: untyped): Element =
  ## Create a DropTarget behavior element (used within "with" blocks)
  result = processElementBody(ekDropTarget, body)

macro Canvas*(body: untyped): Element =
  ## Create a Canvas element for custom drawing
  result = processElementBody(ekCanvas, body)

# ============================================================================
# Style macros
# ============================================================================

proc convertStmtToPropertyAdd(stmt: NimNode): NimNode {.compileTime.} =
  ## Recursively convert property assignments and control flow to runtime code
  ## that builds the seq[Property] result
  case stmt.kind:
  of nnkExprEqExpr, nnkAsgn:
    # Property assignment: backgroundColor = "value"
    let propName = stmt[0].strVal
    let propValue = stmt[1]
    let propNameLit = newLit(propName)

    # Generate code to add property to result
    if propValue.kind == nnkStrLit:
      let strVal = propValue.strVal
      if strVal.startsWith("#"):
        result = quote do:
          result.add((name: `propNameLit`, value: val(parseColor(`propValue`))))
      else:
        result = quote do:
          result.add((name: `propNameLit`, value: val(`propValue`)))
    elif propValue.kind == nnkIntLit:
      result = quote do:
        result.add((name: `propNameLit`, value: val(`propValue`)))
    elif propValue.kind == nnkFloatLit:
      result = quote do:
        result.add((name: `propNameLit`, value: val(`propValue`)))
    else:
      # For complex expressions, use val()
      result = quote do:
        result.add((name: `propNameLit`, value: val(`propValue`)))

  of nnkIfStmt:
    # If statement - recursively process branches
    result = newStmtList()

    for i, branch in stmt:
      case branch.kind:
      of nnkElifBranch:
        # elif or if branch: branch[0] is condition, branch[1] is body
        let condition = branch[0]
        let branchBody = branch[1]

        # Process the body statements
        var processedBody = newStmtList()
        if branchBody.kind == nnkStmtList:
          for s in branchBody:
            processedBody.add(convertStmtToPropertyAdd(s))
        else:
          processedBody.add(convertStmtToPropertyAdd(branchBody))

        if i == 0:
          # First branch - use if
          result = nnkIfStmt.newTree(
            nnkElifBranch.newTree(condition, processedBody)
          )
        else:
          # Subsequent branches - add as elif
          result.add(nnkElifBranch.newTree(condition, processedBody))

      of nnkElse:
        # else branch: branch[0] is body
        let branchBody = branch[0]

        # Process the body statements
        var processedBody = newStmtList()
        if branchBody.kind == nnkStmtList:
          for s in branchBody:
            processedBody.add(convertStmtToPropertyAdd(s))
        else:
          processedBody.add(convertStmtToPropertyAdd(branchBody))

        result.add(nnkElse.newTree(processedBody))

      else:
        discard

  of nnkStmtList:
    # Statement list - process each statement
    result = newStmtList()
    for s in stmt:
      result.add(convertStmtToPropertyAdd(s))

  else:
    # For other node types, just pass them through
    result = stmt


macro style*(styleCall: untyped, body: untyped): untyped =
  ## Macro to define style procs that return seq[Property]
  ## Syntax:
  ##   style containerStyle():
  ##     backgroundColor = "violet"
  ##     padding = 20

  # Parse the function call to get name and parameters
  var funcName: NimNode
  var funcParams: seq[NimNode] = @[]

  case styleCall.kind:
  of nnkCall:
    # containerStyle() - no parameters
    funcName = styleCall[0]
    for i in 1..<styleCall.len:
      funcParams.add(styleCall[i])
  of nnkObjConstr:
    # containerStyle(param: Type) - with type-annotated parameters
    # nnkObjConstr has the function name at [0] and parameters as nnkExprColonExpr nodes
    funcName = styleCall[0]
    for i in 1..<styleCall.len:
      # Each parameter is an nnkExprColonExpr: day: int
      # We need to convert it to nnkIdentDefs for the proc signature
      let paramNode = styleCall[i]
      if paramNode.kind == nnkExprColonExpr:
        # paramNode[0] is the parameter name (e.g., day)
        # paramNode[1] is the type (e.g., int)
        let paramName = paramNode[0]
        let paramType = paramNode[1]
        # Create an nnkIdentDefs node for the parameter
        funcParams.add(nnkIdentDefs.newTree(
          paramName,
          paramType,
          newEmptyNode()  # No default value
        ))
  of nnkIdent:
    # Just containerStyle (no parens)
    funcName = styleCall
  else:
    error("style expects function call syntax: styleName() or styleName(params)", styleCall)

  # Process the body
  let styleBody = if body.kind == nnkStmtList: body else: newStmtList(body)

  # Check if the style has parameters - if so, we need runtime evaluation
  # For parameterless styles, we can extract properties at compile time
  # For parameterized styles, we need to generate runtime code
  let hasParameters = funcParams.len > 0

  # Check if the body contains if statements or other complex logic
  var hasConditionalLogic = false
  for stmt in styleBody:
    if stmt.kind == nnkIfStmt or stmt.kind == nnkCaseStmt:
      hasConditionalLogic = true
      break

  if hasParameters or hasConditionalLogic:
    # Generate a proc that builds properties at runtime
    # This allows if statements and parameter usage
    var procBodyStmts = newStmtList()
    procBodyStmts.add quote do:
      result = @[]

    # Add the body statements directly, but convert property assignments to result.add calls
    for stmt in styleBody:
      procBodyStmts.add(convertStmtToPropertyAdd(stmt))

    # Build params
    var params: seq[NimNode] = @[]
    params.add(nnkBracketExpr.newTree(
      newIdentNode("seq"),
      newIdentNode("Property")
    ))
    for param in funcParams:
      params.add(param)

    result = newProc(
      name = funcName,
      params = params,
      body = procBodyStmts
    )
    return

  # For simple parameterless styles, extract properties at compile time
  var properties = newSeq[NimNode]()

  for stmt in styleBody:
    if stmt.kind == nnkStmtList:
      # If it's a statement list, process each statement
      for s in stmt:
        if s.kind == nnkExprEqExpr or s.kind == nnkAsgn:
          # Property assignment: prop = value
          let propName = s[0].strVal
          let propValue = s[1]
          let propNameLit = newLit(propName)

          # Use the simpler val() constructors directly for known types
          if propValue.kind == nnkStrLit:
            let strVal = propValue.strVal
            if strVal.startsWith("#"):
              properties.add quote do:
                (name: `propNameLit`, value: val(parseColor(`propValue`)))
            else:
              properties.add quote do:
                (name: `propNameLit`, value: val(`propValue`))
          elif propValue.kind == nnkIntLit:
            properties.add quote do:
              (name: `propNameLit`, value: val(`propValue`))
          elif propValue.kind == nnkFloatLit:
            properties.add quote do:
              (name: `propNameLit`, value: val(`propValue`))
          else:
            # For complex expressions, use val()
            properties.add quote do:
              (name: `propNameLit`, value: val(`propValue`))

        elif s.kind == nnkExprColonExpr:
          # Property assignment with colon: prop: value
          let propName = s[0].strVal
          let propValue = s[1]
          let propNameLit = newLit(propName)

          # Use the simpler val() constructors directly for known types
          if propValue.kind == nnkStrLit:
            let strVal = propValue.strVal
            if strVal.startsWith("#"):
              properties.add quote do:
                (name: `propNameLit`, value: val(parseColor(`propValue`)))
            else:
              properties.add quote do:
                (name: `propNameLit`, value: val(`propValue`))
          elif propValue.kind == nnkIntLit:
            properties.add quote do:
              (name: `propNameLit`, value: val(`propValue`))
          elif propValue.kind == nnkFloatLit:
            properties.add quote do:
              (name: `propNameLit`, value: val(`propValue`))
          else:
            # For complex expressions, use val()
            properties.add quote do:
              (name: `propNameLit`, value: val(`propValue`))

    elif stmt.kind == nnkExprEqExpr or stmt.kind == nnkAsgn:
      # Property assignment: prop = value
      let propName = stmt[0].strVal
      let propValue = stmt[1]
      let propNameLit = newLit(propName)

      # Use the simpler val() constructors directly for known types
      if propValue.kind == nnkStrLit:
        let strVal = propValue.strVal
        if strVal.startsWith("#"):
          properties.add quote do:
            (name: `propNameLit`, value: val(parseColor(`propValue`)))
        else:
          properties.add quote do:
            (name: `propNameLit`, value: val(`propValue`))
      elif propValue.kind == nnkIntLit:
        properties.add quote do:
          (name: `propNameLit`, value: val(`propValue`))
      elif propValue.kind == nnkFloatLit:
        properties.add quote do:
          (name: `propNameLit`, value: val(`propValue`))
      else:
        # For complex expressions, use val()
        properties.add quote do:
          (name: `propNameLit`, value: val(`propValue`))

    elif stmt.kind == nnkExprColonExpr:
      # Property assignment with colon: prop: value
      let propName = stmt[0].strVal
      let propValue = stmt[1]
      let propNameLit = newLit(propName)

      # Use the simpler val() constructors directly for known types
      if propValue.kind == nnkStrLit:
        let strVal = propValue.strVal
        if strVal.startsWith("#"):
          properties.add quote do:
            (name: `propNameLit`, value: val(parseColor(`propValue`)))
        else:
          properties.add quote do:
            (name: `propNameLit`, value: val(`propValue`))
      elif propValue.kind == nnkIntLit:
        properties.add quote do:
          (name: `propNameLit`, value: val(`propValue`))
      elif propValue.kind == nnkFloatLit:
        properties.add quote do:
          (name: `propNameLit`, value: val(`propValue`))
      else:
        # For complex expressions, use val()
        properties.add quote do:
          (name: `propNameLit`, value: val(`propValue`))

    else:
      # Skip other node types
      continue


  # Create the properties list
  let propertiesList = newNimNode(nnkBracket)
  for prop in properties:
    propertiesList.add(prop)

  # Generate a proc that returns seq[Property]
  # Build the proc body
  let procBody = quote do:
    var propertiesSeq: seq[Property] = @[]
    for prop in `propertiesList`:
      propertiesSeq.add(prop)
    result = propertiesSeq

  # Build params as seq[NimNode] - return type first, then parameters
  var params: seq[NimNode] = @[]
  params.add(nnkBracketExpr.newTree(
    newIdentNode("seq"),
    newIdentNode("Property")
  ))
  for param in funcParams:
    params.add(param)

  # Create the proc definition
  result = newProc(
    name = funcName,
    params = params,
    body = procBody
  )

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
  result.add quote do:
    clearFontResources()
  var resourcesNode: NimNode = nil
  var bodyChildren = newSeq[NimNode]()

  # Parse body to find Header, Resources, and/or Body elements
  for stmt in body:
    if stmt.kind == nnkCall:
      let name = stmt[0].strVal
      if name == "Header":
        headerNode = stmt
      elif name == "Resources":
        resourcesNode = stmt
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

  # Create app structure with Header, Resources, and Body
  let appVar = genSym(nskVar, "app")
  if resourcesNode != nil:
    result.add quote do:
      var `appVar` = newElement(ekBody)
      `appVar`.addChild(`headerNode`)
      `appVar`.addChild(`resourcesNode`)
      `appVar`.addChild(`bodyNode`)
      # Setup reorderable tabs automatically
      setupReorderableTabs(`appVar`)
      `appVar`
  else:
    result.add quote do:
      var `appVar` = newElement(ekBody)
      `appVar`.addChild(`headerNode`)
      `appVar`.addChild(`bodyNode`)
      # Setup reorderable tabs automatically
      setupReorderableTabs(`appVar`)
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
