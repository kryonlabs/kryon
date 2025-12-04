## .kry Language Lexer
##
## This module provides the tokenizer for the Kryon DSL (.kry files).
## It converts source text into a stream of tokens.

import std/[strutils, strformat, tables]
import kry_ast

type
  Lexer* = object
    source*: string
    filename*: string
    pos: int
    line: int
    column: int
    tokens: seq[Token]
    current: int

  LexerError* = object of CatchableError

# Keywords table
const Keywords = {
  "component": tkComponent,
  "state": tkState,
  "const": tkConst,
  "if": tkIf,
  "else": tkElse,
  "for": tkFor,
  "in": tkIn,
  "true": tkTrue,
  "false": tkFalse,
  "auto": tkAuto,
  "App": tkApp,
  "static": tkStatic,
}.toTable

proc newLexer*(source: string, filename = "<input>"): Lexer =
  Lexer(source: source, filename: filename, pos: 0, line: 1, column: 1, tokens: @[], current: 0)

proc currentLoc(lex: Lexer): SourceLoc =
  SourceLoc(line: lex.line, column: lex.column, filename: lex.filename)

proc error(lex: Lexer, msg: string) =
  raise newException(LexerError, &"{lex.filename}:{lex.line}:{lex.column}: {msg}")

proc peek(lex: Lexer, offset: int = 0): char =
  let idx = lex.pos + offset
  if idx >= lex.source.len:
    '\0'
  else:
    lex.source[idx]

proc advance(lex: var Lexer): char =
  result = lex.peek()
  lex.pos.inc
  if result == '\n':
    lex.line.inc
    lex.column = 1
  else:
    lex.column.inc

proc skipWhitespace(lex: var Lexer) =
  while true:
    case lex.peek()
    of ' ', '\t', '\r':
      discard lex.advance()
    of '\n':
      discard lex.advance()
    of '/':
      if lex.peek(1) == '/':
        # Single-line comment
        while lex.peek() != '\0' and lex.peek() != '\n':
          discard lex.advance()
      elif lex.peek(1) == '*':
        # Multi-line comment
        discard lex.advance()  # /
        discard lex.advance()  # *
        while lex.peek() != '\0':
          if lex.peek() == '*' and lex.peek(1) == '/':
            discard lex.advance()  # *
            discard lex.advance()  # /
            break
          discard lex.advance()
      else:
        break
    else:
      break

proc addToken(lex: var Lexer, kind: TokenKind, value: string = "") =
  lex.tokens.add(Token(kind: kind, value: value, loc: lex.currentLoc()))

proc scanString(lex: var Lexer): string =
  let quote = lex.advance()  # consume opening quote
  var s = ""
  while lex.peek() != '\0' and lex.peek() != quote:
    if lex.peek() == '\\':
      discard lex.advance()
      case lex.peek()
      of 'n': s.add('\n')
      of 't': s.add('\t')
      of 'r': s.add('\r')
      of '\\': s.add('\\')
      of '"': s.add('"')
      of '\'': s.add('\'')
      else:
        lex.error("Invalid escape sequence: \\" & lex.peek())
      discard lex.advance()
    else:
      s.add(lex.advance())
  if lex.peek() == '\0':
    lex.error("Unterminated string literal")
  discard lex.advance()  # consume closing quote
  s

proc scanNumber(lex: var Lexer): string =
  var s = ""
  while lex.peek().isDigit:
    s.add(lex.advance())
  if lex.peek() == '.' and lex.peek(1).isDigit:
    s.add(lex.advance())  # .
    while lex.peek().isDigit:
      s.add(lex.advance())
  s

proc scanIdent(lex: var Lexer): string =
  var s = ""
  while lex.peek().isAlphaNumeric or lex.peek() == '_':
    s.add(lex.advance())
  s

proc scanLangBlockUntilEnd*(lex: var Lexer): string =
  ## Scan a raw language block after @lang until @end
  ## No brace counting needed - just scan until we see @end
  var code = ""
  while lex.peek() != '\0':
    # Check for @end marker
    if lex.peek() == '@':
      # Look ahead for "end"
      let remaining = lex.source[lex.pos..^1]
      if remaining.startsWith("@end"):
        # Found @end - consume it and stop
        for _ in 0..<4:  # consume "@end"
          discard lex.advance()
        break
      else:
        code.add(lex.advance())
    else:
      code.add(lex.advance())

  # Indent each line by one tab
  var indentedCode = ""
  for line in code.strip().splitLines():
    if line.strip().len > 0:
      indentedCode.add("\t" & line & "\n")
    else:
      indentedCode.add("\n")
  indentedCode.strip()

