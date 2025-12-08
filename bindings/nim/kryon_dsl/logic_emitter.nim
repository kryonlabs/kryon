## Logic Emitter - Analyze Nim handlers and convert to universal IR expressions
##
## This module analyzes onClick handler ASTs at compile-time and determines
## if they can be converted to universal expressions (transpilable to any target)
## or if they need to be embedded as language-specific source code.
##
## Supported universal patterns:
## - value += N → assign(value, add(var(value), N))
## - value -= N → assign(value, sub(var(value), N))
## - value = N → assign(value, N)
## - value = !value → toggle
## - Simple variable assignments

import macros, strutils

type
  LogicAnalysisResult* = object
    isUniversal*: bool
    handlerName*: string
    targetVar*: string
    operation*: string  # "increment", "decrement", "assign", "toggle"
    operand*: string    # The literal value (e.g., "1")
    nimSource*: string  # Fallback embedded source

proc analyzeHandlerBody*(body: NimNode): LogicAnalysisResult =
  ## Analyze a handler body AST and determine if it's a universal pattern
  ## Returns analysis result with either universal expression info or embedded source

  result.isUniversal = false
  result.nimSource = body.repr

  # Handle statement list with single statement
  var stmtBody = body
  if stmtBody.kind == nnkStmtList and stmtBody.len == 1:
    stmtBody = stmtBody[0]

  # Pattern: value += N
  if stmtBody.kind == nnkInfix:
    let op = $stmtBody[0]

    case op
    of "+=":
      # value += 1 → increment
      if stmtBody[1].kind == nnkIdent and stmtBody[2].kind == nnkIntLit:
        result.isUniversal = true
        result.targetVar = $stmtBody[1]
        result.operation = "increment"
        result.operand = $stmtBody[2].intVal
        return
    of "-=":
      # value -= 1 → decrement
      if stmtBody[1].kind == nnkIdent and stmtBody[2].kind == nnkIntLit:
        result.isUniversal = true
        result.targetVar = $stmtBody[1]
        result.operation = "decrement"
        result.operand = $stmtBody[2].intVal
        return
    else:
      discard

  # Pattern: value = N (simple assignment)
  if stmtBody.kind == nnkAsgn:
    if stmtBody[0].kind == nnkIdent:
      let target = $stmtBody[0]
      let value = stmtBody[1]

      # value = 0 or value = literal
      if value.kind == nnkIntLit:
        result.isUniversal = true
        result.targetVar = target
        result.operation = "assign"
        result.operand = $value.intVal
        return

      # value = !value → toggle
      # Handle both `not value` and `(not value)` patterns
      var notExpr = value
      # Unwrap parentheses if present
      if value.kind == nnkPar and value.len == 1:
        notExpr = value[0]
      if notExpr.kind == nnkPrefix and $notExpr[0] == "not":
        if notExpr[1].kind == nnkIdent and $notExpr[1] == target:
          result.isUniversal = true
          result.targetVar = target
          result.operation = "toggle"
          result.operand = ""
          return

  # Pattern: value.value += N (reactive state access)
  if stmtBody.kind == nnkInfix:
    let op = $stmtBody[0]
    let lhs = stmtBody[1]
    let rhs = stmtBody[2]

    # Check for value.value pattern
    if lhs.kind == nnkDotExpr and $lhs[1] == "value":
      let varName = $lhs[0]

      case op
      of "+=":
        if rhs.kind == nnkIntLit:
          result.isUniversal = true
          result.targetVar = varName
          result.operation = "increment"
          result.operand = $rhs.intVal
          return
      of "-=":
        if rhs.kind == nnkIntLit:
          result.isUniversal = true
          result.targetVar = varName
          result.operation = "decrement"
          result.operand = $rhs.intVal
          return
      else:
        discard

proc isNamedProcReference*(handler: NimNode): bool =
  ## Check if handler is a reference to a named proc (not inline lambda)
  handler.kind in {nnkIdent, nnkSym}

