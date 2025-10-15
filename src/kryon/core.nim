## Core types and data structures for Kryon UI framework
##
## This module defines the fundamental types used throughout Kryon:
## - Element: Base UI element type
## - Property: Key-value properties
## - ElementKind: Enumeration of all element types
## - Color: Color representation

import tables, strutils, options, hashes, sets, macros

type
  ElementKind* = enum
    ## All supported UI element types
    ekHeader = "Header"       # Window metadata (non-rendering)
    ekBody = "Body"           # Main UI content wrapper (non-rendering)
    ekResources = "Resources" # Application resources (fonts, assets) (non-rendering)
    ekFont = "Font"           # Font resource definition (non-rendering)
    ekConditional = "Conditional"  # Conditional rendering (non-rendering)
    ekForLoop = "ForLoop"     # Dynamic for loop rendering (non-rendering)
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
    ekTabGroup = "TabGroup"
    ekTabBar = "TabBar"
    ekTab = "Tab"
    ekTabContent = "TabContent"
    ekTabPanel = "TabPanel"
    ekLink = "Link"

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
    vkInt, vkFloat, vkString, vkStringSeq, vkBool, vkColor, vkAlignment, vkGetter, vkNil

  Value* = object
    ## Dynamic value type for properties
    case kind*: ValueKind
    of vkInt: intVal*: int
    of vkFloat: floatVal*: float
    of vkString: strVal*: string
    of vkStringSeq: strSeqVal*: seq[string]
    of vkBool: boolVal*: bool
    of vkColor: colorVal*: Color
    of vkAlignment: alignVal*: Alignment
    of vkGetter: getter*: proc(): Value {.closure.}
    of vkNil: discard

  Property* = tuple
    ## Key-value property pair
    name: string
    value: Value

  EventHandler* = proc (data: string = "") {.closure.}
    ## Event handler callback type - accepts optional data parameter

  FontResource* = object
    ## Font resource definition for DSL
    name*: string
    sources*: seq[string]

  AppResources* = object
    ## Application resources container
    fonts*: seq[FontResource]

  StyleParameter* = object
    ## Parameter definition for parameterized styles
    name*: string
    typeNode*: NimNode  # Store the type for compile-time checking

  StyleRule* = object
    ## Individual style rule with optional condition
    properties*: seq[Property]
    condition*: Option[proc(): bool {.closure.}]  # For conditional styles

  Style* = object
    ## Style definition for reusable styling
    name*: string
    parameters*: seq[StyleParameter]  # Empty for parameterless styles
    rules*: seq[StyleRule]            # Style rules (can have conditions)
    isParameterized*: bool

  Stylesheet* = object
    ## Collection of defined styles
    styles*: TableRef[string, Style]

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

    # Conditional rendering properties (for ekConditional)
    condition*: proc(): bool {.closure.}  # Function that evaluates the condition
    trueBranch*: Element                  # Element to render when condition is true
    falseBranch*: Element                 # Element to render when condition is false (optional)

    # For loop properties (for ekForLoop)
    forIterable*: proc(): seq[Value] {.closure.}  # Function that returns the iterable data (generic sequence)
    forBodyTemplate*: proc(item: Value): Element {.closure.}  # Function to create element for each item (generic)
    forBuilder*: proc(elem: Element) {.closure.}  # Alternative builder for type-preserving loops

    # Variable binding for two-way data binding (primarily for Input elements)
    boundVarName*: Option[string]  # Name of the bound variable for two-way binding

    # Reactivity system - dirty flag for selective updates
    dirty*: bool  # Whether this element needs layout recalculation

    # Dropdown-specific properties (for ekDropdown)
    dropdownOptions*: seq[string]  # Available options
    dropdownSelectedIndex*: int    # Currently selected index (-1 for no selection)
    dropdownIsOpen*: bool           # Whether dropdown is currently open
    dropdownHoveredIndex*: int    # Index of currently hovered option

    # Tab-specific properties (for ekTabGroup, ekTabBar, ekTab, ekTabContent, ekTabPanel)
    tabSelectedIndex*: int         # Currently selected tab index (for TabGroup/TabBar)
    tabTitle*: string              # Title text for Tab
    tabIndex*: int                 # Index of this tab (for Tab) or panel (for TabPanel)

# ============================================================================
# Forward Declarations for Reactive System
# ============================================================================

proc registerDependency*(valueIdentifier: string)  # Forward declaration

# ============================================================================
# Color Utilities
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

proc val*(x: seq[string]): Value =
  ## Convert a sequence of strings to a Value (for font sources)
  Value(kind: vkStringSeq, strSeqVal: @x)

