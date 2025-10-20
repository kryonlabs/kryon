## Layout Engine
##
## This module contains the shared layout calculation logic that works across all backends.
## The layout engine is backend-agnostic and only requires text measurement capabilities.
##
## Extracted from the Raylib backend as the reference implementation.

import ../core
import options, algorithm, math

# ============================================================================
# Child Resolution Helpers
# ============================================================================

proc triggerControlFlowGeneration*[T](measurer: T, elem: Element, parentWidth, parentHeight: float) =
  ## Trigger generation of control flow children without laying them out
  ## This is used to populate for-loop children before collecting them
  case elem.kind:
  of ekForLoop:
    if elem.forBuilder != nil:
      # Use custom builder for type-preserving loops
      elem.children.setLen(0)
      elem.forBuilder(elem)
    elif elem.forIterable != nil and elem.forBodyTemplate != nil:
      # Generate elements for each item in the iterable
      let items = elem.forIterable()
      elem.children.setLen(0)
      for item in items:
        let childElement = elem.forBodyTemplate(item)
        if childElement != nil:
          addChild(elem, childElement)
  of ekConditional:
    # Conditionals don't need generation, they already have branches
    discard
  else:
    discard

proc getActualChildren*[T](measurer: T, elem: Element, parentWidth, parentHeight: float): seq[Element] =
  ## Recursively resolve children, expanding control flow elements
  ## This provides a unified way to handle control flow across all containers
  ##
  ## Returns a flat list of actual renderable children, expanding:
  ## - ekForLoop -> generated children
  ## - ekConditional -> active branch children
  ## - Regular elements -> themselves
  result = @[]

  for child in elem.children:
    case child.kind:
    of ekForLoop:
      # Trigger for loop generation first
      triggerControlFlowGeneration(measurer, child, parentWidth, parentHeight)
      # Add all generated children (recursively handle nested control flow)
      for grandchild in child.children:
        result.add(grandchild)

    of ekConditional:
      # Evaluate condition and get active branch
      if child.condition != nil:
        let conditionResult = child.condition()
        let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
        if activeBranch != nil:
          # Recursively get actual children from the active branch
          # This handles nested control flow (if inside for, for inside if, etc.)
          result.add(getActualChildren(measurer, activeBranch, parentWidth, parentHeight))
    else:
      # Regular element - add directly
      result.add(child)

# ============================================================================
# Layout Calculation
# ============================================================================

