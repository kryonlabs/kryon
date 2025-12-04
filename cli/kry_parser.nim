## .kry Language Parser
##
## Recursive descent parser for the Kryon DSL (.kry files).
## Converts a token stream into an AST.

import std/[strutils, strformat, tables]
import kry_ast, kry_lexer

type
  Parser* = object
    lex*: Lexer
    inLangBlock: bool

  ParseError* = object of CatchableError

proc newParser*(source: string, filename = "<input>"): Parser =
  var lex = newLexer(source, filename)
  discard lex.tokenize()
  Parser(lex: lex, inLangBlock: false)

proc error(p: Parser, msg: string) =
  let tok = p.lex.currentToken()
  raise newException(ParseError, &"{tok.loc}: {msg}")

proc currentLoc(p: Parser): SourceLoc =
  p.lex.currentToken().loc

# Forward declarations
proc parseExpression(p: var Parser): KryNode
proc parseBlockContents(p: var Parser): seq[KryNode]
proc parseChild(p: var Parser): KryNode
proc parseForLoop(p: var Parser, isConst = false): KryNode
proc parseStaticBlock(p: var Parser): KryNode

# Expression parsing with precedence climbing
proc parsePrimary(p: var Parser): KryNode =
  let tok = p.lex.currentToken()
  let loc = tok.loc

  case tok.kind
  of tkNumber:
    discard p.lex.advanceToken()
    if tok.value.contains('.'):
      result = newLitFloat(parseFloat(tok.value), loc)
    else:
      result = newLitInt(parseInt(tok.value), loc)
  of tkPercent:
    discard p.lex.advanceToken()
    result = newLitPercent(parseFloat(tok.value), loc)
  of tkString:
    discard p.lex.advanceToken()
    # Check if it's a color literal
    if tok.value.startsWith("#"):
      result = newLitColor(tok.value, loc)
    else:
      result = newLitString(tok.value, loc)
  of tkTrue:
    discard p.lex.advanceToken()
    result = newLitBool(true, loc)
  of tkFalse:
    discard p.lex.advanceToken()
    result = newLitBool(false, loc)
  of tkAuto:
    discard p.lex.advanceToken()
    result = newLitAuto(loc)
  of tkIdent:
    let name = tok.value
    discard p.lex.advanceToken()
    # Check for function call
    if p.lex.check(tkLParen):
      discard p.lex.advanceToken()  # (
      var args: seq[KryNode] = @[]
      if not p.lex.check(tkRParen):
        args.add(p.parseExpression())
        while p.lex.match(tkComma):
          args.add(p.parseExpression())
      discard p.lex.expect(tkRParen)
      result = newCallExpr(name, args, loc)
    else:
      result = newIdent(name, loc)
  of tkDollar:
    # Interpolation: $value
    discard p.lex.advanceToken()  # $
    let identTok = p.lex.expect(tkIdent)
    result = newInterpExpr(newIdent(identTok.value, identTok.loc), loc)
  of tkLParen:
    # Parenthesized expression
    discard p.lex.advanceToken()  # (
    result = p.parseExpression()
    discard p.lex.expect(tkRParen)
  of tkLBracket:
    # Array literal: [a, b, c]
    discard p.lex.advanceToken()  # [
    var elems: seq[KryNode] = @[]
    if not p.lex.check(tkRBracket):
      elems.add(p.parseExpression())
      while p.lex.match(tkComma):
        elems.add(p.parseExpression())
    discard p.lex.expect(tkRBracket)
    result = newArrayExpr(elems, loc)
  of tkNot:
    # Unary not
    discard p.lex.advanceToken()
    result = newUnaryExpr(opNot, p.parsePrimary(), loc)
  of tkMinus:
    # Unary negation
    discard p.lex.advanceToken()
    result = newUnaryExpr(opNeg, p.parsePrimary(), loc)
  of tkLBrace:
    # Block expression: { statements }
    discard p.lex.advanceToken()  # {
    var stmts: seq[KryNode] = @[]
    while not p.lex.check(tkRBrace) and not p.lex.check(tkEof):
      stmts.add(p.parseExpression())
    discard p.lex.expect(tkRBrace)
    result = newBlockExpr(stmts, loc)
  else:
    p.error(&"Unexpected token in expression: {tok.kind} ('{tok.value}')")

