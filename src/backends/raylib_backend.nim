## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../kryon/core
import options, tables, algorithm
import raylib_ffi

# ============================================================================
# Backend Type
# ============================================================================

type
  RaylibBackend* = object
    windowWidth*: int
    windowHeight*: int
    windowTitle*: string
    backgroundColor*: Color
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

proc newRaylibBackend*(width, height: int, title: string): RaylibBackend =
  ## Create a new Raylib backend
  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    running: false,
    focusedDropdown: nil,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05    # 50ms repeat rate
  )

proc newRaylibBackendFromApp*(app: Element): RaylibBackend =
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

  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: if bgColor.isSome: bgColor.get.getColor() else: rgba(30, 30, 30, 255),
    running: false,
    focusedDropdown: nil,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05    # 50ms repeat rate
  )

# ============================================================================
# Color Conversion
# ============================================================================

proc toRaylibColor*(c: Color): RColor {.inline.} =
  ## Convert Kryon Color to Raylib RColor
  rcolor(c.r, c.g, c.b, c.a)

# ============================================================================
# Layout Engine
# ============================================================================

proc calculateLayout*(elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Recursively calculate layout for all elements
  ## Normal layout calculation - reactivity is handled by dirty flags triggering recalculation

  # Set current element for dependency tracking
  let previousElement = currentElementBeingProcessed
  currentElementBeingProcessed = elem

  # Check for absolute positioning (posX, posY)
  let posXOpt = elem.getProp("posX")
  let posYOpt = elem.getProp("posY")

  # Get element dimensions
  let widthOpt = elem.getProp("width")
  let heightOpt = elem.getProp("height")

  # Set position (use posX/posY if provided, otherwise use x/y)
  elem.x = if posXOpt.isSome: posXOpt.get.getFloat() else: x
  elem.y = if posYOpt.isSome: posYOpt.get.getFloat() else: y

  # Special handling for Text elements - measure actual text size
  if elem.kind == ekText:
    let text = elem.getProp("text").get(val("")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Measure text dimensions
    let textWidth = MeasureText(text.cstring, fontSize.cint).float
    let textHeight = fontSize.float

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: textWidth
    elem.height = if heightOpt.isSome: heightOpt.get.getFloat() else: textHeight

  elif elem.kind == ekButton:
    # Special handling for Button elements - size based on text content with padding
    let text = elem.getProp("text").get(val("Button")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Measure text dimensions and add padding
    let textWidth = MeasureText(text.cstring, fontSize.cint).float
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

  # Layout children based on container type
  # Children should be positioned relative to THIS element's calculated position
  case elem.kind:
  of ekHeader:
    # Header is metadata only - set size to 0 so it doesn't take up space
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

      # Layout the active branch at the same position as the conditional element
      # Use the parent dimensions that were passed to this element
      if activeBranch != nil:
        calculateLayout(activeBranch, elem.x, elem.y, parentWidth, parentHeight)

  of ekForLoop:
    # For loop elements generate dynamic content and layout their children
    if elem.forIterable != nil and elem.forBodyTemplate != nil:
      let items = elem.forIterable()
      echo "🔥 ForLoop layout: got " & $items.len & " items: " & $items

      # Clear previous children (if any)
      elem.children.setLen(0)

      # Generate elements for each item in the iterable
      var currentY = elem.y
      let gap = 5.0  # Gap between items

      for item in items:
        echo "🔥 Creating element for item: '" & item & "'"
        let childElement = elem.forBodyTemplate(item)
        if childElement != nil:
          elem.children.add(childElement)
          # Layout the child element
          calculateLayout(childElement, elem.x, currentY, elem.width, 0)
          currentY += childElement.height + gap
        else:
          echo "🔥 Template returned nil for item: '" & item & "'"

      # Set for loop element size based on its children
      elem.width = parentWidth
      elem.height = currentY - elem.y - gap
      echo "🔥 ForLoop complete: " & $elem.children.len & " children created"

  of ekBody:
    # Body implements natural document flow - stack children vertically
    var currentY = elem.y
    let gap = elem.getProp("gap").get(val(5)).getFloat()  # Default gap between elements

    for child in elem.children:
      calculateLayout(child, elem.x, currentY, elem.width, elem.height)
      currentY += child.height + gap

  of ekColumn:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # For Column: mainAxis = vertical (Y), crossAxis = horizontal (X)

    # First pass: Calculate all children sizes (give them full width but not full height)
    var childSizes: seq[tuple[w, h: float]] = @[]
    for child in elem.children:
      # Pass elem.width for width, but don't constrain height
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

    # Column uses parent size by default (unless explicitly sized)
    # This allows mainAxisAlignment/crossAxisAlignment to center children in available space
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
    var dynamicGap = gap  # This might be modified by space distribution modes

    if mainAxisAlignment.isSome:
      let align = mainAxisAlignment.get.getString()
      case align:
      of "start", "flex-start":
        # Default: start from the top
        currentY = elem.y
      of "center":
        # Center children vertically
        currentY = elem.y + (elem.height - totalHeight) / 2.0
      of "end", "flex-end":
        # Align children to the bottom
        currentY = elem.y + elem.height - totalHeight
      of "spaceEvenly":
        # Distribute children evenly with equal space before, between, and after
        # Total available space minus all child heights
        var totalChildHeight = 0.0
        for size in childSizes:
          totalChildHeight += size.h
        let availableSpace = elem.height - totalChildHeight
        let spaceUnit = availableSpace / (childSizes.len + 1).float
        dynamicGap = spaceUnit
        currentY = elem.y + spaceUnit
      of "spaceAround":
        # Distribute children with equal space around each child (half space at edges)
        var totalChildHeight = 0.0
        for size in childSizes:
          totalChildHeight += size.h
        let availableSpace = elem.height - totalChildHeight
        let spaceUnit = availableSpace / childSizes.len.float
        dynamicGap = spaceUnit
        currentY = elem.y + (spaceUnit / 2.0)
      of "spaceBetween":
        # Distribute children with space only between them
        if childSizes.len > 1:
          var totalChildHeight = 0.0
          for size in childSizes:
            totalChildHeight += size.h
          let availableSpace = elem.height - totalChildHeight
          let spaceUnit = availableSpace / (childSizes.len - 1).float
          dynamicGap = spaceUnit
        currentY = elem.y
      else:
        # Default to start
        currentY = elem.y

    # Second pass: Position children
    for i, child in elem.children:
      if i > 0:
        currentY += dynamicGap

      # Determine X position based on crossAxisAlignment
      var childX = elem.x
      if crossAxisAlignment.isSome:
        let align = crossAxisAlignment.get.getString()
        if align == "center":
          childX = elem.x + (elem.width - childSizes[i].w) / 2.0
        elif align == "end":
          childX = elem.x + elem.width - childSizes[i].w

      # Layout child at final position
      calculateLayout(child, childX, currentY, elem.width, 0)
      currentY += childSizes[i].h

  of ekRow:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # For Row: mainAxis = horizontal (X), crossAxis = vertical (Y)

    # First pass: Calculate all children sizes (give them full height but not full width)
    var childSizes: seq[tuple[w, h: float]] = @[]
    for child in elem.children:
      # Pass elem.height for height, but don't constrain width
      calculateLayout(child, 0, 0, 0, elem.height)
      childSizes.add((w: child.width, h: child.height))

    # Calculate total width and max height
    var totalWidth = 0.0
    var maxHeight = 0.0
    for size in childSizes:
      totalWidth += size.w
      maxHeight = max(maxHeight, size.h)
    if elem.children.len > 1:
      totalWidth += gap * (elem.children.len - 1).float

    # Row uses parent size by default (unless explicitly sized)
    # This allows mainAxisAlignment/crossAxisAlignment to center children in available space
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
    var dynamicGap = gap  # This might be modified by space distribution modes

    if mainAxisAlignment.isSome:
      let align = mainAxisAlignment.get.getString()
      case align:
      of "start", "flex-start":
        # Default: start from the left
        currentX = elem.x
      of "center":
        # Center children horizontally
        currentX = elem.x + (elem.width - totalWidth) / 2.0
      of "end", "flex-end":
        # Align children to the right
        currentX = elem.x + elem.width - totalWidth
      of "spaceEvenly":
        # Distribute children evenly with equal space before, between, and after
        # Total available space minus all child widths
        var totalChildWidth = 0.0
        for size in childSizes:
          totalChildWidth += size.w
        let availableSpace = elem.width - totalChildWidth
        let spaceUnit = availableSpace / (childSizes.len + 1).float
        dynamicGap = spaceUnit
        currentX = elem.x + spaceUnit
      of "spaceAround":
        # Distribute children with equal space around each child (half space at edges)
        var totalChildWidth = 0.0
        for size in childSizes:
          totalChildWidth += size.w
        let availableSpace = elem.width - totalChildWidth
        let spaceUnit = availableSpace / childSizes.len.float
        dynamicGap = spaceUnit
        currentX = elem.x + (spaceUnit / 2.0)
      of "spaceBetween":
        # Distribute children with space only between them
        if childSizes.len > 1:
          var totalChildWidth = 0.0
          for size in childSizes:
            totalChildWidth += size.w
          let availableSpace = elem.width - totalChildWidth
          let spaceUnit = availableSpace / (childSizes.len - 1).float
          dynamicGap = spaceUnit
        currentX = elem.x
      else:
        # Default to start
        currentX = elem.x

    # Second pass: Position children
    for i, child in elem.children:
      if i > 0:
        currentX += dynamicGap

      # Determine Y position based on crossAxisAlignment
      var childY = elem.y
      if crossAxisAlignment.isSome:
        let align = crossAxisAlignment.get.getString()
        if align == "center":
          childY = elem.y + (elem.height - childSizes[i].h) / 2.0
        elif align == "end":
          childY = elem.y + elem.height - childSizes[i].h

      # Layout child at final position
      calculateLayout(child, currentX, childY, 0, elem.height)
      currentX += childSizes[i].w

  of ekCenter:
    # Center multiple children as a group - needs multi-pass layout
    if elem.children.len > 0:
      # First pass: Calculate all children sizes at (0, 0) to determine dimensions
      var totalHeight = 0.0
      var maxWidth = 0.0
      var childSizes: seq[tuple[w, h: float]] = @[]

      for child in elem.children:
        calculateLayout(child, 0, 0, elem.width, elem.height)
        childSizes.add((w: child.width, h: child.height))
        totalHeight += child.height
        maxWidth = max(maxWidth, child.width)

      # Calculate starting Y position to center the group vertically
      let startY = elem.y + (elem.height - totalHeight) / 2.0

      # Second pass: Position children centered horizontally and stacked vertically
      var currentY = startY
      let gap = elem.getProp("gap").get(val(10)).getFloat()  # Default gap of 10

      for i, child in elem.children:
        let centerX = elem.x + (elem.width - childSizes[i].w) / 2.0

        # Apply gap between elements (but not after the last one)
        if i > 0:
          currentY += gap

        calculateLayout(child, centerX, currentY, childSizes[i].w, childSizes[i].h)
        currentY += childSizes[i].h

  of ekContainer:
    # Container is a normal wrapper - relative positioning by default
    var currentY = elem.y
    let gap = elem.getProp("gap").get(val(0)).getFloat()  # No gap by default

    # Check for contentAlignment property
    let alignment = elem.getProp("contentAlignment")
    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout
      for child in elem.children:
        # First pass: Calculate child layout at (0, 0) to determine its size
        calculateLayout(child, 0, 0, elem.width, elem.height)

        # Second pass: Now we know child.width and child.height, so center it
        let centerX = elem.x + (elem.width - child.width) / 2.0
        let centerY = elem.y + (elem.height - child.height) / 2.0

        # Recalculate at centered position
        calculateLayout(child, centerX, centerY, child.width, child.height)
    else:
      # Default: normal relative positioning layout
      for child in elem.children:
        calculateLayout(child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

  else:
    # Default: just layout children in same space as parent
    for child in elem.children:
      calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

  # Restore previous element and mark this element as clean
  currentElementBeingProcessed = previousElement
  markClean(elem)

# ============================================================================
# Rendering with Real Raylib
# ============================================================================

proc sortChildrenByZIndex(children: seq[Element]): seq[Element] =
  ## Sort children by z-index for correct layered rendering
  ## Elements with lower z-index are rendered first (behind)
  ## Elements with higher z-index are rendered last (on top)
  result = children
  result.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(aZIndex, bZIndex)
  )

proc renderElement*(backend: var RaylibBackend, elem: Element, inheritedColor: Option[Color] = none(Color)) =
  ## Render an element using Raylib
  ## inheritedColor: color to use for text if not explicitly set

  case elem.kind:
  of ekHeader:
    # Header is metadata only - don't render it
    discard

  of ekConditional:
    # Conditional elements don't render themselves, only their active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch


      # Render the active branch if it exists
      if activeBranch != nil:
        backend.renderElement(activeBranch, inheritedColor)

  of ekForLoop:
    # For loop elements don't render themselves, only their generated children
    # The actual rendering and layout is handled in the layout calculation
    # So this just needs to render the children
    
    for child in elem.children:
      backend.renderElement(child, inheritedColor)

  of ekBody:
    # Body is a wrapper - render its children with inherited color (sorted by z-index)
    let bodyColor = elem.getProp("color")
    let colorToInherit = if bodyColor.isSome:
      some(bodyColor.get.getColor())
    else:
      inheritedColor

    # Sort children by z-index before rendering
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, colorToInherit)

  of ekContainer:
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)

    # Draw background
    let bgColor = elem.getProp("backgroundColor")
    if bgColor.isSome:
      DrawRectangleRec(rect, bgColor.get.getColor().toRaylibColor())

    # Draw border
    let borderColor = elem.getProp("borderColor")
    if borderColor.isSome:
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
      DrawRectangleLinesEx(rect, borderWidth, borderColor.get.getColor().toRaylibColor())

  of ekText:
    let text = elem.getProp("text").get(val("")).getString()
    # Use inherited color if no explicit color is set
    let explicitColor = elem.getProp("color")
    let color = if explicitColor.isSome:
      explicitColor.get.getColor()
    elif inheritedColor.isSome:
      inheritedColor.get
    else:
      parseColor("#FFFFFF")  # Default to white
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    DrawText(text.cstring, elem.x.cint, elem.y.cint, fontSize.cint, color.toRaylibColor())

  of ekButton:
    let text = elem.getProp("text").get(val("Button")).getString()
    let bgColor = elem.getProp("backgroundColor").get(val("#4A90E2"))
    let textColor = elem.getProp("color").get(val("#FFFFFF")).getColor()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Draw button background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    DrawRectangleRec(rect, bgColor.getColor().toRaylibColor())

    # Draw button border
    DrawRectangleLinesEx(rect, 2.0, DARKGRAY)

    # Center text in button
    let textWidth = MeasureText(text.cstring, fontSize.cint)
    let textX = elem.x + (elem.width - textWidth.float) / 2.0
    let textY = elem.y + (elem.height - fontSize.float) / 2.0

    DrawText(text.cstring, textX.cint, textY.cint, fontSize.cint, textColor.toRaylibColor())

  of ekInput:
    # Get current value from backend state (for user input), or use bound variable value, or use initial value from property
    let reactiveValue = elem.getProp("value").get(val("")).getString()

    # Priority: backend state (user typing) > bound variable value > empty string
    # BUT: sync backend state with reactive value when reactive value changes (for two-way binding)
    var value: string
    if backend.inputValues.hasKey(elem):
      value = backend.inputValues[elem]
      # If reactive value changed (e.g., was cleared programmatically), sync backend state
      if reactiveValue != value and (reactiveValue == "" or backend.focusedInput != elem):
        # Sync with reactive value when:
        # 1. Reactive value is empty (programmatic clear)
        # 2. Input is not focused (not actively being typed in)
        value = reactiveValue
        backend.inputValues[elem] = reactiveValue
    else:
      value = reactiveValue

    let placeholder = elem.getProp("placeholder").get(val("")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()

    # Get border properties with defaults
    let borderWidthProp = elem.getProp("borderWidth")
    let borderWidth = if borderWidthProp.isSome:
      borderWidthProp.get.getFloat()
    else:
      1.0  # Default border width

    let borderColorProp = elem.getProp("borderColor")
    let borderColor = if borderColorProp.isSome:
      borderColorProp.get.getColor()
    else:
      if backend.focusedInput == elem:
        parseColor("#4A90E2")  # Blue when focused
      else:
        parseColor("#CCCCCC")  # Gray when not focused

    # Draw input background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    DrawRectangleRec(rect, bgColor.getColor().toRaylibColor())

    # Draw border (thicker if focused or custom width)
    let actualBorderWidth = if backend.focusedInput == elem and borderWidth == 1.0: 2.0 else: borderWidth
    if actualBorderWidth > 0:
      DrawRectangleLinesEx(rect, actualBorderWidth, borderColor.toRaylibColor())

    # Text rendering with scrolling and clipping
    let padding = 8.0
    let displayText = if value.len > 0: value else: placeholder
    let displayColor = if value.len > 0: textColor else: parseColor("#999999")

    # Calculate text dimensions (textWidth not needed for scrolling logic)
    # let textWidth = if displayText.len > 0:
    #   MeasureText(displayText.cstring, fontSize.cint).float
    # else:
    #   0.0

    # Calculate cursor position in text (for focused input)
    let cursorTextPos = if value.len > 0 and backend.focusedInput == elem:
      # Measure text up to cursor position (cursor is at end for now)
      MeasureText(value.cstring, fontSize.cint).float
    else:
      0.0

    # Get or initialize scroll offset for this input
    var scrollOffset = backend.inputScrollOffsets.getOrDefault(elem, 0.0)

    # Calculate visible text area
    let visibleWidth = elem.width - (padding * 2.0)

    # Adjust scroll offset to keep cursor visible
    if backend.focusedInput == elem:
      # Calculate total text width to ensure we don't scroll past the end
      let totalTextWidth = if value.len > 0:
        MeasureText(value.cstring, fontSize.cint).float
      else:
        0.0

      # If cursor is approaching right edge, scroll to keep it visible
      if cursorTextPos - scrollOffset > visibleWidth - 20.0:
        scrollOffset = cursorTextPos - visibleWidth + 20.0

      # If cursor is approaching left edge, scroll left
      if cursorTextPos < scrollOffset + 20.0:
        scrollOffset = max(0.0, cursorTextPos - 20.0)

      # Don't scroll past the beginning or allow scrolling beyond text end
      scrollOffset = max(0.0, scrollOffset)

      # If text is shorter than visible area, reset scroll to 0
      if totalTextWidth <= visibleWidth:
        scrollOffset = 0.0
      else:
        # Don't allow scrolling past the end of the text
        let maxScrollOffset = max(0.0, totalTextWidth - visibleWidth)
        scrollOffset = min(scrollOffset, maxScrollOffset)

      # Store updated scroll offset
      backend.inputScrollOffsets[elem] = scrollOffset

    # Set up clipping to prevent text overflow
    let clipRect = rrect(elem.x + padding, elem.y + 2.0, elem.width - (padding * 2.0), elem.height - 4.0)
    BeginScissorMode(clipRect.x.cint, clipRect.y.cint, clipRect.width.cint, clipRect.height.cint)

    # Draw text with scroll offset
    if displayText.len > 0:
      let textX = elem.x + padding - scrollOffset
      let textY = elem.y + (elem.height - fontSize.float) / 2.0
      DrawText(displayText.cstring, textX.cint, textY.cint, fontSize.cint, displayColor.toRaylibColor())

    # End clipping
    EndScissorMode()

    # Draw cursor if focused and visible
    if backend.focusedInput == elem and backend.cursorBlink < 0.5:
      let cursorX = elem.x + padding + cursorTextPos - scrollOffset
      let cursorY = elem.y + 4.0

      # Only draw cursor if it's within the visible area
      if cursorX >= elem.x + padding and cursorX <= elem.x + elem.width - padding:
        DrawRectangle(cursorX.cint, cursorY.cint, 1, fontSize.cint - 2, textColor.toRaylibColor())

  of ekDropdown:
    # Get dropdown properties
    let selectedIndex = elem.dropdownSelectedIndex
    let isOpen = elem.dropdownIsOpen

    # Get styling properties
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()
    let borderColor = elem.getProp("borderColor").get(val("#CCCCCC")).getColor()
    let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
    let placeholder = elem.getProp("placeholder").get(val("Select...")).getString()

    # Draw the main dropdown button
    let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)
    DrawRectangleRec(mainRect, bgColor.getColor().toRaylibColor())
    DrawRectangleLinesEx(mainRect, borderWidth, borderColor.toRaylibColor())

    # Draw dropdown arrow on the right side
    let arrowSize = 8.0
    let arrowX = elem.x + elem.width - 15.0
    let arrowY = elem.y + (elem.height - arrowSize) / 2.0

    # Draw arrow using ASCII characters
    let arrowChar = if isOpen: "^" else: "v"
    DrawText(arrowChar.cstring, arrowX.cint, arrowY.cint, fontSize.cint, textColor.toRaylibColor())

    # Draw selected text or placeholder
    let displayText = if selectedIndex >= 0 and selectedIndex < elem.dropdownOptions.len:
      elem.dropdownOptions[selectedIndex]
    else:
      placeholder

    let displayColor = if selectedIndex >= 0:
      textColor
    else:
      parseColor("#999999")  # Gray for placeholder

    let textPadding = 10.0
    let textX = elem.x + textPadding
    let textY = elem.y + (elem.height - fontSize.float) / 2.0
    let maxTextWidth = elem.width - (textPadding * 2.0) - 20.0  # Leave space for arrow

    # Set up clipping for text to prevent overflow
    BeginScissorMode(textX.cint, textY.cint, maxTextWidth.cint, fontSize.cint)
    DrawText(displayText.cstring, textX.cint, textY.cint, fontSize.cint, displayColor.toRaylibColor())
    EndScissorMode()

    # NOTE: Dropdown menus are rendered separately in renderDropdownMenus() to ensure they appear on top

  of ekColumn, ekRow, ekCenter:
    # Layout containers don't render themselves, just their children with inherited color (sorted by z-index)
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  else:
    # Unsupported element - skip
    discard

  # Render children for other elements (like Container) sorted by z-index
  if elem.kind != ekColumn and elem.kind != ekRow and elem.kind != ekCenter and elem.kind != ekBody:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

proc renderDropdownMenus*(backend: var RaylibBackend, elem: Element) =
  ## Render all open dropdown menus on top of everything else
  ## This is called AFTER renderElement to ensure dropdowns appear above all other content

  case elem.kind:
  of ekDropdown:
    # Only render the menu if the dropdown is open
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let options = elem.dropdownOptions
      let selectedIndex = elem.dropdownSelectedIndex
      let hoveredIndex = elem.dropdownHoveredIndex

      # Get styling properties
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
      let textColor = elem.getProp("color").get(val("#000000")).getColor()
      let borderColor = elem.getProp("borderColor").get(val("#CCCCCC")).getColor()
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()

      # Calculate dropdown item height
      let itemHeight = fontSize.float + 10.0  # Text height + padding
      let textPadding = 10.0

      # Calculate dropdown menu dimensions and position
      let dropdownWidth = elem.width
      let dropdownHeight = min(options.len.float * itemHeight, 200.0)  # Max height of 200px
      let dropdownX = elem.x
      let dropdownY = elem.y + elem.height  # Position below main button

      # Draw dropdown background
      let dropdownRect = rrect(dropdownX, dropdownY, dropdownWidth, dropdownHeight)
      DrawRectangleRec(dropdownRect, bgColor.getColor().toRaylibColor())
      DrawRectangleLinesEx(dropdownRect, borderWidth, borderColor.toRaylibColor())

      # Calculate visible range (for scrolling if needed)
      let maxVisibleItems = int(dropdownHeight / itemHeight)
      let startIndex = 0  # Simple implementation - no scrolling for now
      let endIndex = min(options.len, startIndex + maxVisibleItems)

      # Draw visible options
      for i in startIndex..<endIndex:
        let optionY = dropdownY + (i - startIndex).float * itemHeight
        let optionRect = rrect(dropdownX, optionY, dropdownWidth, itemHeight)

        # Highlight hovered option
        if i == hoveredIndex:
          DrawRectangleRec(optionRect, parseColor("#E3F2FD").toRaylibColor())  # Light blue

        # Highlight selected option
        if i == selectedIndex:
          DrawRectangleRec(optionRect, parseColor("#BBDEFB").toRaylibColor())  # Medium blue

        # Draw option text
        let optionText = options[i]
        let optionTextX = dropdownX + textPadding
        let optionTextY = optionY + (itemHeight - fontSize.float) / 2.0
        let optionMaxWidth = dropdownWidth - (textPadding * 2.0)

        # Clip text to prevent overflow
        BeginScissorMode(optionTextX.cint, optionTextY.cint, optionMaxWidth.cint, fontSize.cint)
        DrawText(optionText.cstring, optionTextX.cint, optionTextY.cint, fontSize.cint, textColor.toRaylibColor())
        EndScissorMode()

  of ekConditional:
    # Only check the active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      if activeBranch != nil:
        backend.renderDropdownMenus(activeBranch)

  else:
    # Recursively check all children for open dropdowns
    for child in elem.children:
      backend.renderDropdownMenus(child)

# ============================================================================
# Input Handling
# ============================================================================

proc sortChildrenByZIndexReverse(children: seq[Element]): seq[Element] =
  ## Sort children by z-index in reverse order for input handling
  ## Elements with higher z-index should receive input first (on top)
  result = children
  result.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(bZIndex, aZIndex)  # Reverse order
  )

