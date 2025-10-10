## Core types and data structures for Kryon UI framework
##
## This module defines the fundamental types used throughout Kryon:
## - Element: Base UI element type
## - Property: Key-value properties
## - ElementKind: Enumeration of all element types
## - Color: Color representation

import tables, strutils, options

type
  ElementKind* = enum
    ## All supported UI element types
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

proc parseColor*(s: string): Color =
  ## Parse color from hex string (#RRGGBBAA or #RRGGBB)
  var hex = s
  if hex.startsWith("#"):
    hex = hex[1..^1]

  if hex.len == 6:
    hex = hex & "FF"  # Add full opacity

  if hex.len != 8:
    raise newException(ValueError, "Invalid color format: " & s)

  result.r = parseHexInt(hex[0..1]).uint8
  result.g = parseHexInt(hex[2..3]).uint8
  result.b = parseHexInt(hex[4..5]).uint8
  result.a = parseHexInt(hex[6..7]).uint8

proc toHex*(c: Color): string =
  ## Convert color to hex string
  result = "#"
  result.add toHex(c.r.int, 2)
  result.add toHex(c.g.int, 2)
  result.add toHex(c.b.int, 2)
  result.add toHex(c.a.int, 2)

proc rgba*(r, g, b: uint8, a: uint8 = 255): Color =
  ## Create color from RGBA components
  Color(r: r, g: g, b: b, a: a)

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
