## kry_to_kir - Compile .kry files to .kir format
##
## Usage: nim c -r kry_to_kir.nim input.kry output.kir

import std/[os, strutils, sequtils, tables, json]
import ../../bindings/nim/[ir_serialization, ir_core]

type
  Token = object
    kind: TokenKind
    value: string
    line: int
    col: int

  TokenKind = enum
    tkIdentifier, tkString, tkNumber, tkLBrace, tkRBrace,
    tkEquals, tkComma, tkEOF, tkNewline

  Parser = object
    tokens: seq[Token]
    pos: int

proc tokenize(source: string): seq[Token] =
  var tokens: seq[Token] = @[]
  var line = 1
  var col = 1
  var i = 0

  while i < source.len:
    # Skip whitespace (except newlines)
    if source[i] in {' ', '\t', '\r'}:
      if source[i] == '\r':
        line += 1
        col = 1
      inc i
      inc col
      continue

    # Handle newlines
    if source[i] == '\n':
      tokens.add(Token(kind: tkNewline, value: "\n", line: line, col: col))
      inc line
      col = 1
      inc i
      continue

    # Skip comments
    if i + 1 < source.len and source[i..i+1] == "//":
      while i < source.len and source[i] != '\n':
        inc i
      continue

    # String literals
    if source[i] == '"':
      var str = ""
      inc i  # Skip opening quote
      while i < source.len and source[i] != '"':
        str.add(source[i])
        inc i
      inc i  # Skip closing quote
      tokens.add(Token(kind: tkString, value: str, line: line, col: col))
      col += str.len + 2
      continue

    # Braces
    if source[i] == '{':
      tokens.add(Token(kind: tkLBrace, value: "{", line: line, col: col))
      inc i
      inc col
      continue

    if source[i] == '}':
      tokens.add(Token(kind: tkRBrace, value: "}", line: line, col: col))
      inc i
      inc col
      continue

    # Equals
    if source[i] == '=':
      tokens.add(Token(kind: tkEquals, value: "=", line: line, col: col))
      inc i
      inc col
      continue

    # Comma
    if source[i] == ',':
      tokens.add(Token(kind: tkComma, value: ",", line: line, col: col))
      inc i
      inc col
      continue

    # Numbers
    if source[i].isDigit or (source[i] == '-' and i + 1 < source.len and source[i+1].isDigit):
      var num = ""
      if source[i] == '-':
        num.add('-')
        inc i
      while i < source.len and (source[i].isDigit or source[i] == '.'):
        num.add(source[i])
        inc i
      tokens.add(Token(kind: tkNumber, value: num, line: line, col: col))
      col += num.len
      continue

    # Identifiers
    if source[i].isAlphaAscii or source[i] == '_':
      var id = ""
      while i < source.len and (source[i].isAlphaNumeric or source[i] in {'_', '-'}):
        id.add(source[i])
        inc i
      tokens.add(Token(kind: tkIdentifier, value: id, line: line, col: col))
      col += id.len
      continue

    # Unknown character
    echo "Warning: Unknown character at line ", line, ", col ", col, ": '", source[i], "'"
    inc i
    inc col

  tokens.add(Token(kind: tkEOF, value: "", line: line, col: col))
  return tokens

proc parseComponent(p: var Parser, componentType: string): IRComponent

proc parseProperties(p: var Parser): Table[string, string] =
  result = initTable[string, string]()

  while p.pos < p.tokens.len:
    # Skip newlines
    if p.tokens[p.pos].kind == tkNewline:
      inc p.pos
      continue

    # End of properties
    if p.tokens[p.pos].kind in {tkRBrace, tkEOF}:
      break

    # Property name
    if p.tokens[p.pos].kind != tkIdentifier:
      break

    let propName = p.tokens[p.pos].value
    inc p.pos

    # Equals sign
    if p.pos >= p.tokens.len or p.tokens[p.pos].kind != tkEquals:
      echo "Error: Expected '=' after property name"
      break
    inc p.pos

    # Property value (string or number or identifier)
    if p.pos >= p.tokens.len:
      break

    let propValue = case p.tokens[p.pos].kind
      of tkString, tkNumber, tkIdentifier:
        p.tokens[p.pos].value
      else:
        echo "Error: Expected value for property ", propName
        ""

    if propValue != "":
      result[propName] = propValue
      inc p.pos

    # Skip optional comma/newline
    while p.pos < p.tokens.len and p.tokens[p.pos].kind in {tkComma, tkNewline}:
      inc p.pos

