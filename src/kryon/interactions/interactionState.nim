## Global Interaction State
##
## This module manages the global state for user interactions like drag-and-drop,
## tab reordering, and other interactive behaviors.
##
## This state is shared across the framework and accessed by both core logic
## and backends for coordination.

import ../core
import tables

type
  InteractionState* = object
    ## Global state for drag-and-drop and reordering interactions

    # Drag-and-drop state
    draggedElement*: Element            ## Currently dragging element (the visual element being dragged)
    dragSource*: Element                ## Original source element that initiated the drag
    potentialDropTarget*: Element       ## Element currently under the dragged item
    dragOffsetX*, dragOffsetY*: float   ## Offset from element origin to mouse position
    dragStartTime*: float               ## Timestamp when drag started
    dragInsertIndex*: int               ## Calculated insert position for reordering (e.g., tab index)

    # Live reordering state
    reorderableContainer*: Element      ## Container being reordered (TabBar, Row, etc.)
    originalChildOrder*: seq[Element]   ## Original child order before drag started
    liveChildOrder*: seq[Element]       ## Live order during drag (for rendering)
    draggedChildIndex*: int             ## Index of child being dragged
    isLiveReordering*: bool             ## Flag indicating live reordering is active

    # Tab content override during drag
    dragTabContentPanel*: Element       ## Original tab panel content for dragged tab
    dragOriginalTabIndex*: int          ## Original tabIndex value when drag started
    dragOriginalContentMapping*: Table[int, Element]  ## Maps original visual tab order positions to TabPanel elements
    shouldClearDragOverride*: bool      ## Flag to clear drag content override on next frame

# Global singleton instance
var globalInteractionState* = InteractionState(
  draggedElement: nil,
  dragSource: nil,
  potentialDropTarget: nil,
  dragOffsetX: 0.0,
  dragOffsetY: 0.0,
  dragStartTime: 0.0,
  dragInsertIndex: -1,
  reorderableContainer: nil,
  originalChildOrder: @[],
  liveChildOrder: @[],
  draggedChildIndex: -1,
  isLiveReordering: false,
  dragTabContentPanel: nil,
  dragOriginalTabIndex: -1,
  dragOriginalContentMapping: initTable[int, Element](),
  shouldClearDragOverride: false
)

proc getInteractionState*(): var InteractionState =
  ## Get the global interaction state
  ## This is the single source of truth for all interactions
  return globalInteractionState

proc resetDragState*() =
  ## Reset drag-and-drop state to initial values
  globalInteractionState.draggedElement = nil
  globalInteractionState.dragSource = nil
  globalInteractionState.potentialDropTarget = nil
  globalInteractionState.dragOffsetX = 0.0
  globalInteractionState.dragOffsetY = 0.0
  globalInteractionState.dragStartTime = 0.0
  globalInteractionState.dragInsertIndex = -1

proc resetReorderState*() =
  ## Reset live reordering state to initial values
  globalInteractionState.reorderableContainer = nil
  globalInteractionState.originalChildOrder = @[]
  globalInteractionState.liveChildOrder = @[]
  globalInteractionState.draggedChildIndex = -1
  globalInteractionState.isLiveReordering = false

proc resetTabContentOverride*() =
  ## Reset tab content override state
  globalInteractionState.dragTabContentPanel = nil
  globalInteractionState.dragOriginalTabIndex = -1
  globalInteractionState.dragOriginalContentMapping.clear()
  globalInteractionState.shouldClearDragOverride = false

proc resetAllInteractions*() =
  ## Reset all interaction state to initial values
  resetDragState()
  resetReorderState()
  resetTabContentOverride()
