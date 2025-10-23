## Shared Interaction State
##
## This module contains minimal shared state variables that need to be accessed
## across multiple modules without creating circular dependencies.
##
## Only primitive types and basic variables should be placed here.
## This module MUST NOT import core.nim or other modules that import core.nim.

# Global flag to freeze reactive ForLoop updates during tab drag
# This prevents the ForLoop from regenerating tabs while we're manually reordering them
var isDraggingTab* = false