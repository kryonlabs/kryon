0BSD0BSD0BSD## Global Metadata Infrastructure for Kryon DSL
##
## This module provides a global metadata accumulator for bytecode functions,
## reactive states, and host function declarations. The metadata is serialized
## alongside the IR tree when compiling to .kir files.

import ../ir_metadata

# Global metadata accumulator
var globalMetadata*: ptr IRMetadata = nil

proc ensureGlobalMetadata*() =
  ## Initialize global metadata if not already created
  ## This should be called before any bytecode functions are added
  if globalMetadata.isNil:
    globalMetadata = ir_create_metadata()

proc getGlobalMetadata*(): ptr IRMetadata =
  ## Get the global metadata for serialization
  ## Returns nil if no metadata has been created
  return globalMetadata

proc resetGlobalMetadata*() =
  ## Reset global metadata (useful for testing or multi-app scenarios)
  ## WARNING: This does not free the old metadata - caller must handle cleanup
  globalMetadata = nil
