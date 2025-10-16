## SDL2 Backend for Kryon
##
## This backend renders Kryon UI elements using SDL2 for native desktop applications.
## Provides identical functionality to the Raylib backend.

import ../kryon/core
import ../kryon/fonts
import options, tables, algorithm
import sdl2_ffi

# ============================================================================
# Color Conversion
# ============================================================================

proc toSDLColor*(c: Color): SDL_Color {.inline.} =
  ## Convert Kryon Color to SDL2 SDL_Color
  rcolor(c.r, c.g, c.b, c.a)

# ============================================================================
# Backend Type
# ============================================================================

type
  SDL2Backend* = object
    window*: ptr SDL_Window
    renderer*: ptr SDL_Renderer
    font*: ptr TTF_Font
    windowWidth*: int
    windowHeight*: int
    windowTitle*: string
    backgroundColor*: SDL_Color
    running*: bool
    rootElement*: Element
    focusedInput*: Element  # Currently focused input element
    focusedDropdown*: Element  # Currently focused dropdown element
    cursorBlink*: float     # Timer for cursor blinking
    inputValues*: Table[Element, string]  # Store current text for each input
    checkboxStates*: Table[Element, bool]  # Store checked state for each checkbox
    inputScrollOffsets*: Table[Element, float]  # Store scroll offset for each input
    backspaceHoldTimer*: float  # Timer for backspace repeat
    backspaceRepeatDelay*: float  # Initial delay before repeat starts
    backspaceRepeatRate*: float   # Rate of repeat deletion
    textInputEnabled*: bool  # Whether text input is currently enabled
    keyStates*: array[512, bool]  # Track keyboard state

proc newSDL2Backend*(width, height: int, title: string): SDL2Backend =
  ## Create a new SDL2 backend
  result = SDL2Backend(
    window: nil,
    renderer: nil,
    font: nil,
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rcolor(30, 30, 30, 255),
    running: false,
    focusedDropdown: nil,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05,   # 50ms repeat rate
    textInputEnabled: false
  )

proc newSDL2BackendFromApp*(app: Element): SDL2Backend =
  ## Create backend from app element (extracts config from Header and Body)
  ## App structure: Body -> [Header, Body]

  var width = 800
  var height = 600
  var title = "Kryon App"
  var bgColor: Option[Value] = none(Value)

  # Look for Header and Body children in app
  for child in app.children:
    if child.kind == ekHeader:
      # Extract window config from Header (support both full names and aliases)
      # Try windowWidth first, then width
      var widthProp = child.getProp("windowWidth")
      if widthProp.isNone:
        widthProp = child.getProp("width")

      var heightProp = child.getProp("windowHeight")
      if heightProp.isNone:
        heightProp = child.getProp("height")

      var titleProp = child.getProp("windowTitle")
      if titleProp.isNone:
        titleProp = child.getProp("title")

      width = widthProp.get(val(800)).getInt()
      height = heightProp.get(val(600)).getInt()
      title = titleProp.get(val("Kryon App")).getString()
    elif child.kind == ekBody:
      # Extract window background from Body
      bgColor = child.getProp("backgroundColor")

  result = SDL2Backend(
    window: nil,
    renderer: nil,
    font: nil,
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: if bgColor.isSome: bgColor.get.getColor().toSDLColor() else: rcolor(30, 30, 30, 255),
    running: false,
    focusedDropdown: nil,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05,   # 50ms repeat rate
    textInputEnabled: false
  )

# ============================================================================
# Layout Engine (same as Raylib backend)
# ============================================================================

