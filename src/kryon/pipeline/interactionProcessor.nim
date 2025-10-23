## Interaction Processor
##
## This module processes user interactions and updates element state accordingly.
## It does NOT render - it only modifies the element tree and interaction state.
##
## Responsibilities:
## - Process mouse clicks and determine which element was clicked
## - Update drag-and-drop state
## - Process tab reordering (visual position updates)
## - Manage focus state
## - Update input/checkbox/dropdown states
##
## The rendering pipeline calls this AFTER layout but BEFORE extraction.

import ../core
import ../interactions/interactionState
import ../interactions/dragDrop
import ../state/backendState
import ../state/checkboxManager
import ../state/dropdownManager
import options
import tables
import math
import algorithm

# ============================================================================
# Rectangle Helper
# ============================================================================

type
  Rect* = object
    x*, y*, width*, height*: float

proc rect*(x, y, width, height: float): Rect =
  ## Create a rectangle
  Rect(x: x, y: y, width: width, height: height)

# ============================================================================
# Hit Testing
# ============================================================================

proc isPointInElement*(elem: Element, x, y: float): bool =
  ## Check if a point is inside an element's bounds
  x >= elem.x and x <= elem.x + elem.width and
  y >= elem.y and y <= elem.y + elem.height

proc findElementAt*(root: Element, x, y: float): Option[Element] =
  ## Find the topmost element at the given coordinates
  ## Returns the deepest child that contains the point
  ## Respects z-index ordering

  # Sort children by z-index (highest first) for proper hit testing
  var sortedChildren = root.children
  sortedChildren.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(bZIndex, aZIndex)  # Highest z-index first
  )

  # Check children in z-index order (highest first)
  for child in sortedChildren:
    let found = findElementAt(child, x, y)
    if found.isSome:
      return found

  # Check this element
  if isPointInElement(root, x, y):
    return some(root)

  return none(Element)

# ============================================================================
# Click Processing
# ============================================================================

proc findOpenDropdownAt*(root: Element, x, y: float): Option[Element] =
  ## Find if any open dropdown menu contains the given point
  ## This includes the dropdown button and the dropdown menu area

  proc checkDropdownAndMenu(elem: Element, x, y: float): Option[Element] =
    if elem.kind == ekDropdown and isDropdownOpen(elem):
      # Check dropdown button area
      if x >= elem.x and x <= elem.x + elem.width and
         y >= elem.y and y <= elem.y + elem.height:
        return some(elem)

      # Check dropdown menu area
      if elem.dropdownOptions.len > 0:
        let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
        let itemHeight = fontSize.float + 10.0
        let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
        let menuX = elem.x
        let menuY = elem.y + elem.height
        let menuWidth = elem.width
        let menuHeight = dropdownHeight

        if x >= menuX and x <= menuX + menuWidth and
           y >= menuY and y <= menuY + menuHeight:
          return some(elem)

    # Check children recursively
    for child in elem.children:
      let found = checkDropdownAndMenu(child, x, y)
      if found.isSome:
        return found

    return none(Element)

  result = checkDropdownAndMenu(root, x, y)

proc closeAllDropdowns*(elem: Element, state: var BackendState) =
  ## Close all dropdowns in the element tree and reset their z-index
  if elem.kind == ekDropdown and isDropdownOpen(elem):
    closeDropdown(state, elem)
    elem.setProp("zIndex", val(0))

  for child in elem.children:
    closeAllDropdowns(child, state)

