## Input State Manager
##
## This module manages input field state including focus, cursor position, scrolling, and text values.
## Extracted from backend implementations to eliminate duplication.

import ../core
import backendState
import math
import tables

proc focusInput*(state: var BackendState, elem: Element, initialValue: string = "") =
  ## Focus an input element
  ## If the input doesn't have a stored value, initialize it with initialValue
  state.focusedInput = elem
  state.cursorBlink = 0.0  # Reset cursor blink

  if not state.inputValues.hasKey(elem):
    state.inputValues[elem] = initialValue

proc unfocusInput*(state: var BackendState, elem: Element) =
  ## Unfocus an input element if it's currently focused
  if state.focusedInput == elem:
    state.focusedInput = nil

proc isFocused*(state: BackendState, elem: Element): bool =
  ## Check if an input element is currently focused
  return state.focusedInput == elem

proc getInputValue*(state: BackendState, elem: Element, defaultValue: string = ""): string =
  ## Get the current value of an input element
  return state.inputValues.getOrDefault(elem, defaultValue)

proc setInputValue*(state: var BackendState, elem: Element, value: string) =
  ## Set the value of an input element
  state.inputValues[elem] = value

proc updateInputScroll*(state: var BackendState, elem: Element, cursorPos: float,
                       visibleWidth: float, textWidth: float) =
  ## Update scroll offset for an input to keep cursor visible
  ##
  ## Args:
  ##   elem: The input element
  ##   cursorPos: Horizontal position of cursor in text (pixels)
  ##   visibleWidth: Width of visible text area (pixels)
  ##   textWidth: Total width of text (pixels)
  var scrollOffset = state.inputScrollOffsets.getOrDefault(elem, 0.0)

  # If cursor is approaching right edge, scroll right to keep it visible
  if cursorPos - scrollOffset > visibleWidth - 20.0:
    scrollOffset = cursorPos - visibleWidth + 20.0

  # If cursor is approaching left edge, scroll left
  if cursorPos < scrollOffset + 20.0:
    scrollOffset = max(0.0, cursorPos - 20.0)

  # Don't scroll past the beginning
  scrollOffset = max(0.0, scrollOffset)

  # If text is shorter than visible area, reset scroll to 0
  if textWidth <= visibleWidth:
    scrollOffset = 0.0
  else:
    # Don't allow scrolling past the end of the text
    let maxScrollOffset = max(0.0, textWidth - visibleWidth)
    scrollOffset = min(scrollOffset, maxScrollOffset)

  # Store updated scroll offset
  state.inputScrollOffsets[elem] = scrollOffset

proc getInputScrollOffset*(state: BackendState, elem: Element): float =
  ## Get the current scroll offset for an input element
  return state.inputScrollOffsets.getOrDefault(elem, 0.0)

proc shouldShowCursor*(state: BackendState, elem: Element): bool =
  ## Check if cursor should be visible (based on blink timer)
  return state.focusedInput == elem and state.cursorBlink < 0.5

proc handleBackspaceRepeat*(state: var BackendState, isPressed: bool, deltaTime: float,
                           currentValue: var string): bool =
  ## Handle backspace key repeat logic
  ## Returns true if character(s) were deleted
  ##
  ## Args:
  ##   isPressed: Whether backspace key is currently held down
  ##   deltaTime: Frame delta time (e.g., 1.0/60.0)
  ##   currentValue: Current input value (will be modified if characters deleted)
  ##
  ## Returns:
  ##   True if text was modified, false otherwise
  if not isPressed or currentValue.len == 0:
    state.backspaceHoldTimer = 0.0
    return false

  # Increment hold timer
  state.backspaceHoldTimer += deltaTime

  # Check if we should delete more characters (after initial delay)
  if state.backspaceHoldTimer >= state.backspaceRepeatDelay:
    # Calculate how many characters to delete based on hold time
    let holdBeyondDelay = state.backspaceHoldTimer - state.backspaceRepeatDelay
    let charsToDelete = min(int(holdBeyondDelay / state.backspaceRepeatRate), currentValue.len)

    if charsToDelete > 0:
      currentValue.setLen(currentValue.len - charsToDelete)
      # Adjust timer to maintain repeat rate
      state.backspaceHoldTimer = state.backspaceRepeatDelay +
                                (charsToDelete.float * state.backspaceRepeatRate)
      return true

  return false

proc clearInputState*(state: var BackendState) =
  ## Clear all input-related state (useful for cleanup or reset)
  state.focusedInput = nil
  state.inputValues.clear()
  state.inputScrollOffsets.clear()
  state.backspaceHoldTimer = 0.0
