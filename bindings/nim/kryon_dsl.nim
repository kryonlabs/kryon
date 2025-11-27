## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os, sequtils, random
import ./runtime, ./reactive_system, ./ir_core, ./style_vars
# Removed core_kryon and markdown - using IR system now
import ./kryon_dsl/helpers  # Import helper functions

# Export component functions for styling and tree management
export kryon_component_set_background_color, kryon_component_set_text_color
export kryon_component_set_border_color, kryon_component_set_border_width
export kryon_component_add_child
export KryonComponent
export addFontSearchDir, loadFont, registerFont, getFontId

# Export dimension helpers
export ir_percent, ir_px, ir_auto, ir_flex

# Export helpers
export helpers

# Export text effect types and functions (re-export entire module for enum values)
export ir_core
export kryon_component_set_text_fade, kryon_component_set_text_overflow, kryon_component_set_opacity

# Export reactive for loop support
export createReactiveForLoop, ReactiveForLoop

# Export style variables for theming
export style_vars

# Include the main DSL implementation
include ./kryon_dsl/impl