proc processClick*(root: Element, state: var BackendState, x, y: float) =
  ## Process a mouse click at the given coordinates
  ## Fires onClick handlers for the clicked element

  # First, check if clicking on any open dropdown menu (highest priority)
  let openDropdown = findOpenDropdownAt(root, x, y)
  if openDropdown.isSome:
    let dropdown = openDropdown.get()

    # Check if clicking on dropdown options
    if isDropdownOpen(dropdown) and dropdown.dropdownOptions.len > 0:
      let fontSize = dropdown.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(dropdown.dropdownOptions.len.float * itemHeight, 200.0)
      let menuX = dropdown.x
      let menuY = dropdown.y + dropdown.height
      let menuWidth = dropdown.width
      let menuHeight = dropdownHeight

      if x >= menuX and x <= menuX + menuWidth and
         y >= menuY and y <= menuY + menuHeight:
        # Click is within dropdown menu area - select option
        let relativeY = y - menuY
        let clickedIndex = int(relativeY / itemHeight)

        if clickedIndex >= 0 and clickedIndex < dropdown.dropdownOptions.len:
          # Select the clicked option using dropdown manager
          selectDropdownOption(state, dropdown, clickedIndex)

          # Fire change handlers
          let selectedValue = getSelectedOption(dropdown)
          if dropdown.eventHandlers.hasKey("onChange"):
            let handler = dropdown.eventHandlers["onChange"]
            handler(selectedValue)

          if dropdown.eventHandlers.hasKey("onSelectionChange"):
            let handler = dropdown.eventHandlers["onSelectionChange"]
            handler(selectedValue)

        # Close dropdown after selection
        closeDropdown(state, dropdown)
        dropdown.setProp("zIndex", val(0))
        return

    # If clicking on the dropdown button itself, just close it
    closeDropdown(state, dropdown)
    dropdown.setProp("zIndex", val(0))
    return

  # If not clicking on any open dropdown, proceed with normal click processing
  let clicked = findElementAt(root, x, y)
  if clicked.isSome:
    let elem = clicked.get()

    # If opening a new dropdown, close all other dropdowns first
    if elem.kind == ekDropdown:
      closeAllDropdowns(root, state)

    # Fire onClick handler if present
    if elem.eventHandlers.hasKey("onClick"):
      elem.eventHandlers["onClick"]()

    # Handle component-specific click behavior
    case elem.kind:
    of ekButton:
      # Button clicks are handled via onClick
      discard

    of ekCheckbox:
      # Toggle checkbox state using the checkbox manager
      toggleCheckbox(state, elem)

      # Fire onChange handler if present
      if elem.eventHandlers.hasKey("onChange"):
        let currentState = isChecked(state, elem)
        let handler = elem.eventHandlers["onChange"]
        handler($currentState)

    of ekDropdown:
      # Check if clicking on dropdown options when open
      if isDropdownOpen(elem) and elem.dropdownOptions.len > 0:
        let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
        let itemHeight = fontSize.float + 10.0
        let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
        let dropdownRect = rect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

        if y >= dropdownRect.y and y < dropdownRect.y + dropdownHeight:
          # Click is within dropdown menu area - block this click from passing through
          let relativeY = y - dropdownRect.y
          let clickedIndex = int(relativeY / itemHeight)

          if clickedIndex >= 0 and clickedIndex < elem.dropdownOptions.len:
            # Select the clicked option using dropdown manager
            selectDropdownOption(state, elem, clickedIndex)

            # Fire change handlers
            let selectedValue = getSelectedOption(elem)
            if elem.eventHandlers.hasKey("onChange"):
              let handler = elem.eventHandlers["onChange"]
              handler(selectedValue)

            if elem.eventHandlers.hasKey("onSelectionChange"):
              let handler = elem.eventHandlers["onSelectionChange"]
              handler(selectedValue)

          # Close dropdown after selection (selectDropdownOption already does this)
          elem.setProp("zIndex", val(0))
          return

      # Toggle dropdown open/close with z-index management
      if isDropdownOpen(elem):
        closeDropdown(state, elem)
        elem.setProp("zIndex", val(0))
      else:
        openDropdown(state, elem)
        elem.setProp("zIndex", val(1000))

    of ekTab:
      # Switch active tab
      # Find parent TabGroup and update selected index
      var parent = elem.parent
      while parent != nil:
        if parent.kind == ekTabGroup:
          # Fire tab change event
          if parent.eventHandlers.hasKey("onSelectedIndexChanged"):
            parent.eventHandlers["onSelectedIndexChanged"]()
          break
        parent = parent.parent

    else:
      discard

  # If clicking outside any dropdown, close all dropdowns
  if clicked.isNone or clicked.get().kind != ekDropdown:
    closeAllDropdowns(root, state)

# ============================================================================
# Drag Processing
# ============================================================================

proc processDragStart*(root: Element, x, y: float) =
  ## Start dragging an element if it's draggable
  discard handleDragStart(root, x, y)

proc processDragUpdate*(root: Element, x, y: float) =
  ## Update drag position
  handleDragMove(root, x, y)

proc processDragEnd*() =
  ## End the current drag operation
  discard handleDragEnd()

# ============================================================================
# Focus Management
# ============================================================================

var focusedElement: Element = nil

proc setFocus*(elem: Element) =
  ## Set focus to an element
  focusedElement = elem

proc clearFocus*() =
  ## Clear the current focus
  focusedElement = nil

proc getFocusedElement*(): Element =
  ## Get the currently focused element
  focusedElement

proc processFocusClick*(root: Element, x, y: float) =
  ## Process a click for focus management
  let clicked = findElementAt(root, x, y)

  if clicked.isSome:
    let elem = clicked.get()

    # Set focus on inputs
    if elem.kind == ekInput:
      setFocus(elem)
    else:
      # Clicking outside an input clears focus
      if focusedElement != nil and focusedElement.kind == ekInput:
        clearFocus()

# ============================================================================
# Main Interaction Processing
# ============================================================================

proc processInteractions*(root: Element, state: var BackendState, mouseX, mouseY: float, mousePressed: bool, mouseReleased: bool) =
  ## Process all interactions for the current frame
  ## Call this after layout but before extraction

  # Handle mouse press
  if mousePressed:
    processClick(root, state, mouseX, mouseY)
    processFocusClick(root, mouseX, mouseY)
    processDragStart(root, mouseX, mouseY)

  # Handle mouse drag
  let interactionState = getInteractionState()
  if interactionState.draggedElement != nil:
    processDragUpdate(root, mouseX, mouseY)

  # Handle mouse release
  if mouseReleased:
    processDragEnd()
