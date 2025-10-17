## Layout Engine
##
## This module contains the shared layout calculation logic that works across all backends.
## The layout engine is backend-agnostic and only requires text measurement capabilities.
##
## Extracted from the Raylib backend as the reference implementation.

import ../core
import options, algorithm, math

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
    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout
      var currentY = elem.y
      let gap = elem.getProp("gap").get(val(5)).getFloat()  # Default gap between elements

      # First pass: Calculate all children sizes and total height
      var totalHeight = 0.0
      var maxWidth = 0.0
      var childSizes: seq[tuple[w, h: float]] = @[]

      for child in elem.children:
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and measure active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              # First pass: Calculate active branch layout at (0, 0) to determine its size
              calculateLayout(measurer, activeBranch, 0, 0, elem.width, elem.height)
              childSizes.add((w: activeBranch.width, h: activeBranch.height))
              totalHeight += activeBranch.height
              maxWidth = max(maxWidth, activeBranch.width)
        else:
          # First pass: Calculate child layout at (0, 0) to determine its size
          calculateLayout(measurer, child, 0, 0, elem.width, elem.height)
          childSizes.add((w: child.width, h: child.height))
          totalHeight += child.height
          maxWidth = max(maxWidth, child.width)

      if elem.children.len > 1:
        totalHeight += gap * (elem.children.len - 1).float

      # Calculate starting Y position to center the group vertically
      let startY = elem.y + (elem.height - totalHeight) / 2.0

      # Second pass: Position children centered horizontally and stacked vertically
      currentY = startY
      var childIndex = 0
      for child in elem.children:
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and position active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              let centerX = elem.x + (elem.width - childSizes[childIndex].w) / 2.0

              # Apply gap between elements (but not after the last one)
              if childIndex > 0:
                currentY += gap

              # Recalculate at centered position
              calculateLayout(measurer, activeBranch, centerX, currentY, childSizes[childIndex].w, childSizes[childIndex].h)
              currentY += childSizes[childIndex].h
              inc childIndex
        else:
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
      let gap = elem.getProp("gap").get(val(5)).getFloat()  # Default gap between elements

      for child in elem.children:
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and layout active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              calculateLayout(measurer, activeBranch, elem.x, currentY, elem.width, elem.height)
              currentY += activeBranch.height + gap
        else:
          calculateLayout(measurer, child, elem.x, currentY, elem.width, elem.height)
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
        calculateLayout(measurer, child, 0, 0, elem.width, 0)
        # Now calculate sizes for the generated children
        for grandchild in child.children:
          calculateLayout(measurer, grandchild, 0, 0, elem.width, 0)
          childSizes.add((w: grandchild.width, h: grandchild.height))
      elif child.kind == ekConditional:
        # Conditional elements are transparent - evaluate and measure active branch
        if child.condition != nil:
          let conditionResult = child.condition()
          let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
          if activeBranch != nil:
            calculateLayout(measurer, activeBranch, 0, 0, elem.width, 0)
            childSizes.add((w: activeBranch.width, h: activeBranch.height))
      else:
        # Pass elem.width for width, but don't constrain height
        calculateLayout(measurer, child, 0, 0, elem.width, 0)
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
          calculateLayout(measurer, grandchild, childX, currentY, elem.width, childSizes[childIndex].h)
          currentY += childSizes[childIndex].h
          inc childIndex
      elif child.kind == ekConditional:
        # Conditional - position the active branch
        if child.condition != nil:
          let conditionResult = child.condition()
          let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
          if activeBranch != nil:
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

            # Layout active branch at final position
            calculateLayout(measurer, activeBranch, childX, currentY, elem.width, childSizes[childIndex].h)
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
        calculateLayout(measurer, child, childX, currentY, elem.width, childSizes[childIndex].h)
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
        calculateLayout(measurer, child, 0, 0, parentWidth, elem.height)
        # Now calculate sizes for the generated children
        for grandchild in child.children:
          calculateLayout(measurer, grandchild, 0, 0, parentWidth, elem.height)
          childSizes.add((w: grandchild.width, h: grandchild.height))
      elif child.kind == ekConditional:
        # Conditional elements are transparent - evaluate and measure active branch
        if child.condition != nil:
          let conditionResult = child.condition()
          let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
          if activeBranch != nil:
            calculateLayout(measurer, activeBranch, elem.x, elem.y, parentWidth, elem.height)
            childSizes.add((w: activeBranch.width, h: activeBranch.height))
      else:
        # Let children size themselves naturally (don't constrain width)
        calculateLayout(measurer, child, elem.x, elem.y, parentWidth, elem.height)
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
          calculateLayout(measurer, grandchild, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
          currentX += childSizes[childIndex].w
          inc childIndex
      elif child.kind == ekConditional:
        # Conditional - position the active branch
        if child.condition != nil:
          let conditionResult = child.condition()
          let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
          if activeBranch != nil:
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

            # Layout active branch at final position
            calculateLayout(measurer, activeBranch, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
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
        calculateLayout(measurer, child, currentX, childY, childSizes[childIndex].w, childSizes[childIndex].h)
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
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and layout active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              # First pass: Calculate active branch layout at (0, 0) to determine its size
              calculateLayout(measurer, activeBranch, 0, 0, elem.width, elem.height)

              # Second pass: Now we know width and height, so center it
              let centerX = elem.x + (elem.width - activeBranch.width) / 2.0
              let centerY = elem.y + (elem.height - activeBranch.height) / 2.0

              # Recalculate at centered position
              calculateLayout(measurer, activeBranch, centerX, centerY, activeBranch.width, activeBranch.height)
        else:
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
      for child in elem.children:
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and layout active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              calculateLayout(measurer, activeBranch, elem.x, currentY, elem.width, elem.height)
              currentY += activeBranch.height + gap
        else:
          calculateLayout(measurer, child, elem.x, currentY, elem.width, elem.height)
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
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and layout active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              # First pass: Calculate active branch layout at (0, 0) to determine its size
              calculateLayout(measurer, activeBranch, 0, 0, elem.width, elem.height)

              # Second pass: Now we know width and height, so center it
              let centerX = elem.x + (elem.width - activeBranch.width) / 2.0
              let centerY = elem.y + (elem.height - activeBranch.height) / 2.0

              # Recalculate at centered position
              calculateLayout(measurer, activeBranch, centerX, centerY, activeBranch.width, activeBranch.height)
        else:
          # First pass: Calculate child layout at (0, 0) to determine its size
          calculateLayout(measurer, child, 0, 0, elem.width, elem.height)

          # Second pass: Now we know child.width and child.height, so center it
          let centerX = elem.x + (elem.width - child.width) / 2.0
          let centerY = elem.y + (elem.height - child.height) / 2.0

          # Recalculate at centered position
          calculateLayout(measurer, child, centerX, centerY, child.width, child.height)

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
        if child.kind == ekConditional:
          # Conditional elements are transparent - evaluate and layout active branch
          if child.condition != nil:
            let conditionResult = child.condition()
            let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
            if activeBranch != nil:
              calculateLayout(measurer, activeBranch, elem.x, currentY, elem.width, elem.height)
              currentY += activeBranch.height + gap
        else:
          calculateLayout(measurer, child, elem.x, currentY, elem.width, elem.height)
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
        calculateLayout(measurer, child, elem.x, elem.y, elem.width, 0)

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
        calculateLayout(measurer, child, elem.x, elem.y, elem.width, elem.height)

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
        calculateLayout(measurer, panel, elem.x, elem.y, elem.width, elem.height)

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
      # THE FIX: Check for for-loop placeholders and expand them before layout.
      if child.kind == ekForLoop:
        # First, trigger the for-loop to generate its actual children.
        calculateLayout(measurer, child, contentX, currentY, contentWidth, 0)
        # Now, loop through the newly generated children and lay them out.
        for grandchild in child.children:
          calculateLayout(measurer, grandchild, contentX, currentY, contentWidth, 0)
          currentY += grandchild.height + gap
      elif child.kind == ekConditional:
        # Conditional elements are transparent - evaluate and layout active branch
        if child.condition != nil:
          let conditionResult = child.condition()
          let activeBranch = if conditionResult: child.trueBranch else: child.falseBranch
          if activeBranch != nil:
            calculateLayout(measurer, activeBranch, contentX, currentY, contentWidth, 0)
            currentY += activeBranch.height + gap
      else:
        # This is a regular child, lay it out normally.
        calculateLayout(measurer, child, contentX, currentY, contentWidth, 0)
        currentY += child.height + gap

  else:
    # Default: just layout children in same space as parent
    for child in elem.children:
      calculateLayout(measurer, child, elem.x, elem.y, elem.width, elem.height)

  # Restore previous element and mark this element as clean
  currentElementBeingProcessed = previousElement
  markClean(elem)
