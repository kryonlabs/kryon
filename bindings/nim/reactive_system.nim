## Reactive System for Kryon Framework
##
## Provides automatic UI updates when variables change
## Works transparently with existing DSL

import core_kryon
import tables

# Reactive binding types
type
  ReactiveProc* = proc () {.closure.}

  ReactiveBinding* = ref object
    component*: KryonComponent
    updateProc*: ReactiveProc
    isText*: bool  # Optimization for text components
    lastValue*: string  # Cache to avoid unnecessary updates

  ReactiveVar*[T] = ref object
    value*: T
    bindings*: seq[ReactiveBinding]
    version*: int  # Track changes to avoid update loops

# Global reactive state
var reactiveUpdateQueue*: seq[ReactiveBinding] = @[]
var reactiveProcessingEnabled*: bool = true

# Registry for update procedures that can be called to sync regular variables with reactive ones
var reactiveUpdateRegistry*: Table[string, proc ()] = initTable[string, proc ()]()

# Forward declarations
proc updateAllReactiveTextExpressions*()
proc updateAllReactiveConditionals*()

# Initialize reactive system
proc initReactiveSystem*() =
  reactiveUpdateQueue = @[]
  reactiveProcessingEnabled = true

# Create a reactive variable
proc reactiveVar*[T](initialValue: T): ReactiveVar[T] =
  ## Creates a reactive variable that automatically updates bound UI components
  new(result)
  result.value = initialValue
  result.bindings = @[]
  result.version = 0

# Get current value (for reading in reactive expressions)
proc get*[T](rv: ReactiveVar[T]): T =
  ## Gets the current value from a reactive variable
  result = rv.value

# Set new value and trigger updates
proc set*[T](rv: ReactiveVar[T], newValue: T) =
  ## Sets a new value and automatically updates all bound components
  if rv.value != newValue:
    rv.value = newValue
    rv.version += 1

    # Queue all bindings for update
    if reactiveProcessingEnabled:
      for binding in rv.bindings:
        if binding != nil:
          reactiveUpdateQueue.add(binding)

# Bind a component to a reactive variable
proc bindToComponent*[T](rv: ReactiveVar[T], component: KryonComponent, updateProc: ReactiveProc, isText: bool = true) =
  ## Binds a component to automatically update when the reactive variable changes
  let binding = ReactiveBinding(
    component: component,
    updateProc: updateProc,
    isText: isText,
    lastValue: ""
  )
  rv.bindings.add(binding)

# Process all pending reactive updates
proc processReactiveUpdates*() =
  ## Processes all queued reactive updates (called from main loop)
  if not reactiveProcessingEnabled:
    return

  # Process regular reactive variable updates
  if reactiveUpdateQueue.len > 0:
    for binding in reactiveUpdateQueue:
      try:
        if binding != nil and binding.component != nil:
          # Execute the update procedure
          binding.updateProc()
      except:
        # Log error but continue processing other updates
        echo "[kryon][reactive] Error processing reactive update"

    # Clear the queue
    reactiveUpdateQueue.setLen(0)

  # Update all reactive conditionals (evaluates them each frame)
  updateAllReactiveConditionals()

  # Update all reactive text expressions (evaluates them each frame)
  updateAllReactiveTextExpressions()

# Utility: Create reactive text binding
proc bindTextToReactiveVar*[T](rv: ReactiveVar[T], component: KryonComponent) =
  ## Utility to bind text content to a reactive variable
  # Create binding first to capture it in the closure
  let binding = ReactiveBinding(
    component: component,
    updateProc: nil,  # Will be set below
    isText: true,
    lastValue: ""
  )

  # Now create the update proc that captures the binding
  binding.updateProc = proc() {.closure.} =
    let newValue = $rv.value
    # Only update if value actually changed (optimization)
    if binding.lastValue != newValue:
      binding.lastValue = newValue
      component.setText(newValue)

  rv.bindings.add(binding)

