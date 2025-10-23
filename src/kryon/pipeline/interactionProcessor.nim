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
import options
import tables

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

  # Check children first (reverse order for z-index)
  for i in countdown(root.children.high, 0):
    let child = root.children[i]
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

proc processClick*(root: Element, x, y: float) =
  ## Process a mouse click at the given coordinates
  ## Fires onClick handlers for the clicked element

  let clicked = findElementAt(root, x, y)
  if clicked.isSome:
    let elem = clicked.get()

    # Fire onClick handler if present
    if elem.eventHandlers.hasKey("onClick"):
      elem.eventHandlers["onClick"]()

    # Handle component-specific click behavior
    case elem.kind:
    of ekButton:
      # Button clicks are handled via onClick
      discard

    of ekCheckbox:
      # Toggle checkbox state
      # Note: In full refactoring, this will update centralized state
      discard

    of ekDropdown:
      # Toggle dropdown open/close
      # Note: In full refactoring, this will update centralized state
      discard

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

proc processInteractions*(root: Element, mouseX, mouseY: float, mousePressed: bool, mouseReleased: bool) =
  ## Process all interactions for the current frame
  ## Call this after layout but before extraction

  # Handle mouse press
  if mousePressed:
    processClick(root, mouseX, mouseY)
    processFocusClick(root, mouseX, mouseY)
    processDragStart(root, mouseX, mouseY)

  # Handle mouse drag
  let state = getInteractionState()
  if state.draggedElement != nil:
    processDragUpdate(root, mouseX, mouseY)

  # Handle mouse release
  if mouseReleased:
    processDragEnd()
