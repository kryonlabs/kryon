## Reactive Expression Detection and Conditional Conversion
##
## This module handles:
## - Detection of reactive variable references ($var syntax)
## - Extraction of reactive variables from expressions
## - Conversion of if/case statements to reactive conditionals
## - Loop variable transformation for proper closure capture

import macros
import ./properties  # For accessing helper utilities

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

# ============================================================================
# Loop Variable Transformation
# ============================================================================

proc transformLoopVariableReferences*(node: NimNode, originalVar: NimNode, capturedVar: NimNode): NimNode =
  ## Recursively replace all ident nodes that match the original loop variable with captured copy
  result = node.copyNimTree()
  if result.kind == nnkIdent and result.eqIdent(originalVar):
    # Replace with captured variable reference
    result = capturedVar
  else:
    # Recursively process child nodes
    for i in 0..<result.len:
      result[i] = transformLoopVariableReferences(result[i], originalVar, capturedVar)

# ============================================================================
# If Statement to Reactive Conditional Conversion
# ============================================================================

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
      let condExprStr = newLit(repr(condition))

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = `condition`
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil, `condExprStr`)

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
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil, "else")

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
      let condExprStr = newLit(repr(ifCondition))

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = `ifCondition`
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil, `condExprStr`)

  # Return the container
  reactiveCode.add(containerSym)
  result = reactiveCode

# ============================================================================
# Case Statement to Reactive Conditional Conversion
# ============================================================================

proc convertCaseStmtToReactiveConditional*(caseStmt: NimNode, windowWidth: NimNode, windowHeight: NimNode): NimNode =
  ## Convert a case statement to reactive component expression
  ## Creates a reactive container that evaluates the case statement and returns the appropriate component
  let caseExpr = caseStmt[0]

  # Create a container for the reactive expression
  let containerSym = genSym(nskLet, "caseContainer")

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
      let condExprStr = newLit(repr(condition))

      reactiveCode.add quote do:
        let `conditionProc` = proc(): bool {.closure.} = `condition`
        let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
        createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil, `condExprStr`)

  # Handle else branch if present
  let lastBranch = caseStmt[caseStmt.len - 1]
  if lastBranch.kind == nnkElse:
    let elseBody = lastBranch[0]
    let conditionProc = genSym(nskLet, "caseElseConditionProc")
    let thenProc = genSym(nskLet, "caseElseThenProc")
    let thenBody = newCall(ident("@"), newNimNode(nnkBracket).add(elseBody))

    reactiveCode.add quote do:
      let `conditionProc` = proc(): bool {.closure.} = true  # else is always true
      let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBody`
      createReactiveConditional(`containerSym`, `conditionProc`, `thenProc`, nil, "else")

  # Return the container
  reactiveCode.add(containerSym)

  result = reactiveCode

# ============================================================================
# Automatic Reactive Variable Detection (For DSL Integration)
# ============================================================================

proc analyzeHandlerMutations*(handler: NimNode): seq[string] =
  ## Detect which variables are modified in onClick handlers
  ## Returns a list of variable names that are mutated
  var vars: seq[string] = @[]

  proc scan(n: NimNode, vars: var seq[string]) =
    case n.kind:
    of nnkAsgn:
      # value = 10
      if n[0].kind == nnkIdent:
        vars.add($n[0])

    of nnkInfix:
      # value += 1, value -= 1, value *= 2, value /= 2
      if n[0].kind == nnkIdent and n[0].strVal in ["+=", "-=", "*=", "/=", "&=", "|=", "^="]:
        if n.len >= 2 and n[1].kind == nnkIdent:
          vars.add($n[1])

    of nnkCall:
      # inc(value), dec(value)
      if n[0].kind == nnkIdent and n[0].strVal in ["inc", "dec"]:
        if n.len > 1 and n[1].kind == nnkIdent:
          vars.add($n[1])

    else:
      for child in n: scan(child, vars)

  scan(handler, vars)

  # Remove duplicates
  var seen: seq[string] = @[]
  result = @[]
  for varName in vars:
    if varName notin seen:
      seen.add(varName)
      result.add(varName)

proc extractVariableReferences*(expr: NimNode): seq[string] =
  ## Extract variable names from text expressions like $value
  ## Returns a list of variable names referenced in the expression
  var vars: seq[string] = @[]

  proc scan(n: NimNode, vars: var seq[string]): void =
    if n.kind == nnkIdent:
      vars.add($n)
    elif n.kind == nnkPrefix and n.len >= 1:
      if n[0].kind == nnkIdent and n[0].strVal == "$":
        # Found $variable pattern
        if n.len >= 2 and n[1].kind == nnkIdent:
          vars.add($n[1])
        elif n.len >= 2:
          scan(n[1], vars)
    elif n.kind == nnkDotExpr and n.len >= 1:
      # field.value - extract the base
      scan(n[0], vars)
    else:
      for child in n: scan(child, vars)

  scan(expr, vars)

  # Remove duplicates
  var seen: seq[string] = @[]
  result = @[]
  for varName in vars:
    if varName notin seen:
      seen.add(varName)
      result.add(varName)
