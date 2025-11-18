## Kryon Navigation Module
##
## Provides simple enum-based navigation with reactive state management
## Integrates with Kryon's reactive system for automatic UI updates

import reactive_system

# ============================================================================
# Navigation Core
# ============================================================================

# Global navigation counter to track route changes
var navigationCounter* = 0

proc navigateTo*[T](routeVar: var T, newRoute: T) =
  ## Navigate to a new route by updating the route variable
  ## This triggers reactive updates automatically
  ##
  ## Example:
  ##   var currentRoute = HomeScreen
  ##   navigateTo(currentRoute, AboutScreen)
  ##
  ## The route variable must be used in reactive expressions (case statements, etc.)
  ## for automatic UI updates to occur.

  if routeVar != newRoute:
    routeVar = newRoute
    navigationCounter += 1  # Increment counter on each navigation

    # Trigger reactive system updates
    # This ensures any UI bound to the route variable will update
    updateAllReactiveTextExpressions()
    updateAllReactiveConditionals()

    # Force a complete re-render to ensure case statements are re-evaluated
    # This is a brute-force approach but should work for now
    echo "[kryon][nav] Route changed to: ", newRoute

# ============================================================================
# Utility Functions
# ============================================================================

proc goBack*[T](routeVar: var T, previousRoute: T) =
  ## Navigate back to a previous route
  ## This is a convenience wrapper around navigateTo
  navigateTo(routeVar, previousRoute)

proc canNavigate*[T](currentRoute: T, targetRoute: T): bool =
  ## Check if navigation to a target route is allowed
  ## By default, all navigation is allowed (returns true)
  ## Override this in your application if you need navigation guards
  result = true
