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
      if value.kind == nnkPrefix and $value[0] == "not":
        if value[1].kind == nnkIdent and $value[1] == target:
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
