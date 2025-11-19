## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os, sequtils, random
import ./runtime, ./core_kryon, ./reactive_system, ./markdown
import ./kryon_dsl/helpers  # Import helper functions

# Export font management functions from runtime
export addFontSearchDir, loadFont, getFontId

# Export core component functions for styling
export kryon_component_set_background_color, kryon_component_set_border_color
export kryon_component_set_border_width, kryon_component_set_text_color
export KryonComponent

# Export helpers
export helpers

# Include the main DSL implementation
include ./kryon_dsl/impl