proc calculateLayout*(elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Recursively calculate layout for all elements
  ## This is identical to the Raylib backend implementation

  # Set current element for dependency tracking
  let previousElement = currentElementBeingProcessed
  currentElementBeingProcessed = elem

  # Check for absolute positioning (posX, posY)
  let posXOpt = elem.getProp("posX")
  let posYOpt = elem.getProp("posY")

  # Get element dimensions
  let widthOpt = elem.getProp("width")
  let heightOpt = elem.getProp("height")

  # Track whether dimensions were explicitly set (for auto-sizing later)
  let hasExplicitWidth = widthOpt.isSome
  let hasExplicitHeight = heightOpt.isSome

  # Set position (use posX/posY if provided, otherwise use x/y)
  elem.x = if posXOpt.isSome: posXOpt.get.getFloat() else: x
  elem.y = if posYOpt.isSome: posYOpt.get.getFloat() else: y

  # Special handling for Text elements - measure actual text size
  if elem.kind == ekText:
    let text = elem.getProp("text").get(val("")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # For SDL2, we'll use estimated text width based on character count * font size
    # This is an approximation since we need the font loaded to measure precisely
    let textWidth = text.len.float * fontSize.float * 0.6  # Approximate width
    let textHeight = fontSize.float

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: textWidth
    elem.height = if heightOpt.isSome: heightOpt.get.getFloat() else: textHeight

  elif elem.kind == ekButton:
    # Special handling for Button elements - size based on text content with padding
    let text = elem.getProp("text").get(val("Button")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Measure text dimensions and add padding
    let textWidth = text.len.float * fontSize.float * 0.6  # Approximate width
    let textHeight = fontSize.float
    let padding = 20.0  # Horizontal padding
    let verticalPadding = 10.0  # Vertical padding

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: textWidth + (padding * 2.0)
    elem.height = if heightOpt.isSome: heightOpt.get.getFloat() else: textHeight + (verticalPadding * 2.0)

  else:
    # Regular element dimension handling (containers, etc.)
    if widthOpt.isSome:
      elem.width = widthOpt.get.getFloat()
    else:
      elem.width = parentWidth

    if heightOpt.isSome:
      elem.height = heightOpt.get.getFloat()
    else:
      elem.height = parentHeight

  # Layout children based on container type (same as Raylib backend)
  case elem.kind:
  of ekHeader:
    # Header is metadata only - set size to 0 so it doesn't take up space
    elem.width = 0
    elem.height = 0

  of ekResources, ekFont:
    # Resource declarations do not participate in layout
    elem.width = 0
    elem.height = 0

  of ekConditional:
    # Conditional elements evaluate their condition and only include the active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      # Set conditional element size to 0 (it doesn't render itself)
      elem.width = 0
      elem.height = 0

      # Layout the active branch at the conditional's position with full parent dimensions
      if activeBranch != nil:
        calculateLayout(activeBranch, elem.x, elem.y, parentWidth, parentHeight)

  of ekForLoop:
    # For loop elements generate dynamic content
    if elem.forBuilder != nil:
      elem.children.setLen(0)
      elem.forBuilder(elem)
      elem.width = parentWidth
      elem.height = parentHeight

    elif elem.forIterable != nil and elem.forBodyTemplate != nil:
      registerDependency("forLoopIterable")
      let items = elem.forIterable()

      # Clear previous children (if any) before regenerating
      elem.children.setLen(0)

      # Generate elements for each item in the iterable
      for item in items:
        let childElement = elem.forBodyTemplate(item)
        if childElement != nil:
          addChild(elem, childElement)

      # For loop is transparent to layout - use parent size
      elem.width = parentWidth
      elem.height = parentHeight

  of ekBody:
    # Body implements natural document flow - stack children vertically
    let alignment = elem.getProp("contentAlignment")
    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout
      var currentY = elem.y
      let gap = elem.getProp("gap").get(val(5)).getFloat()

      # First pass: Calculate all children sizes and total height
      var totalHeight = 0.0
      var maxWidth = 0.0
      var childSizes: seq[tuple[w, h: float]] = @[]

      for child in elem.children:
        calculateLayout(child, 0, 0, elem.width, elem.height)
        childSizes.add((w: child.width, h: child.height))
        totalHeight += child.height
        maxWidth = max(maxWidth, child.width)

      if elem.children.len > 1:
        totalHeight += gap * (elem.children.len - 1).float

      # Calculate starting Y position to center the group vertically
      let startY = elem.y + (elem.height - totalHeight) / 2.0

      # Second pass: Position children centered horizontally and stacked vertically
      currentY = startY
      for i, child in elem.children:
        let centerX = elem.x + (elem.width - childSizes[i].w) / 2.0

        if i > 0:
          currentY += gap

        calculateLayout(child, centerX, currentY, childSizes[i].w, childSizes[i].h)
        currentY += childSizes[i].h
    else:
      # Default: normal relative positioning layout
      var currentY = elem.y
      let gap = elem.getProp("gap").get(val(5)).getFloat()

      for child in elem.children:
        calculateLayout(child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

  of ekColumn:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # First pass: Calculate all children sizes
    var childSizes: seq[tuple[w, h: float]] = @[]
    for child in elem.children:
      if child.kind == ekForLoop:
        calculateLayout(child, 0, 0, elem.width, 0)
        for grandchild in child.children:
          calculateLayout(grandchild, 0, 0, elem.width, 0)
          childSizes.add((w: grandchild.width, h: grandchild.height))
      else:
        calculateLayout(child, 0, 0, elem.width, 0)
        childSizes.add((w: child.width, h: child.height))

    # Calculate total height and max width
    var totalHeight = 0.0
    var maxWidth = 0.0
    for size in childSizes:
      totalHeight += size.h
      maxWidth = max(maxWidth, size.w)
    if elem.children.len > 1:
      totalHeight += gap * (elem.children.len - 1).float

    # Set column dimensions
    let widthOpt = elem.getProp("width")
    let heightOpt = elem.getProp("height")
    if widthOpt.isSome:
      elem.width = widthOpt.get.getFloat()
    else:
      elem.width = if parentWidth > 0: parentWidth else: maxWidth
    if heightOpt.isSome:
      elem.height = heightOpt.get.getFloat()
    else:
      elem.height = if parentHeight > 0: parentHeight else: totalHeight

    # Determine starting Y position and gaps based on mainAxisAlignment
    var currentY = elem.y
    var dynamicGap = gap

    if mainAxisAlignment.isSome:
      let align = mainAxisAlignment.get.getString()
      case align:
      of "start", "flex-start":
        currentY = elem.y
      of "center":
        currentY = elem.y + (elem.height - totalHeight) / 2.0
      of "end", "flex-end":
        currentY = elem.y + elem.height - totalHeight
      of "spaceEvenly":
        var totalChildHeight = 0.0
        for size in childSizes:
          totalChildHeight += size.h
        let availableSpace = elem.height - totalChildHeight
        let spaceUnit = availableSpace / (childSizes.len + 1).float
        dynamicGap = spaceUnit
        currentY = elem.y + spaceUnit
      of "spaceAround":
        var totalChildHeight = 0.0
        for size in childSizes:
          totalChildHeight += size.h
        let availableSpace = elem.height - totalChildHeight
        let spaceUnit = availableSpace / childSizes.len.float
        dynamicGap = spaceUnit
        currentY = elem.y + (spaceUnit / 2.0)
      of "spaceBetween":
        if childSizes.len > 1:
          var totalChildHeight = 0.0
          for size in childSizes:
            totalChildHeight += size.h
          let availableSpace = elem.height - totalChildHeight
          let spaceUnit = availableSpace / (childSizes.len - 1).float
          dynamicGap = spaceUnit
        currentY = elem.y
      else:
        currentY = elem.y

    # Second pass: Position children
    var childIndex = 0
    for child in elem.children:
      if childIndex > 0:
        currentY += dynamicGap

      if child.kind == ekForLoop:
        for grandchild in child.children:
          if childIndex > 0:
            currentY += dynamicGap

          var childX = elem.x
          if crossAxisAlignment.isSome:
            let align = crossAxisAlignment.get.getString()
            if align == "center":
              childX = elem.x + (elem.width - childSizes[childIndex].w) / 2.0
            elif align == "end":
              childX = elem.x + elem.width - childSizes[childIndex].w

          calculateLayout(grandchild, childX, currentY, elem.width, childSizes[childIndex].h)
          currentY += childSizes[childIndex].h
          inc childIndex
      else:
        var childX = elem.x
        if crossAxisAlignment.isSome:
          let align = crossAxisAlignment.get.getString()
          if align == "center":
            childX = elem.x + (elem.width - childSizes[childIndex].w) / 2.0
          elif align == "end":
            childX = elem.x + elem.width - childSizes[childIndex].w

        calculateLayout(child, childX, currentY, elem.width, childSizes[childIndex].h)
        currentY += childSizes[childIndex].h
        inc childIndex

  of ekRow:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # First pass: Calculate all children sizes
    var childSizes: seq[tuple[w, h: float]] = @[]
    for child in elem.children:
      if child.kind == ekForLoop:
        calculateLayout(child, 0, 0, parentWidth, elem.height)
        for grandchild in child.children:
          calculateLayout(grandchild, 0, 0, parentWidth, elem.height)
          childSizes.add((w: grandchild.width, h: grandchild.height))
      else:
        calculateLayout(child, elem.x, elem.y, parentWidth, elem.height)
        childSizes.add((w: child.width, h: child.height))

    # Calculate total width and max height
    var totalWidth = 0.0
    var maxHeight = 0.0
    for size in childSizes:
      totalWidth += size.w
      maxHeight = max(maxHeight, size.h)
    if elem.children.len > 1:
      totalWidth += gap * (elem.children.len - 1).float

    # Set row dimensions
    let widthOpt = elem.getProp("width")
    let heightOpt = elem.getProp("height")
    if widthOpt.isSome:
      elem.width = widthOpt.get.getFloat()
    else:
      elem.width = if parentWidth > 0: parentWidth else: totalWidth
    if heightOpt.isSome:
      elem.height = heightOpt.get.getFloat()
    else:
      elem.height = if parentHeight > 0: parentHeight else: maxHeight

    # Determine starting X position and gaps based on mainAxisAlignment
    var currentX = elem.x
    var dynamicGap = gap

    if mainAxisAlignment.isSome:
      let align = mainAxisAlignment.get.getString()
      case align:
      of "start", "flex-start":
        currentX = elem.x
      of "center":
        currentX = elem.x + (elem.width - totalWidth) / 2.0
      of "end", "flex-end":
        currentX = elem.x + elem.width - totalWidth
      of "spaceEvenly":
        var totalChildWidth = 0.0
        for size in childSizes:
          totalChildWidth += size.w
        let availableSpace = elem.width - totalChildWidth
        let spaceUnit = availableSpace / (childSizes.len + 1).float
        dynamicGap = spaceUnit
        currentX = elem.x + spaceUnit
      of "spaceAround":
        var totalChildWidth = 0.0
        for size in childSizes:
          totalChildWidth += size.w
        let availableSpace = elem.width - totalChildWidth
        let spaceUnit = availableSpace / childSizes.len.float
        dynamicGap = spaceUnit
        currentX = elem.x + (spaceUnit / 2.0)
      of "spaceBetween":
        if childSizes.len > 1:
          var totalChildWidth = 0.0
          for size in childSizes:
            totalChildWidth += size.w
          let availableSpace = elem.width - totalChildWidth
          let spaceUnit = availableSpace / (childSizes.len - 1).float
          dynamicGap = spaceUnit
        currentX = elem.x
      else:
        currentX = elem.x

    # Second pass: Position children
    var childIndex = 0
    for child in elem.children:
      if childIndex > 0:
        currentX += dynamicGap

      if child.kind == ekForLoop:
        for grandchild in child.children:
          if childIndex > 0:
            currentX += dynamicGap

          var childY = elem.y
          if crossAxisAlignment.isSome:
            let align = crossAxisAlignment.get.getString()
            if align == "center":
              childY = elem.y + (elem.height - childSizes[childIndex].h) / 2.0
            elif align == "end":
              childY = elem.y + elem.height - childSizes[childIndex].h

          calculateLayout(grandchild, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
          currentX += childSizes[childIndex].w
          inc childIndex
      else:
        var childY = elem.y
        if crossAxisAlignment.isSome:
          let align = crossAxisAlignment.get.getString()
          if align == "center":
            childY = elem.y + (elem.height - childSizes[childIndex].h) / 2.0
          elif align == "end":
            childY = elem.y + elem.height - childSizes[childIndex].h

        calculateLayout(child, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
        currentX += childSizes[childIndex].w
        inc childIndex

  of ekCenter:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let alignment = elem.getProp("contentAlignment").get(val("center"))

    if alignment.getString() == "center":
      for child in elem.children:
        calculateLayout(child, 0, 0, elem.width, elem.height)
        let centerX = elem.x + (elem.width - child.width) / 2.0
        let centerY = elem.y + (elem.height - child.height) / 2.0
        calculateLayout(child, centerX, centerY, child.width, child.height)
    else:
      var currentY = elem.y
      for child in elem.children:
        calculateLayout(child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

  of ekContainer:
    var currentY = elem.y
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let alignment = elem.getProp("contentAlignment")

    if alignment.isSome and alignment.get.getString() == "center":
      for child in elem.children:
        calculateLayout(child, 0, 0, elem.width, elem.height)
        let centerX = elem.x + (elem.width - child.width) / 2.0
        let centerY = elem.y + (elem.height - child.height) / 2.0
        calculateLayout(child, centerX, centerY, child.width, child.height)

      if not hasExplicitHeight and elem.children.len > 0:
        var maxHeight = 0.0
        for child in elem.children:
          maxHeight = max(maxHeight, child.height)
        let paddingValue = elem.getProp("padding").get(val(0)).getFloat()
        elem.height = maxHeight + (paddingValue * 2.0)
    else:
      for child in elem.children:
        calculateLayout(child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

      if not hasExplicitHeight and elem.children.len > 0:
        let paddingValue = elem.getProp("padding").get(val(0)).getFloat()
        elem.height = (currentY - elem.y - gap) + (paddingValue * 2.0)

  of ekTabGroup:
    let selectedIndexProp = elem.getProp("selectedIndex")
    if selectedIndexProp.isSome:
      elem.tabSelectedIndex = selectedIndexProp.get.getInt()

    registerDependency("tabSelectedIndex")
    registerDependency("selectedIndex")

    var currentY = elem.y
    for child in elem.children:
      if child.kind != ekConditional and child.kind != ekForLoop:
        calculateLayout(child, elem.x, currentY, elem.width, 0)
        currentY += child.height

  of ekTabBar:
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup:
        elem.tabSelectedIndex = parent.tabSelectedIndex
        break
      parent = parent.parent

    var currentX = elem.x
    var maxHeight = 40.0
    var tabIndex = 0

    var allTabs: seq[Element] = @[]

    # First pass: collect all Tab elements
    for child in elem.children:
      if child.kind == ekTab:
        allTabs.add(child)
      elif child.kind == ekForLoop:
        calculateLayout(child, elem.x, elem.y, elem.width, 0)
        for grandchild in child.children:
          if grandchild.kind == ekTab:
            allTabs.add(grandchild)

    # Second pass: assign indices and layout all tabs
    for tab in allTabs:
      tab.tabIndex = tabIndex
      tabIndex += 1
      let title = tab.getProp("title").get(val("Tab")).getString()
      let fontSize = tab.getProp("fontSize").get(val(16)).getInt()
      let textWidth = title.len.float * fontSize.float * 0.6
      let tabWidth = tab.getProp("width").get(val(textWidth + 40.0)).getFloat()
      let tabHeight = tab.getProp("height").get(val(40.0)).getFloat()
      maxHeight = max(maxHeight, tabHeight)

      calculateLayout(tab, currentX, elem.y, tabWidth, tabHeight)
      currentX += tabWidth

    elem.height = maxHeight

  of ekTab:
    discard  # Already handled in TabBar layout

  of ekTabContent:
    var selectedIndex = 0
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup:
        selectedIndex = parent.tabSelectedIndex
        break
      parent = parent.parent

    registerDependency("tabSelectedIndex")
    registerDependency("selectedIndex")

    var allTabPanels: seq[Element] = @[]
    var panelIndex = 0

    for child in elem.children:
      if child.kind == ekTabPanel:
        allTabPanels.add(child)
      elif child.kind == ekForLoop:
        calculateLayout(child, elem.x, elem.y, elem.width, elem.height)
        for grandchild in child.children:
          if grandchild.kind == ekTabPanel:
            allTabPanels.add(grandchild)

    for panel in allTabPanels:
      panel.tabIndex = panelIndex
      panelIndex += 1
      if panel.tabIndex == selectedIndex:
        calculateLayout(panel, elem.x, elem.y, elem.width, elem.height)

  of ekTabPanel:
    var currentY = elem.y
    let gap = elem.getProp("gap").get(val(10)).getFloat()
    let padding = elem.getProp("padding").get(val(0)).getFloat()

    let contentX = elem.x + padding
    let contentY = elem.y + padding
    let contentWidth = elem.width - (padding * 2.0)

    currentY = contentY
    for child in elem.children:
      calculateLayout(child, contentX, currentY, contentWidth, 0)
      currentY += child.height + gap

  else:
    # Default: just layout children in same space as parent
    for child in elem.children:
      calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

  # Restore previous element and mark this element as clean
  currentElementBeingProcessed = previousElement
  markClean(elem)

# ============================================================================
# Rendering with SDL2
# ============================================================================

proc sortChildrenByZIndex(children: seq[Element]): seq[Element] =
  ## Sort children by z-index for correct layered rendering
  result = children
  result.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(aZIndex, bZIndex)
  )

proc renderText*(backend: SDL2Backend, text: string, x, y: float, fontSize: int, color: SDL_Color) =
  ## Render text using SDL_ttf
  if backend.font == nil or text.len == 0:
    return

  let textCStr = text.cstring

  # Get text dimensions first
  var textW, textH: cint
  if TTF_SizeUTF8(backend.font, textCStr, addr textW, addr textH) != 0:
    return

  var textSurface = TTF_RenderUTF8_Blended(backend.font, textCStr, color)
  if textSurface == nil:
    return

  var textTexture = SDL_CreateTextureFromSurface(backend.renderer, textSurface)
  if textTexture != nil:
    var textRect = rrecti(x.cint, y.cint, textW, textH)
    discard SDL_RenderCopy(backend.renderer, textTexture, nil, addr textRect)
    SDL_DestroyTexture(textTexture)

  SDL_FreeSurface(textSurface)

proc measureText*(backend: SDL2Backend, text: string, fontSize: int): tuple[w, h: int] =
  ## Measure text dimensions using SDL_ttf
  if backend.font == nil or text.len == 0:
    return (w: 0, h: fontSize)

  let textCStr = text.cstring
  var w, h: cint
  if TTF_SizeUTF8(backend.font, textCStr, addr w, addr h) == 0:
    return (w: w.int, h: h.int)
  else:
    # Fallback to approximation
    return (w: text.len * fontSize * 6 div 10, h: fontSize)

proc approximateTextWidth*(text: string, fontSize: int): int =
  ## Approximate text width without needing a font/backend
  ## Used in contexts where we don't have access to the backend
  result = text.len * fontSize * 6 div 10

proc renderElement*(backend: var SDL2Backend, elem: Element, inheritedColor: Option[Color] = none(Color)) =
  ## Render an element using SDL2

  case elem.kind:
  of ekHeader:
    discard

  of ekResources, ekFont:
    discard

  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        backend.renderElement(activeBranch, inheritedColor)

  of ekForLoop:
    for child in elem.children:
      backend.renderElement(child, inheritedColor)

  of ekBody:
    let bodyColor = elem.getProp("color")
    let colorToInherit = if bodyColor.isSome:
      some(bodyColor.get.getColor())
    else:
      inheritedColor

    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, colorToInherit)

  of ekContainer:
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)

    # Draw background
    var bgColor = elem.getProp("backgroundColor")
    if bgColor.isNone:
      bgColor = elem.getProp("background")
    if bgColor.isSome:
      let sdlColor = bgColor.get.getColor().toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlColor.r, sdlColor.g, sdlColor.b, sdlColor.a)
      discard SDL_RenderFillRect(backend.renderer, addr rect)

    # Draw border
    let borderColor = elem.getProp("borderColor")
    if borderColor.isSome:
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
      let sdlBorderColor = borderColor.get.getColor().toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
      discard SDL_RenderDrawRect(backend.renderer, addr rect)

  of ekText:
    let text = elem.getProp("text").get(val("")).getString()
    let explicitColor = elem.getProp("color")
    let color = if explicitColor.isSome:
      explicitColor.get.getColor()
    elif inheritedColor.isSome:
      inheritedColor.get
    else:
      parseColor("#FFFFFF")
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    backend.renderText(text, elem.x, elem.y, fontSize, color.toSDLColor())

  of ekButton:
    let text = elem.getProp("text").get(val("Button")).getString()
    let bgColor = elem.getProp("backgroundColor").get(val("#4A90E2"))
    let textColor = elem.getProp("color").get(val("#FFFFFF")).getColor()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Get border properties with defaults
    let borderWidthProp = elem.getProp("borderWidth")
    let borderWidth = if borderWidthProp.isSome:
      borderWidthProp.get.getFloat()
    else:
      2.0  # Default border width for buttons

    let borderColorProp = elem.getProp("borderColor")
    let borderColor = if borderColorProp.isSome:
      borderColorProp.get.getColor()
    else:
      parseColor("#D1D5DB")  # Default border color for buttons

    # Draw button background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let sdlBgColor = bgColor.getColor().toSDLColor()
    discard SDL_SetRenderDrawColor(backend.renderer, sdlBgColor.r, sdlBgColor.g, sdlBgColor.b, sdlBgColor.a)
    discard SDL_RenderFillRect(backend.renderer, addr rect)

    # Draw button border
    if borderWidth > 0:
      let sdlBorderColor = borderColor.toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
      discard SDL_RenderDrawRect(backend.renderer, addr rect)

    # Center text in button
    let textMetrics = backend.measureText(text, fontSize)
    let textX = elem.x + (elem.width - textMetrics.w.float) / 2.0
    let textY = elem.y + (elem.height - textMetrics.h.float) / 2.0

    backend.renderText(text, textX, textY, fontSize, textColor.toSDLColor())

  of ekInput:
    let reactiveValue = elem.getProp("value").get(val("")).getString()

    var value: string
    if backend.inputValues.hasKey(elem):
      value = backend.inputValues[elem]
      if reactiveValue != value and backend.focusedInput != elem:
        value = reactiveValue
        backend.inputValues[elem] = reactiveValue
    else:
      value = reactiveValue

    let placeholder = elem.getProp("placeholder").get(val("")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()

    let borderWidthProp = elem.getProp("borderWidth")
    let borderWidth = if borderWidthProp.isSome:
      borderWidthProp.get.getFloat()
    else:
      1.0

    let borderColorProp = elem.getProp("borderColor")
    let borderColor = if borderColorProp.isSome:
      borderColorProp.get.getColor()
    else:
      if backend.focusedInput == elem:
        parseColor("#4A90E2")
      else:
        parseColor("#CCCCCC")

    # Draw input background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let sdlBgColor = bgColor.getColor().toSDLColor()
    discard SDL_SetRenderDrawColor(backend.renderer, sdlBgColor.r, sdlBgColor.g, sdlBgColor.b, sdlBgColor.a)
    discard SDL_RenderFillRect(backend.renderer, addr rect)

    # Draw border
    let actualBorderWidth = if backend.focusedInput == elem and borderWidth == 1.0: 2.0 else: borderWidth
    if actualBorderWidth > 0:
      let sdlBorderColor = borderColor.toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
      discard SDL_RenderDrawRect(backend.renderer, addr rect)

    # Text rendering with clipping
    let padding = 8.0
    let displayText = if value.len > 0: value else: placeholder
    let displayColor = if value.len > 0: textColor else: parseColor("#999999")

    # Calculate cursor position
    let cursorTextPos = if value.len > 0 and backend.focusedInput == elem:
      backend.measureText(value, fontSize).w.float
    else:
      0.0

    var scrollOffset = backend.inputScrollOffsets.getOrDefault(elem, 0.0)
    let visibleWidth = elem.width - (padding * 2.0)

    # Adjust scroll offset to keep cursor visible
    if backend.focusedInput == elem:
      let totalTextWidth = if value.len > 0:
        backend.measureText(value, fontSize).w.float
      else:
        0.0

      if cursorTextPos - scrollOffset > visibleWidth - 20.0:
        scrollOffset = cursorTextPos - visibleWidth + 20.0

      if cursorTextPos < scrollOffset + 20.0:
        scrollOffset = max(0.0, cursorTextPos - 20.0)

      scrollOffset = max(0.0, scrollOffset)

      if totalTextWidth <= visibleWidth:
        scrollOffset = 0.0
      else:
        let maxScrollOffset = max(0.0, totalTextWidth - visibleWidth)
        scrollOffset = min(scrollOffset, maxScrollOffset)

      backend.inputScrollOffsets[elem] = scrollOffset

    # Set up clipping
    let clipRect = rrect(elem.x + padding, elem.y + 2.0, elem.width - (padding * 2.0), elem.height - 4.0)
    discard SDL_RenderSetClipRect(backend.renderer, addr clipRect)

    # Draw text with scroll offset
    if displayText.len > 0:
      let textX = elem.x + padding - scrollOffset
      let textY = elem.y + (elem.height - fontSize.float) / 2.0
      backend.renderText(displayText, textX, textY, fontSize, displayColor.toSDLColor())

    # Reset clipping
    discard SDL_RenderSetClipRect(backend.renderer, nil)

    # Draw cursor if focused and visible
    if backend.focusedInput == elem and backend.cursorBlink < 0.5:
      let cursorX = elem.x + padding + cursorTextPos - scrollOffset
      let cursorY = elem.y + 4.0

      if cursorX >= elem.x + padding and cursorX <= elem.x + elem.width - padding:
        let cursorRect = rrect(cursorX, cursorY, 1.0, fontSize.float - 2.0)
        let sdlTextColor = textColor.toSDLColor()
        discard SDL_SetRenderDrawColor(backend.renderer, sdlTextColor.r, sdlTextColor.g, sdlTextColor.b, sdlTextColor.a)
        discard SDL_RenderFillRect(backend.renderer, addr cursorRect)

  of ekCheckbox:
    let label = elem.getProp("label").get(val("")).getString()
    let checkedProp = elem.getProp("checked").get(val(false))
    let initialChecked = checkedProp.getBool()

    var isChecked = initialChecked
    if backend.checkboxStates.hasKey(elem):
      isChecked = backend.checkboxStates[elem]
    else:
      backend.checkboxStates[elem] = initialChecked

    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()
    let borderColor = elem.getProp("borderColor").get(val("#CCCCCC")).getColor()
    let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let labelColor = if elem.getProp("labelColor").isSome:
      elem.getProp("labelColor").get.getColor()
    else:
      textColor

    let checkboxX = elem.x
    let checkboxY = elem.y + (elem.height - checkboxSize) / 2.0
    let checkboxRect = rrect(checkboxX, checkboxY, checkboxSize, checkboxSize)

    # Draw checkbox background
    let sdlBgColor = bgColor.getColor().toSDLColor()
    discard SDL_SetRenderDrawColor(backend.renderer, sdlBgColor.r, sdlBgColor.g, sdlBgColor.b, sdlBgColor.a)
    discard SDL_RenderFillRect(backend.renderer, addr checkboxRect)

    # Draw checkbox border
    if borderWidth > 0:
      let sdlBorderColor = borderColor.toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
      discard SDL_RenderDrawRect(backend.renderer, addr checkboxRect)

    # Draw checkmark if checked
    if isChecked:
      let checkColor = elem.getProp("checkColor").get(val("#4A90E2")).getColor()
      let padding = checkboxSize * 0.2
      let sdlCheckColor = checkColor.toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlCheckColor.r, sdlCheckColor.g, sdlCheckColor.b, sdlCheckColor.a)

      # Draw checkmark lines
      let startX = checkboxX + padding
      let startY = checkboxY + checkboxSize / 2.0
      let middleX = checkboxX + checkboxSize / 2.5
      let middleY = checkboxY + checkboxSize - padding
      let endX = checkboxX + checkboxSize - padding
      let endY = checkboxY + padding
      let checkThickness = max(1.0, checkboxSize / 8.0)

      # First line
      var lineRect = rrect(startX, startY - checkThickness/2, middleX - startX, checkThickness)
      discard SDL_RenderFillRect(backend.renderer, addr lineRect)

      # Second line
      lineRect = rrect(middleX - checkThickness/2, middleY - (endY - middleY), checkThickness, endY - middleY)
      discard SDL_RenderFillRect(backend.renderer, addr lineRect)

    # Draw label text if provided
    if label.len > 0:
      let textX = checkboxX + checkboxSize + 10.0
      let textY = elem.y + (elem.height - fontSize.float) / 2.0
      backend.renderText(label, textX, textY, fontSize, labelColor.toSDLColor())

  of ekDropdown:
    let selectedIndex = elem.dropdownSelectedIndex
    let isOpen = elem.dropdownIsOpen

    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()
    let borderColor = elem.getProp("borderColor").get(val("#CCCCCC")).getColor()
    let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
    let placeholder = elem.getProp("placeholder").get(val("Select...")).getString()

    # Draw the main dropdown button
    let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)
    let sdlBgColor = bgColor.getColor().toSDLColor()
    discard SDL_SetRenderDrawColor(backend.renderer, sdlBgColor.r, sdlBgColor.g, sdlBgColor.b, sdlBgColor.a)
    discard SDL_RenderFillRect(backend.renderer, addr mainRect)

    let sdlBorderColor = borderColor.toSDLColor()
    discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
    discard SDL_RenderDrawRect(backend.renderer, addr mainRect)

    # Draw dropdown arrow
    let arrowSize = 8.0
    let arrowX = elem.x + elem.width - 15.0
    let arrowY = elem.y + (elem.height - arrowSize) / 2.0
    let arrowChar = if isOpen: "^" else: "v"
    backend.renderText(arrowChar, arrowX, arrowY, fontSize, textColor.toSDLColor())

    # Draw selected text or placeholder
    let displayText = if selectedIndex >= 0 and selectedIndex < elem.dropdownOptions.len:
      elem.dropdownOptions[selectedIndex]
    else:
      placeholder

    let displayColor = if selectedIndex >= 0:
      textColor
    else:
      parseColor("#999999")

    let textPadding = 10.0
    let textX = elem.x + textPadding
    let textY = elem.y + (elem.height - fontSize.float) / 2.0
    let maxTextWidth = elem.width - (textPadding * 2.0) - 20.0

    # Set up clipping for text
    let clipRect = rrect(textX, textY, maxTextWidth, fontSize.float)
    discard SDL_RenderSetClipRect(backend.renderer, addr clipRect)
    backend.renderText(displayText, textX, textY, fontSize, displayColor.toSDLColor())
    discard SDL_RenderSetClipRect(backend.renderer, nil)

  of ekColumn, ekRow, ekCenter:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTabGroup:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      if child.kind != ekConditional and child.kind != ekForLoop:
        backend.renderElement(child, inheritedColor)

  of ekTabBar:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTab:
    let title = elem.getProp("title").get(val(elem.tabTitle)).getString()
    let tabIndex = elem.tabIndex

    var isSelected = false
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup or parent.kind == ekTabBar:
        isSelected = (parent.tabSelectedIndex == tabIndex)
        break
      parent = parent.parent

    let bgColor = if isSelected:
      elem.getProp("activeBackgroundColor").get(val("#4a90e2")).getColor()
    else:
      elem.getProp("backgroundColor").get(val("#3d3d3d")).getColor()

    let textColor = if isSelected:
      elem.getProp("activeTextColor").get(val("#ffffff")).getColor()
    else:
      elem.getProp("textColor").get(val("#ffffff")).getColor()

    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()

    # Draw tab background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let sdlBgColor = bgColor.toSDLColor()
    discard SDL_SetRenderDrawColor(backend.renderer, sdlBgColor.r, sdlBgColor.g, sdlBgColor.b, sdlBgColor.a)
    discard SDL_RenderFillRect(backend.renderer, addr rect)

    # Draw tab border
    let borderColor = elem.getProp("borderColor")
    if borderColor.isSome:
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
      let sdlBorderColor = borderColor.get.getColor().toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
      discard SDL_RenderDrawRect(backend.renderer, addr rect)

    # Center text in tab
    if title.len > 0:
      let textMetrics = backend.measureText(title, fontSize)
      let textX = elem.x + (elem.width - textMetrics.w.float) / 2.0
      let textY = elem.y + (elem.height - textMetrics.h.float) / 2.0
      backend.renderText(title, textX, textY, fontSize, textColor.toSDLColor())

    # Render children
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTabContent:
    var selectedIndex = 0
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup:
        selectedIndex = parent.tabSelectedIndex
        break
      parent = parent.parent

    var allTabPanels: seq[Element] = @[]

    for child in elem.children:
      if child.kind == ekTabPanel:
        allTabPanels.add(child)
      elif child.kind == ekForLoop:
        for grandchild in child.children:
          if grandchild.kind == ekTabPanel:
            allTabPanels.add(grandchild)

    for panel in allTabPanels:
      if panel.tabIndex == selectedIndex:
        backend.renderElement(panel, inheritedColor)
        break

  of ekTabPanel:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  else:
    discard

  # Render children for other elements
  if elem.kind != ekColumn and elem.kind != ekRow and elem.kind != ekCenter and elem.kind != ekBody and
     elem.kind != ekTabGroup and elem.kind != ekTabBar and elem.kind != ekTabContent and elem.kind != ekTabPanel:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

proc renderDropdownMenus*(backend: var SDL2Backend, elem: Element) =
  ## Render all open dropdown menus on top of everything else

  case elem.kind:
  of ekDropdown:
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let options = elem.dropdownOptions
      let selectedIndex = elem.dropdownSelectedIndex
      let hoveredIndex = elem.dropdownHoveredIndex

      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
      let textColor = elem.getProp("color").get(val("#000000")).getColor()
      let borderColor = elem.getProp("borderColor").get(val("#CCCCCC")).getColor()
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()

      let itemHeight = fontSize.float + 10.0
      let textPadding = 10.0

      let dropdownWidth = elem.width
      let dropdownHeight = min(options.len.float * itemHeight, 200.0)
      let dropdownX = elem.x
      let dropdownY = elem.y + elem.height

      # Draw dropdown background
      let dropdownRect = rrect(dropdownX, dropdownY, dropdownWidth, dropdownHeight)
      let sdlBgColor = bgColor.getColor().toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBgColor.r, sdlBgColor.g, sdlBgColor.b, sdlBgColor.a)
      discard SDL_RenderFillRect(backend.renderer, addr dropdownRect)

      let sdlBorderColor = borderColor.toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, sdlBorderColor.r, sdlBorderColor.g, sdlBorderColor.b, sdlBorderColor.a)
      discard SDL_RenderDrawRect(backend.renderer, addr dropdownRect)

      # Draw visible options
      let maxVisibleItems = int(dropdownHeight / itemHeight)
      let endIndex = min(options.len, maxVisibleItems)

      for i in 0..<endIndex:
        let optionY = dropdownY + i.float * itemHeight
        let optionRect = rrect(dropdownX, optionY, dropdownWidth, itemHeight)

        # Highlight hovered option
        if i == hoveredIndex:
          let hoverColor = parseColor("#E3F2FD").toSDLColor()
          discard SDL_SetRenderDrawColor(backend.renderer, hoverColor.r, hoverColor.g, hoverColor.b, hoverColor.a)
          discard SDL_RenderFillRect(backend.renderer, addr optionRect)

        # Highlight selected option
        if i == selectedIndex:
          let selectedColor = parseColor("#BBDEFB").toSDLColor()
          discard SDL_SetRenderDrawColor(backend.renderer, selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a)
          discard SDL_RenderFillRect(backend.renderer, addr optionRect)

        # Draw option text
        let optionText = options[i]
        let optionTextX = dropdownX + textPadding
        let optionTextY = optionY + (itemHeight - fontSize.float) / 2.0
        let optionMaxWidth = dropdownWidth - (textPadding * 2.0)

        let clipRect = rrect(optionTextX, optionTextY, optionMaxWidth, fontSize.float)
        discard SDL_RenderSetClipRect(backend.renderer, addr clipRect)
        backend.renderText(optionText, optionTextX, optionTextY, fontSize, textColor.toSDLColor())
        discard SDL_RenderSetClipRect(backend.renderer, nil)

  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        backend.renderDropdownMenus(activeBranch)

  else:
    for child in elem.children:
      backend.renderDropdownMenus(child)

# ============================================================================
# Input Handling
# ============================================================================

proc sortChildrenByZIndexReverse(children: seq[Element]): seq[Element] =
  ## Sort children by z-index in reverse order for input handling
  result = children
  result.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(bZIndex, aZIndex)
  )