proc valGetter*(getter: proc(): Value {.closure.}): Value =
  ## Create a reactive getter value that re-evaluates on each access
  ## Note: Dependency tracking for generic getters is handled at the DSL level
  Value(kind: vkGetter, getter: getter)

proc `$`*(v: Value): string =
  ## Convert Value to string representation
  case v.kind:
  of vkInt: $v.intVal
  of vkFloat: $v.floatVal
  of vkString: v.strVal
  of vkStringSeq: "[" & v.strSeqVal.join(", ") & "]"
  of vkBool: $v.boolVal
  of vkColor: v.colorVal.toHex()
  of vkAlignment: $v.alignVal
  of vkGetter: "<getter>"
  of vkNil: "nil"

proc getInt*(v: Value, default: int = 0): int =
  if v.kind == vkGetter:
    v.getter().getInt(default)
  elif v.kind == vkInt:
    v.intVal
  else:
    default

proc getFloat*(v: Value, default: float = 0.0): float =
  if v.kind == vkGetter:
    v.getter().getFloat(default)
  else:
    case v.kind:
    of vkFloat: v.floatVal
    of vkInt: v.intVal.float
    else: default

proc getString*(v: Value, default: string = ""): string =
  if v.kind == vkGetter:
    v.getter().getString(default)
  elif v.kind == vkString:
    v.strVal
  else:
    default

proc getStringSeq*(v: Value): seq[string] =
  ## Extract a sequence of strings from a Value.
  case v.kind
  of vkGetter:
    v.getter().getStringSeq()
  of vkStringSeq:
    v.strSeqVal
  of vkString:
    @[v.strVal]
  else:
    @[]

proc getBool*(v: Value, default: bool = false): bool =
  if v.kind == vkGetter:
    v.getter().getBool(default)
  elif v.kind == vkBool:
    v.boolVal
  else:
    default

proc getColor*(v: Value): Color =
  if v.kind == vkGetter:
    v.getter().getColor()
  elif v.kind == vkColor:
    v.colorVal
  elif v.kind == vkString:
    parseColor(v.strVal)
  else:
    rgba(0, 0, 0, 255)

proc getAlignment*(v: Value, default: Alignment = alStart): Alignment =
  if v.kind == vkGetter:
    v.getter().getAlignment(default)
  elif v.kind == vkAlignment:
    v.alignVal
  else:
    default

# ============================================================================
# Element constructors and utilities
# ============================================================================

proc newElement*(kind: ElementKind): Element =
  ## Create a new element of the given kind
  result = Element(
    kind: kind,
    properties: initTable[string, Value](),
    children: @[],
    eventHandlers: initTable[string, EventHandler](),
    dirty: true  # New elements start as dirty to ensure initial layout
  )

  # Initialize dropdown-specific properties
  if kind == ekDropdown:
    result.dropdownOptions = @[]
    result.dropdownSelectedIndex = -1
    result.dropdownIsOpen = false
    result.dropdownHoveredIndex = -1

proc newConditionalElement*(condition: proc(): bool {.closure.}, trueBranch: Element, falseBranch: Element = nil): Element =
  ## Create a conditional element that renders different content based on a condition
  result = Element(
    kind: ekConditional,
    properties: initTable[string, Value](),
    children: @[],
    eventHandlers: initTable[string, EventHandler](),
    condition: condition,
    trueBranch: trueBranch,
    falseBranch: falseBranch
  )

