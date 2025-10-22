## Core Drag-and-Drop System
##
## This module provides the core drag-and-drop functionality for the Kryon UI framework.
## It is backend-agnostic and handles all drag/drop logic, state management, and event triggering.
##
## Backends should only:
## - Call these functions when mouse events occur
## - Render visual feedback based on element.dragState and interaction state
## - NOT implement any drag/drop logic themselves

import ../core
import ../layout/zindexSort
import interactionState
import times
import options
import tables
import math

const
  DragActivationThreshold = 5.0

# ============================================================================
# Element Discovery
# ============================================================================

proc hasDraggableBehavior*(elem: Element): bool =
  ## Check if element has a Draggable behavior attached
  if elem.withBehaviors.len == 0:
    return false
  for behavior in elem.withBehaviors:
    if behavior.kind == ekDraggable:
      return true
  return false

proc hasDropTargetBehavior*(elem: Element): bool =
  ## Check if element has a DropTarget behavior attached
  if elem.withBehaviors.len == 0:
    return false
  for behavior in elem.withBehaviors:
    if behavior.kind == ekDropTarget:
      return true
  return false

proc getDraggableBehavior*(elem: Element): Element =
  ## Get the Draggable behavior element from an element
  if elem.withBehaviors.len > 0:
    for behavior in elem.withBehaviors:
      if behavior.kind == ekDraggable:
        return behavior
  return nil

proc getDropTargetBehavior*(elem: Element): Element =
  ## Get the DropTarget behavior element from an element
  if elem.withBehaviors.len > 0:
    for behavior in elem.withBehaviors:
      if behavior.kind == ekDropTarget:
        return behavior
  return nil

proc isPointInElement*(elem: Element, x, y: float): bool =
  ## Check if a point is within the element's bounds
  return x >= elem.x and x <= elem.x + elem.width and
         y >= elem.y and y <= elem.y + elem.height

proc findDraggableElementAtPoint*(elem: Element, x, y: float): Element =
  ## Recursively find a draggable element at the given point
  ## Returns the topmost draggable element at that position

  # Check children first (reverse z-order, highest first)
  let sortedChildren = sortChildrenByZIndexReverse(elem.children)
  for child in sortedChildren:
    let found = findDraggableElementAtPoint(child, x, y)
    if found != nil:
      return found

  # Then check this element
  if hasDraggableBehavior(elem) and isPointInElement(elem, x, y):
    return elem

  return nil

proc findDropTargetAtPoint*(elem: Element, x, y: float, itemType: string): Element =
  ## Recursively find a drop target element at the given point
  ## that accepts the given itemType
  ## Returns the topmost matching drop target

  # Check children first (reverse z-order, highest first)
  let sortedChildren = sortChildrenByZIndexReverse(elem.children)
  for child in sortedChildren:
    let found = findDropTargetAtPoint(child, x, y, itemType)
    if found != nil:
      return found

  # Then check this element
  if hasDropTargetBehavior(elem) and isPointInElement(elem, x, y):
    # Check if itemType matches
    let dropBehavior = getDropTargetBehavior(elem)
    let targetItemType = dropBehavior.getProp("itemType").get(val("")).getString()
    if targetItemType.len == 0 or targetItemType == itemType:
      return elem

  return nil

# ============================================================================
# Drag State Management
# ============================================================================

proc handleDragStart*(root: Element, mouseX, mouseY: float): bool =
  ## Initialize a drag operation if a draggable element is under the mouse
  ## Returns true if drag started, false otherwise

  # Only start if not already dragging
  if globalInteractionState.draggedElement != nil:
    return false

  let draggableElem = findDraggableElementAtPoint(root, mouseX, mouseY)
  if draggableElem == nil:
    return false

  # Start dragging
  globalInteractionState.draggedElement = draggableElem
  globalInteractionState.dragSource = draggableElem
  globalInteractionState.dragOffsetX = mouseX - draggableElem.x
  globalInteractionState.dragOffsetY = mouseY - draggableElem.y
  globalInteractionState.dragStartTime = epochTime()
  globalInteractionState.dragHasMoved = false
  draggableElem.dragState.elementStartX = draggableElem.x
  draggableElem.dragState.elementStartY = draggableElem.y

  # Store drag data from the Draggable behavior
  let dragBehavior = getDraggableBehavior(draggableElem)
  if dragBehavior != nil:
    let dragDataValue = dragBehavior.getProp("data").get(val(""))
    draggableElem.dragState.dragData = dragDataValue
    draggableElem.dragState.isDragging = true
    draggableElem.dragState.dragStartX = mouseX
    draggableElem.dragState.dragStartY = mouseY

  echo "[DRAG START] Started dragging element at (", draggableElem.x, ", ", draggableElem.y, ")"
  return true

