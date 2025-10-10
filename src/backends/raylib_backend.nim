## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../kryon/core
import options, tables
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

    # Determine starting Y position based on mainAxisAlignment
    var currentY = elem.y
    if mainAxisAlignment.isSome and mainAxisAlignment.get.getString() == "center":
      currentY = elem.y + (elem.height - totalHeight) / 2.0

    # Second pass: Position children
    for i, child in elem.children:
      if i > 0:
        currentY += gap

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

    # Determine starting X position based on mainAxisAlignment
    var currentX = elem.x
    if mainAxisAlignment.isSome and mainAxisAlignment.get.getString() == "center":
      currentX = elem.x + (elem.width - totalWidth) / 2.0

    # Second pass: Position children
    for i, child in elem.children:
      if i > 0:
        currentX += gap

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
    echo "🔥 Rendering ForLoop with " & $elem.children.len & " children"

    for child in elem.children:
      backend.renderElement(child, inheritedColor)

  of ekBody:
    # Body is a wrapper - render its children with inherited color
    let bodyColor = elem.getProp("color")
    let colorToInherit = if bodyColor.isSome:
      some(bodyColor.get.getColor())
    else:
      inheritedColor

    for child in elem.children:
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

  of ekColumn, ekRow, ekCenter:
    # Layout containers don't render themselves, just their children with inherited color
    for child in elem.children:
      backend.renderElement(child, inheritedColor)

  else:
    # Unsupported element - skip
    discard

  # Render children for other elements (like Container)
  if elem.kind != ekColumn and elem.kind != ekRow and elem.kind != ekCenter and elem.kind != ekBody:
    for child in elem.children:
      backend.renderElement(child, inheritedColor)

# ============================================================================
# Input Handling
# ============================================================================

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
    # Body is a wrapper - handle children's input
    for child in elem.children:
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

  of ekColumn, ekRow, ekCenter, ekContainer:
    # Layout containers - only handle children's input, don't process children twice
    for child in elem.children:
      backend.handleInput(child)

  else:
    # Other element types - just check children
    for child in elem.children:
      backend.handleInput(child)

proc handleKeyboardInput*(backend: var RaylibBackend) =
  ## Handle keyboard input for focused input element
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

    backend.renderElement(root)

    EndDrawing()

  # Cleanup
  CloseWindow()
  backend.running = false