proc closeOtherDropdowns*(backend: var SDL2Backend, keepOpen: Element) =
  ## Close all dropdowns except the specified one
  proc closeDropdownsRecursive(elem: Element) =
    case elem.kind:
    of ekDropdown:
      if elem != keepOpen:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
    else:
      for child in elem.children:
        closeDropdownsRecursive(child)

  closeDropdownsRecursive(backend.rootElement)

proc getMousePosition*(): tuple[x, y: float] =
  ## Get current mouse position
  var mouseX, mouseY: cint
  let _ = SDL_GetMouseState(addr mouseX, addr mouseY)
  (x: mouseX.float, y: mouseY.float)

proc checkHoverCursor*(elem: Element): bool =
  ## Check if mouse is hovering over any interactive element

  let mousePos = getMousePosition()

  case elem.kind:
  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        if checkHoverCursor(activeBranch):
          return true

  of ekButton, ekTab:
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)
    var contains = false
    # Simple point-in-rect check
    if mousePoint.x >= rect.x and mousePoint.x < rect.x + rect.w and
       mousePoint.y >= rect.y and mousePoint.y < rect.y + rect.h:
      contains = true
    if contains:
      return true

  of ekDropdown:
    let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)
    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)

    var containsMain = false
    if mousePoint.x >= mainRect.x and mousePoint.x < mainRect.x + mainRect.w and
       mousePoint.y >= mainRect.y and mousePoint.y < mainRect.y + mainRect.h:
      containsMain = true

    if containsMain:
      return true

    # Check dropdown options
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

      var containsDropdown = false
      if mousePoint.x >= dropdownRect.x and mousePoint.x < dropdownRect.x + dropdownRect.w and
         mousePoint.y >= dropdownRect.y and mousePoint.y < dropdownRect.y + dropdownRect.h:
        containsDropdown = true

      if containsDropdown:
        return true

  of ekCheckbox:
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

    let label = elem.getProp("label").get(val("")).getString()
    var hoverArea = checkboxRect

    if label.len > 0:
      let textWidth = approximateTextWidth(label, fontSize)  # Approximation
      hoverArea.w = (checkboxSize + 10.0 + textWidth.float).cint
      hoverArea.y = elem.y.cint
      hoverArea.h = elem.height.cint

    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)
    if mousePoint.x >= hoverArea.x and mousePoint.x < hoverArea.x + hoverArea.w and
       mousePoint.y >= hoverArea.y and mousePoint.y < hoverArea.y + hoverArea.h:
      return true

  else:
    discard

  # Check children recursively
  for child in elem.children:
    if checkHoverCursor(child):
      return true

  return false

