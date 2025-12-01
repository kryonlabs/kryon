# 0BSD
## Reactive Engine - Reactive State Management
## Phase 1, Week 2: Thin wrapper over IR reactive manifest
##
## This module provides a high-level Nim API over IR's reactive system.
## ALL LOGIC IS IN IR - this is just a convenience wrapper.
##
## Design Goals:
## - Thin wrapper - no state duplication
## - All reactive state lives in IRReactiveManifest (IR layer)
## - Type-safe Nim API over C functions
## - Future: Expression evaluation will be added to IR (6-week plan)
##
## Note: Currently IR stores expressions as strings but doesn't evaluate them.
## Full expression evaluation in IR is planned for the 6-week refactoring.

import ../ir_core

type
  ReactiveEngine* = ref object
    ## Reactive state coordinator - wraps IR reactive manifest
    ## All reactive state lives in IR, not in Nim
    manifest*: ptr IRReactiveManifest

# ============================================================================
# Reactive Engine Creation
# ============================================================================

proc createReactiveEngine*(): ReactiveEngine =
  ## Creates a new reactive engine with IR manifest
  result = ReactiveEngine(
    manifest: ir_reactive_manifest_create()
  )

proc destroyReactiveEngine*(engine: ReactiveEngine) =
  ## Destroys the reactive engine and frees IR manifest
  if not engine.manifest.isNil:
    ir_reactive_manifest_destroy(engine.manifest)
    engine.manifest = nil

# ============================================================================
# Reactive Variable Management (Direct IR Wrappers)
# ============================================================================

proc addVariable*(engine: ReactiveEngine,
                 name: string,
                 varType: IRReactiveVarType,
                 value: IRReactiveValue): uint32 =
  ## Adds a reactive variable to the manifest (IR manages variables)
  ## Returns: Variable ID for use in bindings
  result = ir_reactive_manifest_add_var(
    engine.manifest,
    cstring(name),
    varType,
    value
  )

proc findVariable*(engine: ReactiveEngine,
                  name: string): ptr IRReactiveVarDescriptor =
  ## Finds a variable by name (O(n) search in IR)
  ## Returns: Variable descriptor if found, nil otherwise
  result = ir_reactive_manifest_find_var(engine.manifest, cstring(name))

proc getVariable*(engine: ReactiveEngine,
                 varId: uint32): ptr IRReactiveVarDescriptor =
  ## Gets variable by ID (O(1) access in IR)
  ## Returns: Variable descriptor if valid, nil otherwise
  result = ir_reactive_manifest_get_var(engine.manifest, varId)

proc updateVariable*(engine: ReactiveEngine,
                    varId: uint32,
                    newValue: IRReactiveValue): bool =
  ## Updates variable value (IR manages updates and triggers bindings)
  ## Returns: true if updated successfully
  result = ir_reactive_manifest_update_var(engine.manifest, varId, newValue)

# ============================================================================
# Reactive Binding Management (Direct IR Wrappers)
# ============================================================================

proc addBinding*(engine: ReactiveEngine,
                componentId: uint32,
                reactiveVarId: uint32,
                bindingType: IRBindingType,
                expression: string) =
  ## Adds a reactive binding (IR stores binding metadata)
  ## When variable updates, IR will know which components need updates
  ##
  ## Note: Expression evaluation not yet in IR (planned for 6-week refactor)
  ## Currently IR stores expression as string, Nim evaluates it
  ir_reactive_manifest_add_binding(
    engine.manifest,
    componentId,
    reactiveVarId,
    bindingType,
    cstring(expression)
  )

# ============================================================================
# Reactive Conditional Management
# ============================================================================

proc addConditional*(engine: ReactiveEngine,
                    componentId: uint32,
                    condition: string,
                    dependentVarIds: seq[uint32]) =
  ## Adds a reactive conditional (show/hide component based on condition)
  ##
  ## Note: Condition evaluation not yet in IR (planned for 6-week refactor)
  ## Currently IR stores condition as string, Nim evaluates it
  if dependentVarIds.len > 0:
    var varIds = dependentVarIds  # Make mutable copy
    ir_reactive_manifest_add_conditional(
      engine.manifest,
      componentId,
      cstring(condition),
      cast[ptr uint32](addr varIds[0]),
      dependentVarIds.len.uint32
    )
  else:
    ir_reactive_manifest_add_conditional(
      engine.manifest,
      componentId,
      cstring(condition),
      nil,
      0
    )

# ============================================================================
# Reactive For Loop Management
# ============================================================================