proc tokenize*(lex: var Lexer): seq[Token] =
  ## Tokenize the entire source and return the token stream
  while lex.pos < lex.source.len:
    lex.skipWhitespace()
    if lex.pos >= lex.source.len:
      break

    let c = lex.peek()
    let loc = lex.currentLoc()

    case c
    of '\0':
      break
    of '{':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkLBrace, value: "{", loc: loc))
    of '}':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkRBrace, value: "}", loc: loc))
    of '(':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkLParen, value: "(", loc: loc))
    of ')':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkRParen, value: ")", loc: loc))
    of '[':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkLBracket, value: "[", loc: loc))
    of ']':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkRBracket, value: "]", loc: loc))
    of ':':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkColon, value: ":", loc: loc))
    of ',':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkComma, value: ",", loc: loc))
    of '.':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkDot, value: ".", loc: loc))
    of '?':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkQuestion, value: "?", loc: loc))
    of '$':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkDollar, value: "$", loc: loc))
    of '@':
      discard lex.advance()
      # Check for @lang ... @end pattern
      lex.skipWhitespace()
      if lex.peek().isAlphaAscii:
        # Scan language name
        var langName = ""
        while lex.peek().isAlphaNumeric or lex.peek() == '_':
          langName.add(lex.advance())

        # Check if it's @end (not a language block start)
        if langName == "end":
          # This is an @end token - shouldn't appear standalone
          lex.error("Unexpected @end without matching @lang")
        else:
          # This is a language block - scan raw code until @end
          let code = lex.scanLangBlockUntilEnd()
          # Store as "langName:code" in value
          lex.tokens.add(Token(kind: tkLangBlock, value: langName & ":" & code, loc: loc))
      else:
        lex.tokens.add(Token(kind: tkAt, value: "@", loc: loc))
    of '+':
      discard lex.advance()
      if lex.peek() == '=':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkPlusEq, value: "+=", loc: loc))
      else:
        lex.tokens.add(Token(kind: tkPlus, value: "+", loc: loc))
    of '-':
      discard lex.advance()
      if lex.peek() == '=':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkMinusEq, value: "-=", loc: loc))
      else:
        lex.tokens.add(Token(kind: tkMinus, value: "-", loc: loc))
    of '*':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkStar, value: "*", loc: loc))
    of '/':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkSlash, value: "/", loc: loc))
    of '%':
      discard lex.advance()
      lex.tokens.add(Token(kind: tkPercOp, value: "%", loc: loc))
    of '=':
      discard lex.advance()
      if lex.peek() == '=':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkEqEq, value: "==", loc: loc))
      else:
        lex.tokens.add(Token(kind: tkEquals, value: "=", loc: loc))
    of '!':
      discard lex.advance()
      if lex.peek() == '=':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkNotEq, value: "!=", loc: loc))
      else:
        lex.tokens.add(Token(kind: tkNot, value: "!", loc: loc))
    of '<':
      discard lex.advance()
      if lex.peek() == '=':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkLessEq, value: "<=", loc: loc))
      else:
        lex.tokens.add(Token(kind: tkLess, value: "<", loc: loc))
    of '>':
      discard lex.advance()
      if lex.peek() == '=':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkGreaterEq, value: ">=", loc: loc))
      else:
        lex.tokens.add(Token(kind: tkGreater, value: ">", loc: loc))
    of '&':
      discard lex.advance()
      if lex.peek() == '&':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkAnd, value: "&&", loc: loc))
      else:
        lex.error("Expected '&&' but got single '&'")
    of '|':
      discard lex.advance()
      if lex.peek() == '|':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkOr, value: "||", loc: loc))
      else:
        lex.error("Expected '||' but got single '|'")
    of '"', '\'':
      let s = lex.scanString()
      lex.tokens.add(Token(kind: tkString, value: s, loc: loc))
    of '#':
      # Color literal #RRGGBB or #RRGGBBAA
      discard lex.advance()
      var color = "#"
      while lex.peek().isAlphaNumeric:
        color.add(lex.advance())
      lex.tokens.add(Token(kind: tkString, value: color, loc: loc))
    of '0'..'9':
      let numStr = lex.scanNumber()
      if lex.peek() == '%':
        discard lex.advance()
        lex.tokens.add(Token(kind: tkPercent, value: numStr, loc: loc))
      else:
        lex.tokens.add(Token(kind: tkNumber, value: numStr, loc: loc))
    of 'a'..'z', 'A'..'Z', '_':
      let ident = lex.scanIdent()
      if ident in Keywords:
        lex.tokens.add(Token(kind: Keywords[ident], value: ident, loc: loc))
      else:
        lex.tokens.add(Token(kind: tkIdent, value: ident, loc: loc))
    else:
      lex.error(&"Unexpected character: '{c}' (0x{ord(c):02X})")

  lex.tokens.add(Token(kind: tkEof, value: "", loc: lex.currentLoc()))
  lex.tokens

# Token stream interface for the parser
proc currentToken*(lex: Lexer): Token =
  if lex.current < lex.tokens.len:
    lex.tokens[lex.current]
  else:
    Token(kind: tkEof, value: "", loc: lex.currentLoc())

proc peekToken*(lex: Lexer, offset: int = 0): Token =
  let idx = lex.current + offset
  if idx < lex.tokens.len:
    lex.tokens[idx]
  else:
    Token(kind: tkEof, value: "", loc: lex.currentLoc())

proc advanceToken*(lex: var Lexer): Token =
  result = lex.currentToken()
  if lex.current < lex.tokens.len:
    lex.current.inc

proc check*(lex: Lexer, kind: TokenKind): bool =
  lex.currentToken().kind == kind

proc checkAny*(lex: Lexer, kinds: set[TokenKind]): bool =
  lex.currentToken().kind in kinds

proc match*(lex: var Lexer, kind: TokenKind): bool =
  if lex.check(kind):
    discard lex.advanceToken()
    true
  else:
    false

proc expect*(lex: var Lexer, kind: TokenKind): Token =
  if lex.check(kind):
    lex.advanceToken()
  else:
    let tok = lex.currentToken()
    raise newException(LexerError,
      &"{tok.loc}: Expected {kind} but got {tok.kind} ('{tok.value}')")
