## Backend State Management
##
## This module defines the shared state type used by interactive backends (Raylib, SDL2).
## This centralizes state management for inputs, dropdowns, checkboxes, and other interactive elements.

import ../core
import tables

type
  BackendState* = object
    ## Shared state for interactive UI backends
    ## Tracks focus, input values, and interactive component state

    # Input state
    focusedInput*: Element              ## Currently focused input element
    cursorBlink*: float                 ## Timer for cursor blinking animation
    inputValues*: Table[Element, string] ## Current text value for each input
    inputScrollOffsets*: Table[Element, float] ## Horizontal scroll offset for each input

    # Keyboard repeat state
    backspaceHoldTimer*: float          ## Timer for backspace key repeat
    backspaceRepeatDelay*: float        ## Initial delay before repeat starts (default: 0.5s)
    backspaceRepeatRate*: float         ## Rate of repeat deletion (default: 0.05s)

    # Dropdown state
    focusedDropdown*: Element           ## Currently open/focused dropdown element

    # Checkbox state
    checkboxStates*: Table[Element, bool] ## Checked state for each checkbox

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

proc newBackendState*(): BackendState =
  ## Create a new backend state with default values
  result = BackendState(
    focusedInput: nil,
    cursorBlink: 0.0,
    inputValues: initTable[Element, string](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05,   # 50ms repeat rate
    focusedDropdown: nil,
    checkboxStates: initTable[Element, bool](),
    # Drag-and-drop state
    draggedElement: nil,
    dragSource: nil,
    potentialDropTarget: nil,
    dragOffsetX: 0.0,
    dragOffsetY: 0.0,
    dragStartTime: 0.0,
    dragInsertIndex: -1,
    # Live reordering state
    reorderableContainer: nil,
    originalChildOrder: @[],
    liveChildOrder: @[],
    draggedChildIndex: -1,
    isLiveReordering: false
  )

proc updateCursorBlink*(state: var BackendState, deltaTime: float) =
  ## Update cursor blink timer
  ## Call this once per frame with frame delta time (e.g., 1.0/60.0 for 60fps)
  state.cursorBlink += deltaTime
  if state.cursorBlink >= 1.0:
    state.cursorBlink = 0.0

proc resetBackspaceTimer*(state: var BackendState) =
  ## Reset backspace hold timer (call when backspace is released or typing occurs)
  state.backspaceHoldTimer = 0.0
