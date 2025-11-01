## JavaScript Code Generation from Nim AST
##
## This module converts Nim AST nodes to equivalent JavaScript code.
## Used for generating web event handlers from Nim proc bodies.

import macros
import strutils, sequtils

proc astToJavaScript*(node: NimNode, indent: int = 2): string =
  ## Convert a Nim AST node to JavaScript code
  ## Returns a string containing the equivalent JavaScript

  let indentStr = " ".repeat(indent)

  case node.kind:

  # Literals
  of nnkIntLit, nnkInt8Lit, nnkInt16Lit, nnkInt32Lit, nnkInt64Lit,
     nnkUIntLit, nnkUInt8Lit, nnkUInt16Lit, nnkUInt32Lit, nnkUInt64Lit:
    result = $node.intVal

  of nnkFloatLit, nnkFloat32Lit, nnkFloat64Lit:
    result = $node.floatVal

  of nnkStrLit, nnkRStrLit, nnkTripleStrLit:
    # Escape quotes in string
    let escaped = node.strVal.replace("\"", "\\\"").replace("\n", "\\n")
    result = "\"" & escaped & "\""

  of nnkIdent, nnkSym:
    # Identifier or symbol
    result = node.strVal

  # Boolean literals
  of nnkEmpty:
    result = "null"

  # Operators and expressions
  of nnkInfix:
    # Binary operators: a + b, a == b, etc.
    let op = node[0].strVal
    let left = astToJavaScript(node[1], indent)
    let right = astToJavaScript(node[2], indent)

    # Map Nim operators to JavaScript
    let jsOp = case op:
      of "and": "&&"
      of "or": "||"
      of "mod": "%"
      of "div": "/"  # Integer division - JS doesn't distinguish
      of "shl": "<<"
      of "shr": ">>"
      else: op

    result = left & " " & jsOp & " " & right

  of nnkPrefix:
    # Unary operators: not x, -x, etc.
    let op = node[0].strVal
    let operand = astToJavaScript(node[1], indent)

    let jsOp = case op:
      of "not": "!"
      else: op

    result = jsOp & operand

  # Assignments
  of nnkAsgn:
    # Assignment: x = value
    # Wrap with updateState() for reactive state management
    let lhs = astToJavaScript(node[0], 0)
    let rhs = astToJavaScript(node[1], 0)

    # Check if this is a simple variable assignment (not array/object access)
    if node[0].kind == nnkIdent or node[0].kind == nnkSym:
      # Wrap in updateState call for reactivity
      result = indentStr & "updateState('" & lhs & "', " & rhs & ");\n"
    else:
      # Complex LHS (array/object access) - use direct assignment
      result = indentStr & lhs & " = " & rhs & ";\n"

  # Function calls
  of nnkCall, nnkCommand:
    # Function call: func(args)
    let funcName = astToJavaScript(node[0], 0)
    var args: seq[string] = @[]

    for i in 1 ..< node.len:
      args.add(astToJavaScript(node[i], 0))

    result = funcName & "(" & args.join(", ") & ")"

  # Dot expressions (member access)
  of nnkDotExpr:
    # Member access: obj.field
    let obj = astToJavaScript(node[0], 0)
    let field = astToJavaScript(node[1], 0)
    result = obj & "." & field

  # Bracket expressions (array/object access)
  of nnkBracketExpr:
    # Array access: arr[i]
    let arr = astToJavaScript(node[0], 0)
    let index = astToJavaScript(node[1], 0)
    result = arr & "[" & index & "]"

  # Control flow
  of nnkIfStmt, nnkIfExpr:
    # If statement/expression
    # Use ternary operator for expressions (indent == 0), if-statement for statements
    if indent == 0 and node.kind == nnkIfExpr:
      # Expression context - use ternary operator
      # Handle simple if-else case
      if node.len == 2 and node[0].kind in {nnkElifBranch, nnkElifExpr} and
         node[1].kind in {nnkElse, nnkElseExpr}:
        let cond = astToJavaScript(node[0][0], 0)
        let thenVal = astToJavaScript(node[0][1], 0)
        let elseVal = astToJavaScript(node[1][0], 0)
        result = "(" & cond & " ? " & thenVal & " : " & elseVal & ")"
      else:
        # Complex if-expression with multiple branches - use nested ternary
        result = ""
        for i in countdown(node.len - 1, 0):
          let branch = node[i]

          if branch.kind == nnkElse or branch.kind == nnkElseExpr:
            result = astToJavaScript(branch[0], 0)
          elif branch.kind == nnkElifBranch or branch.kind == nnkElifExpr:
            let cond = astToJavaScript(branch[0], 0)
            let thenVal = astToJavaScript(branch[1], 0)
            if result == "":
              result = thenVal  # No else clause yet
            else:
              result = "(" & cond & " ? " & thenVal & " : " & result & ")"
    else:
      # Statement context - use if-statement
      result = ""
      for i in 0 ..< node.len:
        let branch = node[i]

        if branch.kind == nnkElifBranch or branch.kind == nnkElifExpr:
          let cond = astToJavaScript(branch[0], 0)
          let body = astToJavaScript(branch[1], indent + 2)

          if i == 0:
            result &= indentStr & "if (" & cond & ") {\n"
          else:
            result &= indentStr & "} else if (" & cond & ") {\n"

          result &= body

        elif branch.kind == nnkElse or branch.kind == nnkElseExpr:
          result &= indentStr & "} else {\n"
          result &= astToJavaScript(branch[0], indent + 2)

      result &= indentStr & "}\n"

  # Loops
  of nnkForStmt:
    # For loop: for i in 0..<n
    let loopVar = node[0].strVal
    let iterable = node[1]
    let body = node[2]

    # Try to detect range loops: 0..<n or 0..n
    if iterable.kind == nnkInfix and iterable[0].strVal in ["..<", ".."]:
      let start = astToJavaScript(iterable[1], 0)
      let endVal = astToJavaScript(iterable[2], 0)
      let inclusive = if iterable[0].strVal == "..": " <= " else: " < "

      result = indentStr & "for (let " & loopVar & " = " & start & "; " &
               loopVar & inclusive & endVal & "; " & loopVar & "++) {\n"
      result &= astToJavaScript(body, indent + 2)
      result &= indentStr & "}\n"
    else:
      # Generic for-in loop: for item in collection
      let collection = astToJavaScript(iterable, 0)
      result = indentStr & "for (let " & loopVar & " of " & collection & ") {\n"
      result &= astToJavaScript(body, indent + 2)
      result &= indentStr & "}\n"

  of nnkWhileStmt:
    # While loop
    let cond = astToJavaScript(node[0], 0)
    let body = astToJavaScript(node[1], indent + 2)
    result = indentStr & "while (" & cond & ") {\n"
    result &= body
    result &= indentStr & "}\n"

  # Statement lists
  of nnkStmtList:
    # Multiple statements
    for child in node:
      result &= astToJavaScript(child, indent)

  # Parentheses
  of nnkPar:
    if node.len == 1:
      result = "(" & astToJavaScript(node[0], 0) & ")"
    else:
      # Tuple - convert to array
      var elements: seq[string] = @[]
      for child in node:
        elements.add(astToJavaScript(child, 0))
      result = "[" & elements.join(", ") & "]"

  # Arrays/sequences
  of nnkBracket:
    var elements: seq[string] = @[]
    for child in node:
      elements.add(astToJavaScript(child, 0))
    result = "[" & elements.join(", ") & "]"

  # Expression statement
  of nnkExprEqExpr:
    # Property assignment in object literal: key = value
    let key = astToJavaScript(node[0], 0)
    let value = astToJavaScript(node[1], 0)
    result = key & ": " & value

  # Discard statement
  of nnkDiscardStmt:
    if node.len > 0:
      result = indentStr & astToJavaScript(node[0], 0) & ";\n"

  # Return statement
  of nnkReturnStmt:
    if node.len > 0 and node[0].kind != nnkEmpty:
      result = indentStr & "return " & astToJavaScript(node[0], 0) & ";\n"
    else:
      result = indentStr & "return;\n"

  # Procedure calls as statements
  of nnkCallStrLit:
    # Special string call like echo "text"
    result = indentStr & astToJavaScript(node, 0) & ";\n"

  # Type conversions
  of nnkConv, nnkHiddenStdConv:
    # Type conversion - just convert the value
    result = astToJavaScript(node[1], indent)

  # Pragmas - ignore
  of nnkPragma:
    result = ""

  # Comments
  of nnkCommentStmt:
    result = indentStr & "// " & node.strVal & "\n"

  else:
    # Unsupported node - add comment
    result = indentStr & "/* Unsupported Nim construct: " & $node.kind & " */\n"


