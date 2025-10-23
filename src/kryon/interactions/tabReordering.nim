## Tab Reordering System
##
## This module provides tab reordering functionality for TabBar elements.
## It handles both static tabs and dynamically generated tabs from ForLoops.
##
## The system works by:
## 1. Detecting when a Tab is being dragged within a reorderable TabBar
## 2. Calculating insert positions based on mouse X coordinate
## 3. Performing live visual reordering during drag
## 4. Finalizing tab order and syncing TabPanel content after drop

import ../core
import dragDrop
import interactionState
import tables

# Global variables to communicate reorder indices to app handlers
# Apps can read these in their onReorder/onDrop handlers
var lastTabReorderFromIndex* = -1
var lastTabReorderToIndex* = -1

# ============================================================================
# Tab Collection (ForLoop-aware)
# ============================================================================

proc collectActualTabs*(container: Element): seq[Element] =
  ## Recursively collect all Tab elements from a container, expanding ForLoops
  ## This handles both static tabs and tabs generated dynamically via for loops
  result = @[]
  for child in container.children:
    if child.kind == ekTab:
      result.add(child)
    elif child.kind == ekForLoop:
      # Recursively collect tabs from ForLoop children
      for grandchild in child.children:
        if grandchild.kind == ekTab:
          result.add(grandchild)

proc enableManualLoopOrder(container: Element) =
  ## Mark any for-loop children so they preserve manual order during drag
  if container == nil:
    return
  for child in container.children:
    if child.kind == ekForLoop:
      child.setProp(ManualReorderPropName, true)

proc disableManualLoopOrder(container: Element) =
  ## Clear manual reorder mode and rebuild for-loop children from source data
  if container == nil:
    return
  for child in container.children:
    if child.kind == ekForLoop:
      child.setProp(ManualReorderPropName, false)
      regenerateForLoopChildren(child)
      markDirty(child)
  markDirty(container)

proc collectTabPanels(tabContent: Element): seq[Element] =
  ## Collect TabPanel elements from TabContent, expanding ForLoops
  result = @[]
  if tabContent == nil:
    return
  for child in tabContent.children:
    case child.kind
    of ekTabPanel:
      result.add(child)
    of ekForLoop:
      for grandchild in child.children:
        if grandchild.kind == ekTabPanel:
          result.add(grandchild)
    else:
      discard

# ============================================================================
# Tab Reorder Initialization
# ============================================================================