proc closeOtherDropdowns*(backend: var RaylibBackend, keepOpen: Element) =
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

proc checkHoverCursor*(elem: Element): bool =
  ## Check if mouse is hovering over any interactive element that needs pointer cursor
  ## Returns true if pointer cursor should be shown

  case elem.kind:
  of ekConditional:
    # Only check hover cursor for the active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      # Check hover cursor for the active branch if it exists
      if activeBranch != nil:
        if checkHoverCursor(activeBranch):
          return true

  of ekButton:
    let mousePos = GetMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    if CheckCollisionPointRec(mousePos, rect):
      return true

  of ekDropdown:
    let mousePos = GetMousePosition()
    let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)

    # Check if hovering over main dropdown button
    if CheckCollisionPointRec(mousePos, mainRect):
      return true

    # Check if hovering over dropdown options (when open)
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0  # Match rendering code
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)
      if CheckCollisionPointRec(mousePos, dropdownRect):
        return true

  else:
    discard

  # Check children recursively
  for child in elem.children:
    if checkHoverCursor(child):
      return true

  return false

proc handleInput*(backend: var RaylibBackend, elem: Element) =
  ## Handle mouse input for interactive elements

  case elem.kind:
  of ekHeader:
    # Header is metadata only - no input handling
    discard

  of ekConditional:
    # Conditional elements only handle input for their active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      # Handle input for the active branch if it exists
      if activeBranch != nil:
        backend.handleInput(activeBranch)

  of ekBody:
    # Body is a wrapper - handle children's input (reverse z-index order, highest first)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  of ekButton:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()
      let rect = rrect(elem.x, elem.y, elem.width, elem.height)

      if CheckCollisionPointRec(mousePos, rect):
        echo "🔥 BUTTON CLICKED! Checking handlers..."
        # Button was clicked - trigger onClick handler
        if elem.eventHandlers.hasKey("onClick"):
          echo "🔥 Found onClick handler, calling it!"
          elem.eventHandlers["onClick"]()
        else:
          echo "🔥 No onClick handler found!"

  of ekInput:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()
      let rect = rrect(elem.x, elem.y, elem.width, elem.height)

      if CheckCollisionPointRec(mousePos, rect):
        # Input was clicked - focus it
        backend.focusedInput = elem
        backend.cursorBlink = 0.0  # Reset cursor blink

        # Initialize input value if not already present
        if not backend.inputValues.hasKey(elem):
          let initialValue = elem.getProp("value").get(val("")).getString()
          backend.inputValues[elem] = initialValue
      else:
        # Clicked outside - unfocus if this was focused
        if backend.focusedInput == elem:
          backend.focusedInput = nil

  of ekDropdown:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()
      let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)

      if CheckCollisionPointRec(mousePos, mainRect):
        # Main dropdown button was clicked
        if elem.dropdownIsOpen:
          # Close the dropdown
          elem.dropdownIsOpen = false
          elem.dropdownHoveredIndex = -1
          backend.focusedDropdown = nil
        else:
          # Open the dropdown
          elem.dropdownIsOpen = true
          elem.dropdownHoveredIndex = elem.dropdownSelectedIndex  # Start with current selection
          backend.focusedDropdown = elem

          # Close any other dropdowns
          backend.closeOtherDropdowns(elem)

      elif elem.dropdownIsOpen:
        # Check if clicked on dropdown options
        if elem.dropdownOptions.len > 0:
          let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
          let itemHeight = fontSize.float + 10.0  # Match rendering code
          let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
          let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

          if CheckCollisionPointRec(mousePos, dropdownRect):
            # Calculate which option was clicked
            let relativeY = mousePos.y - dropdownRect.y
            let clickedIndex = int(relativeY / itemHeight)

            if clickedIndex >= 0 and clickedIndex < elem.dropdownOptions.len:
              # Select the clicked option
              elem.dropdownSelectedIndex = clickedIndex
              elem.dropdownIsOpen = false
              elem.dropdownHoveredIndex = -1
              backend.focusedDropdown = nil

              # Trigger onChange handler if present
              if elem.eventHandlers.hasKey("onChange"):
                let handler = elem.eventHandlers["onChange"]
                handler(elem.dropdownOptions[clickedIndex])

              # For onSelectionChange, we pass the selected value as data (index can be parsed if needed)
              if elem.eventHandlers.hasKey("onSelectionChange"):
                let handler = elem.eventHandlers["onSelectionChange"]
                handler(elem.dropdownOptions[clickedIndex])
          else:
            # Clicked outside dropdown - close it
            elem.dropdownIsOpen = false
            elem.dropdownHoveredIndex = -1
            backend.focusedDropdown = nil
        else:
          # No options - close dropdown
          elem.dropdownIsOpen = false
          elem.dropdownHoveredIndex = -1
          backend.focusedDropdown = nil
      else:
        # Clicked outside while closed - just ensure it's not focused
        if backend.focusedDropdown == elem:
          backend.focusedDropdown = nil

    # Handle hover state for dropdown options
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let mousePos = GetMousePosition()
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0  # Match rendering code
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

      if CheckCollisionPointRec(mousePos, dropdownRect):
        let relativeY = mousePos.y - dropdownRect.y
        let hoveredIndex = int(relativeY / itemHeight)

        if hoveredIndex >= 0 and hoveredIndex < elem.dropdownOptions.len:
          elem.dropdownHoveredIndex = hoveredIndex
        else:
          elem.dropdownHoveredIndex = -1
      else:
        elem.dropdownHoveredIndex = -1

  of ekColumn, ekRow, ekCenter, ekContainer:
    # Layout containers - only handle children's input (reverse z-index order, highest first)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  else:
    # Other element types - check children (reverse z-index order, highest first)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