proc addForLoop*(engine: ReactiveEngine,
                parentComponentId: uint32,
                collectionExpr: string,
                collectionVarId: uint32) =
  ## Adds a reactive for loop (dynamic children based on collection)
  ##
  ## Note: Collection evaluation not yet in IR (planned for 6-week refactor)
  ## Currently IR stores expression as string, Nim evaluates it
  ir_reactive_manifest_add_for_loop(
    engine.manifest,
    parentComponentId,
    cstring(collectionExpr),
    collectionVarId
  )

# ============================================================================
# Convenience Wrappers for Common Types
# ============================================================================

proc addIntVariable*(engine: ReactiveEngine, name: string, value: int): uint32 =
  ## Convenience: Add integer variable
  var irValue: IRReactiveValue
  irValue.as_int = value.cint
  result = engine.addVariable(name, IR_REACTIVE_TYPE_INT, irValue)

proc addFloatVariable*(engine: ReactiveEngine, name: string, value: float): uint32 =
  ## Convenience: Add float variable
  var irValue: IRReactiveValue
  irValue.as_float = value.cfloat
  result = engine.addVariable(name, IR_REACTIVE_TYPE_FLOAT, irValue)

proc addStringVariable*(engine: ReactiveEngine, name: string, value: string): uint32 =
  ## Convenience: Add string variable
  var irValue: IRReactiveValue
  irValue.as_string = cstring(value)
  result = engine.addVariable(name, IR_REACTIVE_TYPE_STRING, irValue)

proc addBoolVariable*(engine: ReactiveEngine, name: string, value: bool): uint32 =
  ## Convenience: Add boolean variable
  var irValue: IRReactiveValue
  irValue.as_bool = value
  result = engine.addVariable(name, IR_REACTIVE_TYPE_BOOL, irValue)

proc updateIntVariable*(engine: ReactiveEngine, varId: uint32, value: int): bool =
  ## Convenience: Update integer variable
  var irValue: IRReactiveValue
  irValue.as_int = value.cint
  result = engine.updateVariable(varId, irValue)

proc updateFloatVariable*(engine: ReactiveEngine, varId: uint32, value: float): bool =
  ## Convenience: Update float variable
  var irValue: IRReactiveValue
  irValue.as_float = value.cfloat
  result = engine.updateVariable(varId, irValue)

proc updateStringVariable*(engine: ReactiveEngine, varId: uint32, value: string): bool =
  ## Convenience: Update string variable
  var irValue: IRReactiveValue
  irValue.as_string = cstring(value)
  result = engine.updateVariable(varId, irValue)

proc updateBoolVariable*(engine: ReactiveEngine, varId: uint32, value: bool): bool =
  ## Convenience: Update boolean variable
  var irValue: IRReactiveValue
  irValue.as_bool = value
  result = engine.updateVariable(varId, irValue)

# ============================================================================
# Debugging and Statistics
# ============================================================================

proc printManifest*(engine: ReactiveEngine) =
  ## Prints the reactive manifest for debugging (IR handles formatting)
  if not engine.manifest.isNil:
    ir_reactive_manifest_print(engine.manifest)

proc getVariableCount*(engine: ReactiveEngine): int =
  ## Gets total number of reactive variables
  if engine.manifest.isNil:
    return 0
  return engine.manifest.variable_count.int

proc getBindingCount*(engine: ReactiveEngine): int =
  ## Gets total number of reactive bindings
  if engine.manifest.isNil:
    return 0
  return engine.manifest.binding_count.int

proc getConditionalCount*(engine: ReactiveEngine): int =
  ## Gets total number of reactive conditionals
  if engine.manifest.isNil:
    return 0
  return engine.manifest.conditional_count.int

proc getForLoopCount*(engine: ReactiveEngine): int =
  ## Gets total number of reactive for loops
  if engine.manifest.isNil:
    return 0
  return engine.manifest.for_loop_count.int

# ============================================================================
# Future: Expression Evaluation (6-Week Plan)
# ============================================================================

# The following functions will be added once IR expression evaluation is implemented:
#
# proc evaluateExpression*(engine: ReactiveEngine, expr: string): IRReactiveValue
# proc updateBindings*(engine: ReactiveEngine, varId: uint32)
# proc evaluateConditional*(engine: ReactiveEngine, conditionalId: uint32): bool
#
# These are part of the 6-week IR-pure architecture plan where:
# - ir_reactive_eval.c implements expression parser/evaluator
# - IR evaluates expressions like "${count} + 1" directly
# - No more Nim-side expression evaluation