proc parsePostfix(p: var Parser): KryNode =
  result = p.parsePrimary()

  while true:
    let loc = p.currentLoc()
    if p.lex.match(tkDot):
      # Member access: a.b
      let memberTok = p.lex.expect(tkIdent)
      result = newMemberExpr(result, memberTok.value, loc)
    elif p.lex.check(tkLParen):
      # Function call on result
      discard p.lex.advanceToken()  # (
      var args: seq[KryNode] = @[]
      if not p.lex.check(tkRParen):
        args.add(p.parseExpression())
        while p.lex.match(tkComma):
          args.add(p.parseExpression())
      discard p.lex.expect(tkRParen)
      # Convert member to call
      if result.kind == nkExprMember:
        result = newCallExpr(result.memberName, @[result.memberObj] & args, loc)
      elif result.kind == nkExprIdent:
        result = newCallExpr(result.identName, args, loc)
      else:
        p.error("Invalid call target")
    elif p.lex.check(tkLBracket):
      # Array index: a[b]
      discard p.lex.advanceToken()  # [
      let index = p.parseExpression()
      discard p.lex.expect(tkRBracket)
      result = newCallExpr("index", @[result, index], loc)
    else:
      break

proc parseMultiplicative(p: var Parser): KryNode =
  result = p.parsePostfix()
  while true:
    let loc = p.currentLoc()
    if p.lex.match(tkStar):
      result = newBinaryExpr(opMul, result, p.parsePostfix(), loc)
    elif p.lex.match(tkSlash):
      result = newBinaryExpr(opDiv, result, p.parsePostfix(), loc)
    elif p.lex.match(tkPercOp):
      result = newBinaryExpr(opMod, result, p.parsePostfix(), loc)
    else:
      break

proc parseAdditive(p: var Parser): KryNode =
  result = p.parseMultiplicative()
  while true:
    let loc = p.currentLoc()
    if p.lex.match(tkPlus):
      result = newBinaryExpr(opAdd, result, p.parseMultiplicative(), loc)
    elif p.lex.match(tkMinus):
      result = newBinaryExpr(opSub, result, p.parseMultiplicative(), loc)
    else:
      break

proc parseComparison(p: var Parser): KryNode =
  result = p.parseAdditive()
  while true:
    let loc = p.currentLoc()
    if p.lex.match(tkLess):
      result = newBinaryExpr(opLt, result, p.parseAdditive(), loc)
    elif p.lex.match(tkLessEq):
      result = newBinaryExpr(opLe, result, p.parseAdditive(), loc)
    elif p.lex.match(tkGreater):
      result = newBinaryExpr(opGt, result, p.parseAdditive(), loc)
    elif p.lex.match(tkGreaterEq):
      result = newBinaryExpr(opGe, result, p.parseAdditive(), loc)
    else:
      break

proc parseEquality(p: var Parser): KryNode =
  result = p.parseComparison()
  while true:
    let loc = p.currentLoc()
    if p.lex.match(tkEqEq):
      result = newBinaryExpr(opEq, result, p.parseComparison(), loc)
    elif p.lex.match(tkNotEq):
      result = newBinaryExpr(opNe, result, p.parseComparison(), loc)
    else:
      break

proc parseLogicalAnd(p: var Parser): KryNode =
  result = p.parseEquality()
  while p.lex.match(tkAnd):
    let loc = p.currentLoc()
    result = newBinaryExpr(opAnd, result, p.parseEquality(), loc)

proc parseLogicalOr(p: var Parser): KryNode =
  result = p.parseLogicalAnd()
  while p.lex.match(tkOr):
    let loc = p.currentLoc()
    result = newBinaryExpr(opOr, result, p.parseLogicalAnd(), loc)

proc parseTernary(p: var Parser): KryNode =
  result = p.parseLogicalOr()
  if p.lex.match(tkQuestion):
    let loc = p.currentLoc()
    let thenExpr = p.parseExpression()
    discard p.lex.expect(tkColon)
    let elseExpr = p.parseTernary()
    result = newTernaryExpr(result, thenExpr, elseExpr, loc)

proc parseAssignment(p: var Parser): KryNode =
  result = p.parseTernary()
  let loc = p.currentLoc()
  if p.lex.match(tkEquals):
    let right = p.parseAssignment()
    result = newBinaryExpr(opAssign, result, right, loc)
  elif p.lex.match(tkPlusEq):
    let right = p.parseAssignment()
    result = newBinaryExpr(opAddAssign, result, right, loc)
  elif p.lex.match(tkMinusEq):
    let right = p.parseAssignment()
    result = newBinaryExpr(opSubAssign, result, right, loc)

proc parseExpression(p: var Parser): KryNode =
  p.parseAssignment()

