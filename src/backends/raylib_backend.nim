## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../kryon/core
import options, tables, algorithm, math
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

      # Layout the active branch at the conditional's position with full parent dimensions
      # This ensures the active branch gets proper space for layout and rendering
      if activeBranch != nil:
        calculateLayout(activeBranch, elem.x, elem.y, parentWidth, parentHeight)

  of ekForLoop:
    # For loop elements generate dynamic content - let parent handle layout
    if elem.forIterable != nil and elem.forBodyTemplate != nil:
      # Register dependency on the iterable for automatic regeneration
      # This is crucial for reactive updates when the underlying data changes
      registerDependency("forLoopIterable")

      let items = elem.forIterable()

      when defined(debugTabs):
        echo "Debug: ekForLoop layout - got ", items.len, " items"
        for i, item in items:
          echo "Debug:  [", i, "] ", item

      # Clear previous children (if any) before regenerating
      # This ensures we always reflect the current state of the iterable
      elem.children.setLen(0)

      # Generate elements for each item in the iterable
      for item in items:
        let childElement = elem.forBodyTemplate(item)
        if childElement != nil:
          when defined(debugTabs):
            echo "Debug: Created child element of kind: ", childElement.kind
          addChild(elem, childElement)  # Use addChild to properly set parent pointer

      # For loop is transparent to layout - use parent size
      elem.width = parentWidth
      elem.height = parentHeight
    else:
      when defined(debugTabs):
        echo "Debug: ekForLoop with nil forIterable or forBodyTemplate"

  of ekBody:
    # Body implements natural document flow - stack children vertically
    # Check for contentAlignment property
    let alignment = elem.getProp("contentAlignment")
    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout
      var currentY = elem.y
      let gap = elem.getProp("gap").get(val(5)).getFloat()  # Default gap between elements

      # First pass: Calculate all children sizes and total height
      var totalHeight = 0.0
      var maxWidth = 0.0
      var childSizes: seq[tuple[w, h: float]] = @[]

      for child in elem.children:
        # First pass: Calculate child layout at (0, 0) to determine its size
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

        # Apply gap between elements (but not after the last one)
        if i > 0:
          currentY += gap

        # Recalculate at centered position
        calculateLayout(child, centerX, currentY, childSizes[i].w, childSizes[i].h)
        currentY += childSizes[i].h
    else:
      # Default: normal relative positioning layout
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
      # Special handling for ForLoop children - trigger child generation first
      if child.kind == ekForLoop:
        # Trigger for loop to generate its children by calling layout with dummy coordinates
        calculateLayout(child, 0, 0, elem.width, 0)
        # Now calculate sizes for the generated children
        for grandchild in child.children:
          calculateLayout(grandchild, 0, 0, elem.width, 0)
          childSizes.add((w: grandchild.width, h: grandchild.height))
      else:
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
    var childIndex = 0
    for child in elem.children:
      if childIndex > 0:
        currentY += dynamicGap

      if child.kind == ekForLoop:
        # For loop - position all its generated children vertically
        for grandchild in child.children:
          if childIndex > 0:
            currentY += dynamicGap

          # Determine X position based on crossAxisAlignment
          var childX = elem.x
          if crossAxisAlignment.isSome:
            let align = crossAxisAlignment.get.getString()
            if align == "center":
              childX = elem.x + (elem.width - childSizes[childIndex].w) / 2.0
            elif align == "end":
              childX = elem.x + elem.width - childSizes[childIndex].w

          # Layout grandchild at final position
          calculateLayout(grandchild, childX, currentY, elem.width, childSizes[childIndex].h)
          currentY += childSizes[childIndex].h
          inc childIndex
      else:
        # Regular child - position it
        # Determine X position based on crossAxisAlignment
        var childX = elem.x
        if crossAxisAlignment.isSome:
          let align = crossAxisAlignment.get.getString()
          if align == "center":
            childX = elem.x + (elem.width - childSizes[childIndex].w) / 2.0
          elif align == "end":
            childX = elem.x + elem.width - childSizes[childIndex].w

        # Layout child at final position
        calculateLayout(child, childX, currentY, elem.width, childSizes[childIndex].h)
        currentY += childSizes[childIndex].h
        inc childIndex

  of ekRow:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # For Row: mainAxis = horizontal (X), crossAxis = vertical (Y)

    # First pass: Calculate all children sizes (give them full height and let them size themselves naturally)
    var childSizes: seq[tuple[w, h: float]] = @[]
    for child in elem.children:
      # Special handling for ForLoop children - trigger child generation first
      if child.kind == ekForLoop:
        # Trigger for loop to generate its children by calling layout with dummy coordinates
        calculateLayout(child, 0, 0, parentWidth, elem.height)
        # Now calculate sizes for the generated children
        for grandchild in child.children:
          calculateLayout(grandchild, 0, 0, parentWidth, elem.height)
          childSizes.add((w: grandchild.width, h: grandchild.height))
      else:
        # Let children size themselves naturally (don't constrain width)
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
    var childIndex = 0
    for child in elem.children:
      if childIndex > 0:
        currentX += dynamicGap

      if child.kind == ekForLoop:
        # For loop - position all its generated children horizontally
        for grandchild in child.children:
          if childIndex > 0:
            currentX += dynamicGap

          # Determine Y position based on crossAxisAlignment
          var childY = elem.y
          if crossAxisAlignment.isSome:
            let align = crossAxisAlignment.get.getString()
            if align == "center":
              childY = elem.y + (elem.height - childSizes[childIndex].h) / 2.0
            elif align == "end":
              childY = elem.y + elem.height - childSizes[childIndex].h

          # Layout grandchild at final position
          calculateLayout(grandchild, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
          currentX += childSizes[childIndex].w
          inc childIndex
      else:
        # Regular child - position it
        # Determine Y position based on crossAxisAlignment
        var childY = elem.y
        if crossAxisAlignment.isSome:
          let align = crossAxisAlignment.get.getString()
          if align == "center":
            childY = elem.y + (elem.height - childSizes[childIndex].h) / 2.0
          elif align == "end":
            childY = elem.y + elem.height - childSizes[childIndex].h

        # Layout child at final position
        calculateLayout(child, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
        currentX += childSizes[childIndex].w
        inc childIndex

  of ekCenter:
    # Center is just like a Container but with contentAlignment = "center" by default
    let gap = elem.getProp("gap").get(val(0)).getFloat()

    # Check for contentAlignment property (default to center)
    let alignment = elem.getProp("contentAlignment").get(val("center"))

    if alignment.getString() == "center":
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
      var currentY = elem.y
      for child in elem.children:
        calculateLayout(child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

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

      # Auto-size height if not explicitly set (use max child height + padding)
      if not hasExplicitHeight and elem.children.len > 0:
        var maxHeight = 0.0
        for child in elem.children:
          maxHeight = max(maxHeight, child.height)
        let paddingValue = elem.getProp("padding").get(val(0)).getFloat()
        elem.height = maxHeight + (paddingValue * 2.0)
    else:
      # Default: normal relative positioning layout
      for child in elem.children:
        calculateLayout(child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

      # Auto-size height if not explicitly set (use total stacked height)
      if not hasExplicitHeight and elem.children.len > 0:
        # Account for padding if present
        let paddingValue = elem.getProp("padding").get(val(0)).getFloat()
        elem.height = (currentY - elem.y - gap) + (paddingValue * 2.0)

  of ekTabGroup:
    # TabGroup lays out TabBar and TabContent vertically
    # Get selectedIndex from property if set
    let selectedIndexProp = elem.getProp("selectedIndex")
    if selectedIndexProp.isSome:
      elem.tabSelectedIndex = selectedIndexProp.get.getInt()

    # Register dependency on tab selection for reactive updates
    # This allows elements to automatically update when tab selection changes
    registerDependency("tabSelectedIndex")
    registerDependency("selectedIndex")

    var currentY = elem.y
    # Only layout structural children (TabBar, TabContent), skip control flow elements
    for child in elem.children:
      if child.kind != ekConditional and child.kind != ekForLoop:
        calculateLayout(child, elem.x, currentY, elem.width, 0)
        currentY += child.height

  of ekTabBar:
    # TabBar lays out Tab children horizontally like a Row
    # Copy selectedIndex from parent TabGroup
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup:
        elem.tabSelectedIndex = parent.tabSelectedIndex
        break
      parent = parent.parent

    var currentX = elem.x
    var maxHeight = 40.0  # Default tab height
    var tabIndex = 0

    # Assign indices to tabs and layout horizontally
    # Look for Tab elements in both direct children and grandchildren through ekForLoop
    var allTabs: seq[Element] = @[]

    # First pass: collect all Tab elements
    # IMPORTANT: For ekForLoop children, we need to trigger child generation first
    when defined(debugTabs):
      echo "Debug: TabBar collecting tabs, have ", elem.children.len, " children"
    for child in elem.children:
      when defined(debugTabs):
        echo "Debug: TabBar child of kind: ", child.kind
      if child.kind == ekTab:
        when defined(debugTabs):
          echo "Debug: Found direct Tab child"
        allTabs.add(child)
      elif child.kind == ekForLoop:
        # Trigger for loop child generation by calling its layout
        # This populates child.children with the generated elements
        when defined(debugTabs):
          echo "Debug: Found ekForLoop, triggering child generation"
        calculateLayout(child, elem.x, elem.y, elem.width, 0)

        when defined(debugTabs):
          echo "Debug: ekForLoop now has ", child.children.len, " generated children"
        # Look for Tab elements inside the for loop
        for grandchild in child.children:
          when defined(debugTabs):
            echo "Debug: Checking grandchild of kind: ", grandchild.kind
          if grandchild.kind == ekTab:
            when defined(debugTabs):
              echo "Debug: Found Tab grandchild in for loop"
            allTabs.add(grandchild)

    # Second pass: assign indices and layout all tabs
    for tab in allTabs:
      tab.tabIndex = tabIndex
      tabIndex += 1
      # Calculate tab width based on title if not explicitly set
      let title = tab.getProp("title").get(val("Tab")).getString()
      let fontSize = tab.getProp("fontSize").get(val(16)).getInt()
      let textWidth = MeasureText(title.cstring, fontSize.cint).float
      let tabWidth = tab.getProp("width").get(val(textWidth + 40.0)).getFloat()
      let tabHeight = tab.getProp("height").get(val(40.0)).getFloat()
      maxHeight = max(maxHeight, tabHeight)

      calculateLayout(tab, currentX, elem.y, tabWidth, tabHeight)
      currentX += tabWidth

    # Set TabBar size
    elem.height = maxHeight

  of ekTab:
    # Tab is sized by parent (TabBar) - just set dimensions
    discard  # Already handled in TabBar layout

  of ekTabContent:
    # TabContent lays out only the active TabPanel
    # Get selectedIndex from parent TabGroup
    var selectedIndex = 0
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup:
        selectedIndex = parent.tabSelectedIndex
        break
      parent = parent.parent

    # Register dependency on tab selection for reactive updates
    # This ensures TabContent recalculates when tab selection changes
    registerDependency("tabSelectedIndex")
    registerDependency("selectedIndex")

    # Collect TabPanel elements from both direct children and grandchildren through ekForLoop
    # IMPORTANT: For ekForLoop children, we need to trigger child generation first
    var allTabPanels: seq[Element] = @[]
    var panelIndex = 0

    # First pass: collect all TabPanel elements
    for child in elem.children:
      if child.kind == ekTabPanel:
        allTabPanels.add(child)
      elif child.kind == ekForLoop:
        # Trigger for loop child generation by calling its layout
        # This populates child.children with the generated elements
        calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

        # Look for TabPanel elements inside the for loop
        for grandchild in child.children:
          if grandchild.kind == ekTabPanel:
            allTabPanels.add(grandchild)

    # Second pass: assign indices and layout only the active panel
    for panel in allTabPanels:
      panel.tabIndex = panelIndex
      panelIndex += 1
      # Only layout the active panel
      if panel.tabIndex == selectedIndex:
        calculateLayout(panel, elem.x, elem.y, elem.width, elem.height)

  of ekTabPanel:
    # TabPanel fills its parent space and lays out children vertically
    var currentY = elem.y
    let gap = elem.getProp("gap").get(val(10)).getFloat()
    let padding = elem.getProp("padding").get(val(0)).getFloat()

    # Apply padding
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

    # Draw background (check both "backgroundColor" and "background" alias)
    var bgColor = elem.getProp("backgroundColor")
    if bgColor.isNone:
      bgColor = elem.getProp("background")
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

  of ekCheckbox:
    # Get checkbox properties
    let label = elem.getProp("label").get(val("")).getString()
    let checkedProp = elem.getProp("checked").get(val(false))
    let initialChecked = checkedProp.getBool()

    # Get current checkbox state from backend, or initialize from property
    var isChecked = initialChecked
    if backend.checkboxStates.hasKey(elem):
      isChecked = backend.checkboxStates[elem]
    else:
      # Initialize checkbox state from property
      backend.checkboxStates[elem] = initialChecked

    # Get styling properties
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()
    let borderColor = elem.getProp("borderColor").get(val("#CCCCCC")).getColor()
    let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)  # Keep checkbox proportional to height
    let labelColor = if elem.getProp("labelColor").isSome:
      elem.getProp("labelColor").get.getColor()
    else:
      textColor

    # Calculate checkbox square position and size
    let checkboxX = elem.x
    let checkboxY = elem.y + (elem.height - checkboxSize) / 2.0
    let checkboxRect = rrect(checkboxX, checkboxY, checkboxSize, checkboxSize)

    # Draw checkbox background
    DrawRectangleRec(checkboxRect, bgColor.getColor().toRaylibColor())

    # Draw checkbox border
    if borderWidth > 0:
      DrawRectangleLinesEx(checkboxRect, borderWidth, borderColor.toRaylibColor())

    # Draw checkmark if checked
    if isChecked:
      let checkColor = elem.getProp("checkColor").get(val("#4A90E2")).getColor()
      let padding = checkboxSize * 0.2  # Padding around the checkmark

      # Draw checkmark as lines forming a ✓ shape
      let startX = checkboxX + padding
      let startY = checkboxY + checkboxSize / 2.0
      let middleX = checkboxX + checkboxSize / 2.5
      let middleY = checkboxY + checkboxSize - padding
      let endX = checkboxX + checkboxSize - padding
      let endY = checkboxY + padding

      # Draw checkmark using rectangles to simulate lines
      let checkThickness = max(1.0, checkboxSize / 8.0)

      # First part of checkmark (diagonal from bottom-left to middle)
      let line1Length = sqrt(pow(middleX - startX, 2) + pow(middleY - startY, 2))

      # Draw first line as a series of small rectangles
      let segments1 = int(line1Length / 2.0)
      for i in 0..<segments1:
        let t = i.float / segments1.float
        let x = startX + t * (middleX - startX) - checkThickness / 2
        let y = startY + t * (middleY - startY) - checkThickness / 2
        DrawRectangle(x.cint, y.cint, checkThickness.cint, checkThickness.cint, checkColor.toRaylibColor())

      # Second part of checkmark (diagonal from middle to top-right)
      let line2Length = sqrt(pow(endX - middleX, 2) + pow(endY - middleY, 2))

      # Draw second line as a series of small rectangles
      let segments2 = int(line2Length / 2.0)
      for i in 0..<segments2:
        let t = i.float / segments2.float
        let x = middleX + t * (endX - middleX) - checkThickness / 2
        let y = middleY + t * (endY - middleY) - checkThickness / 2
        DrawRectangle(x.cint, y.cint, checkThickness.cint, checkThickness.cint, checkColor.toRaylibColor())

    # Draw label text if provided
    if label.len > 0:
      let textX = checkboxX + checkboxSize + 10.0  # 10px spacing between checkbox and label
      let textY = elem.y + (elem.height - fontSize.float) / 2.0
      DrawText(label.cstring, textX.cint, textY.cint, fontSize.cint, labelColor.toRaylibColor())

  of ekColumn, ekRow, ekCenter:
    # Layout containers don't render themselves, just their children with inherited color (sorted by z-index)
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTabGroup:
    # TabGroup is a container that manages tab state - render its structural children (TabBar and TabContent)
    # Skip control flow elements (ekConditional, ekForLoop) as they handle their own rendering
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      if child.kind != ekConditional and child.kind != ekForLoop:
        backend.renderElement(child, inheritedColor)

  of ekTabBar:
    # TabBar renders tab buttons horizontally - render its children (Tab elements)
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTab:
    # Tab renders as a button with title text
    let title = elem.getProp("title").get(val(elem.tabTitle)).getString()
    let tabIndex = elem.tabIndex

    # Get parent TabGroup to check if this tab is selected
    var isSelected = false
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup or parent.kind == ekTabBar:
        isSelected = (parent.tabSelectedIndex == tabIndex)
        break
      parent = parent.parent

    # Get colors (support both active and inactive states)
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
    DrawRectangleRec(rect, bgColor.toRaylibColor())

    # Draw tab border (optional)
    let borderColor = elem.getProp("borderColor")
    if borderColor.isSome:
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
      DrawRectangleLinesEx(rect, borderWidth, borderColor.get.getColor().toRaylibColor())

    # Center text in tab
    if title.len > 0:
      let textWidth = MeasureText(title.cstring, fontSize.cint)
      let textX = elem.x + (elem.width - textWidth.float) / 2.0
      let textY = elem.y + (elem.height - fontSize.float) / 2.0
      DrawText(title.cstring, textX.cint, textY.cint, fontSize.cint, textColor.toRaylibColor())

    # Render children (if any - for custom content like icons)
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTabContent:
    # TabContent only renders the active TabPanel based on parent's selectedIndex
    # Find parent TabGroup to get selectedIndex
    var selectedIndex = 0
    var parent = elem.parent
    while parent != nil:
      if parent.kind == ekTabGroup:
        selectedIndex = parent.tabSelectedIndex
        break
      parent = parent.parent

    # Collect TabPanel elements from both direct children and grandchildren through ekForLoop
    # IMPORTANT: For ekForLoop children, we need to trigger child generation first
    var allTabPanels: seq[Element] = @[]

    # First pass: collect all TabPanel elements
    for child in elem.children:
      if child.kind == ekTabPanel:
        allTabPanels.add(child)
      elif child.kind == ekForLoop:
        # NOTE: The children should already be generated from the layout phase,
        # but during first render we may need to ensure they exist
        # Look for TabPanel elements inside the for loop
        for grandchild in child.children:
          if grandchild.kind == ekTabPanel:
            allTabPanels.add(grandchild)

    # Only render the TabPanel that matches the selected index
    for panel in allTabPanels:
      if panel.tabIndex == selectedIndex:
        backend.renderElement(panel, inheritedColor)
        break

  of ekTabPanel:
    # TabPanel renders like a container - just render its children
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

  else:
    # Unsupported element - skip
    discard

  # Render children for other elements (like Container) sorted by z-index
  if elem.kind != ekColumn and elem.kind != ekRow and elem.kind != ekCenter and elem.kind != ekBody and
     elem.kind != ekTabGroup and elem.kind != ekTabBar and elem.kind != ekTabContent and elem.kind != ekTabPanel:
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

  of ekButton, ekTab:
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

  of ekCheckbox:
    let mousePos = GetMousePosition()

    # Calculate checkbox clickable area (including label)
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

    # Check if hovering over checkbox or label
    let label = elem.getProp("label").get(val("")).getString()
    var hoverArea = checkboxRect

    if label.len > 0:
      # Extend hover area to include label text
      let textWidth = MeasureText(label.cstring, fontSize.cint).float
      hoverArea.width = checkboxSize + 10.0 + textWidth  # checkbox + spacing + text
      hoverArea.y = elem.y  # Use full element height for hover area
      hoverArea.height = elem.height

    if CheckCollisionPointRec(mousePos, hoverArea):
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
        # Button was clicked - trigger onClick handler
        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

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

  of ekCheckbox:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()

      # Calculate checkbox clickable area (including label)
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let checkboxSize = min(elem.height, fontSize.float + 8.0)
      let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

      # Check if click is on checkbox or label
      let label = elem.getProp("label").get(val("")).getString()
      var clickArea = checkboxRect

      if label.len > 0:
        # Extend clickable area to include label text
        let textWidth = MeasureText(label.cstring, fontSize.cint).float
        clickArea.width = checkboxSize + 10.0 + textWidth  # checkbox + spacing + text
        clickArea.y = elem.y  # Use full element height for click area
        clickArea.height = elem.height

      if CheckCollisionPointRec(mousePos, clickArea):
        # Checkbox was clicked - toggle state
        var currentState = backend.checkboxStates.getOrDefault(elem, false)
        currentState = not currentState
        backend.checkboxStates[elem] = currentState

        # Trigger onClick handler if present
        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

        # Trigger onChange handler if present (pass the new state as data)
        if elem.eventHandlers.hasKey("onChange"):
          let handler = elem.eventHandlers["onChange"]
          handler($currentState)  # Pass boolean state as string

  of ekTabGroup, ekTabBar, ekTabContent:
    # Tab containers - explicitly handle structural children's input (reverse z-index order)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      if child.kind == ekConditional:
        # Conditionals handle their own input (see ekConditional case)
        backend.handleInput(child)
      elif child.kind == ekForLoop:
        # For loops contain dynamic children - recurse into them
        for grandchild in child.children:
          backend.handleInput(grandchild)
      else:
        backend.handleInput(child)

  of ekTab:
    let mousePos = GetMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let isPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    let isDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT)

    if isPressed:
      if CheckCollisionPointRec(mousePos, rect):
        # Tab was clicked - find and update parent TabGroup's selectedIndex
        var parent = elem.parent
        var tabGroup: Element = nil

        # Find the TabGroup (not just TabBar)
        while parent != nil:
          if parent.kind == ekTabGroup:
            tabGroup = parent
            break
          parent = parent.parent

        if tabGroup != nil:
          # Update the selected index
          let oldSelectedIndex = tabGroup.tabSelectedIndex
          tabGroup.tabSelectedIndex = elem.tabIndex

          # Enhanced reactivity: invalidate reactive values that depend on tab selection
          # This triggers automatic updates for any element that depends on tab state
          if oldSelectedIndex != tabGroup.tabSelectedIndex:
            # Log the tab change for debugging
            echo "TAB CHANGED: from index ", oldSelectedIndex, " to index ", tabGroup.tabSelectedIndex

            # Trigger the onSelectedIndexChanged event for two-way binding
            if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
              echo "TRIGGERING: onSelectedIndexChanged event for TabGroup"
              tabGroup.eventHandlers["onSelectedIndexChanged"]()

            # Use the improved reactive system to invalidate related values
            # This automatically handles cross-element dependencies
            invalidateRelatedValues("tabSelectedIndex")

            # Also mark TabGroup and related elements as dirty for immediate visual updates
            markDirty(tabGroup)
            for child in tabGroup.children:
              markDirty(child)
              # Also mark TabContent's children (TabPanels) for complete re-render
              if child.kind == ekTabContent:
                echo "Marking TabContent children as dirty for tab change"
                for panel in child.children:
                  markDirty(panel)
                  # Recursively mark all descendants as dirty to ensure calendar updates
                  markAllDescendantsDirty(panel)

        # Trigger onClick handler if present
        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

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

        # Early return to prevent click-through to underlying elements
        return

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

              # Early return to prevent click-through to underlying elements
              return
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

proc handleKeyboardInput*(backend: var RaylibBackend, root: Element) =
  ## Handle keyboard input for focused input and dropdown elements

  # Handle checkbox keyboard input
  # For simplicity, we'll handle basic checkbox toggling with a focused element state
  # Note: This is a basic implementation. A full implementation would need checkbox focus management
  let mousePos = GetMousePosition()
  proc findCheckboxUnderMouse(elem: Element): Element =
    if elem.kind == ekCheckbox:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let checkboxSize = min(elem.height, fontSize.float + 8.0)
      let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

      let label = elem.getProp("label").get(val("")).getString()
      var clickArea = checkboxRect

      if label.len > 0:
        let textWidth = MeasureText(label.cstring, fontSize.cint).float
        clickArea.width = checkboxSize + 10.0 + textWidth
        clickArea.y = elem.y
        clickArea.height = elem.height

      if CheckCollisionPointRec(mousePos, clickArea):
        return elem

    # Check children
    for child in elem.children:
      let found = findCheckboxUnderMouse(child)
      if found != nil:
        return found

    return nil

  let hoveredCheckbox = findCheckboxUnderMouse(root)
  if hoveredCheckbox != nil:
    if IsKeyPressed(KEY_ENTER):
      # Toggle checkbox state
      var currentState = backend.checkboxStates.getOrDefault(hoveredCheckbox, false)
      currentState = not currentState
      backend.checkboxStates[hoveredCheckbox] = currentState

      # Trigger onClick handler if present
      if hoveredCheckbox.eventHandlers.hasKey("onClick"):
        hoveredCheckbox.eventHandlers["onClick"]()

      # Trigger onChange handler if present (pass the new state as data)
      if hoveredCheckbox.eventHandlers.hasKey("onChange"):
        let handler = hoveredCheckbox.eventHandlers["onChange"]
        handler($currentState)  # Pass boolean state as string

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
    backend.handleKeyboardInput(root)

    # Only recalculate layout when there are dirty elements (intelligent updates)
    if hasDirtyElements():
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
