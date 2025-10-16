## Checkbox State Manager
##
## This module manages checkbox checked/unchecked state.
## Extracted from backend implementations to eliminate duplication.

import ../core
import backendState
import tables

proc initCheckbox*(state: var BackendState, elem: Element, initialChecked: bool) =
  ## Initialize a checkbox with an initial checked state
  ## Only sets the state if it doesn't already exist
  if not state.checkboxStates.hasKey(elem):
    state.checkboxStates[elem] = initialChecked

proc isChecked*(state: BackendState, elem: Element, defaultValue: bool = false): bool =
  ## Get the checked state of a checkbox
  ## Returns defaultValue if checkbox state hasn't been initialized
  return state.checkboxStates.getOrDefault(elem, defaultValue)

proc setChecked*(state: var BackendState, elem: Element, checked: bool) =
  ## Set the checked state of a checkbox
  state.checkboxStates[elem] = checked

proc toggleCheckbox*(state: var BackendState, elem: Element) =
  ## Toggle a checkbox's checked state
  let currentState = state.checkboxStates.getOrDefault(elem, false)
  state.checkboxStates[elem] = not currentState

proc clearCheckboxState*(state: var BackendState) =
  ## Clear all checkbox state (useful for cleanup or reset)
  state.checkboxStates.clear()