proc wrapInEventHandler*(jsBody: string): string =
  ## Wrap JavaScript code in an event handler function
  result = "  // Auto-generated from Nim event handler\n"
  result &= jsBody


proc substituteLoopVariables*(jsCode: string, substitutions: openArray[tuple[varName: string, value: string]]): string =
  ## Replace loop variable references with actual values
  ## Example: substituteLoopVariables("updateState('x', i);", [("i", "2")]) -> "updateState('x', 2);"
  result = jsCode

  for (varName, value) in substitutions:
    # Replace standalone variable references
    # Match word boundaries to avoid replacing partial matches
    result = result.replace(varName & ")", value & ")")
    result = result.replace(varName & ";", value & ";")
    result = result.replace(varName & ",", value & ",")
    result = result.replace(varName & " ", value & " ")


proc injectStateManagement*(jsCode: string, stateVars: seq[string]): string =
  ## Inject updateState calls for known state variables
  ## This is a simple pattern matcher - can be enhanced
  result = jsCode

  for varName in stateVars:
    # Pattern: varName = expr; -> updateState('varName', expr);
    # This is a simple replacement - a proper implementation would use AST
    let pattern = varName & " = "
    if pattern in result:
      # For now, just add a comment reminder
      result = "  // TODO: Auto-inject updateState for: " & varName & "\n" & result
