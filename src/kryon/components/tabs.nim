## Tab Components
##
## Extracts tab system component properties (TabGroup, TabBar, Tab, TabContent, TabPanel).

import ../core
import ../rendering/renderingContext
import ../layout/zindexSort
import options

type
  TabBarData* = object
    ## Extracted tab bar rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    hasBorder*: bool
    borderColor*: Color
    borderWidth*: float
    borderY*: float
    sortedChildren*: seq[Element]

  TabData* = object
    ## Extracted tab button rendering data
    x*, y*, width*, height*: float
    label*: string
    active*: bool
    fontSize*: int
    backgroundColor*: Color
    textColor*: Color
    borderColor*: Color
    borderWidth*: float
    activeBorderColor*: Color
    bottomBorderY*: float
    bottomBorderWidth*: float
    bottomBorderColor*: Color
    labelColor*: Color

  TabContentData* = object
    ## Extracted tab content rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    activeTab*: int
    hasActivePanel*: bool
    activePanel*: Element

  TabPanelData* = object
    ## Extracted tab panel rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    sortedChildren*: seq[Element]

  TabGroupData* = object
    ## Extracted tab group rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    sortedChildren*: seq[Element]

proc extractTabBarData*(elem: Element, inheritedColor: Option[Color] = none(Color)): TabBarData =
  ## Extract tab bar properties from element
  ##
  ## Properties supported:
  ##   - backgroundColor: Background color for bar (default: transparent)
  ##   - borderColor: Bottom border color (default: #CCCCCC)
  ##   - borderWidth: Bottom border thickness (default: 1.0)

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Check for bottom border
  let borderColor = elem.getProp("borderColor")
  if borderColor.isSome:
    result.hasBorder = true
    result.borderWidth = getBorderWidth(elem, 1.0)
    result.borderColor = borderColor.get.getColor()
    result.borderY = elem.y + elem.height - result.borderWidth

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)

proc extractTabData*(elem: Element, inheritedColor: Option[Color] = none(Color)): TabData =
  ## Extract tab button properties from element
  ##
  ## Properties supported:
  ##   - label: Tab label text (default: "Tab")
  ##   - active: Whether this tab is active (default: false)
  ##   - fontSize: Text size (default: 16)
  ##   - backgroundColor: Background color (default: #F0F0F0 inactive, #FFFFFF active)
  ##   - color: Text color (default: #666666 inactive, #000000 active)
  ##   - borderColor: Border color (default: #CCCCCC)
  ##   - borderWidth: Border thickness (default: 1.0)
  ##   - activeColor: Text color when active (overrides color)
  ##   - activeBorderColor: Bottom border for active tab (default: #4A90E2)

  # Extract properties
  result.label = elem.getProp("title").get(val("Tab")).getString()

  # Determine if this tab is active by checking parent's tabSelectedIndex
  var isActive = false
  var parent = elem.parent
  while parent != nil:
    if parent.kind == ekTabGroup or parent.kind == ekTabBar:
      isActive = (parent.tabSelectedIndex == elem.tabIndex)
      break
    parent = parent.parent
  result.active = isActive

  result.fontSize = getFontSize(elem, 16)

  # Default colors based on active state
  let defaultBgColor = if result.active: "#FFFFFF" else: "#F0F0F0"
  let defaultTextColor = if result.active: "#000000" else: "#666666"

  result.backgroundColor = getBackgroundColor(elem, defaultBgColor)
  result.textColor = elem.getProp("color").get(val(defaultTextColor)).getColor()
  result.borderColor = getBorderColor(elem, "#CCCCCC")
  result.borderWidth = getBorderWidth(elem, 1.0)
  let activeColor = elem.getProp("activeColor").get(val(result.textColor)).getColor()
  result.activeBorderColor = elem.getProp("activeBorderColor").get(val("#4A90E2")).getColor()

  # Position
  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Bottom border (different for active)
  if result.active:
    result.bottomBorderY = elem.y + elem.height
    result.bottomBorderWidth = result.borderWidth * 2.0
    result.bottomBorderColor = result.activeBorderColor
  else:
    result.bottomBorderY = elem.y + elem.height - result.borderWidth
    result.bottomBorderWidth = result.borderWidth
    result.bottomBorderColor = result.borderColor

  # Label color
  result.labelColor = if result.active: activeColor else: result.textColor

proc extractTabContentData*(elem: Element, inheritedColor: Option[Color] = none(Color)): TabContentData =
  ## Extract tab content properties from element
  ##
  ## Only provides the child TabPanel whose index matches the active tab.
  ## The activeTab property should be set by the parent TabGroup.
  ##
  ## Properties supported:
  ##   - activeTab: Index of active tab (default: 0)
  ##   - backgroundColor: Background color (default: transparent)

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Get active tab index from parent TabGroup
  var selectedIndex = 0
  var parent = elem.parent
  while parent != nil:
    if parent.kind == ekTabGroup:
      selectedIndex = parent.tabSelectedIndex
      break
    parent = parent.parent
  result.activeTab = selectedIndex

  # Only provide the active TabPanel
  if result.activeTab >= 0 and result.activeTab < elem.children.len:
    result.hasActivePanel = true
    result.activePanel = elem.children[result.activeTab]

proc extractTabPanelData*(elem: Element, inheritedColor: Option[Color] = none(Color)): TabPanelData =
  ## Extract tab panel properties from element
  ##
  ## Properties supported:
  ##   - backgroundColor: Background color (default: transparent)
  ##   - padding: Padding around content (default: 0)

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)

proc extractTabGroupData*(elem: Element, inheritedColor: Option[Color] = none(Color)): TabGroupData =
  ## Extract tab group properties from element
  ##
  ## TabGroup manages the coordination between tabs and content.
  ## The layout engine handles the vertical arrangement.
  ##
  ## Properties supported:
  ##   - backgroundColor: Background color (default: transparent)

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Render children (TabBar and TabContent) sorted by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)
