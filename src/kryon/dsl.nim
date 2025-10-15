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

import macros, strutils, options, unicode
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
                      "Dropdown", "Grid", "Image", "Center", "ScrollView", "Header", "Body",
                      "TabGroup", "TabBar", "Tab", "TabContent", "TabPanel", "Link"]:
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

const builtinElementNames = [
  "Container", "Text", "Button", "Column", "Row", "Input", "Checkbox",
  "Dropdown", "Grid", "Image", "Center", "ScrollView", "Header", "Body",
  "Resources", "Font", "TabGroup", "TabBar", "Tab", "TabContent", "TabPanel", "Link"
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

proc ensureEventHandler(node: NimNode): NimNode =
  ## Wrap the node in a zero-arg proc to use as an event handler
  let invocation = toInvocation(node)
  quote do:
    proc(data: string = "") {.closure.} =
      `invocation`
      invalidateAllReactiveValues()

proc emitEventAssignment(elemVar: NimNode, propName: string, handlerNode: NimNode): seq[NimNode] =
  ## Generate statements to attach an event handler to an element
  var handlerExpr = handlerNode

  if handlerExpr.kind == nnkStmtList:
    if handlerExpr.len == 1:
      handlerExpr = handlerExpr[0]
    elif handlerExpr.len > 1:
      let body = copyNimTree(handlerExpr)
      let closureProc = quote do:
        proc(data: string = "") {.closure.} =
          `body`
          invalidateAllReactiveValues()
      return @[
        quote do:
          `elemVar`.setEventHandler(resolvePropertyAlias(`propName`), `closureProc`)
      ]

  let cleanHandler = ensureEventHandler(copyNimTree(handlerExpr))
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

  let parseResult = parsePropertyValue(valueNode)
  let propValue = parseResult.value

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
          # This is likely a regular procedure call like `echo`.
          # It should not be added to the UI tree. We discard it.
          # The user should place such calls inside event handlers if needed.
          discard
    of nnkCommand:
      # Command syntax: might be a UI component or a regular procedure call.
      # We check if it's a capitalized component name.
      if stmt.len > 0 and stmt[0].kind == nnkIdent:
        let nameStr = stmt[0].strVal
        if nameStr.len > 0 and nameStr[0] in 'A'..'Z':
          # Assumed to be a UI component, add it as a child.
          result.add quote do:
            `elemVar`.addChild(`stmt`)

    of nnkAsgn:
      # Also handle nnkAsgn (rare, but valid)
      when defined(debugDropdown):
        echo "Debug: nnkAsgn: stmt[0] = ", stmt[0].repr, " kind = ", stmt[0].kind
      let propName = stmt[0].strVal
      when defined(debugDropdown):
        echo "Debug: nnkAsgn: propName = ", propName

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
      `appVar`
  else:
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
