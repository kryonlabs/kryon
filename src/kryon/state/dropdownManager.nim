## Dropdown State Manager
##
## This module manages dropdown state including open/close, hover, and selection.
## Extracted from backend implementations to eliminate duplication.

import ../core
import backendState

proc openDropdown*(state: var BackendState, elem: Element) =
  ## Open a dropdown element
  ## Sets the hovered index to the currently selected item
  elem.dropdownIsOpen = true
  elem.dropdownHoveredIndex = elem.dropdownSelectedIndex
  state.focusedDropdown = elem

proc closeDropdown*(state: var BackendState, elem: Element) =
  ## Close a dropdown element
  elem.dropdownIsOpen = false
  elem.dropdownHoveredIndex = -1
  if state.focusedDropdown == elem:
    state.focusedDropdown = nil

proc toggleDropdown*(state: var BackendState, elem: Element) =
  ## Toggle dropdown open/close state
  if elem.dropdownIsOpen:
    closeDropdown(state, elem)
  else:
    openDropdown(state, elem)

proc isDropdownOpen*(elem: Element): bool =
  ## Check if a dropdown is currently open
  return elem.dropdownIsOpen

proc isFocusedDropdown*(state: BackendState, elem: Element): bool =
  ## Check if this dropdown is the focused one
  return state.focusedDropdown == elem

proc setDropdownHover*(elem: Element, index: int) =
  ## Set the hovered index for a dropdown
  ## Use -1 to clear hover
  elem.dropdownHoveredIndex = index

proc getDropdownHoverIndex*(elem: Element): int =
  ## Get the currently hovered index (-1 if none)
  return elem.dropdownHoveredIndex

proc selectDropdownOption*(state: var BackendState, elem: Element, index: int) =
  ## Select an option in a dropdown by index
  ## Automatically closes the dropdown after selection
  if index >= 0 and index < elem.dropdownOptions.len:
    elem.dropdownSelectedIndex = index
    closeDropdown(state, elem)

proc getSelectedIndex*(elem: Element): int =
  ## Get the currently selected index
  return elem.dropdownSelectedIndex

proc getSelectedOption*(elem: Element): string =
  ## Get the currently selected option text
  ## Returns empty string if no valid selection
  let idx = elem.dropdownSelectedIndex
  if idx >= 0 and idx < elem.dropdownOptions.len:
    return elem.dropdownOptions[idx]
  return ""

proc moveHoverUp*(elem: Element) =
  ## Move hover selection up (with wrapping)
  if elem.dropdownOptions.len == 0:
    return

  if elem.dropdownHoveredIndex <= 0:
    elem.dropdownHoveredIndex = elem.dropdownOptions.len - 1  # Wrap to bottom
  else:
    elem.dropdownHoveredIndex -= 1

proc moveHoverDown*(elem: Element) =
  ## Move hover selection down (with wrapping)
  if elem.dropdownOptions.len == 0:
    return

  if elem.dropdownHoveredIndex >= elem.dropdownOptions.len - 1:
    elem.dropdownHoveredIndex = 0  # Wrap to top
  else:
    elem.dropdownHoveredIndex += 1

proc selectHoveredOption*(state: var BackendState, elem: Element) =
  ## Select the currently hovered option
  let hoverIdx = elem.dropdownHoveredIndex
  if hoverIdx >= 0 and hoverIdx < elem.dropdownOptions.len:
    selectDropdownOption(state, elem, hoverIdx)

proc closeAllDropdowns*(state: var BackendState, root: Element, keepOpen: Element = nil) =
  ## Recursively close all dropdowns except the specified one
  proc closeDropdownsRecursive(elem: Element) =
    case elem.kind:
    of ekDropdown:
      if elem != keepOpen:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
    else:
      for child in elem.children:
        closeDropdownsRecursive(child)

  closeDropdownsRecursive(root)

  # Clear focused dropdown if it was closed
  if state.focusedDropdown != nil and state.focusedDropdown != keepOpen:
    state.focusedDropdown = nil
