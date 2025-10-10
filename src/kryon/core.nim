## Core types and data structures for Kryon UI framework
##
## This module defines the fundamental types used throughout Kryon:
## - Element: Base UI element type
## - Property: Key-value properties
## - ElementKind: Enumeration of all element types
## - Color: Color representation

import tables, strutils, options, hashes

type
  ElementKind* = enum
    ## All supported UI element types
    ekHeader = "Header"       # Window metadata (non-rendering)
    ekBody = "Body"           # Main UI content wrapper (non-rendering)
    ekContainer = "Container"
    ekText = "Text"
    ekButton = "Button"
    ekColumn = "Column"
    ekRow = "Row"
    ekInput = "Input"
    ekCheckbox = "Checkbox"
    ekDropdown = "Dropdown"
    ekGrid = "Grid"
    ekImage = "Image"
    ekCenter = "Center"
    ekScrollView = "ScrollView"

  Alignment* = enum
    ## Alignment options for layout
    alStart = "start"
    alCenter = "center"
    alEnd = "end"
    alStretch = "stretch"
    alSpaceBetween = "spaceBetween"
    alSpaceAround = "spaceAround"

  Color* = object
    ## RGBA color representation
    r*, g*, b*, a*: uint8

  ValueKind* = enum
    ## Value types for properties
    vkInt, vkFloat, vkString, vkBool, vkColor, vkAlignment, vkNil

  Value* = object
    ## Dynamic value type for properties
    case kind*: ValueKind
    of vkInt: intVal*: int
    of vkFloat: floatVal*: float
    of vkString: strVal*: string
    of vkBool: boolVal*: bool
    of vkColor: colorVal*: Color
    of vkAlignment: alignVal*: Alignment
    of vkNil: discard

  Property* = tuple
    ## Key-value property pair
    name: string
    value: Value

  EventHandler* = proc () {.closure.}
    ## Event handler callback type

  Element* = ref object
    ## Base UI element type
    kind*: ElementKind
    id*: string
    properties*: Table[string, Value]
    children*: seq[Element]
    parent*: Element
    eventHandlers*: Table[string, EventHandler]

    # Computed layout (filled by layout engine)
    x*, y*: float
    width*, height*: float

# ============================================================================
# Color utilities
# ============================================================================

proc rgba*(r, g, b: uint8, a: uint8 = 255): Color =
  ## Create color from RGBA components
  Color(r: r, g: g, b: b, a: a)