proc handleInput*(backend: var SDL2Backend, elem: Element) =
  ## Handle mouse input for interactive elements

  case elem.kind:
  of ekHeader:
    discard

  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        backend.handleInput(activeBranch)

  of ekBody:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  of ekButton:
    let mousePos = getMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)

    if mousePoint.x >= rect.x and mousePoint.x < rect.x + rect.w and
       mousePoint.y >= rect.y and mousePoint.y < rect.y + rect.h:
      # Button was clicked - trigger onClick handler
      if elem.eventHandlers.hasKey("onClick"):
        elem.eventHandlers["onClick"]()

  of ekInput:
    let mousePos = getMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)

    if mousePoint.x >= rect.x and mousePoint.x < rect.x + rect.w and
       mousePoint.y >= rect.y and mousePoint.y < rect.y + rect.h:
      # Input was clicked - focus it
      backend.focusedInput = elem
      backend.cursorBlink = 0.0
      backend.textInputEnabled = true

      if not backend.inputValues.hasKey(elem):
        let initialValue = elem.getProp("value").get(val("")).getString()
        backend.inputValues[elem] = initialValue
    else:
      # Clicked outside - unfocus if this was focused
      if backend.focusedInput == elem:
        backend.focusedInput = nil
        backend.textInputEnabled = false

  of ekCheckbox:
    let mousePos = getMousePosition()
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

    let label = elem.getProp("label").get(val("")).getString()
    var clickArea = checkboxRect

    if label.len > 0:
      let textWidth = approximateTextWidth(label, fontSize)  # Approximation
      clickArea.w = (checkboxSize + 10.0 + textWidth.float).cint
      clickArea.y = elem.y.cint
      clickArea.h = elem.height.cint

    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)
    if mousePoint.x >= clickArea.x and mousePoint.x < clickArea.x + clickArea.w and
       mousePoint.y >= clickArea.y and mousePoint.y < clickArea.y + clickArea.h:
      # Checkbox was clicked - toggle state
      var currentState = backend.checkboxStates.getOrDefault(elem, false)
      currentState = not currentState
      backend.checkboxStates[elem] = currentState

      if elem.eventHandlers.hasKey("onClick"):
        elem.eventHandlers["onClick"]()

      if elem.eventHandlers.hasKey("onChange"):
        let handler = elem.eventHandlers["onChange"]
        handler($currentState)

  of ekTabGroup, ekTabBar, ekTabContent:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      if child.kind == ekConditional:
        backend.handleInput(child)
      elif child.kind == ekForLoop:
        for grandchild in child.children:
          backend.handleInput(grandchild)
      else:
        backend.handleInput(child)

  of ekTab:
    let mousePos = getMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)

    if mousePoint.x >= rect.x and mousePoint.x < rect.x + rect.w and
       mousePoint.y >= rect.y and mousePoint.y < rect.y + rect.h:
      # Tab was clicked
      var parent = elem.parent
      var tabGroup: Element = nil

      while parent != nil:
        if parent.kind == ekTabGroup:
          tabGroup = parent
          break
        parent = parent.parent

      if tabGroup != nil:
        let oldSelectedIndex = tabGroup.tabSelectedIndex
        tabGroup.tabSelectedIndex = elem.tabIndex

        if oldSelectedIndex != tabGroup.tabSelectedIndex:
          if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
            tabGroup.eventHandlers["onSelectedIndexChanged"]()

          invalidateRelatedValues("tabSelectedIndex")
          markDirty(tabGroup)
          for child in tabGroup.children:
            markDirty(child)
            if child.kind == ekTabContent:
              for panel in child.children:
                markDirty(panel)
                markAllDescendantsDirty(panel)

      if elem.eventHandlers.hasKey("onClick"):
        elem.eventHandlers["onClick"]()

  of ekDropdown:
    let mousePos = getMousePosition()
    let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)
    let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)

    if mousePoint.x >= mainRect.x and mousePoint.x < mainRect.x + mainRect.w and
       mousePoint.y >= mainRect.y and mousePoint.y < mainRect.y + mainRect.h:
      # Main dropdown button was clicked
      if elem.dropdownIsOpen:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
        backend.focusedDropdown = nil
      else:
        elem.dropdownIsOpen = true
        elem.dropdownHoveredIndex = elem.dropdownSelectedIndex
        backend.focusedDropdown = elem
        backend.closeOtherDropdowns(elem)

    elif elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

      if mousePoint.x >= dropdownRect.x and mousePoint.x < dropdownRect.x + dropdownRect.w and
         mousePoint.y >= dropdownRect.y and mousePoint.y < dropdownRect.y + dropdownRect.h:
        # Calculate which option was clicked
        let relativeY = mousePos.y - dropdownRect.y.float
        let clickedIndex = int(relativeY / itemHeight)

        if clickedIndex >= 0 and clickedIndex < elem.dropdownOptions.len:
          elem.dropdownSelectedIndex = clickedIndex
          elem.dropdownIsOpen = false
          elem.dropdownHoveredIndex = -1
          backend.focusedDropdown = nil

          if elem.eventHandlers.hasKey("onChange"):
            let handler = elem.eventHandlers["onChange"]
            handler(elem.dropdownOptions[clickedIndex])

          if elem.eventHandlers.hasKey("onSelectionChange"):
            let handler = elem.eventHandlers["onSelectionChange"]
            handler(elem.dropdownOptions[clickedIndex])
      else:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
        backend.focusedDropdown = nil
    else:
      if backend.focusedDropdown == elem:
        backend.focusedDropdown = nil

    # Handle hover state for dropdown options
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

      if mousePoint.x >= dropdownRect.x and mousePoint.x < dropdownRect.x + dropdownRect.w and
         mousePoint.y >= dropdownRect.y and mousePoint.y < dropdownRect.y + dropdownRect.h:
        let relativeY = mousePos.y - dropdownRect.y.float
        let hoveredIndex = int(relativeY / itemHeight)

        if hoveredIndex >= 0 and hoveredIndex < elem.dropdownOptions.len:
          elem.dropdownHoveredIndex = hoveredIndex
        else:
          elem.dropdownHoveredIndex = -1
      else:
        elem.dropdownHoveredIndex = -1

  of ekColumn, ekRow, ekCenter, ekContainer:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  else:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

