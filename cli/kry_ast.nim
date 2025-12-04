## .kry Language AST Types
##
## This module defines the Abstract Syntax Tree (AST) types for the Kryon
## domain-specific language (.kry files).

import std/[json, tables, strutils, strformat]

type
  # Source location for error reporting
  SourceLoc* = object
    line*: int
    column*: int
    filename*: string

  # Token types for the lexer
  TokenKind* = enum
    tkEof
    tkNewline
    tkIndent
    tkDedent
    # Literals
    tkIdent       # identifier
    tkString      # "string"
    tkNumber      # 123, 123.45
    tkPercent     # 100%
    # Keywords
    tkComponent   # component
    tkState       # state
    tkConst       # const
    tkIf          # if
    tkElse        # else
    tkFor         # for
    tkIn          # in
    tkTrue        # true
    tkFalse       # false
    tkAuto        # auto
    tkApp         # App
    tkStatic      # static
    # Operators
    tkEquals      # =
    tkPlus        # +
    tkMinus       # -
    tkStar        # *
    tkSlash       # /
    tkPercOp      # %
    tkPlusEq      # +=
    tkMinusEq     # -=
    tkEqEq        # ==
    tkNotEq       # !=
    tkLess        # <
    tkLessEq      # <=
    tkGreater     # >
    tkGreaterEq   # >=
    tkAnd         # &&
    tkOr          # ||
    tkNot         # !
    tkQuestion    # ?
    tkDollar      # $
    tkAt          # @
    tkLangBlock   # @lang ... @end - raw language-specific code block
    tkColon       # :
    tkComma       # ,
    tkDot         # .
    # Brackets
    tkLBrace      # {
    tkRBrace      # }
    tkLParen      # (
    tkRParen      # )
    tkLBracket    # [
    tkRBracket    # ]
    # Comments
    tkComment     # // or /* */

  Token* = object
    kind*: TokenKind
    value*: string
    loc*: SourceLoc

  # AST Node Types
  NodeKind* = enum
    nkProgram
    nkComponentDef
    nkStateDef
    nkConstDef
    nkAppBlock
    nkComponent       # Component instantiation (e.g., Container { })
    nkProperty        # Property assignment (e.g., width = 100)
    nkConditional     # if/else blocks
    nkForLoop         # for loops
    nkStaticBlock     # static { } - compile-time evaluation
    nkLangBlock       # @nim { }, @lua { }, etc.
    # Expressions
    nkExprLiteral     # literal value
    nkExprIdent       # identifier reference
    nkExprBinary      # binary operation
    nkExprUnary       # unary operation
    nkExprCall        # function call
    nkExprMember      # member access (a.b)
    nkExprTernary     # ternary (a ? b : c)
    nkExprInterp      # string interpolation ($value)
    nkExprArray       # array literal [a, b, c]
    nkExprBlock       # { statements } for handlers

  # Literal types
  LiteralKind* = enum
    lkString
    lkInt
    lkFloat
    lkBool
    lkPercent
    lkAuto
    lkColor

  # Binary operators
  BinaryOp* = enum
    opAdd           # +
    opSub           # -
    opMul           # *
    opDiv           # /
    opMod           # %
    opEq            # ==
    opNe            # !=
    opLt            # <
    opLe            # <=
    opGt            # >
    opGe            # >=
    opAnd           # &&
    opOr            # ||
    opAssign        # =
    opAddAssign     # +=
    opSubAssign     # -=

  # Unary operators
  UnaryOp* = enum
    opNot           # !
    opNeg           # -

  # Parameter definition
  ParamDef* = object
    name*: string
    typeName*: string
    defaultValue*: KryNode

  # Forward declaration of KryNode
  KryNode* = ref KryNodeObj

  KryNodeObj* = object
    loc*: SourceLoc
    case kind*: NodeKind
    of nkProgram:
      statements*: seq[KryNode]
    of nkComponentDef:
      compName*: string
      compParams*: seq[ParamDef]
      compBody*: seq[KryNode]  # properties, children, lang blocks
    of nkStateDef:
      stateName*: string
      stateType*: string
      stateInit*: KryNode
    of nkConstDef:
      constName*: string
      constType*: string        # Optional type annotation
      constValue*: KryNode
    of nkAppBlock:
      appBody*: seq[KryNode]
    of nkComponent:
      componentType*: string
      componentArgs*: seq[KryNode]  # positional args for instantiation
      componentProps*: Table[string, KryNode]
      componentChildren*: seq[KryNode]
    of nkProperty:
      propName*: string
      propValue*: KryNode
    of nkConditional:
      condExpr*: KryNode
      thenBranch*: seq[KryNode]
      elseBranch*: seq[KryNode]
    of nkForLoop:
      loopVar*: string
      loopExpr*: KryNode
      loopBody*: seq[KryNode]
      loopIsConst*: bool        # If true, expand at compile time
    of nkStaticBlock:
      staticBody*: seq[KryNode]  # Contents are evaluated at compile time
    of nkLangBlock:
      langName*: string       # nim, lua, js, etc.
      langCode*: string       # raw code block
    of nkExprLiteral:
      litKind*: LiteralKind
      litString*: string
      litInt*: int64
      litFloat*: float64
      litBool*: bool
    of nkExprIdent:
      identName*: string
    of nkExprBinary:
      binOp*: BinaryOp
      binLeft*: KryNode
      binRight*: KryNode
    of nkExprUnary:
      unaryOp*: UnaryOp
      unaryExpr*: KryNode
    of nkExprCall:
      callName*: string
      callArgs*: seq[KryNode]
    of nkExprMember:
      memberObj*: KryNode
      memberName*: string
    of nkExprTernary:
      ternCond*: KryNode
      ternThen*: KryNode
      ternElse*: KryNode
    of nkExprInterp:
      interpExpr*: KryNode
    of nkExprArray:
      arrayElems*: seq[KryNode]
    of nkExprBlock:
      blockStmts*: seq[KryNode]