proc newForLoopElement*(iterable: proc(): seq[Value] {.closure.}, bodyTemplate: proc(item: Value): Element {.closure.}): Element =
  ## Create a for loop element that renders content for each item in a generic sequence
  result = Element(
    kind: ekForLoop,
    properties: initTable[string, Value](),
    children: @[],
    eventHandlers: initTable[string, EventHandler](),
    forIterable: iterable,
    forBodyTemplate: bodyTemplate,
    forBuilder: nil
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
  ## Get a property from an element (evaluates getters)
  if name in elem.properties:
    let propVal = elem.properties[name]
    if propVal.kind == vkGetter:
      # Evaluate getter to get current value
      some(propVal.getter())
    else:
      # Static value
      some(propVal)
  else:
    none(Value)

proc getProp*(elem: Element, name: string, default: Value): Value =
  ## Get a property with a default value (evaluates getters)
  if name in elem.properties:
    let propVal = elem.properties[name]
    if propVal.kind == vkGetter:
      # Evaluate getter to get current value
      propVal.getter()
    else:
      # Static value
      propVal
  else:
    default

proc addChild*(parent, child: Element) =
  ## Add a child element
  parent.children.add(child)
  child.parent = parent

proc setEventHandler*(elem: Element, event: string, handler: EventHandler) =
  ## Set an event handler
  elem.eventHandlers[event] = handler

proc setBoundVarName*(elem: Element, varName: string) =
  ## Set the bound variable name for two-way data binding
  elem.boundVarName = some(varName)

proc getBoundVarName*(elem: Element): Option[string] =
  ## Get the bound variable name for two-way data binding
  elem.boundVarName

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

# ============================================================================
# Global Reactive State (defined after hash function is available)
# ============================================================================

# Global dirty elements set for tracking elements that need updates
var dirtyElements*: HashSet[Element] = initHashSet[Element]()
var dirtyElementsCount*: int = 0  # Simple counter for performance

# Thread-local variable to track the current element being processed during layout
# This allows us to establish dependencies between elements and reactive values
var currentElementBeingProcessed*: Element = nil

# Map from reactive value identifiers to dependent elements for targeted invalidation
var reactiveDependencies*: Table[string, HashSet[Element]] = initTable[string, HashSet[Element]]()

# Global application resources
var appResources*: AppResources = AppResources(fonts: @[])

# Simple style registry using a sequence
var registeredStyles*: seq[Style] = @[]

# Global style registry for direct property access (used by DSL)
var globalStyleRegistry*: Table[string, seq[Property]] = initTable[string, seq[Property]]()

# Simple style registration function
proc registerStyle*(style: Style) =
  ## Register a style in the simple registry
  registeredStyles.add(style)

# Simple style lookup function
proc getStyle*(name: string): Option[Style] =
  ## Get a style by name from the simple registry
  for style in registeredStyles:
    if style.name == name:
      return some(style)
  none(Style)

# Reactive System Helper Function Implementations

proc markDirty*(elem: Element) =
  ## Mark an element as needing layout recalculation
  if elem != nil and not elem.dirty:
    elem.dirty = true
    dirtyElements.incl(elem)
    dirtyElementsCount += 1

    # Also mark parent as dirty since layout may need to adjust
    if elem.parent != nil:
      markDirty(elem.parent)

proc markAllDescendantsDirty*(elem: Element) =
  ## Mark an element and all its descendants as dirty
  ## Useful for ensuring complete re-render of complex UI components
  if elem != nil:
    markDirty(elem)
    for child in elem.children:
      markAllDescendantsDirty(child)

proc isDirty*(elem: Element): bool =
  ## Check if an element is marked as dirty
  elem != nil and elem.dirty

proc markClean*(elem: Element) =
  ## Mark an element as clean (no layout recalculation needed)
  if elem != nil and elem.dirty:
    elem.dirty = false
    dirtyElements.excl(elem)
    dirtyElementsCount = max(0, dirtyElementsCount - 1)

proc hasDirtyElements*(): bool =
  ## Check if there are any dirty elements that need processing
  dirtyElementsCount > 0

proc clearDirtyElements*() =
  ## Clear all dirty flags and the global dirty set
  for elem in dirtyElements:
    if elem != nil:
      elem.dirty = false
  dirtyElements.clear()
  dirtyElementsCount = 0

proc registerDependency*(valueIdentifier: string) =
  ## Register the current element as dependent on a reactive value
  if currentElementBeingProcessed != nil:
    if valueIdentifier notin reactiveDependencies:
      reactiveDependencies[valueIdentifier] = initHashSet[Element]()
    reactiveDependencies[valueIdentifier].incl(currentElementBeingProcessed)

proc invalidateReactiveValue*(valueIdentifier: string) =
  ## Mark all elements dependent on a reactive value as dirty
  echo "🔥 INVALIDATING REACTIVE VALUE: ", valueIdentifier
  if valueIdentifier in reactiveDependencies:
    echo "🔥 Found ", reactiveDependencies[valueIdentifier].len, " dependent elements"
    for elem in reactiveDependencies[valueIdentifier]:
      echo "🔥 Marking element as dirty: ", elem.kind
      markDirty(elem)
  else:
    echo "🔥 No dependent elements found for: ", valueIdentifier

proc invalidateReactiveValues*(valueIdentifiers: openArray[string]) =
  ## Mark all elements dependent on multiple reactive values as dirty
  ## More efficient than calling invalidateReactiveValue multiple times
  for valueIdentifier in valueIdentifiers:
    if valueIdentifier in reactiveDependencies:
      for elem in reactiveDependencies[valueIdentifier]:
        markDirty(elem)

proc invalidateAllReactiveValues*() =
  ## Mark all reactive elements as dirty (fallback when specific dependencies are unknown)
  var processed = initHashSet[Element]()
  for _, elemSet in reactiveDependencies:
    for elem in elemSet:
      if elem != nil and elem notin processed:
        processed.incl(elem)
        markDirty(elem)

proc invalidateRelatedValues*(baseIdentifier: string) =
  ## Invalidate reactive values that are commonly related to the base identifier
  ## This helps with cross-element dependencies like tab selection affecting content
  let relatedValues = case baseIdentifier:
    of "selectedIndex": @["tabSelectedIndex", "selectedTab", "activeTab"]
    of "tabSelectedIndex": @["selectedIndex", "selectedTab", "activeTab"]
    of "forLoopIterable": @["forLoopData", "iterableContent"]
    else: @[]

  # Invalidate the base identifier and all related values
  invalidateReactiveValue(baseIdentifier)
  invalidateReactiveValues(relatedValues)

proc createReactiveEventHandler*(handler: proc(), invalidatedVars: seq[string]): EventHandler =
  ## Create an event handler that automatically invalidates reactive variables and marks elements dirty
  result = proc(data: string = "") {.closure.} =
    handler()
    # Invalidate all specified reactive variables
    for varName in invalidatedVars:
      invalidateReactiveValue(varName)

# ============================================================================
# Resource Management
# ============================================================================

proc addFontResource*(name: string, sources: seq[string]) =
  ## Add a font resource to the global application resources
  let fontResource = FontResource(name: name, sources: sources)
  # Replace existing resource with the same name
  for i, existing in appResources.fonts.mpairs:
    if existing.name == name:
      appResources.fonts[i] = fontResource
      return
  appResources.fonts.add(fontResource)

proc getFontResource*(name: string): Option[FontResource] =
  ## Get a font resource by name
  for font in appResources.fonts:
    if font.name == name:
      return some(font)
  none(FontResource)

proc clearFontResources*() =
  ## Clear all font resources
  appResources.fonts = @[]

proc getRegisteredFontNames*(): seq[string] =
  ## Get all registered font names
  result = @[]
  for font in appResources.fonts:
    result.add(font.name)

proc getRegisteredFonts*(): seq[FontResource] =
  ## Get a copy of all registered font resources
  appResources.fonts

# ============================================================================
# Style Management (simple implementation)
# ============================================================================

proc clearStyles*() =
  ## Clear all registered styles
  registeredStyles.setLen(0)

proc getRegisteredStyleNames*(): seq[string] =
  ## Get all registered style names
  result = @[]
  for style in registeredStyles:
    result.add(style.name)

proc applyStyleToElement*(elem: Element, styleName: string, params: seq[Value] = @[]) =
  ## Apply a style to an element by name
  let styleOpt = getStyle(styleName)
  if styleOpt.isNone:
    echo "Warning: Style '" & styleName & "' not found"
    return

  let style = styleOpt.get()

  # Check parameter count
  if style.isParameterized and params.len != style.parameters.len:
    echo "Warning: Style '" & styleName & "' expects " & $style.parameters.len & " parameters, got " & $params.len
    return

  if not style.isParameterized and params.len > 0:
    echo "Warning: Style '" & styleName & "' is parameterless but parameters were provided"
    return

  # Apply style rules
  for rule in style.rules:
    # Check condition if present
    if rule.condition.isSome:
      if not rule.condition.get()():
        continue

    # Apply properties
    for prop in rule.properties:
      elem.setProp(prop.name, prop.value)

proc createParameterizedStyleApplication*(styleName: string, params: seq[NimNode]): NimNode =
  ## Create a style application with parameters for compile-time generation
  let styleOpt = getStyle(styleName)
  if styleOpt.isNone:
    error("Style '" & styleName & "' not found at compile time")

  let style = styleOpt.get()
  var propertyAssignments = newStmtList()

  # Generate property assignments for each rule
  for rule in style.rules:
    # For parameterized styles, we'll handle conditions in the generated code
    var conditionCheck: NimNode = nil
    if rule.condition.isSome:
      # This is a simplified approach - in a full implementation,
      # we'd need to generate proper condition checking
      conditionCheck = quote do:
        true

    # Apply properties
    for prop in rule.properties:
      let propName = newLit(prop.name)
      let propValue = prop.value

      if conditionCheck != nil:
        propertyAssignments.add quote do:
          if `conditionCheck`:
            elem.setProp(`propName`, `propValue`)
      else:
        propertyAssignments.add quote do:
          elem.setProp(`propName`, `propValue`)

  result = propertyAssignments