# Enable/disable reactive processing (for testing)
proc setReactiveProcessing*(enabled: bool) =
  reactiveProcessingEnabled = enabled

# Check if reactive system is initialized
proc isReactiveSystemInitialized*(): bool =
  reactiveProcessingEnabled

# Store reactive update procedure in registry
proc storeReactiveUpdate*(varName: string, updateProc: proc ()) =
  ## Store an update procedure that can be called to sync a regular variable with its reactive counterpart
  reactiveUpdateRegistry[varName] = updateProc
  echo "[kryon][reactive] Stored update proc for variable: ", varName

# ============================================================================
# Live Expression Evaluation System
# ============================================================================

# Type for storing reactive text expressions
type
  ReactiveTextExpression* = ref object
    component*: KryonComponent
    expression*: string  # The original expression as string (e.g., "$value")
    evalProc*: proc (): string {.closure.}  # Procedure that evaluates the expression
    lastValue*: string  # Cache to avoid unnecessary updates
    varNames*: seq[string]  # Variable names used in this expression

var reactiveTextExpressions*: seq[ReactiveTextExpression] = @[]

proc createReactiveTextExpression*(component: KryonComponent, expression: string,
                                  evalProc: proc (): string {.closure.},
                                  varNames: seq[string]) =
  ## Create a new reactive text expression that will be evaluated each frame
  let expr = ReactiveTextExpression(
    component: component,
    expression: expression,
    evalProc: evalProc,
    lastValue: "",
    varNames: varNames
  )
  reactiveTextExpressions.add(expr)
  echo "[kryon][reactive] Created reactive text expression: ", expression, " with vars: ", varNames

proc updateAllReactiveTextExpressions*() =
  ## Update all reactive text expressions by re-evaluating them
  # Closures are now created at runtime (in kryon_dsl.nim), so Nim properly
  # heap-allocates captured variables and they remain valid
  for expr in reactiveTextExpressions:
    if expr != nil and expr.component != nil and expr.evalProc != nil:
      try:
        let currentValue = expr.evalProc()
        # Only update if value actually changed (optimization)
        if expr.lastValue != currentValue:
          expr.lastValue = currentValue
          # Verify component is still valid before setText
          if expr.component != nil:
            expr.component.setText(currentValue)
            echo "[kryon][reactive] Updated text: ", expr.expression, " -> ", currentValue
      except Exception as e:
        echo "[kryon][reactive] Error evaluating expression: ", expr.expression, " - ", e.msg

# Update a specific reactive variable with a new value
proc updateReactiveVar*(varName: string, newValue: string) =
  ## Update a specific reactive variable by name with a new value
  if reactiveUpdateRegistry.hasKey(varName):
    echo "[kryon][reactive] Updating reactive ", varName, " to: ", newValue
    reactiveUpdateRegistry[varName]()

# Trigger update for all registered variables (call this after regular variable changes)
proc triggerReactiveUpdates*() =
  ## Call all registered update procedures to sync reactive variables with regular variables
  for varName, updateProc in reactiveUpdateRegistry:
    updateProc()

# ============================================================================
# Note: Reactive component expressions are handled by the existing ReactiveConditional system
# ============================================================================

# ============================================================================
# Automatic Reactive Variable Conversion
# ============================================================================

# Global registry to track automatically created reactive variables
var autoReactiveVars*: Table[string, pointer] = initTable[string, pointer]()

proc makeAutoReactive*[T](variable: T): ReactiveVar[T] =
  ## Automatically convert a regular variable to a reactive variable
  ## This is a simplified approach that creates a reactive wrapper
  # Note: This is a basic implementation - in practice, we'd need
  # to track the original variable and update it when the reactive var changes

  let varName = "auto_" & $T
  if autoReactiveVars.hasKey(varName):
    return cast[ReactiveVar[T]](autoReactiveVars[varName])

  result = reactiveVar[T](variable)
  autoReactiveVars[varName] = cast[pointer](result)

