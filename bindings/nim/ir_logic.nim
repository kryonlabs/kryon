## IR Logic Bindings - C bindings for universal logic system
##
## Provides Nim access to:
## - Logic block creation and management
## - Logic function definitions (universal + embedded sources)
## - Event bindings
## - JSON v3 serialization with logic support

import ir_core

const
  irLib = "/home/wao/Projects/kryon/build/libkryon_ir.so"

# Forward declarations for opaque types
type
  IRLogicBlockPtr* = pointer
  IRLogicFunctionPtr* = pointer
  IREventBindingPtr* = pointer
  IRExpressionPtr* = pointer
  IRStatementPtr* = pointer

# ============================================================================
# Logic Block Management
# ============================================================================

proc ir_logic_block_create*(): IRLogicBlockPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_free*(logic: IRLogicBlockPtr)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_add_function*(logic: IRLogicBlockPtr, fn: IRLogicFunctionPtr)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_add_binding*(logic: IRLogicBlockPtr, binding: IREventBindingPtr)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_find_function*(logic: IRLogicBlockPtr, name: cstring): IRLogicFunctionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_get_handler*(logic: IRLogicBlockPtr, componentId: uint32, eventType: cstring): cstring
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_get_function_count*(logic: IRLogicBlockPtr): cint
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_block_get_binding_count*(logic: IRLogicBlockPtr): cint
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# Logic Function Creation
# ============================================================================

proc ir_logic_function_create*(name: cstring): IRLogicFunctionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_function_free*(fn: IRLogicFunctionPtr)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_function_add_param*(fn: IRLogicFunctionPtr, name: cstring, paramType: cstring)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_function_add_stmt*(fn: IRLogicFunctionPtr, stmt: IRStatementPtr)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_function_add_source*(fn: IRLogicFunctionPtr, language: cstring, source: cstring)
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# Convenience Functions (create common patterns)
# ============================================================================

proc ir_logic_create_increment*(name: cstring, varName: cstring): IRLogicFunctionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_create_decrement*(name: cstring, varName: cstring): IRLogicFunctionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_create_toggle*(name: cstring, varName: cstring): IRLogicFunctionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_create_reset*(name: cstring, varName: cstring, initialValue: int64): IRLogicFunctionPtr
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# Event Binding Creation
# ============================================================================

proc ir_event_binding_create*(componentId: uint32, eventType: cstring, handlerName: cstring): IREventBindingPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_event_binding_free*(binding: IREventBindingPtr)
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# Expression Creation (for manual statement building)
# ============================================================================

proc ir_expr_int*(value: int64): IRExpressionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_expr_var*(name: cstring): IRExpressionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_expr_add*(left, right: IRExpressionPtr): IRExpressionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_expr_sub*(left, right: IRExpressionPtr): IRExpressionPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_expr_free*(expr: IRExpressionPtr)
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# Statement Creation
# ============================================================================

proc ir_stmt_assign*(target: cstring, expr: IRExpressionPtr): IRStatementPtr
  {.cdecl, importc, dynlib: irLib.}

proc ir_stmt_free*(stmt: IRStatementPtr)
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# JSON v3 Serialization (with logic support)
# ============================================================================

proc ir_serialize_json_v3*(root: pointer, manifest: pointer, logic: IRLogicBlockPtr): cstring
  {.cdecl, importc, dynlib: irLib.}

proc ir_write_json_file*(root: pointer, manifest: pointer, logic: IRLogicBlockPtr, filename: cstring): bool
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# Debug / Print
# ============================================================================

proc ir_logic_block_print*(logic: IRLogicBlockPtr)
  {.cdecl, importc, dynlib: irLib.}

proc ir_logic_function_print*(fn: IRLogicFunctionPtr)
  {.cdecl, importc, dynlib: irLib.}

# ============================================================================
# High-Level Nim Wrapper
# ============================================================================

type
  LogicBlock* = object
    handle*: IRLogicBlockPtr

  LogicFunction* = object
    handle*: IRLogicFunctionPtr

  EventBinding* = object
    handle*: IREventBindingPtr