# Constructor helpers
proc newProgram*(stmts: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkProgram, statements: stmts, loc: loc)

proc newComponentDef*(name: string, params: seq[ParamDef], body: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkComponentDef, compName: name, compParams: params, compBody: body, loc: loc)

proc newStateDef*(name, typeName: string, init: KryNode, loc = SourceLoc()): KryNode =
  KryNode(kind: nkStateDef, stateName: name, stateType: typeName, stateInit: init, loc: loc)

proc newConstDef*(name: string, value: KryNode, typeName = "", loc = SourceLoc()): KryNode =
  KryNode(kind: nkConstDef, constName: name, constType: typeName, constValue: value, loc: loc)

proc newAppBlock*(body: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkAppBlock, appBody: body, loc: loc)

proc newComponent*(typeName: string, loc = SourceLoc()): KryNode =
  KryNode(kind: nkComponent, componentType: typeName, componentArgs: @[],
          componentProps: initTable[string, KryNode](), componentChildren: @[], loc: loc)

proc newProperty*(name: string, value: KryNode, loc = SourceLoc()): KryNode =
  KryNode(kind: nkProperty, propName: name, propValue: value, loc: loc)

proc newConditional*(cond: KryNode, thenB, elseB: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkConditional, condExpr: cond, thenBranch: thenB, elseBranch: elseB, loc: loc)

proc newForLoop*(varName: string, expr: KryNode, body: seq[KryNode], isConst = false, loc = SourceLoc()): KryNode =
  KryNode(kind: nkForLoop, loopVar: varName, loopExpr: expr, loopBody: body, loopIsConst: isConst, loc: loc)

proc newStaticBlock*(body: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkStaticBlock, staticBody: body, loc: loc)

proc newLangBlock*(lang, code: string, loc = SourceLoc()): KryNode =
  KryNode(kind: nkLangBlock, langName: lang, langCode: code, loc: loc)

# Expression constructors
proc newLitString*(s: string, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkString, litString: s, loc: loc)

proc newLitInt*(i: int64, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkInt, litInt: i, loc: loc)

proc newLitFloat*(f: float64, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkFloat, litFloat: f, loc: loc)

proc newLitBool*(b: bool, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkBool, litBool: b, loc: loc)

proc newLitPercent*(f: float64, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkPercent, litFloat: f, loc: loc)

proc newLitAuto*(loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkAuto, loc: loc)

proc newLitColor*(color: string, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprLiteral, litKind: lkColor, litString: color, loc: loc)

proc newIdent*(name: string, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprIdent, identName: name, loc: loc)

proc newBinaryExpr*(op: BinaryOp, left, right: KryNode, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprBinary, binOp: op, binLeft: left, binRight: right, loc: loc)

proc newUnaryExpr*(op: UnaryOp, expr: KryNode, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprUnary, unaryOp: op, unaryExpr: expr, loc: loc)

proc newCallExpr*(name: string, args: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprCall, callName: name, callArgs: args, loc: loc)

proc newMemberExpr*(obj: KryNode, member: string, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprMember, memberObj: obj, memberName: member, loc: loc)

proc newTernaryExpr*(cond, thenE, elseE: KryNode, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprTernary, ternCond: cond, ternThen: thenE, ternElse: elseE, loc: loc)

proc newInterpExpr*(expr: KryNode, loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprInterp, interpExpr: expr, loc: loc)

proc newArrayExpr*(elems: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprArray, arrayElems: elems, loc: loc)

proc newBlockExpr*(stmts: seq[KryNode], loc = SourceLoc()): KryNode =
  KryNode(kind: nkExprBlock, blockStmts: stmts, loc: loc)

# Pretty printing for debugging
proc `$`*(loc: SourceLoc): string =
  if loc.filename.len > 0:
    &"{loc.filename}:{loc.line}:{loc.column}"
  else:
    &"{loc.line}:{loc.column}"