proc initTabReorder*(tabBar: Element, draggedTab: Element): bool =
  ## Initialize tab reordering when a tab drag starts
  ## Sets up live reordering state and captures original tab order
  ## Returns true if reordering was initialized, false otherwise

  # Only initialize if this is a reorderable TabBar
  if tabBar.kind != ekTabBar:
    return false

  if not hasDropTargetBehavior(tabBar):
    return false

  # Enable live reordering mode
  globalInteractionState.isLiveReordering = true
  globalInteractionState.reorderableContainer = tabBar
  globalInteractionState.originalChildOrder = @[]
  globalInteractionState.potentialDropTarget = tabBar

  # Collect actual tabs (expanding ForLoops for dynamic tabs)
  globalInteractionState.originalChildOrder = collectActualTabs(tabBar)
  echo "[LIVE REORDER] TabBar detected, collected ", globalInteractionState.originalChildOrder.len, " actual tabs (expanding ForLoops)"

  # Find dragged child index
  globalInteractionState.draggedChildIndex = -1
  for i, child in globalInteractionState.originalChildOrder:
    if child == draggedTab:
      globalInteractionState.draggedChildIndex = i
      break

  echo "[LIVE REORDER] Enabled for TabBar, dragged child index: ", globalInteractionState.draggedChildIndex
  globalInteractionState.dragInsertIndex = globalInteractionState.draggedChildIndex

  # Handle tab selection and content locking
  # Find the TabGroup (parent of TabBar)
  var tabGroup: Element = nil
  var parent = tabBar.parent
  while parent != nil:
    if parent.kind == ekTabGroup:
      tabGroup = parent
      break
    parent = parent.parent

  if tabGroup != nil:
    # Store the original tab index and content for drag content locking
    globalInteractionState.dragOriginalTabIndex = draggedTab.tabIndex

    # Capture original content mapping from ALL tabs to their panels
    globalInteractionState.dragOriginalContentMapping.clear()

    # Find the TabContent to get all panels
    var tabContent: Element = nil
    for child in tabGroup.children:
      if child.kind == ekTabContent:
        tabContent = child
        break

    if tabContent != nil:
      # Create mapping from original visual indices to their corresponding panels
      for idx, tab in globalInteractionState.originalChildOrder:
        if tab.kind == ekTab:
          let originalTabIndex = tab.tabIndex
          # Find the panel that corresponds to this tab's original content
          for panel in tabContent.children:
            if panel.kind == ekTabPanel and panel.tabIndex == originalTabIndex:
              globalInteractionState.dragOriginalContentMapping[idx] = panel
              echo "[CONTENT MAPPING] Mapped visual index ", idx, " (tabIndex ", originalTabIndex, ") to panel"
              break

      # Store the dragged tab's specific panel for quick access
      if globalInteractionState.draggedChildIndex >= 0 and globalInteractionState.dragOriginalContentMapping.hasKey(globalInteractionState.draggedChildIndex):
        globalInteractionState.dragTabContentPanel = globalInteractionState.dragOriginalContentMapping[globalInteractionState.draggedChildIndex]
        echo "[DRAG CONTENT LOCK] Stored original content for dragged tab at visual index ", globalInteractionState.draggedChildIndex

    # Select the dragged tab immediately
    let draggedVisualIndex = globalInteractionState.draggedChildIndex
    if draggedVisualIndex >= 0:
      let oldSelectedIndex = tabGroup.tabSelectedIndex
      tabGroup.tabSelectedIndex = draggedVisualIndex

      if oldSelectedIndex != draggedVisualIndex:
        echo "[TAB DRAG SELECT] Immediately selected dragged tab at index ", draggedVisualIndex, " (original index: ", globalInteractionState.dragOriginalTabIndex, ")"

        # Trigger the onSelectedIndexChanged event
        if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
          tabGroup.eventHandlers["onSelectedIndexChanged"]()

        # Invalidate related reactive values
        invalidateRelatedValues("tabSelectedIndex")

        # Mark TabGroup and children as dirty for immediate visual updates
        markDirty(tabGroup)
        for child in tabGroup.children:
          markDirty(child)
          if child.kind == ekTabContent:
            for panel in child.children:
              markDirty(panel)
              markAllDescendantsDirty(panel)

  return true

# ============================================================================
# Insert Index Calculation
# ============================================================================

proc updateTabInsertIndex*(tabBar: Element, mouseX: float) =
  ## Calculate and update the insert position for tab reordering based on mouse X position
  ## Updates the global interaction state with the calculated insert index

  # Only calculate if we're reordering a TabBar
  if tabBar.kind != ekTabBar:
    globalInteractionState.dragInsertIndex = -1
    return

  # Calculate which tab position the mouse is over using actual tab positions
  let relativeX = mouseX - tabBar.x

  # Calculate insert index based on actual tab positions and widths
  var insertIdx = 0
  var currentX = 0.0

  # Collect all actual tabs (expanding ForLoops for dynamic tabs)
  let allTabs = collectActualTabs(tabBar)

  # Find which tab position the mouse is closest to
  for child in allTabs:
    if child.kind == ekTab:
      let tabWidth = child.width
      let tabCenter = currentX + tabWidth / 2.0

      if relativeX >= tabCenter:
        insertIdx += 1
      else:
        break
      currentX += tabWidth

  # Clamp to valid range [0, tabCount]
  let tabCount = allTabs.len
  insertIdx = max(0, min(tabCount, insertIdx))

  globalInteractionState.dragInsertIndex = insertIdx
  echo "[INSERT CALC] Mouse at X=", relativeX, ", calculated insert index: ", insertIdx, " (tabCount: ", tabCount, ")"

# ============================================================================
# Live Visual Reordering
# ============================================================================

