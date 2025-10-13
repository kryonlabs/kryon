## Navigation System for Kryon
##
## Simple state-driven enum routing that leverages the existing reactive system.
## This provides a straightforward way to implement multi-screen applications.

import core

# ============================================================================
# Navigation Helper
# ============================================================================

proc navigateTo*[T: enum](currentRoute: var T, target: T) =
  ## Navigate to a new route by updating the currentRoute variable.
  ## This triggers the reactive system to re-render the UI.
  ##
  ## Args:
  ##   currentRoute: The current route variable (passed by reference)
  ##   target: The target route to navigate to
  ##
  ## Usage:
  ##   var currentRoute = HomeScreen
  ##   navigateTo(currentRoute, AboutScreen)

  if currentRoute != target:
    currentRoute = target
    # Invalidate the state variable to trigger UI updates
    # Note: This assumes the variable is named "currentRoute" in user code
    invalidateReactiveValue("currentRoute")

# ============================================================================
# Future Extensions (Optional)
# ============================================================================

# proc navigateWithHistory*[T: enum](currentRoute: var T, target: T, history: var seq[T]) =
#   ## Navigate with history tracking (future enhancement)
#   if currentRoute != target:
#     history.add(currentRoute)
#     currentRoute = target
#     invalidateReactiveValue("currentRoute")
#     invalidateReactiveValue("navigationHistory")

# proc goBack*[T: enum](currentRoute: var T, history: var seq[T]): bool =
#   ## Go back to previous route (future enhancement)
#   if history.len > 0:
#     currentRoute = history.pop()
#     invalidateReactiveValue("currentRoute")
#     invalidateReactiveValue("navigationHistory")
#     return true
#   return false