proc `$`*(tok: Token): string =
  &"Token({tok.kind}, \"{tok.value}\", {tok.loc})"

proc treeRepr*(node: KryNode, indent: int = 0): string =
  ## Generate a tree representation of the AST for debugging
  let prefix = "  ".repeat(indent)
  if node.isNil:
    return prefix & "(nil)"

  case node.kind
  of nkProgram:
    result = prefix & "Program:\n"
    for stmt in node.statements:
      result &= treeRepr(stmt, indent + 1)
  of nkComponentDef:
    result = prefix & &"ComponentDef: {node.compName}\n"
    if node.compParams.len > 0:
      result &= prefix & "  Params:\n"
      for p in node.compParams:
        result &= prefix & &"    {p.name}: {p.typeName}\n"
    for child in node.compBody:
      result &= treeRepr(child, indent + 1)
  of nkStateDef:
    result = prefix & &"StateDef: {node.stateName}: {node.stateType}\n"
    if not node.stateInit.isNil:
      result &= treeRepr(node.stateInit, indent + 1)
  of nkConstDef:
    result = prefix & &"ConstDef: {node.constName}\n"
    result &= treeRepr(node.constValue, indent + 1)
  of nkAppBlock:
    result = prefix & "App:\n"
    for child in node.appBody:
      result &= treeRepr(child, indent + 1)
  of nkComponent:
    result = prefix & &"Component: {node.componentType}\n"
    for name, value in node.componentProps:
      result &= prefix & &"  {name} =\n"
      result &= treeRepr(value, indent + 2)
    for child in node.componentChildren:
      result &= treeRepr(child, indent + 1)
  of nkProperty:
    result = prefix & &"Property: {node.propName}\n"
    result &= treeRepr(node.propValue, indent + 1)
  of nkConditional:
    result = prefix & "If:\n"
    result &= treeRepr(node.condExpr, indent + 1)
    result &= prefix & "  Then:\n"
    for child in node.thenBranch:
      result &= treeRepr(child, indent + 2)
    if node.elseBranch.len > 0:
      result &= prefix & "  Else:\n"
      for child in node.elseBranch:
        result &= treeRepr(child, indent + 2)
  of nkForLoop:
    result = prefix & &"For: {node.loopVar} in\n"
    result &= treeRepr(node.loopExpr, indent + 1)
    for child in node.loopBody:
      result &= treeRepr(child, indent + 1)
  of nkStaticBlock:
    result = prefix & "Static:\n"
    for child in node.staticBody:
      result &= treeRepr(child, indent + 1)
  of nkLangBlock:
    result = prefix & &"LangBlock: @{node.langName}\n"
    result &= prefix & &"  {node.langCode.len} chars of code\n"
  of nkExprLiteral:
    case node.litKind
    of lkString: result = prefix & &"Literal(string): \"{node.litString}\"\n"
    of lkInt: result = prefix & &"Literal(int): {node.litInt}\n"
    of lkFloat: result = prefix & &"Literal(float): {node.litFloat}\n"
    of lkBool: result = prefix & &"Literal(bool): {node.litBool}\n"
    of lkPercent: result = prefix & &"Literal(percent): {node.litFloat}%\n"
    of lkAuto: result = prefix & "Literal(auto)\n"
    of lkColor: result = prefix & &"Literal(color): {node.litString}\n"
  of nkExprIdent:
    result = prefix & &"Ident: {node.identName}\n"
  of nkExprBinary:
    result = prefix & &"Binary: {node.binOp}\n"
    result &= treeRepr(node.binLeft, indent + 1)
    result &= treeRepr(node.binRight, indent + 1)
  of nkExprUnary:
    result = prefix & &"Unary: {node.unaryOp}\n"
    result &= treeRepr(node.unaryExpr, indent + 1)
  of nkExprCall:
    result = prefix & &"Call: {node.callName}\n"
    for arg in node.callArgs:
      result &= treeRepr(arg, indent + 1)
  of nkExprMember:
    result = prefix & &"Member: .{node.memberName}\n"
    result &= treeRepr(node.memberObj, indent + 1)
  of nkExprTernary:
    result = prefix & "Ternary:\n"
    result &= treeRepr(node.ternCond, indent + 1)
    result &= treeRepr(node.ternThen, indent + 1)
    result &= treeRepr(node.ternElse, indent + 1)
  of nkExprInterp:
    result = prefix & "Interp:\n"
    result &= treeRepr(node.interpExpr, indent + 1)
  of nkExprArray:
    result = prefix & "Array:\n"
    for elem in node.arrayElems:
      result &= treeRepr(elem, indent + 1)
  of nkExprBlock:
    result = prefix & "Block:\n"
    for stmt in node.blockStmts:
      result &= treeRepr(stmt, indent + 1)
