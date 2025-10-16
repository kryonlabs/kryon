## Component Base Module
##
## This module defines the component rendering system foundation.
## Components implement their logic once and work with any renderer.
##
## Each component is a procedure that takes:
##   - renderer: A type implementing the renderer interface
##   - elem: The element to render
##   - state: Backend state for interactive elements
##   - inheritedColor: Optional color inheritance from parent

import ../core
import ../state/backendState
import ../rendering/rendererInterface
import ../rendering/renderingContext
import options

# Component renderer signature
# Components use duck typing - any type implementing the renderer interface works
type
  ComponentRenderer*[R] = proc(renderer: var R, elem: Element, state: var BackendState,
                                inheritedColor: Option[Color])

# For components that don't need state
type
  StatelessComponentRenderer*[R] = proc(renderer: var R, elem: Element, inheritedColor: Option[Color])