proc updateLiveTabOrder*() =
  ## Update the visual tab order during drag based on current insert index
  ## This provides live visual feedback as the user drags

  # Only update if live reordering is active
  if not globalInteractionState.isLiveReordering or globalInteractionState.dragInsertIndex < 0:
    return

  let fromIdx = globalInteractionState.draggedChildIndex
  var toIdx = globalInteractionState.dragInsertIndex

  # Only update if indices are different
  if fromIdx < 0 or fromIdx == toIdx:
    return

  let container = globalInteractionState.reorderableContainer

  # Build new reordered visual children
  var reorderedVisualChildren: seq[Element] = @[]
  var insertedDragged = false

  for i in 0..<globalInteractionState.originalChildOrder.len:
    # Skip the dragged element at its original position
    if i == fromIdx:
      continue

    # Insert dragged element at the target position
    if reorderedVisualChildren.len == toIdx and not insertedDragged:
      reorderedVisualChildren.add(globalInteractionState.originalChildOrder[fromIdx])
      insertedDragged = true

    reorderedVisualChildren.add(globalInteractionState.originalChildOrder[i])

  # If we haven't inserted yet (inserting at end), add it now
  if not insertedDragged:
    reorderedVisualChildren.add(globalInteractionState.originalChildOrder[fromIdx])

  # For TabBars with dynamic tabs (ForLoop), update the ForLoop.children
  # For static tabs, update container.children directly
  if container.kind == ekTabBar:
    # Find if there's a ForLoop among the container children
    var forLoopElem: Element = nil
    var nonForLoopChildren: seq[Element] = @[]

    for child in container.children:
      if child.kind == ekForLoop:
        forLoopElem = child
      else:
        # Keep other children (like the "+" tab)
        nonForLoopChildren.add(child)

    if forLoopElem != nil:
      enableManualLoopOrder(container)
      # Dynamic tabs: Update ForLoop's children with reordered tabs
      forLoopElem.children = reorderedVisualChildren
      for child in forLoopElem.children:
        if child != nil:
          child.parent = forLoopElem
      markDirty(forLoopElem)
      echo "[LIVE REORDER] Updated ForLoop.children with ", reorderedVisualChildren.len, " reordered tabs"
    else:
      # Static tabs: Update container.children directly
      container.children = reorderedVisualChildren
      echo "[LIVE REORDER] Updated TabBar.children directly (static tabs)"

  else:
    # Non-TabBar containers: Update container.children directly
    container.children = reorderedVisualChildren

  globalInteractionState.liveChildOrder = reorderedVisualChildren

  # Sync tab content during drag
  var tabGroup = container.parent
  while tabGroup != nil and tabGroup.kind != ekTabGroup:
    tabGroup = tabGroup.parent

  if tabGroup != nil:
    # During drag, ensure dragged tab's content follows the dragged tab
    if globalInteractionState.draggedElement != nil:
      let draggedTabIndex = globalInteractionState.draggedElement.tabIndex
      tabGroup.tabSelectedIndex = draggedTabIndex
      echo "[DRAG CONTENT SYNC] Set selected index to dragged tab's content: ", draggedTabIndex

    let state = getInteractionState()
    for child in tabGroup.children:
      if child.kind == ekTabContent:
        var reorderedPanels: seq[Element] = @[]
        for tab in reorderedVisualChildren:
          var originalIndex = -1
          for idx, originalTab in state.originalChildOrder:
            if originalTab == tab:
              originalIndex = idx
              break

          if originalIndex >= 0 and state.dragOriginalContentMapping.hasKey(originalIndex):
            let mappedPanel = state.dragOriginalContentMapping[originalIndex]
            if mappedPanel != nil:
              reorderedPanels.add(mappedPanel)

        # Apply reordered panels to TabContent (handling ForLoop-generated panels)
        var contentForLoop: Element = nil
        for grandchild in child.children:
          if grandchild.kind == ekForLoop:
            contentForLoop = grandchild
            break

        if contentForLoop != nil:
          enableManualLoopOrder(child)
          contentForLoop.children = reorderedPanels
          for panel in contentForLoop.children:
            if panel != nil:
              panel.parent = contentForLoop
          markDirty(contentForLoop)
        else:
          child.children = reorderedPanels
          for panel in child.children:
            if panel != nil:
              panel.parent = child

        # Update panel tabIndex values to match new visual order
        for i, panel in reorderedPanels:
          if panel != nil:
            panel.tabIndex = i

        markDirty(child)
        echo "[LIVE REORDER SYNC] Reordered ", reorderedPanels.len, " tab panels to follow dragged tab"
        break

  # Mark container as dirty to trigger layout recalculation
  markDirty(container)
  echo "[LIVE REORDER] Updated container.children directly, dragged from ", fromIdx, " to ", toIdx

# ============================================================================
# Finalize Tab Reordering
# ============================================================================