proc calculateLayout*[T](measurer: T, elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Recursively calculate layout for all elements
  ## Normal layout calculation - reactivity is handled by dirty flags triggering recalculation
  ##
  ## Uses duck typing for text measurement - measurer must implement:
  ##   proc measureText(measurer: T, text: string, fontSize: int): tuple[width: float, height: float]
  ##
  ## Args:
  ##   measurer: Backend-specific text measurer for determining text dimensions
  ##   elem: Element to layout
  ##   x, y: Position coordinates
  ##   parentWidth, parentHeight: Parent container dimensions

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

    # Measure text dimensions using backend's text measurer
    let (textWidth, textHeight) = measurer.measureText(text, fontSize)

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: textWidth
    elem.height = if heightOpt.isSome: heightOpt.get.getFloat() else: textHeight

  elif elem.kind == ekButton:
    # Special handling for Button elements - size based on text content with padding
    let text = elem.getProp("text").get(val("Button")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Measure text dimensions and add padding
    let textWidth = measurer.measureTextWidth(text, fontSize)
    let textHeight = fontSize.float
    let padding = 20.0  # Horizontal padding
    let verticalPadding = 10.0  # Vertical padding

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: textWidth + (padding * 2.0)
    elem.height = if heightOpt.isSome: heightOpt.get.getFloat() else: textHeight + (verticalPadding * 2.0)

  elif elem.kind == ekInput:
    # Special handling for Input elements - size based on fontSize with padding
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()
    let value = elem.getProp("value").get(val("")).getString()

    # Calculate default dimensions based on fontSize
    let textHeight = fontSize.float
    let padding = 8.0  # Padding inside the input (matches input.nim)
    let verticalPadding = 8.0  # Vertical padding
    let minWidth = 200.0  # Minimum width for usability

    # If value is provided and no explicit width, size to fit content (with some extra space)
    let contentWidth = if value.len > 0:
      measurer.measureTextWidth(value, fontSize) + (padding * 2.0) + 20.0  # Extra space for typing
    else:
      minWidth

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: max(minWidth, contentWidth)
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

  of ekResources, ekFont:
    # Resource elements do not participate in layout
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
        calculateLayout(measurer, activeBranch, elem.x, elem.y, parentWidth, parentHeight)

  of ekForLoop:
    # For loop elements generate dynamic content - let parent handle layout
    if elem.forBuilder != nil:
      # Use custom builder for type-preserving loops
      elem.children.setLen(0)
      elem.forBuilder(elem)

      # For loop is transparent to layout - use parent size
      elem.width = parentWidth
      elem.height = parentHeight

    elif elem.forIterable != nil and elem.forBodyTemplate != nil:
      # The dependency on the iterable is already registered in the for loop template
      # This ensures proper reactive updates when the underlying data changes
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
        echo "Debug: ekForLoop with no builder and nil iterable/bodyTemplate"

  of ekBody:
    # Body implements natural document flow - stack children vertically
    # Check for contentAlignment property
    let alignment = elem.getProp("contentAlignment")
    let gap = elem.getProp("gap").get(val(5)).getFloat()  # Default gap between elements

    # Get all actual children (expanding control flow)
    let actualChildren = getActualChildren(measurer, elem, elem.width, elem.height)

    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout

      # First pass: Calculate all children sizes and total height
      var totalHeight = 0.0
      var maxWidth = 0.0
      var childSizes: seq[tuple[w, h: float]] = @[]

      for child in actualChildren:
        # First pass: Calculate child layout at (0, 0) to determine its size
        calculateLayout(measurer, child, 0, 0, elem.width, elem.height)
        childSizes.add((w: child.width, h: child.height))
        totalHeight += child.height
        maxWidth = max(maxWidth, child.width)

      if actualChildren.len > 1:
        totalHeight += gap * (actualChildren.len - 1).float

      # Calculate starting Y position to center the group vertically
      let startY = elem.y + (elem.height - totalHeight) / 2.0

      # Second pass: Position children centered horizontally and stacked vertically
      var currentY = startY
      var childIndex = 0
      for child in actualChildren:
        let centerX = elem.x + (elem.width - childSizes[childIndex].w) / 2.0

        # Apply gap between elements (but not after the last one)
        if childIndex > 0:
          currentY += gap

        # Recalculate at centered position
        calculateLayout(measurer, child, centerX, currentY, childSizes[childIndex].w, childSizes[childIndex].h)
        currentY += childSizes[childIndex].h
        inc childIndex
    else:
      # Default: normal relative positioning layout
      var currentY = elem.y

      for child in actualChildren:
        calculateLayout(measurer, child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

  of ekColumn:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # For Column: mainAxis = vertical (Y), crossAxis = horizontal (X)

    # First pass: Get actual children (expanding control flow) and calculate sizes
    var childSizes: seq[tuple[w, h: float]] = @[]
    let actualChildren = getActualChildren(measurer, elem, elem.width, 0)

    # Calculate sizes for all actual children
    for child in actualChildren:
      # Pass elem.width for width, but don't constrain height
      calculateLayout(measurer, child, 0, 0, elem.width, 0)
      childSizes.add((w: child.width, h: child.height))

    # Calculate total height and max width
    var totalHeight = 0.0
    var maxWidth = 0.0
    for size in childSizes:
      totalHeight += size.h
      maxWidth = max(maxWidth, size.w)
    if actualChildren.len > 1:
      totalHeight += gap * (actualChildren.len - 1).float

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
    for child in actualChildren:
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

      # Layout child at final position
      calculateLayout(measurer, child, childX, currentY, elem.width, childSizes[childIndex].h)
      currentY += childSizes[childIndex].h
      inc childIndex

  of ekRow:
    let gap = elem.getProp("gap").get(val(0)).getFloat()
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    let crossAxisAlignment = elem.getProp("crossAxisAlignment")

    # For Row: mainAxis = horizontal (X), crossAxis = vertical (Y)

    # First pass: Get actual children (expanding control flow) and calculate sizes
    var childSizes: seq[tuple[w, h: float]] = @[]
    let actualChildren = getActualChildren(measurer, elem, parentWidth, elem.height)

    # Calculate sizes for all actual children
    for child in actualChildren:
      # Let children size themselves naturally (don't constrain width)
      calculateLayout(measurer, child, elem.x, elem.y, parentWidth, elem.height)
      childSizes.add((w: child.width, h: child.height))

    # Calculate total width and max height
    var totalWidth = 0.0
    var maxHeight = 0.0
    for size in childSizes:
      totalWidth += size.w
      maxHeight = max(maxHeight, size.h)
    if actualChildren.len > 1:
      totalWidth += gap * (actualChildren.len - 1).float

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
    for child in actualChildren:
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

      # Layout child at final position
      calculateLayout(measurer, child, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
      currentX += childSizes[childIndex].w
      inc childIndex

  of ekCenter:
    # Center is just like a Container but with contentAlignment = "center" by default
    let gap = elem.getProp("gap").get(val(0)).getFloat()

    # Check for contentAlignment property (default to center)
    let alignment = elem.getProp("contentAlignment").get(val("center"))

    # Get all actual children (expanding control flow)
    let actualChildren = getActualChildren(measurer, elem, elem.width, elem.height)

    if alignment.getString() == "center":
      # Center children - needs two-pass layout
      for child in actualChildren:
        # First pass: Calculate child layout at (0, 0) to determine its size
        calculateLayout(measurer, child, 0, 0, elem.width, elem.height)

        # Second pass: Now we know child.width and child.height, so center it
        let centerX = elem.x + (elem.width - child.width) / 2.0
        let centerY = elem.y + (elem.height - child.height) / 2.0

        # Recalculate at centered position
        calculateLayout(measurer, child, centerX, centerY, child.width, child.height)
    else:
      # Default: normal relative positioning layout
      var currentY = elem.y
      for child in actualChildren:
        calculateLayout(measurer, child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

  of ekContainer:
    # Container is a normal wrapper - relative positioning by default
    let gap = elem.getProp("gap").get(val(0)).getFloat()  # No gap by default

    # Check for contentAlignment property
    let alignment = elem.getProp("contentAlignment")

    # Get all actual children (expanding control flow)
    let actualChildren = getActualChildren(measurer, elem, elem.width, elem.height)

    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout
      for child in actualChildren:
        # First pass: Calculate child layout at (0, 0) to determine its size
        calculateLayout(measurer, child, 0, 0, elem.width, elem.height)

        # Second pass: Now we know child.width and child.height, so center it
        let centerX = elem.x + (elem.width - child.width) / 2.0
        let centerY = elem.y + (elem.height - child.height) / 2.0

        # Recalculate at centered position
        calculateLayout(measurer, child, centerX, centerY, child.width, child.height)

      # Auto-size height if not explicitly set (use max child height + padding)
      if not hasExplicitHeight and actualChildren.len > 0:
        var maxHeight = 0.0
        for child in actualChildren:
          maxHeight = max(maxHeight, child.height)
        let paddingValue = elem.getProp("padding").get(val(0)).getFloat()
        elem.height = maxHeight + (paddingValue * 2.0)
    else:
      # Default: normal relative positioning layout
      var currentY = elem.y
      for child in actualChildren:
        calculateLayout(measurer, child, elem.x, currentY, elem.width, elem.height)
        currentY += child.height + gap

      # Auto-size height if not explicitly set (use total stacked height)
      if not hasExplicitHeight and actualChildren.len > 0:
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
        calculateLayout(measurer, child, elem.x, currentY, elem.width, 0)
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

    # Get all actual children (expanding control flow)
    let allChildren = getActualChildren(measurer, elem, elem.width, 0)

    # Filter for Tab elements only
    var allTabs: seq[Element] = @[]
    for child in allChildren:
      if child.kind == ekTab:
        allTabs.add(child)

    # Second pass: assign indices and layout all tabs
    for tab in allTabs:
      tab.tabIndex = tabIndex
      tabIndex += 1
      # Calculate tab width based on title if not explicitly set
      let title = tab.getProp("title").get(val("Tab")).getString()
      let fontSize = tab.getProp("fontSize").get(val(16)).getInt()
      let textWidth = measurer.measureTextWidth(title, fontSize)
      let tabWidth = tab.getProp("width").get(val(textWidth + 40.0)).getFloat()
      let tabHeight = tab.getProp("height").get(val(40.0)).getFloat()
      maxHeight = max(maxHeight, tabHeight)

      calculateLayout(measurer, tab, currentX, elem.y, tabWidth, tabHeight)
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

    # Get all actual children (expanding control flow)
    let allChildren = getActualChildren(measurer, elem, elem.width, elem.height)

    # Filter for TabPanel elements only
    var allTabPanels: seq[Element] = @[]
    for child in allChildren:
      if child.kind == ekTabPanel:
        allTabPanels.add(child)

    var panelIndex = 0

    # Second pass: assign indices and layout only the active panel
    for panel in allTabPanels:
      panel.tabIndex = panelIndex
      panelIndex += 1
      # Only layout the active panel
      if panel.tabIndex == selectedIndex:
        calculateLayout(measurer, panel, elem.x, elem.y, elem.width, elem.height)

  of ekTabPanel:
    # TabPanel fills its parent space and lays out children vertically
    let gap = elem.getProp("gap").get(val(10)).getFloat()
    let padding = elem.getProp("padding").get(val(0)).getFloat()

    # Apply padding
    let contentX = elem.x + padding
    let contentY = elem.y + padding
    let contentWidth = elem.width - (padding * 2.0)

    # Get all actual children (expanding control flow)
    let actualChildren = getActualChildren(measurer, elem, contentWidth, 0)

    # Layout children vertically
    var currentY = contentY
    for child in actualChildren:
      calculateLayout(measurer, child, contentX, currentY, contentWidth, 0)
      currentY += child.height + gap

  else:
    # Default: just layout children in same space as parent
    for child in elem.children:
      calculateLayout(measurer, child, elem.x, elem.y, elem.width, elem.height)

  # Restore previous element and mark this element as clean
  currentElementBeingProcessed = previousElement
  markClean(elem)
