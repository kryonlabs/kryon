## Preprocessor
##
## This module handles the preprocessing stage of the rendering pipeline.
## It resolves control flow elements (ForLoops, Conditionals) into a flat
## element tree ready for layout and rendering.
##
## Responsibilities:
## - Resolve ForLoop elements by generating their children
## - Resolve Conditional elements by selecting the active branch
## - Handle manual reorder flags (for tab reordering)
## - Flatten the tree while preserving parent-child relationships
##
## This separates control flow logic from rendering logic, making both cleaner.

import ../core
import options

# ============================================================================
# ForLoop Resolution
# ============================================================================

proc resolveForLoop*(elem: Element): seq[Element] =
  ## Resolve a ForLoop element into its generated children
  ## Respects manual reorder flag for tab reordering

  # Check if manual reordering is active (tab drag-and-drop)
  let manualReorderProp = elem.getProp("__manualReorderActive")
  let manualReorderActive = manualReorderProp.isSome and manualReorderProp.get().getBool(false)

  if manualReorderActive and elem.children.len > 0:
    # During manual reordering (e.g., tab drag), preserve existing children
    # Don't regenerate from source data - use the manually reordered children
    return elem.children

  # Normal case: generate children from ForLoop data
  if elem.forBuilder != nil:
    # Use custom builder for type-preserving loops
    elem.children.setLen(0)
    elem.forBuilder(elem)
    return elem.children

  elif elem.forIterable != nil and elem.forBodyTemplate != nil:
    # Generate elements for each item in the iterable
    let items = elem.forIterable()
    elem.children.setLen(0)

    for item in items:
      let childElement = elem.forBodyTemplate(item)
      if childElement != nil:
        elem.addChild(childElement)

    return elem.children

  else:
    # No builder or iterable - return existing children
    return elem.children

# ============================================================================
# Conditional Resolution
# ============================================================================

proc resolveConditional*(elem: Element): Option[Element] =
  ## Resolve a Conditional element by evaluating its condition
  ## Returns the active branch (trueBranch or falseBranch)

  if elem.condition != nil:
    let conditionResult = elem.condition()
    let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
    return if activeBranch != nil: some(activeBranch) else: none(Element)

  return none(Element)

# ============================================================================
# Tree Preprocessing
# ============================================================================

proc preprocessElement*(elem: Element) =
  ## Recursively preprocess an element tree
  ## Resolves ForLoops and Conditionals in-place
  ## This modifies the tree to make it ready for layout

  case elem.kind:
  of ekForLoop:
    # Resolve ForLoop children
    discard resolveForLoop(elem)

    # Recursively preprocess the generated children
    for child in elem.children:
      preprocessElement(child)

  of ekConditional:
    # Conditionals are handled during rendering
    # We resolve them there to allow reactive updates
    # Just preprocess both branches
    if elem.trueBranch != nil:
      preprocessElement(elem.trueBranch)
    if elem.falseBranch != nil:
      preprocessElement(elem.falseBranch)

  else:
    # Recursively preprocess all children
    for child in elem.children:
      preprocessElement(child)

proc preprocessTree*(root: Element) =
  ## Preprocess the entire element tree
  ## This is the entry point for the preprocessing stage
  ## Call this before layout calculation

  preprocessElement(root)

# ============================================================================
# Utility Functions
# ============================================================================

proc enableManualReorder*(elem: Element) =
  ## Enable manual reorder mode for an element
  ## Used by tab reordering system
  elem.setProp("__manualReorderActive", true)

proc disableManualReorder*(elem: Element) =
  ## Disable manual reorder mode for an element
  ## ForLoop will regenerate from source data on next frame
  elem.setProp("__manualReorderActive", false)

proc isManualReorderActive*(elem: Element): bool =
  ## Check if manual reorder mode is active
  let prop = elem.getProp("__manualReorderActive")
  return prop.isSome and prop.get().getBool(false)
