## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os, sequtils, random
import ./runtime, ./reactive_system, ./ir_core
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

# Export reactive for loop support
export createReactiveForLoop, ReactiveForLoop

# Include the main DSL implementation
include ./kryon_dsl/impl