template autoReactive*(variable: untyped): untyped =
  ## Template to automatically make variables reactive
  when compiles(makeAutoReactive(variable)):
    makeAutoReactive(variable)
  else:
    # Fallback: treat as regular variable
    variable

# ============================================================================
# Reactive Conditional System
# ============================================================================

type
  ReactiveConditional* = ref object
    parent*: KryonComponent
    conditionProc*: proc(): bool {.closure.}
    thenProc*: proc(): seq[KryonComponent] {.closure.}
    elseProc*: proc(): seq[KryonComponent] {.closure.}
    currentChildren*: seq[KryonComponent]
    lastCondition*: bool
    initialized*: bool

var reactiveConditionals*: seq[ReactiveConditional] = @[]

proc createReactiveConditional*(
  parent: KryonComponent,
  conditionProc: proc(): bool {.closure.},
  thenProc: proc(): seq[KryonComponent] {.closure.},
  elseProc: proc(): seq[KryonComponent] {.closure.} = nil
) =
  ## Create and register a reactive conditional that re-evaluates when variables change
  let conditional = ReactiveConditional(
    parent: parent,
    conditionProc: conditionProc,
    thenProc: thenProc,
    elseProc: elseProc,
    currentChildren: @[],
    lastCondition: false,
    initialized: false
  )
  reactiveConditionals.add(conditional)
  echo "[kryon][reactive] Created reactive conditional"

proc updateAllReactiveConditionals*() =
  ## Update all reactive conditionals by re-evaluating conditions and updating component tree
  for cond in reactiveConditionals:
    if cond != nil and cond.parent != nil:
      try:
        let currentCondition = cond.conditionProc()

        # Update if condition changed or first time
        if not cond.initialized or currentCondition != cond.lastCondition:
          echo "[kryon][reactive] Conditional changed: ", currentCondition

          # Remove old children
          for child in cond.currentChildren:
            if child != nil:
              kryon_component_remove_child(cond.parent, child)
          cond.currentChildren.setLen(0)

          # Add new children based on condition
          let newChildren = if currentCondition:
            cond.thenProc()
          else:
            if cond.elseProc != nil: cond.elseProc() else: @[]

          for child in newChildren:
            if child != nil:
              discard kryon_component_add_child(cond.parent, child)
              kryon_component_mark_dirty(child)

          cond.currentChildren = newChildren
          cond.lastCondition = currentCondition
          cond.initialized = true
          kryon_component_mark_dirty(cond.parent)

          # Force layout recalculation to ensure newly added components get proper bounds
          # This is critical for reactive components to be visible to the event system
          var parentX, parentY, parentWidth, parentHeight: KryonFp
          kryon_component_get_absolute_bounds(cond.parent, addr parentX, addr parentY, addr parentWidth, addr parentHeight)
          echo "[kryon][reactive] Layout: parent bounds before: ", parentX, ", ", parentY, ", w=", parentWidth, ", h=", parentHeight
          kryon_layout_component(cond.parent, parentWidth, parentHeight)
          kryon_component_get_absolute_bounds(cond.parent, addr parentX, addr parentY, addr parentWidth, addr parentHeight)
          echo "[kryon][reactive] Layout: parent bounds after: ", parentX, ", ", parentY, ", w=", parentWidth, ", h=", parentHeight

          # Debug: Log child component bounds
          for i, child in newChildren:
            if child != nil:
              var childX, childY, childWidth, childHeight: KryonFp
              kryon_component_get_absolute_bounds(child, addr childX, addr childY, addr childWidth, addr childHeight)
              echo "[kryon][reactive] Layout: child[", i, "] bounds: ", childX, ", ", childY, ", w=", childWidth, ", h=", childHeight
      except Exception as e:
        echo "[kryon][reactive] Error updating conditional: ", e.msg