proc parseComponent(p: var Parser, componentType: string): IRComponent =
  # Create component based on type
  case componentType.toLowerAscii
  of "app":
    result = ir_create_component(IR_COMPONENT_CONTAINER)
  of "container":
    result = ir_create_component(IR_COMPONENT_CONTAINER)
  of "row":
    result = ir_create_component(IR_COMPONENT_ROW)
  of "column":
    result = ir_create_component(IR_COMPONENT_COLUMN)
  of "text":
    result = ir_create_component(IR_COMPONENT_TEXT)
  of "button":
    result = ir_create_component(IR_COMPONENT_BUTTON)
  of "input":
    result = ir_create_component(IR_COMPONENT_INPUT)
  of "checkbox":
    result = ir_create_component(IR_COMPONENT_CHECKBOX)
  of "dropdown":
    result = ir_create_component(IR_COMPONENT_DROPDOWN)
  else:
    echo "Warning: Unknown component type: ", componentType
    result = ir_create_component(IR_COMPONENT_CONTAINER)

  # Expect opening brace
  if p.pos >= p.tokens.len or p.tokens[p.pos].kind != tkLBrace:
    echo "Error: Expected '{' after component type"
    return result
  inc p.pos

  # Parse properties and child components
  while p.pos < p.tokens.len:
    # Skip newlines
    if p.tokens[p.pos].kind == tkNewline:
      inc p.pos
      continue

    # End of component
    if p.tokens[p.pos].kind == tkRBrace:
      inc p.pos
      break

    if p.tokens[p.pos].kind != tkIdentifier:
      inc p.pos
      continue

    let name = p.tokens[p.pos].value
    inc p.pos

    # Check if it's a child component (followed by {) or a property (followed by =)
    if p.pos < p.tokens.len:
      if p.tokens[p.pos].kind == tkLBrace:
        # Child component
        dec p.pos  # Back up to re-parse component type
        let child = p.parseComponent(name)
        ir_add_child(result, child)
      elif p.tokens[p.pos].kind == tkEquals:
        # Property - back up and parse all properties
        dec p.pos
        let props = p.parseProperties()

        # Apply properties to component
        for key, val in props:
          case key.toLowerAscii
          of "text":
            ir_set_text(result, val.cstring)
          of "width":
            if val.isDigit:
              ir_set_width_px(result, parseFloat(val))
            else:
              discard  # Handle other width types later
          of "height":
            if val.isDigit:
              ir_set_height_px(result, parseFloat(val))
          of "backgroundcolor", "background":
            ir_set_background_color(result, val.cstring)
          of "color":
            ir_set_text_color(result, val.cstring)
          of "bordercolor":
            ir_set_border_color(result, val.cstring)
          of "borderwidth":
            ir_set_border_width(result, parseFloat(val))
          of "borderrradius":
            ir_set_border_radius(result, parseFloat(val))
          of "posx", "left":
            ir_set_position_x(result, parseFloat(val))
          of "posy", "top":
            ir_set_position_y(result, parseFloat(val))
          of "windowtitle", "windowwidth", "windowheight":
            discard  # Window properties handled separately
          else:
            discard

        break  # Properties parsed, continue to next element
      else:
        # Unknown token after identifier
        discard

  return result

proc parse(tokens: seq[Token]): IRComponent =
  var p = Parser(tokens: tokens, pos: 0)

  # Expect App { ... }
  if p.pos < tokens.len and p.tokens[p.pos].kind == tkIdentifier:
    result = p.parseComponent(p.tokens[p.pos].value)
  else:
    echo "Error: Expected component definition"
    result = ir_create_component(IR_COMPONENT_CONTAINER)

proc main() =
  if paramCount() < 2:
    echo "Usage: kry_to_kir <input.kry> <output.kir>"
    quit(1)

  let inputFile = paramStr(1)
  let outputFile = paramStr(2)

  if not fileExists(inputFile):
    echo "Error: Input file not found: ", inputFile
    quit(1)

  # Read and parse .kry file
  let source = readFile(inputFile)
  let tokens = tokenize(source)
  let root = parse(tokens)

  # Write to .kir format (JSON v2)
  let success = ir_write_json_v2_file(root, outputFile.cstring)

  if success:
    echo "✓ Generated: ", outputFile
    quit(0)
  else:
    echo "✗ Failed to write output file"
    quit(1)

when isMainModule:
  main()