proc getNamedProcSource*(handler: NimNode): string =
  ## For a named proc reference, use getImpl to get its full source
  ## Returns the full proc definition if available, otherwise just the identifier
  ## Note: getImpl only works on resolved symbols (nnkSym), not identifiers (nnkIdent)
  ## For identifiers, we return just the name - the source will come from registerSource
  if handler.kind == nnkSym:
    let impl = handler.getImpl()
    if impl != nil and impl.kind == nnkProcDef:
      return impl.repr
  # For nnkIdent or failed getImpl, return just the name
  return handler.repr

proc analyzeHandler*(handler: NimNode): LogicAnalysisResult =
  ## Analyze a full handler (proc or lambda) and extract the body analysis

  result.isUniversal = false
  result.nimSource = handler.repr

  case handler.kind
  of nnkLambda, nnkProcDef:
    # Extract body from proc/lambda
    let body = handler.body
    result = analyzeHandlerBody(body)
    if not result.isUniversal:
      result.nimSource = handler.repr
  of nnkStmtList:
    # Direct statement list
    result = analyzeHandlerBody(handler)
  of nnkIdent, nnkSym:
    # Named proc reference - use getImpl to get full source
    result.nimSource = getNamedProcSource(handler)
    result.handlerName = $handler
  else:
    # Unrecognized handler type
    result.nimSource = handler.repr

proc generateLogicFunctionName*(componentType: string, eventType: string, varName: string): string =
  ## Generate a unique logic function name
  ## e.g., "Counter:onClick_value_increment"
  result = componentType & ":" & eventType & "_" & varName

proc isSimpleHandler*(handler: NimNode): bool =
  ## Quick check if handler might be convertible to universal expression
  let analysis = analyzeHandler(handler)
  return analysis.isUniversal

# Debug helper to print AST structure
proc debugHandlerAST*(handler: NimNode): string =
  result = "Handler AST:\n"
  result &= "  Kind: " & $handler.kind & "\n"
  result &= "  Repr: " & handler.repr & "\n"

  if handler.kind in {nnkLambda, nnkProcDef}:
    result &= "  Body kind: " & $handler.body.kind & "\n"
    result &= "  Body repr: " & handler.body.repr & "\n"

# =============================================================================
# Compile-time source file scanning for named proc extraction
# =============================================================================

proc extractProcSourceFromFile*(sourceFile: string, procName: string): string {.compileTime.} =
  ## Extract the source code of a proc from a source file at compile time
  ## Scans for "proc procName" and captures until the next proc or end of indentation

  try:
    let content = staticRead(sourceFile)
    let lines = content.split('\n')

    # Find the proc definition
    var inProc = false
    var procLines: seq[string] = @[]
    var baseIndent = 0

    for line in lines:
      if not inProc:
        # Look for proc definition: "proc procName" or "proc procName*"
        let procPattern1 = "proc " & procName & "*"
        let procPattern2 = "proc " & procName & "("
        let procPattern3 = "proc " & procName & " "

        if line.contains(procPattern1) or line.contains(procPattern2) or line.contains(procPattern3):
          inProc = true
          procLines.add(line)
          # Calculate base indentation
          baseIndent = 0
          for c in line:
            if c == ' ':
              baseIndent += 1
            elif c == '\t':
              baseIndent += 4
            else:
              break
      else:
        # Check if we've left the proc (new proc, or back to base indentation with content)
        let stripped = line.strip()
        if stripped.len > 0:
          # Calculate current indentation
          var currentIndent = 0
          for c in line:
            if c == ' ':
              currentIndent += 1
            elif c == '\t':
              currentIndent += 4
            else:
              break

          # If we're at base indent or less and it's a new definition, stop
          if currentIndent <= baseIndent and stripped.startsWith("proc "):
            break

          # If we're back to base indentation with content, check if it's part of the proc
          if currentIndent <= baseIndent and not line.startsWith(" ") and not line.startsWith("\t"):
            # Line at column 0 that's not empty - probably a new definition
            if stripped.len > 0 and not stripped.startsWith("#"):
              break

        procLines.add(line)

    if procLines.len > 0:
      return procLines.join("\n")
  except:
    discard  # staticRead can fail, fallback to procName

  return procName  # fallback

proc findProcSourceInCallerFile*(callerInfo: LineInfo, procName: string): string {.compileTime.} =
  ## Find the proc source in the file where the macro is being called
  let sourceFile = $callerInfo.filename
  return extractProcSourceFromFile(sourceFile, procName)