proc newLogicBlock*(): LogicBlock =
  result.handle = ir_logic_block_create()

proc functionCount*(lb: LogicBlock): int =
  if lb.handle != nil:
    return int(ir_logic_block_get_function_count(lb.handle))
  return 0

proc bindingCount*(lb: LogicBlock): int =
  if lb.handle != nil:
    return int(ir_logic_block_get_binding_count(lb.handle))
  return 0

proc destroy*(lb: var LogicBlock) =
  if lb.handle != nil:
    ir_logic_block_free(lb.handle)
    lb.handle = nil

proc addFunction*(lb: LogicBlock, fn: LogicFunction) =
  ir_logic_block_add_function(lb.handle, fn.handle)

proc addBinding*(lb: LogicBlock, binding: EventBinding) =
  ir_logic_block_add_binding(lb.handle, binding.handle)

proc createIncrementFunction*(name, varName: string): LogicFunction =
  result.handle = ir_logic_create_increment(name.cstring, varName.cstring)

proc createDecrementFunction*(name, varName: string): LogicFunction =
  result.handle = ir_logic_create_decrement(name.cstring, varName.cstring)

proc createToggleFunction*(name, varName: string): LogicFunction =
  result.handle = ir_logic_create_toggle(name.cstring, varName.cstring)

proc createResetFunction*(name, varName: string, value: int64): LogicFunction =
  result.handle = ir_logic_create_reset(name.cstring, varName.cstring, value)

proc createFunctionWithSource*(name, language, source: string): LogicFunction =
  result.handle = ir_logic_function_create(name.cstring)
  ir_logic_function_add_source(result.handle, language.cstring, source.cstring)

proc createEventBinding*(componentId: uint32, eventType, handler: string): EventBinding =
  result.handle = ir_event_binding_create(componentId, eventType.cstring, handler.cstring)

proc print*(lb: LogicBlock) =
  ir_logic_block_print(lb.handle)

proc print*(fn: LogicFunction) =
  ir_logic_function_print(fn.handle)

# ============================================================================
# Global Logic Block for current application
# ============================================================================

var globalLogicBlock*: LogicBlock

proc initGlobalLogicBlock*() =
  if globalLogicBlock.handle == nil:
    globalLogicBlock = newLogicBlock()

proc getGlobalLogicBlock*(): LogicBlock =
  initGlobalLogicBlock()
  return globalLogicBlock

proc findFunction*(lb: LogicBlock, name: string): IRLogicFunctionPtr =
  ## Find a function by name in the logic block
  result = ir_logic_block_find_function(lb.handle, name.cstring)

proc registerLogicFunction*(name, varName, operation: string): bool =
  ## Register a logic function to the global logic block
  ## operation: "increment", "decrement", "toggle", "assign"
  ## Returns true if newly registered, false if already exists
  initGlobalLogicBlock()

  # Check if function already exists (avoid duplicates)
  if globalLogicBlock.findFunction(name) != nil:
    return false  # Already registered

  var fn: LogicFunction
  case operation
  of "increment":
    fn = createIncrementFunction(name, varName)
  of "decrement":
    fn = createDecrementFunction(name, varName)
  of "toggle":
    fn = createToggleFunction(name, varName)
  else:
    # Default: create reset function with 0
    fn = createResetFunction(name, varName, 0)

  globalLogicBlock.addFunction(fn)
  return true

proc registerLogicFunctionWithSource*(name, language, source: string): bool =
  ## Register a logic function with embedded source code
  ## Returns true if newly registered, false if already exists
  initGlobalLogicBlock()

  # Check if function already exists (avoid duplicates)
  if globalLogicBlock.findFunction(name) != nil:
    return false  # Already registered

  let fn = createFunctionWithSource(name, language, source)
  globalLogicBlock.addFunction(fn)
  return true

proc registerEventBinding*(componentId: uint32, eventType, handlerName: string) =
  ## Register an event binding to the global logic block
  initGlobalLogicBlock()
  let binding = createEventBinding(componentId, eventType, handlerName)
  globalLogicBlock.addBinding(binding)