proc handleKeyboardInput*(backend: var RaylibBackend) =
  ## Handle keyboard input for focused input and dropdown elements

  # Handle dropdown keyboard input
  if backend.focusedDropdown != nil:
    let dropdown = backend.focusedDropdown

    if IsKeyPressed(KEY_ESCAPE):
      # Close dropdown on ESC
      dropdown.dropdownIsOpen = false
      dropdown.dropdownHoveredIndex = -1
      backend.focusedDropdown = nil

    elif dropdown.dropdownIsOpen:
      if dropdown.dropdownOptions.len > 0:
        var handled = false

        if IsKeyPressed(KEY_UP):
          # Move selection up
          dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex <= 0:
            dropdown.dropdownOptions.len - 1  # Wrap to bottom
          else:
            dropdown.dropdownHoveredIndex - 1
          handled = true

        elif IsKeyPressed(KEY_DOWN):
          # Move selection down
          dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex >= dropdown.dropdownOptions.len - 1:
            0  # Wrap to top
          else:
            dropdown.dropdownHoveredIndex + 1
          handled = true

        elif IsKeyPressed(KEY_ENTER):
          # Select current hover item
          if dropdown.dropdownHoveredIndex >= 0 and dropdown.dropdownHoveredIndex < dropdown.dropdownOptions.len:
            dropdown.dropdownSelectedIndex = dropdown.dropdownHoveredIndex
            dropdown.dropdownIsOpen = false
            dropdown.dropdownHoveredIndex = -1
            backend.focusedDropdown = nil

            # Trigger onChange handler if present
            if dropdown.eventHandlers.hasKey("onChange"):
              let handler = dropdown.eventHandlers["onChange"]
              handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])

            # Trigger onSelectionChange handler if present
            if dropdown.eventHandlers.hasKey("onSelectionChange"):
              let handler = dropdown.eventHandlers["onSelectionChange"]
              handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])
          handled = true

        elif IsKeyPressed(KEY_TAB):
          # Close dropdown on TAB (don't select)
          dropdown.dropdownIsOpen = false
          dropdown.dropdownHoveredIndex = -1
          backend.focusedDropdown = nil
          handled = true

      # If no dropdown-specific key was pressed, don't handle further
      return

  # Handle input field keyboard input
  if backend.focusedInput == nil:
    backend.backspaceHoldTimer = 0.0  # Reset timer when no focus
    return

  # Get current value
  var currentValue = backend.inputValues.getOrDefault(backend.focusedInput, "")
  var textChanged = false

  # Handle character input
  while true:
    let char = GetCharPressed()
    if char == 0:
      break
    # Add printable characters
    if char >= 32 and char < 127:
      currentValue.add(char.chr)
      textChanged = true
      backend.backspaceHoldTimer = 0.0  # Reset backspace timer when typing

  # Handle backspace with repeat logic
  let backspacePressed = IsKeyDown(KEY_BACKSPACE)
  if backspacePressed and currentValue.len > 0:
    if IsKeyPressed(KEY_BACKSPACE):
      # First press - delete one character immediately
      currentValue.setLen(currentValue.len - 1)
      textChanged = true
      backend.backspaceHoldTimer = 0.0  # Start the hold timer
    else:
      # Key is being held down
      backend.backspaceHoldTimer += 1.0 / 60.0  # Increment by frame time (~16ms at 60fps)

      # Check if we should delete more characters
      if backend.backspaceHoldTimer >= backend.backspaceRepeatDelay:
        # Calculate how many characters to delete based on hold time
        let holdBeyondDelay = backend.backspaceHoldTimer - backend.backspaceRepeatDelay
        let charsToDelete = min(int(holdBeyondDelay / backend.backspaceRepeatRate), currentValue.len)

        if charsToDelete > 0:
          currentValue.setLen(currentValue.len - charsToDelete)
          textChanged = true
          # Adjust timer to maintain repeat rate
          backend.backspaceHoldTimer = backend.backspaceRepeatDelay +
                                      (charsToDelete.float * backend.backspaceRepeatRate)
  else:
    # Backspace not pressed - reset timer
    backend.backspaceHoldTimer = 0.0

  # Handle other special keys
  if IsKeyPressed(KEY_ENTER):
    # Trigger onSubmit handler if present
    if backend.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value if changed
  if textChanged:
    backend.inputValues[backend.focusedInput] = currentValue

  # Trigger onChange handler if present
  if textChanged and backend.focusedInput.eventHandlers.hasKey("onChange"):
    let handler = backend.focusedInput.eventHandlers["onChange"]
    handler(currentValue)

  # Trigger onValueChange handler for two-way binding
  if textChanged and backend.focusedInput.eventHandlers.hasKey("onValueChange"):
    let handler = backend.focusedInput.eventHandlers["onValueChange"]
    handler(currentValue)