proc handleDragMove*(root: Element, mouseX, mouseY: float) =
  ## Update drag state during mouse movement
  ## Updates element drag offsets, finds potential drop targets, and updates hover states

  # Only update if currently dragging
  if globalInteractionState.draggedElement == nil:
    return

  let draggedElem = globalInteractionState.draggedElement

  # Update drag state (apply axis locking if specified)
  let dragBehavior = getDraggableBehavior(draggedElem)
  var currentOffsetX = mouseX - draggedElem.dragState.dragStartX
  var currentOffsetY = mouseY - draggedElem.dragState.dragStartY

  # Apply axis locking
  if dragBehavior != nil:
    let lockAxis = dragBehavior.getProp("lockAxis").get(val("none")).getString()
    if lockAxis == "x":
      # Lock to horizontal movement only
      currentOffsetY = 0.0
    elif lockAxis == "y":
      # Lock to vertical movement only
      currentOffsetX = 0.0

  draggedElem.dragState.currentOffsetX = currentOffsetX
  draggedElem.dragState.currentOffsetY = currentOffsetY

  if (not globalInteractionState.dragHasMoved) and
     max(abs(currentOffsetX), abs(currentOffsetY)) >= DragActivationThreshold:
    globalInteractionState.dragHasMoved = true

  # Find potential drop target under mouse
  if dragBehavior != nil:
    let itemType = dragBehavior.getProp("itemType").get(val("")).getString()
    let dropTarget = findDropTargetAtPoint(root, mouseX, mouseY, itemType)

    # Update hover states
    if globalInteractionState.potentialDropTarget != dropTarget:
      # Clear old target hover state
      if globalInteractionState.potentialDropTarget != nil:
        globalInteractionState.potentialDropTarget.dropTargetState.isHovered = false

      # Set new target hover state
      globalInteractionState.potentialDropTarget = dropTarget
      if dropTarget != nil:
        dropTarget.dropTargetState.isHovered = true
        dropTarget.dropTargetState.canAcceptDrop = true
        echo "[HOVER] Hovering over drop target: ", dropTarget.kind, " id=", dropTarget.id

proc handleDragEnd*(): bool =
  ## Finalize a drag operation
  ## Triggers onDrop event if over a valid drop target
  ## Returns true if drop was successful, false otherwise

  # Only end if currently dragging
  if globalInteractionState.draggedElement == nil:
    return false

  let draggedElem = globalInteractionState.draggedElement
  let dropTarget = globalInteractionState.potentialDropTarget

  var dropSuccessful = false

  # Check if we're over a valid drop target
  if dropTarget != nil:
    let dropBehavior = getDropTargetBehavior(dropTarget)
    if dropBehavior != nil and dropBehavior.eventHandlers.hasKey("onDrop"):
      # Trigger the onDrop event with the drag data
      let dragData = draggedElem.dragState.dragData
      var dataStr = if dragData.kind == vkString:
        dragData.getString()
      elif dragData.kind == vkInt:
        $dragData.getInt()
      elif dragData.kind == vkFloat:
        $dragData.getFloat()
      else:
        ""

      # If we have a calculated insert index, append it to the data
      # Format: "originalData|insertIndex"
      if globalInteractionState.dragInsertIndex >= 0:
        dataStr = dataStr & "|" & $globalInteractionState.dragInsertIndex
        echo "[DROP] Drop successful! Data: ", dataStr, " (insert index: ", globalInteractionState.dragInsertIndex, ")"
      else:
        echo "[DROP] Drop successful! Data: ", dataStr

      let handler = dropBehavior.eventHandlers["onDrop"]
      handler(dataStr)

      dropSuccessful = true

  # Clean up drag state
  draggedElem.dragState.isDragging = false
  draggedElem.dragState.currentOffsetX = 0.0
  draggedElem.dragState.currentOffsetY = 0.0

  if dropTarget != nil:
    dropTarget.dropTargetState.isHovered = false
    dropTarget.dropTargetState.canAcceptDrop = false

  # Reset interaction state (but preserve reordering state - handled separately)
  globalInteractionState.draggedElement = nil
  globalInteractionState.dragSource = nil
  globalInteractionState.potentialDropTarget = nil
  globalInteractionState.dragOffsetX = 0.0
  globalInteractionState.dragOffsetY = 0.0
  globalInteractionState.dragInsertIndex = -1
  globalInteractionState.dragHasMoved = false

  echo "[DRAG END] Drag ended"
  return dropSuccessful