# CSS Named Colors (matching original Kryon color_utils.c)
const cssColors* = {
  "aliceblue": 0xf0f8ffff'u32, "antiquewhite": 0xfaebd7ff'u32, "aqua": 0x00ffffff'u32,
  "aquamarine": 0x7fffd4ff'u32, "azure": 0xf0ffffff'u32, "beige": 0xf5f5dcff'u32,
  "bisque": 0xffe4c4ff'u32, "black": 0x000000ff'u32, "blanchedalmond": 0xffebcdff'u32,
  "blue": 0x0000ffff'u32, "blueviolet": 0x8a2be2ff'u32, "brown": 0xa52a2aff'u32,
  "burlywood": 0xdeb887ff'u32, "cadetblue": 0x5f9ea0ff'u32, "chartreuse": 0x7fff00ff'u32,
  "chocolate": 0xd2691eff'u32, "coral": 0xff7f50ff'u32, "cornflowerblue": 0x6495edff'u32,
  "cornsilk": 0xfff8dcff'u32, "crimson": 0xdc143cff'u32, "cyan": 0x00ffffff'u32,
  "darkblue": 0x00008bff'u32, "darkcyan": 0x008b8bff'u32, "darkgoldenrod": 0xb8860bff'u32,
  "darkgray": 0xa9a9a9ff'u32, "darkgreen": 0x006400ff'u32, "darkgrey": 0xa9a9a9ff'u32,
  "darkkhaki": 0xbdb76bff'u32, "darkmagenta": 0x8b008bff'u32, "darkolivegreen": 0x556b2fff'u32,
  "darkorange": 0xff8c00ff'u32, "darkorchid": 0x9932ccff'u32, "darkred": 0x8b0000ff'u32,
  "darksalmon": 0xe9967aff'u32, "darkseagreen": 0x8fbc8fff'u32, "darkslateblue": 0x483d8bff'u32,
  "darkslategray": 0x2f4f4fff'u32, "darkslategrey": 0x2f4f4fff'u32, "darkturquoise": 0x00ced1ff'u32,
  "darkviolet": 0x9400d3ff'u32, "deeppink": 0xff1493ff'u32, "deepskyblue": 0x00bfffff'u32,
  "dimgray": 0x696969ff'u32, "dimgrey": 0x696969ff'u32, "dodgerblue": 0x1e90ffff'u32,
  "firebrick": 0xb22222ff'u32, "floralwhite": 0xfffaf0ff'u32, "forestgreen": 0x228b22ff'u32,
  "fuchsia": 0xff00ffff'u32, "gainsboro": 0xdcdcdcff'u32, "ghostwhite": 0xf8f8ffff'u32,
  "gold": 0xffd700ff'u32, "goldenrod": 0xdaa520ff'u32, "gray": 0x808080ff'u32,
  "green": 0x008000ff'u32, "greenyellow": 0xadff2fff'u32, "grey": 0x808080ff'u32,
  "honeydew": 0xf0fff0ff'u32, "hotpink": 0xff69b4ff'u32, "indianred": 0xcd5c5cff'u32,
  "indigo": 0x4b0082ff'u32, "ivory": 0xfffff0ff'u32, "khaki": 0xf0e68cff'u32,
  "lavender": 0xe6e6faff'u32, "lavenderblush": 0xfff0f5ff'u32, "lawngreen": 0x7cfc00ff'u32,
  "lemonchiffon": 0xfffacdff'u32, "lightblue": 0xadd8e6ff'u32, "lightcoral": 0xf08080ff'u32,
  "lightcyan": 0xe0ffffff'u32, "lightgoldenrodyellow": 0xfafad2ff'u32, "lightgray": 0xd3d3d3ff'u32,
  "lightgreen": 0x90ee90ff'u32, "lightgrey": 0xd3d3d3ff'u32, "lightpink": 0xffb6c1ff'u32,
  "lightsalmon": 0xffa07aff'u32, "lightseagreen": 0x20b2aaff'u32, "lightskyblue": 0x87cefaff'u32,
  "lightslategray": 0x778899ff'u32, "lightslategrey": 0x778899ff'u32, "lightsteelblue": 0xb0c4deff'u32,
  "lightyellow": 0xffffe0ff'u32, "lime": 0x00ff00ff'u32, "limegreen": 0x32cd32ff'u32,
  "linen": 0xfaf0e6ff'u32, "magenta": 0xff00ffff'u32, "maroon": 0x800000ff'u32,
  "mediumaquamarine": 0x66cdaaff'u32, "mediumblue": 0x0000cdff'u32, "mediumorchid": 0xba55d3ff'u32,
  "mediumpurple": 0x9370dbff'u32, "mediumseagreen": 0x3cb371ff'u32, "mediumslateblue": 0x7b68eeff'u32,
  "mediumspringgreen": 0x00fa9aff'u32, "mediumturquoise": 0x48d1ccff'u32, "mediumvioletred": 0xc71585ff'u32,
  "midnightblue": 0x191970ff'u32, "mintcream": 0xf5fffaff'u32, "mistyrose": 0xffe4e1ff'u32,
  "moccasin": 0xffe4b5ff'u32, "navajowhite": 0xffdeadff'u32, "navy": 0x000080ff'u32,
  "oldlace": 0xfdf5e6ff'u32, "olive": 0x808000ff'u32, "olivedrab": 0x6b8e23ff'u32,
  "orange": 0xffa500ff'u32, "orangered": 0xff4500ff'u32, "orchid": 0xda70d6ff'u32,
  "palegoldenrod": 0xeee8aaff'u32, "palegreen": 0x98fb98ff'u32, "paleturquoise": 0xafeeeeff'u32,
  "palevioletred": 0xdb7093ff'u32, "papayawhip": 0xffefd5ff'u32, "peachpuff": 0xffdab9ff'u32,
  "peru": 0xcd853fff'u32, "pink": 0xffc0cbff'u32, "plum": 0xdda0ddff'u32,
  "powderblue": 0xb0e0e6ff'u32, "purple": 0x800080ff'u32, "rebeccapurple": 0x663399ff'u32,
  "red": 0xff0000ff'u32, "rosybrown": 0xbc8f8fff'u32, "royalblue": 0x4169e1ff'u32,
  "saddlebrown": 0x8b4513ff'u32, "salmon": 0xfa8072ff'u32, "sandybrown": 0xf4a460ff'u32,
  "seagreen": 0x2e8b57ff'u32, "seashell": 0xfff5eeff'u32, "sienna": 0xa0522dff'u32,
  "silver": 0xc0c0c0ff'u32, "skyblue": 0x87ceebff'u32, "slateblue": 0x6a5acdff'u32,
  "slategray": 0x708090ff'u32, "slategrey": 0x708090ff'u32, "snow": 0xfffafaff'u32,
  "springgreen": 0x00ff7fff'u32, "steelblue": 0x4682b4ff'u32, "tan": 0xd2b48cff'u32,
  "teal": 0x008080ff'u32, "thistle": 0xd8bfd8ff'u32, "tomato": 0xff6347ff'u32,
  "transparent": 0x00000000'u32, "turquoise": 0x40e0d0ff'u32, "violet": 0xee82eeff'u32,
  "wheat": 0xf5deb3ff'u32, "white": 0xffffffff'u32, "whitesmoke": 0xf5f5f5ff'u32,
  "yellow": 0xffff00ff'u32, "yellowgreen": 0x9acd32ff'u32
}.toTable