# Parse parameter list: (name: type = default, ...)
proc parseParams(p: var Parser): seq[ParamDef] =
  discard p.lex.expect(tkLParen)
  if p.lex.check(tkRParen):
    discard p.lex.advanceToken()
    return @[]

  var params: seq[ParamDef] = @[]
  while true:
    let nameTok = p.lex.expect(tkIdent)
    var param = ParamDef(name: nameTok.value, typeName: "any", defaultValue: nil)

    if p.lex.match(tkColon):
      # Type annotation
      let typeTok = p.lex.expect(tkIdent)
      param.typeName = typeTok.value
      # Handle generic types like array<string>
      if p.lex.match(tkLess):
        let innerType = p.lex.expect(tkIdent)
        param.typeName &= "<" & innerType.value & ">"
        discard p.lex.expect(tkGreater)

    if p.lex.match(tkEquals):
      # Default value
      param.defaultValue = p.parseExpression()

    params.add(param)

    if not p.lex.match(tkComma):
      break

  discard p.lex.expect(tkRParen)
  params

# Parse state declaration: state name: type = value
proc parseStateDecl(p: var Parser): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkState)
  let nameTok = p.lex.expect(tkIdent)
  discard p.lex.expect(tkColon)
  let typeTok = p.lex.expect(tkIdent)
  var typeName = typeTok.value
  # Handle generic types
  if p.lex.match(tkLess):
    let innerType = p.lex.expect(tkIdent)
    typeName &= "<" & innerType.value & ">"
    discard p.lex.expect(tkGreater)
  discard p.lex.expect(tkEquals)
  let initValue = p.parseExpression()
  newStateDef(nameTok.value, typeName, initValue, loc)

# Parse const declaration: const name: type = value OR const name = value
proc parseConstDecl(p: var Parser): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkConst)

  # Check if this is "const for" (compile-time loop)
  if p.lex.check(tkFor):
    return p.parseForLoop(isConst = true)

  let nameTok = p.lex.expect(tkIdent)

  var typeName = ""
  if p.lex.match(tkColon):
    # Type annotation: const name: type = value
    let typeTok = p.lex.expect(tkIdent)
    typeName = typeTok.value
    # Handle generic types like array<string>
    if p.lex.match(tkLess):
      let innerType = p.lex.expect(tkIdent)
      typeName &= "<" & innerType.value & ">"
      discard p.lex.expect(tkGreater)

  discard p.lex.expect(tkEquals)
  let value = p.parseExpression()
  newConstDef(nameTok.value, value, typeName, loc)

# Parse conditional: if expr { } else { }
proc parseConditional(p: var Parser): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkIf)
  let cond = p.parseExpression()
  discard p.lex.expect(tkLBrace)
  let thenBranch = p.parseBlockContents()
  discard p.lex.expect(tkRBrace)

  var elseBranch: seq[KryNode] = @[]
  if p.lex.match(tkElse):
    if p.lex.check(tkIf):
      # else if
      elseBranch.add(p.parseConditional())
    else:
      discard p.lex.expect(tkLBrace)
      elseBranch = p.parseBlockContents()
      discard p.lex.expect(tkRBrace)

  newConditional(cond, thenBranch, elseBranch, loc)

# Parse for loop: for item in expr { } or const for item in expr { }
proc parseForLoop(p: var Parser, isConst = false): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkFor)
  let varTok = p.lex.expect(tkIdent)
  discard p.lex.expect(tkIn)
  let iterExpr = p.parseExpression()
  discard p.lex.expect(tkLBrace)
  let body = p.parseBlockContents()
  discard p.lex.expect(tkRBrace)
  newForLoop(varTok.value, iterExpr, body, isConst, loc)

# Parse language block: @nim { code }
proc parseLangBlock(p: var Parser): KryNode =
  let loc = p.currentLoc()
  let tok = p.lex.expect(tkLangBlock)
  # Value is "langName:code"
  let colonPos = tok.value.find(':')
  let langName = tok.value[0..<colonPos]
  let code = tok.value[colonPos+1..^1]
  newLangBlock(langName, code, loc)

# Parse static block: static { ... } - compile-time evaluation
proc parseStaticBlock(p: var Parser): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkStatic)
  discard p.lex.expect(tkLBrace)
  let body = p.parseBlockContents()
  discard p.lex.expect(tkRBrace)
  newStaticBlock(body, loc)

