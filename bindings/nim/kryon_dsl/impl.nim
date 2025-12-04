## Kryon DSL - Modular Implementation
##
## This is the main entry point for the Kryon DSL system.
## All DSL macros and utilities are now organized into focused modules:
##
## - properties.nim: Property parsing and value detection utilities
## - reactive.nim: Reactive expression detection and conditional conversion
## - helpers.nim: Helper functions (colors, typography, gradients, etc.)
## - core.nim: Core application infrastructure (kryonApp, component, Header, Resources)
## - utils.nim: Style system and utility macros
## - layout.nim: Layout components (Container, Grid, Row, Column, Center, Spacer, Gap)
## - animations.nim: Animation support (Body macro)
## - components.nim: All UI components (Text, Button, Input, etc.)
##
## This module simply re-exports all the symbols from the submodules.

# Import and re-export all DSL modules in dependency order
import ./properties
import ./reactive
import ./helpers
import ./core
import ./utils
import ./layout
import ./animations
import ./components
import ./logic_emitter

# Re-export all symbols
export properties
export reactive
export helpers
export core
export utils
export layout
export animations
export components
export logic_emitter

# Re-export runtime and IR core for convenience
import ../runtime
import ../ir_core
import ../ir_logic

export runtime
export ir_core
export ir_logic
