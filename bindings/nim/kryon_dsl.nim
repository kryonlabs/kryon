## Kryon DSL - Nim Macro System
##
## Provides declarative UI syntax that compiles to C core component trees

import macros, strutils, os, sequtils, random
import ./runtime, ./reactive_system, ./ir_core
# Removed core_kryon and markdown - using IR system now
import ./kryon_dsl/helpers  # Import helper functions

# Export component functions for styling and tree management
export kryon_component_set_background_color, kryon_component_set_text_color
export kryon_component_add_child
export KryonComponent

# Export helpers
export helpers

# Include the main DSL implementation
include ./kryon_dsl/impl