proc finalizeTabReorder*(draggedElem: Element, shouldCommit: bool) =
  ## Finalize tab reordering after drag ends.
  ## `shouldCommit` indicates whether the drop landed inside the reorderable TabBar.
  ## Commits or reverts visual order and keeps tab content in sync with TabBar state.

  var state = getInteractionState()

  # Only finalize if live reordering was active
  if not state.isLiveReordering:
    return

  let container = state.reorderableContainer
  if container == nil:
    resetReorderState()
    return

  # Locate the owning TabGroup (if any)
  var tabGroup: Element = nil
  var parent = container.parent
  while parent != nil:
    if parent.kind == ekTabGroup:
      tabGroup = parent
      break
    parent = parent.parent

  proc restoreOriginalOrder() =
    ## Restore the order and selection captured at drag start.
    let originalTabs = state.originalChildOrder

    if container.kind == ekTabBar:
      var forLoopElem: Element = nil
      for child in container.children:
        if child.kind == ekForLoop:
          forLoopElem = child
          break

      if forLoopElem != nil:
        enableManualLoopOrder(container)
        forLoopElem.children = originalTabs
        for tab in forLoopElem.children:
          if tab != nil:
            tab.parent = forLoopElem
        markDirty(forLoopElem)
      else:
        container.children = originalTabs
        for tab in container.children:
          if tab != nil:
            tab.parent = container
    else:
      container.children = originalTabs
      for tab in container.children:
        if tab != nil:
          tab.parent = container

    if tabGroup != nil:
      # Restore tab indices and selection
      if state.draggedChildIndex >= 0:
        tabGroup.tabSelectedIndex = state.draggedChildIndex

      for tabIndex, tab in originalTabs:
        tab.tabIndex = tabIndex

      for child in tabGroup.children:
        if child.kind == ekTabContent:
          var restoredPanels = newSeq[Element](originalTabs.len)

          for originalIndex, panel in state.dragOriginalContentMapping:
            if originalIndex >= 0 and originalIndex < restoredPanels.len:
              restoredPanels[originalIndex] = panel
              if panel != nil:
                panel.tabIndex = originalIndex

          # Fill any missing slots with existing panels to avoid nils
          for panel in child.children:
            if panel != nil:
              let idx = panel.tabIndex
              if idx >= 0 and idx < restoredPanels.len and restoredPanels[idx] == nil:
                restoredPanels[idx] = panel

          var contentLoop: Element = nil
          for grandchild in child.children:
            if grandchild.kind == ekForLoop:
              contentLoop = grandchild
              break

          if contentLoop != nil:
            enableManualLoopOrder(child)
            contentLoop.children = restoredPanels
            for panel in contentLoop.children:
              if panel != nil:
                panel.parent = contentLoop
            markDirty(contentLoop)
          else:
            child.children = restoredPanels
            for panel in child.children:
              if panel != nil:
                panel.parent = child
          break

      markDirty(tabGroup)
      for child in tabGroup.children:
        markDirty(child)
        if child.kind == ekTabContent:
          for panel in child.children:
            if panel != nil:
              markDirty(panel)
              markAllDescendantsDirty(panel)

    markDirty(container)

  # If the drop failed or we lost the dragged element reference, revert to the original order.
  if not shouldCommit or draggedElem == nil or draggedElem.kind != ekTab:
    echo "[LIVE REORDER] Drag canceled or invalid, restoring original tab order"
    restoreOriginalOrder()
    state.shouldClearDragOverride = true
    if container.kind == ekTabBar:
      disableManualLoopOrder(container)
      if tabGroup != nil:
        for child in tabGroup.children:
          if child.kind == ekTabContent:
            disableManualLoopOrder(child)
            break
    resetReorderState()
    globalInteractionState.liveChildOrder = @[]
    return

  echo "[LIVE REORDER] Drag ended, committing reordered tab state"

  if tabGroup != nil:
    # Determine the dragged tab's new visual index in the TabBar
    var newTabIndex = -1
    for child in tabGroup.children:
      if child.kind == ekTabBar:
        let allTabs = collectActualTabs(child)
        for j, tab in allTabs:
          if tab == draggedElem:
            newTabIndex = j
            break
        break

    if newTabIndex >= 0:
      let oldSelectedIndex = tabGroup.tabSelectedIndex
      tabGroup.tabSelectedIndex = newTabIndex

      # Update tab indices on the final visual order
      for child in tabGroup.children:
        if child.kind == ekTabBar:
          let allTabs = collectActualTabs(child)
          for j, tab in allTabs:
            tab.tabIndex = j
            echo "[TAB INDEX FIX] Updated tab[", j, "] tabIndex to ", j
          break

      # Synchronize TabContent panels using captured mapping
      for child in tabGroup.children:
        if child.kind == ekTabContent:
          var tabBar: Element = nil
          for sibling in tabGroup.children:
            if sibling.kind == ekTabBar:
              tabBar = sibling
              break

          if tabBar != nil:
            let allTabs = collectActualTabs(tabBar)
            var originalIndexToNewPosition: Table[int, int]

            for newPosition, tab in allTabs:
              var originalIndex = -1
              for idx, originalTab in state.originalChildOrder:
                if originalTab == tab:
                  originalIndex = idx
                  break

              if originalIndex >= 0:
                originalIndexToNewPosition[originalIndex] = newPosition
                echo "[POST-DRAG MAPPING] Original tab ", originalIndex, " is now at position ", newPosition

            var reorderedPanels = newSeq[Element](allTabs.len)

            for originalIndex, newPosition in originalIndexToNewPosition:
              if state.dragOriginalContentMapping.hasKey(originalIndex):
                let originalPanel = state.dragOriginalContentMapping[originalIndex]
                if newPosition >= 0 and newPosition < reorderedPanels.len:
                  reorderedPanels[newPosition] = originalPanel
                  echo "[POST-DRAG SYNC] Original panel ", originalIndex, " moved to position ", newPosition

            # Fill any gaps with existing panels to avoid nil entries
            for idx in 0..<reorderedPanels.len:
              if reorderedPanels[idx] == nil:
                for panel in child.children:
                  if panel.kind == ekTabPanel and panel.tabIndex == idx:
                    reorderedPanels[idx] = panel
                    break

            child.children = reorderedPanels

            for j, panel in reorderedPanels:
              if panel != nil:
                panel.tabIndex = j
                echo "[POST-DRAG SYNC] Updated panel[", j, "] tabIndex to ", j

            tabGroup.tabSelectedIndex = newTabIndex
            echo "[POST-DRAG SYNC] Selected dragged tab at final position ", newTabIndex
          break

      if oldSelectedIndex != newTabIndex:
        echo "[TAB REORDER SELECT] Updated selected index to dragged tab at position ", newTabIndex, " after reordering"

        if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
          tabGroup.eventHandlers["onSelectedIndexChanged"]()

        invalidateRelatedValues("tabSelectedIndex")

      markDirty(tabGroup)
      for child in tabGroup.children:
        markDirty(child)
        if child.kind == ekTabContent:
          for panel in child.children:
            if panel != nil:
              markDirty(panel)
              markAllDescendantsDirty(panel)

      # Trigger onReorder event to let app update its data array
      # This fires BEFORE we disable manual loop order, so the app can update its data
      # and the ForLoop will regenerate with the new data order on the next frame
      if state.draggedChildIndex >= 0 and newTabIndex >= 0:
        # Set global variables so apps can access the reorder indices
        lastTabReorderFromIndex = state.draggedChildIndex
        lastTabReorderToIndex = newTabIndex

        # Support both onReorder (new API) and onDrop (legacy compatibility)
        if container.eventHandlers.hasKey("onReorder"):
          echo "[TAB REORDER EVENT] Triggering onReorder: from ", state.draggedChildIndex, " to ", newTabIndex
          let onReorderHandler = container.eventHandlers["onReorder"]
          onReorderHandler()
        elif container.eventHandlers.hasKey("onDrop"):
          # Legacy onDrop handler
          echo "[TAB REORDER EVENT] Triggering legacy onDrop: from ", state.draggedChildIndex, " to ", newTabIndex
          let onDropHandler = container.eventHandlers["onDrop"]
          onDropHandler()

  if container.kind == ekTabBar:
    disableManualLoopOrder(container)
    if tabGroup != nil:
      for child in tabGroup.children:
        if child.kind == ekTabContent:
          disableManualLoopOrder(child)
          break

  # Clear drag content override on next frame
  state.shouldClearDragOverride = true
  echo "[DRAG CLEANUP] Preserving drag content override for one more frame to ensure proper sync"

  # Reset live reordering state
  resetReorderState()
  globalInteractionState.dragHasMoved = false
  globalInteractionState.liveChildOrder = @[]