proc handleKeyboardInput*(backend: var SDL2Backend, root: Element) =
  ## Handle keyboard input for focused input and dropdown elements

  # Handle checkbox keyboard input
  let mousePos = getMousePosition()
  proc findCheckboxUnderMouse(elem: Element): Element =
    if elem.kind == ekCheckbox:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let checkboxSize = min(elem.height, fontSize.float + 8.0)
      let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

      let label = elem.getProp("label").get(val("")).getString()
      var clickArea = checkboxRect

      if label.len > 0:
        let textWidth = approximateTextWidth(label, fontSize)  # Approximation
        clickArea.w = (checkboxSize + 10.0 + textWidth.float).cint
        clickArea.y = elem.y.cint
        clickArea.h = elem.height.cint

      let mousePoint = SDL_Point(x: mousePos.x.cint, y: mousePos.y.cint)
      if mousePoint.x >= clickArea.x and mousePoint.x < clickArea.x + clickArea.w and
         mousePoint.y >= clickArea.y and mousePoint.y < clickArea.y + clickArea.h:
        return elem

    for child in elem.children:
      let found = findCheckboxUnderMouse(child)
      if found != nil:
        return found

    return nil

  let hoveredCheckbox = findCheckboxUnderMouse(root)
  if hoveredCheckbox != nil and backend.keyStates[KEY_ENTER]:
    var currentState = backend.checkboxStates.getOrDefault(hoveredCheckbox, false)
    currentState = not currentState
    backend.checkboxStates[hoveredCheckbox] = currentState

    if hoveredCheckbox.eventHandlers.hasKey("onClick"):
      hoveredCheckbox.eventHandlers["onClick"]()

    if hoveredCheckbox.eventHandlers.hasKey("onChange"):
      let handler = hoveredCheckbox.eventHandlers["onChange"]
      handler($currentState)

  # Handle dropdown keyboard input
  if backend.focusedDropdown != nil:
    let dropdown = backend.focusedDropdown

    if backend.keyStates[KEY_ESCAPE]:
      dropdown.dropdownIsOpen = false
      dropdown.dropdownHoveredIndex = -1
      backend.focusedDropdown = nil

    elif dropdown.dropdownIsOpen and dropdown.dropdownOptions.len > 0:
      if backend.keyStates[KEY_UP]:
        dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex <= 0:
          dropdown.dropdownOptions.len - 1
        else:
          dropdown.dropdownHoveredIndex - 1

      elif backend.keyStates[KEY_DOWN]:
        dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex >= dropdown.dropdownOptions.len - 1:
          0
        else:
          dropdown.dropdownHoveredIndex + 1

      elif backend.keyStates[KEY_ENTER]:
        if dropdown.dropdownHoveredIndex >= 0 and dropdown.dropdownHoveredIndex < dropdown.dropdownOptions.len:
          dropdown.dropdownSelectedIndex = dropdown.dropdownHoveredIndex
          dropdown.dropdownIsOpen = false
          dropdown.dropdownHoveredIndex = -1
          backend.focusedDropdown = nil

          if dropdown.eventHandlers.hasKey("onChange"):
            let handler = dropdown.eventHandlers["onChange"]
            handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])

          if dropdown.eventHandlers.hasKey("onSelectionChange"):
            let handler = dropdown.eventHandlers["onSelectionChange"]
            handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])

      elif backend.keyStates[KEY_TAB]:
        dropdown.dropdownIsOpen = false
        dropdown.dropdownHoveredIndex = -1
        backend.focusedDropdown = nil

  # Handle input field keyboard input
  if backend.focusedInput == nil:
    backend.backspaceHoldTimer = 0.0
    return

  var currentValue = backend.inputValues.getOrDefault(backend.focusedInput, "")
  var textChanged = false

  # Handle backspace with repeat logic
  if backend.keyStates[KEY_BACKSPACE] and currentValue.len > 0:
    if backend.keyStates[KEY_BACKSPACE]:  # First press
      currentValue.setLen(currentValue.len - 1)
      textChanged = true
      backend.backspaceHoldTimer = 0.0
    else:
      # Key is being held down
      backend.backspaceHoldTimer += 1.0 / 60.0

      if backend.backspaceHoldTimer >= backend.backspaceRepeatDelay:
        let holdBeyondDelay = backend.backspaceHoldTimer - backend.backspaceRepeatDelay
        let charsToDelete = min(int(holdBeyondDelay / backend.backspaceRepeatRate), currentValue.len)

        if charsToDelete > 0:
          currentValue.setLen(currentValue.len - charsToDelete)
          textChanged = true
          backend.backspaceHoldTimer = backend.backspaceRepeatDelay + (charsToDelete.float * backend.backspaceRepeatRate)
  else:
    backend.backspaceHoldTimer = 0.0

  # Handle other special keys
  if backend.keyStates[KEY_ENTER]:
    if backend.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value if changed
  if textChanged:
    backend.inputValues[backend.focusedInput] = currentValue

  if textChanged and backend.focusedInput.eventHandlers.hasKey("onChange"):
    let handler = backend.focusedInput.eventHandlers["onChange"]
    handler(currentValue)

  if textChanged and backend.focusedInput.eventHandlers.hasKey("onValueChange"):
    let handler = backend.focusedInput.eventHandlers["onValueChange"]
    handler(currentValue)