proc parseColor*(s: string): Color =
  ## Parse color from hex string, CSS named color, or rgb()/rgba()
  ## Supports: #RGB, #RRGGBB, #RRGGBBAA, named colors (yellow, red, etc.)
  var input = s.strip().toLowerAscii()

  # 1. Check for CSS named colors first
  if cssColors.hasKey(input):
    let u32val = cssColors[input]
    result.r = ((u32val shr 24) and 0xFF).uint8
    result.g = ((u32val shr 16) and 0xFF).uint8
    result.b = ((u32val shr 8) and 0xFF).uint8
    result.a = (u32val and 0xFF).uint8
    return

  # 2. Handle hex formats (#RGB, #RRGGBB, #RRGGBBAA)
  if input.startsWith("#"):
    var hex = input[1..^1]

    if hex.len == 3:
      # #RGB -> #RRGGBBAA
      let r = parseHexInt($hex[0] & $hex[0]).uint8
      let g = parseHexInt($hex[1] & $hex[1]).uint8
      let b = parseHexInt($hex[2] & $hex[2]).uint8
      return rgba(r, g, b, 255)

    if hex.len == 6:
      hex = hex & "FF"  # Add full opacity

    if hex.len == 8:
      result.r = parseHexInt(hex[0..1]).uint8
      result.g = parseHexInt(hex[2..3]).uint8
      result.b = parseHexInt(hex[4..5]).uint8
      result.a = parseHexInt(hex[6..7]).uint8
      return

  # 3. Default to black if no format matches
  result = rgba(0, 0, 0, 255)

proc toHex*(c: Color): string =
  ## Convert color to hex string
  result = "#"
  result.add toHex(c.r.int, 2)
  result.add toHex(c.g.int, 2)
  result.add toHex(c.b.int, 2)
  result.add toHex(c.a.int, 2)

# ============================================================================
# Value constructors and utilities
# ============================================================================