# ============================================================================
# Main Loop
# ============================================================================

proc run*(backend: var RaylibBackend, root: Element) =
  ## Run the application with the given root element

  backend.rootElement = root
  backend.running = true

  # Initialize Raylib window
  InitWindow(backend.windowWidth.cint, backend.windowHeight.cint, backend.windowTitle.cstring)
  SetTargetFPS(60)

  # Calculate initial layout
  calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

  # Main game loop
  while not WindowShouldClose():
    # Update cursor blink timer
    backend.cursorBlink += 1.0 / 60.0  # Assuming 60 FPS
    if backend.cursorBlink >= 1.0:
      backend.cursorBlink = 0.0

    # Update mouse cursor based on hover state
    if checkHoverCursor(root):
      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND)
    else:
      SetMouseCursor(MOUSE_CURSOR_DEFAULT)

    # Handle mouse input
    backend.handleInput(root)

    # Handle keyboard input
    backend.handleKeyboardInput()

    # Only recalculate layout when there are dirty elements (intelligent updates)
    if hasDirtyElements():
      echo "🔥 Layout update triggered - " & $dirtyElementsCount & " dirty elements"
      calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

    # Render
    BeginDrawing()

    # Find the actual Body element (child of root) and check for background color
    var actualBody: Element = nil
    for child in root.children:
      if child.kind == ekBody:
        actualBody = child
        break

    # Check for both "background" and "backgroundColor" properties
    var bodyBg: Option[Value] = none(Value)
    if actualBody != nil:
      bodyBg = actualBody.getProp("backgroundColor")
      if bodyBg.isNone:
        bodyBg = actualBody.getProp("background")

    if bodyBg.isSome:
      let bgColor = bodyBg.get.getColor()
      ClearBackground(bgColor.toRaylibColor())
    else:
      ClearBackground(backend.backgroundColor.toRaylibColor())

    # Render all elements with normal z-index sorting
    backend.renderElement(root)

    # Render dropdown menus on top of everything else
    backend.renderDropdownMenus(root)

    EndDrawing()

  # Cleanup
  CloseWindow()
  backend.running = false