# ============================================================================
# Main Loop
# ============================================================================

proc run*(backend: var SDL2Backend, root: Element) =
  ## Run the application with the given root element

  backend.rootElement = root
  backend.running = true

  # Initialize SDL2
  if SDL_Init(SDL_INIT_VIDEO) != 0:
    echo "Failed to initialize SDL2"
    return

  # Initialize SDL_ttf
  if TTF_Init() != 0:
    echo "Failed to initialize SDL_ttf"
    SDL_Quit()
    return

  # Create window
  backend.window = SDL_CreateWindow(
    backend.windowTitle.cstring,
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    backend.windowWidth.cint, backend.windowHeight.cint,
    SDL_WINDOW_SHOWN
  )

  if backend.window == nil:
    echo "Failed to create SDL2 window"
    TTF_Quit()
    SDL_Quit()
    return

  # Create renderer
  backend.renderer = SDL_CreateRenderer(
    backend.window,
    -1,
    SDL_RENDERER_ACCELERATED or SDL_RENDERER_PRESENTVSYNC
  )

  if backend.renderer == nil:
    echo "Failed to create SDL2 renderer"
    SDL_DestroyWindow(backend.window)
    TTF_Quit()
    SDL_Quit()
    return

  # Attempt to load font declared via fontFamily/resources
  var fontName = resolvePreferredFont(root)
  var fontPath = ""

  if fontName.len > 0:
    fontPath = findFontByName(fontName)
    if fontPath.len > 0:
      echo "Loading font: " & fontPath
      backend.font = TTF_OpenFont(fontPath.cstring, DEFAULT_FONT_SIZE.cint)
      if backend.font != nil:
        echo "Successfully loaded font family: " & fontName
      else:
        echo "Warning: Failed to load font family '" & fontName & "'"

  # Load default configured font if no resource font was loaded
  if backend.font == nil:
    let fontInfo = getDefaultFontInfo()
    if fontInfo.path.len > 0:
      echo "Loading default font: " & fontInfo.path
      backend.font = TTF_OpenFont(fontInfo.path.cstring, fontInfo.size.cint)
      if backend.font != nil:
        echo "Successfully loaded " & fontInfo.name & " (" & fontInfo.format & ")"
      else:
        echo "Warning: Failed to load default font, trying fallbacks"

  # Fallback to system fonts if no custom font was loaded
  if backend.font == nil:
    for fallbackPath in FALLBACK_FONTS:
      backend.font = TTF_OpenFont(fallbackPath.cstring, DEFAULT_FONT_SIZE.cint)
      if backend.font != nil:
        echo "Loaded fallback font: " & fallbackPath
        break

  if backend.font == nil:
    echo "Warning: Could not load any font, text rendering may not work"
    # Continue without font - basic rectangles will still work

  # Calculate initial layout
  calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

  # Initialize cursor
  var defaultCursor = SDL_CreateSystemCursor(MOUSE_CURSOR_DEFAULT)
  var handCursor = SDL_CreateSystemCursor(MOUSE_CURSOR_POINTING_HAND)
  var currentCursor = defaultCursor

  # Main game loop
  var running = true
  var event: SDL_Event

  while running:
    # Handle events
    while SDL_PollEvent(addr event) != 0:
      let eventType = getEventType(addr event)

      case eventType:
      of SDL_EVENT_QUIT:
        running = false

      of SDL_EVENT_KEYDOWN:
        let keyEvent = getKeyEvent(addr event)
        if keyEvent.keysym.scancode < 512:
          backend.keyStates[keyEvent.keysym.scancode] = true

      of SDL_EVENT_KEYUP:
        let keyEvent = getKeyEvent(addr event)
        if keyEvent.keysym.scancode < 512:
          backend.keyStates[keyEvent.keysym.scancode] = false

      of SDL_EVENT_MOUSEBUTTONDOWN:
        let buttonEvent = getMouseButtonEvent(addr event)
        if buttonEvent.button == SDL_BUTTON_LEFT:
          backend.handleInput(root)

      of SDL_EVENT_MOUSEMOTION:
        # Update cursor based on hover state
        if checkHoverCursor(root):
          if currentCursor != handCursor:
            SDL_SetCursor(handCursor)
            currentCursor = handCursor
        else:
          if currentCursor != defaultCursor:
            SDL_SetCursor(defaultCursor)
            currentCursor = defaultCursor

      of SDL_EVENT_TEXTINPUT:
        if backend.textInputEnabled and backend.focusedInput != nil:
          let textEvent = getTextInputEvent(addr event)
          let text = $textEvent.text
          var currentValue = backend.inputValues.getOrDefault(backend.focusedInput, "")
          currentValue.add(text)
          backend.inputValues[backend.focusedInput] = currentValue

          if backend.focusedInput.eventHandlers.hasKey("onChange"):
            let handler = backend.focusedInput.eventHandlers["onChange"]
            handler(currentValue)

          if backend.focusedInput.eventHandlers.hasKey("onValueChange"):
            let handler = backend.focusedInput.eventHandlers["onValueChange"]
            handler(currentValue)

      else:
        discard

    # Update cursor blink timer
    backend.cursorBlink += 1.0 / 60.0
    if backend.cursorBlink >= 1.0:
      backend.cursorBlink = 0.0

    # Handle keyboard input
    backend.handleKeyboardInput(root)

    # Only recalculate layout when there are dirty elements
    if hasDirtyElements():
      calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

    # Render
    # Clear background
    var actualBody: Element = nil
    for child in root.children:
      if child.kind == ekBody:
        actualBody = child
        break

    var bodyBg: Option[Value] = none(Value)
    if actualBody != nil:
      bodyBg = actualBody.getProp("backgroundColor")
      if bodyBg.isNone:
        bodyBg = actualBody.getProp("background")

    if bodyBg.isSome:
      let bgColor = bodyBg.get.getColor().toSDLColor()
      discard SDL_SetRenderDrawColor(backend.renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a)
    else:
      discard SDL_SetRenderDrawColor(backend.renderer, backend.backgroundColor.r, backend.backgroundColor.g, backend.backgroundColor.b, backend.backgroundColor.a)

    discard SDL_RenderClear(backend.renderer)

    # Render all elements
    backend.renderElement(root)

    # Render dropdown menus on top
    backend.renderDropdownMenus(root)

    SDL_RenderPresent(backend.renderer)

  # Cleanup
  if backend.font != nil:
    TTF_CloseFont(backend.font)
  if defaultCursor != nil:
    SDL_FreeCursor(defaultCursor)
  if handCursor != nil:
    SDL_FreeCursor(handCursor)
  if backend.renderer != nil:
    SDL_DestroyRenderer(backend.renderer)
  if backend.window != nil:
    SDL_DestroyWindow(backend.window)
  TTF_Quit()
  SDL_Quit()
  backend.running = false
