## Nim bindings for ir_executor.h
## Provides handler execution for .kir files

import ir_core
import ir_logic

const irHeader = "../../ir/ir_executor.h"
const irLib = "../../build/libkryon_ir.so"

type
  IRExecutorContext* {.importc: "IRExecutorContext", header: irHeader.} = object

# Lifecycle
proc ir_executor_create*(): ptr IRExecutorContext
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_destroy*(ctx: ptr IRExecutorContext)
  {.importc, dynlib: irLib, header: irHeader.}

# Configuration
proc ir_executor_set_logic*(ctx: ptr IRExecutorContext, logic: IRLogicBlockPtr)
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_set_manifest*(ctx: ptr IRExecutorContext, manifest: ptr IRReactiveManifest)
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_set_root*(ctx: ptr IRExecutorContext, root: ptr IRComponent)
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_apply_initial_conditionals*(ctx: ptr IRExecutorContext)
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_add_source*(ctx: ptr IRExecutorContext, lang: cstring, code: cstring)
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_get_source*(ctx: ptr IRExecutorContext, lang: cstring): cstring
  {.importc, dynlib: irLib, header: irHeader.}

# Event handling
proc ir_executor_handle_event*(ctx: ptr IRExecutorContext, componentId: uint32, eventType: cstring): bool
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_call_handler*(ctx: ptr IRExecutorContext, handlerName: cstring): bool
  {.importc, dynlib: irLib, header: irHeader.}

# Global executor
proc ir_executor_get_global*(): ptr IRExecutorContext
  {.importc, dynlib: irLib, header: irHeader.}

proc ir_executor_set_global*(ctx: ptr IRExecutorContext)
  {.importc, dynlib: irLib, header: irHeader.}

# File loading
proc ir_executor_load_kir_file*(ctx: ptr IRExecutorContext, kirPath: cstring): bool
  {.importc, dynlib: irLib, header: irHeader.}