# Parse a single property or nested component
proc parseChild(p: var Parser): KryNode =
  let tok = p.lex.currentToken()
  let loc = tok.loc

  case tok.kind
  of tkState:
    return p.parseStateDecl()
  of tkConst:
    return p.parseConstDecl()
  of tkIf:
    return p.parseConditional()
  of tkFor:
    return p.parseForLoop()
  of tkLangBlock:
    return p.parseLangBlock()
  of tkStatic:
    return p.parseStaticBlock()
  of tkIdent:
    let name = tok.value
    discard p.lex.advanceToken()

    # Check if it's a component (followed by { or ()
    if p.lex.check(tkLBrace):
      # Component instantiation: ComponentName { }
      discard p.lex.advanceToken()  # {
      var comp = newComponent(name, loc)
      let contents = p.parseBlockContents()
      for item in contents:
        if item.kind == nkProperty:
          comp.componentProps[item.propName] = item.propValue
        else:
          comp.componentChildren.add(item)
      discard p.lex.expect(tkRBrace)
      return comp
    elif p.lex.check(tkLParen):
      # Component instantiation with args: Counter(5)
      discard p.lex.advanceToken()  # (
      var comp = newComponent(name, loc)
      var argIdx = 0
      if not p.lex.check(tkRParen):
        while true:
          # Check for named argument
          if p.lex.check(tkIdent) and p.lex.peekToken(1).kind == tkEquals:
            let argName = p.lex.advanceToken().value
            discard p.lex.advanceToken()  # =
            let argValue = p.parseExpression()
            comp.componentProps[argName] = argValue
          else:
            # Positional argument
            comp.componentArgs.add(p.parseExpression())
          if not p.lex.match(tkComma):
            break
      discard p.lex.expect(tkRParen)

      # Check for optional body
      if p.lex.check(tkLBrace):
        discard p.lex.advanceToken()  # {
        let contents = p.parseBlockContents()
        for item in contents:
          if item.kind == nkProperty:
            comp.componentProps[item.propName] = item.propValue
          else:
            comp.componentChildren.add(item)
        discard p.lex.expect(tkRBrace)
      return comp
    elif p.lex.check(tkEquals):
      # Property: name = value
      discard p.lex.advanceToken()  # =
      let value = p.parseExpression()
      return newProperty(name, value, loc)
    else:
      p.error(&"Expected '{{', '(' or '=' after identifier '{name}'")
  else:
    p.error(&"Unexpected token: {tok.kind} ('{tok.value}')")

proc parseBlockContents(p: var Parser): seq[KryNode] =
  var items: seq[KryNode] = @[]
  while not p.lex.check(tkRBrace) and not p.lex.check(tkEof):
    items.add(p.parseChild())
  items

# Parse component definition: component Name(params) { }
proc parseComponentDef(p: var Parser): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkComponent)
  let nameTok = p.lex.expect(tkIdent)
  let params = p.parseParams()
  discard p.lex.expect(tkLBrace)
  let body = p.parseBlockContents()
  discard p.lex.expect(tkRBrace)
  newComponentDef(nameTok.value, params, body, loc)

# Parse App block: App { }
proc parseAppBlock(p: var Parser): KryNode =
  let loc = p.currentLoc()
  discard p.lex.expect(tkApp)
  discard p.lex.expect(tkLBrace)
  let body = p.parseBlockContents()
  discard p.lex.expect(tkRBrace)
  newAppBlock(body, loc)

# Parse top-level statement
proc parseStatement(p: var Parser): KryNode =
  let tok = p.lex.currentToken()
  case tok.kind
  of tkComponent:
    result = p.parseComponentDef()
  of tkState:
    result = p.parseStateDecl()
  of tkConst:
    result = p.parseConstDecl()
  of tkApp:
    result = p.parseAppBlock()
  of tkLangBlock:
    # Language-specific code block: @nim { }, @lua { }, @js { }, etc.
    result = p.parseLangBlock()
  of tkIdent:
    # Could be a component instantiation at top level
    result = p.parseChild()
  else:
    p.error(&"Unexpected token at top level: {tok.kind} ('{tok.value}')")
    result = nil  # Never reached, but needed for type

# Main parse entry point
proc parse*(p: var Parser): KryNode =
  var statements: seq[KryNode] = @[]
  while not p.lex.check(tkEof):
    statements.add(p.parseStatement())
  newProgram(statements, SourceLoc(line: 1, column: 1, filename: p.lex.filename))

# Convenience function
proc parseKry*(source: string, filename = "<input>"): KryNode =
  var parser = newParser(source, filename)
  parser.parse()