proc val*(x: int): Value = Value(kind: vkInt, intVal: x)
proc val*(x: float): Value = Value(kind: vkFloat, floatVal: x)
proc val*(x: string): Value = Value(kind: vkString, strVal: x)
proc val*(x: bool): Value = Value(kind: vkBool, boolVal: x)
proc val*(x: Color): Value = Value(kind: vkColor, colorVal: x)
proc val*(x: Alignment): Value = Value(kind: vkAlignment, alignVal: x)

proc `$`*(v: Value): string =
  ## Convert Value to string representation
  case v.kind:
  of vkInt: $v.intVal
  of vkFloat: $v.floatVal
  of vkString: v.strVal
  of vkBool: $v.boolVal
  of vkColor: v.colorVal.toHex()
  of vkAlignment: $v.alignVal
  of vkNil: "nil"

proc getInt*(v: Value, default: int = 0): int =
  if v.kind == vkInt: v.intVal else: default

proc getFloat*(v: Value, default: float = 0.0): float =
  case v.kind:
  of vkFloat: v.floatVal
  of vkInt: v.intVal.float
  else: default

proc getString*(v: Value, default: string = ""): string =
  if v.kind == vkString: v.strVal else: default

proc getBool*(v: Value, default: bool = false): bool =
  if v.kind == vkBool: v.boolVal else: default

proc getColor*(v: Value): Color =
  if v.kind == vkColor: v.colorVal
  elif v.kind == vkString: parseColor(v.strVal)
  else: rgba(0, 0, 0, 255)

proc getAlignment*(v: Value, default: Alignment = alStart): Alignment =
  if v.kind == vkAlignment: v.alignVal else: default

# ============================================================================
# Element constructors and utilities
# ============================================================================

proc newElement*(kind: ElementKind): Element =
  ## Create a new element of the given kind
  result = Element(
    kind: kind,
    properties: initTable[string, Value](),
    children: @[],
    eventHandlers: initTable[string, EventHandler]()
  )

proc setProp*(elem: Element, name: string, value: Value) =
  ## Set a property on an element
  elem.properties[name] = value

proc setProp*(elem: Element, name: string, value: int) =
  elem.setProp(name, val(value))

proc setProp*(elem: Element, name: string, value: float) =
  elem.setProp(name, val(value))

proc setProp*(elem: Element, name: string, value: string) =
  elem.setProp(name, val(value))

proc setProp*(elem: Element, name: string, value: bool) =
  elem.setProp(name, val(value))

proc setProp*(elem: Element, name: string, value: Color) =
  elem.setProp(name, val(value))

proc getProp*(elem: Element, name: string): Option[Value] =
  ## Get a property from an element
  if name in elem.properties:
    some(elem.properties[name])
  else:
    none(Value)

proc getProp*(elem: Element, name: string, default: Value): Value =
  ## Get a property with a default value
  elem.properties.getOrDefault(name, default)

proc addChild*(parent, child: Element) =
  ## Add a child element
  parent.children.add(child)
  child.parent = parent

proc setEventHandler*(elem: Element, event: string, handler: EventHandler) =
  ## Set an event handler
  elem.eventHandlers[event] = handler

# ============================================================================
# Debugging utilities
# ============================================================================

proc `$`*(elem: Element, indent: int = 0): string =
  ## Pretty-print element tree
  let ind = "  ".repeat(indent)
  result = ind & $elem.kind

  if elem.id.len > 0:
    result.add " #" & elem.id

  result.add "\n"

  # Print properties
  for name, value in elem.properties:
    result.add ind & "  " & name & ": " & $value & "\n"

  # Print event handlers
  for event, _ in elem.eventHandlers:
    result.add ind & "  " & event & ": <handler>\n"

  # Print children
  for child in elem.children:
    result.add `$`(child, indent + 1)

proc printTree*(elem: Element) =
  ## Print the element tree to stdout
  echo elem

# ============================================================================
# Common property accessors
# ============================================================================
# Note: Removed templates to avoid conflicts with DSL property names
# Use elem.getProp("width") instead

# ============================================================================
# Hash function for Element (for use as Table key)
# ============================================================================

proc hash*(elem: Element): Hash =
  ## Hash element by its address (reference identity)
  hash(cast[pointer](elem))
