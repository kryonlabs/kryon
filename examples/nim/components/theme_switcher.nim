## ThemeSwitcher Component
##
## A preconfigured Dropdown for selecting themes

import kryon_dsl
import ./themes

export themes

# ThemeSwitcher - A dropdown for theme selection
# Usage: ThemeSwitcher()
template ThemeSwitcher*() =
  discard Dropdown:
    width = 180
    height = 40
    placeholder = "Select Theme..."
    options = themeNames()
    selectedIndex = currentThemeIndex
    backgroundColor = "#3d3d3d"
    borderColor = "#555555"
    textColor = "#ffffff"
    onChange = proc(index: int) =
      setThemeByIndex(index)
