## Reactive System for Kryon Framework
##
## Provides automatic UI updates when variables change
## Works transparently with existing DSL

import tables
import sets
import ir_core

# ============================================================================
# Component Definition Registry
# ============================================================================

type
  ComponentProp* = object
    name*: string
    propType*: string
    defaultValue*: string

  ComponentStateVar* = object
    name*: string
    varType*: string
    initialExpr*: string  # e.g., "{{initialValue}}" to reference prop

  ComponentDefinition* = object
    name*: string
    props*: seq[ComponentProp]
    stateVars*: seq[ComponentStateVar]
    templateRoot*: ptr IRComponent  # First instance captures the template

# Global registry for custom component definitions
var componentDefinitions*: Table[string, ComponentDefinition] = initTable[string, ComponentDefinition]()

proc registerComponentDefinition*(name: string, props: seq[ComponentProp], stateVars: seq[ComponentStateVar]) =
  ## Register a custom component definition (called by kryonComponent macro)
  if name notin componentDefinitions:
    componentDefinitions[name] = ComponentDefinition(
      name: name,
      props: props,
      stateVars: stateVars,
      templateRoot: nil
    )
    echo "[kryonComponent] Registered component definition: ", name

proc setComponentTemplate*(name: string, templateRoot: ptr IRComponent) =
  ## Set the template root for a component (first instantiation)
  if name in componentDefinitions and componentDefinitions[name].templateRoot == nil:
    componentDefinitions[name].templateRoot = templateRoot
    # Mark the root component with the component name
    ir_set_component_ref(templateRoot, cstring(name))
    echo "[kryonComponent] Captured template for: ", name

proc getComponentDefinitions*(): Table[string, ComponentDefinition] =
  ## Get all registered component definitions
  result = componentDefinitions

proc syncComponentDefinitionsToManifest*(manifest: ptr IRReactiveManifest) =
  ## Sync Nim component definitions to C manifest for serialization
  if manifest == nil:
    return

  for name, def in componentDefinitions:
    # Create C arrays for props and state vars
    var cProps: seq[IRComponentProp] = @[]
    var cStateVars: seq[IRComponentStateVar] = @[]

    for prop in def.props:
      cProps.add(IRComponentProp(
        name: cstring(prop.name),
        `type`: cstring(prop.propType),
        default_value: cstring(prop.defaultValue)
      ))

    for sv in def.stateVars:
      cStateVars.add(IRComponentStateVar(
        name: cstring(sv.name),
        `type`: cstring(sv.varType),
        initial_expr: cstring(sv.initialExpr)
      ))

    # Call C function to add to manifest
    let propsPtr = if cProps.len > 0: addr cProps[0] else: nil
    let stateVarsPtr = if cStateVars.len > 0: addr cStateVars[0] else: nil

    ir_reactive_manifest_add_component_def(
      manifest,
      cstring(name),
      propsPtr,
      uint32(cProps.len),
      stateVarsPtr,
      uint32(cStateVars.len),
      def.templateRoot
    )

# Hook for TabGroup resync (set by runtime.nim to avoid circular import)
var onReactiveForLoopParentChanged*: proc(parent: ptr IRComponent) {.closure.} = nil

# Legacy function wrappers for reactive system (until fully migrated to IR)
# These are temporary shims that provide the old API using IR functions

proc setText(component: ptr IRComponent, text: string) =
  ## setText wrapper for reactive system - calls IR function
  ir_set_text_content(component, cstring(text))

proc kryon_component_add_child(parent: ptr IRComponent, child: ptr IRComponent): cint {.discardable.} =
  ## Legacy wrapper for ir_add_child
  ir_add_child(parent, child)
  return 0

proc kryon_component_remove_child*(parent: ptr IRComponent, child: ptr IRComponent) =
  ## Legacy wrapper for ir_remove_child
  ir_remove_child(parent, child)

proc kryon_component_mark_dirty(component: ptr IRComponent) =
  ## Legacy wrapper - IR doesn't need explicit dirty marking (handled automatically)
  discard

type KryonFp = cfloat

proc kryon_component_get_absolute_bounds(component: ptr IRComponent, x: ptr KryonFp, y: ptr KryonFp, width: ptr KryonFp, height: ptr KryonFp) =
  ## Legacy wrapper - stub for now (IR handles bounds differently)
  x[] = 0.0
  y[] = 0.0
  width[] = 100.0
  height[] = 100.0

proc kryon_layout_component(component: ptr IRComponent, width: KryonFp, height: KryonFp) =
  ## Legacy wrapper - stub for now (IR handles layout automatically)
  discard

# Reactive binding types
type
  ReactiveProc* = proc () {.closure.}

  ReactiveBinding* = ref object
    component*: ptr IRComponent
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
proc updateAllReactiveRebuilds*()
proc updateAllReactiveForLoops*()
proc isComponentInVisibleTree*(component: ptr IRComponent): bool
proc isComponentInSubtree(component: ptr IRComponent, root: ptr IRComponent): bool
proc unregisterReactiveTextFor*(components: seq[ptr IRComponent])
proc unregisterReactiveConditionalsFor*(components: seq[ptr IRComponent])
proc suspendReactiveForLoopsFor*(root: ptr IRComponent)
proc resumeReactiveForLoopsFor*(root: ptr IRComponent)

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
proc bindToComponent*[T](rv: ReactiveVar[T], component: ptr IRComponent, updateProc: ReactiveProc, isText: bool = true) =
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

  # Update all reactive for loops (checks for collection changes)
  updateAllReactiveForLoops()

  # Re-run any registered rebuild callbacks (e.g., dynamic for-loops)
  updateAllReactiveRebuilds()

# Utility: Create reactive text binding
proc bindTextToReactiveVar*[T](rv: ReactiveVar[T], component: ptr IRComponent) =
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
    component*: ptr IRComponent
    expression*: string  # The original expression as string (e.g., "$value")
    evalProc*: proc (): string {.closure.}  # Procedure that evaluates the expression
    lastValue*: string  # Cache to avoid unnecessary updates
    varNames*: seq[string]  # Variable names used in this expression
    suspended*: bool  # True when parent panel is hidden - skip updates but preserve state

var reactiveTextExpressions*: seq[ReactiveTextExpression] = @[]

proc createReactiveTextExpression*(component: ptr IRComponent, expression: string,
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
  # NOTE: We no longer clean up expressions - they're preserved across tab switches
  # Expressions for hidden panels will be skipped by the visibility check

  # Closures are now created at runtime (in kryon_dsl.nim), so Nim properly
  # heap-allocates captured variables and they remain valid
  for expr in reactiveTextExpressions:
    if expr == nil or expr.component == nil or expr.evalProc == nil:
      continue

    # Skip suspended expressions entirely - they're hidden and will be resumed later
    if expr.suspended:
      continue

    # Skip update if component is not in the visible tree (e.g., panel is hidden)
    if not isComponentInVisibleTree(expr.component):
      continue

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

# Remove queued reactive bindings for components that are about to be destroyed/removed
proc unregisterReactiveBindingsFor*(components: seq[ptr IRComponent]) =
  if components.len == 0: return
  var filtered: seq[ReactiveBinding] = @[]
  for b in reactiveUpdateQueue:
    var keep = true
    if b != nil:
      for comp in components:
        if comp == b.component:
          keep = false
          break
    if keep:
      filtered.add(b)
  reactiveUpdateQueue = filtered


# ============================================================================
# Reactive rebuild bindings (e.g., dynamic for-loops)
# ============================================================================

var reactiveRebuilds*: seq[proc ()] = @[]

proc registerReactiveRebuild*(rebuild: proc ()) =
  if rebuild != nil:
    reactiveRebuilds.add(rebuild)

proc updateAllReactiveRebuilds*() =
  for r in reactiveRebuilds:
    if r != nil:
      try:
        r()
      except:
        echo "[kryon][reactive] Error in reactive rebuild"

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
    parent*: ptr IRComponent
    conditionProc*: proc(): bool {.closure.}
    thenProc*: proc(): seq[ptr IRComponent] {.closure.}
    elseProc*: proc(): seq[ptr IRComponent] {.closure.}
    currentChildren*: seq[ptr IRComponent]
    lastCondition*: bool
    initialized*: bool
    suspended*: bool  # True when parent panel is hidden - skip updates but preserve state
    conditionExpr*: string  # Original condition expression as string for serialization
    # Captured children for serialization (both branches)
    thenChildrenIds*: seq[uint32]  # Component IDs of then-branch children
    elseChildrenIds*: seq[uint32]  # Component IDs of else-branch children

var reactiveConditionals*: seq[ReactiveConditional] = @[]

# Track which component IDs have been cleaned up to prevent double cleanup crashes
var cleanedUpComponentIds: HashSet[uint32] = initHashSet[uint32]()

proc resetCleanedUpTracking*() =
  ## Reset the cleaned up tracking - call this when rebuilding the UI
  cleanedUpComponentIds.clear()

proc createReactiveConditional*(
  parent: ptr IRComponent,
  conditionProc: proc(): bool {.closure.},
  thenProc: proc(): seq[ptr IRComponent] {.closure.},
  elseProc: proc(): seq[ptr IRComponent] {.closure.} = nil,
  conditionExpr: string = ""
) =
  ## Create and register a reactive conditional that re-evaluates when variables change
  let conditional = ReactiveConditional(
    parent: parent,
    conditionProc: conditionProc,
    thenProc: thenProc,
    elseProc: elseProc,
    currentChildren: @[],
    lastCondition: false,
    initialized: false,
    conditionExpr: conditionExpr,
    thenChildrenIds: @[],
    elseChildrenIds: @[]
  )
  reactiveConditionals.add(conditional)
  echo "[kryon][reactive] Created reactive conditional: ", if conditionExpr.len > 0: conditionExpr else: "<closure>"

proc updateAllReactiveConditionals*() =
  ## Update all reactive conditionals by re-evaluating conditions and updating component tree
  for cond in reactiveConditionals:
    # Safety checks: ensure cond, parent, and closures are valid
    if cond == nil or cond.parent == nil or cond.conditionProc == nil:
      continue

    # Skip suspended conditionals entirely - they're hidden and will be resumed later
    if cond.suspended:
      continue

    # Skip update if parent is not in the visible tree (e.g., panel is hidden)
    if not isComponentInVisibleTree(cond.parent):
      continue

    try:
      let currentCondition = cond.conditionProc()

      # Update if condition changed or first time
      if not cond.initialized or currentCondition != cond.lastCondition:
        echo "[kryon][reactive] Conditional changed: ", currentCondition

        # Unregister reactive expressions for old children BEFORE removing them
        # This prevents stale expressions from pointing to freed components
        if cond.currentChildren.len > 0:
          unregisterReactiveTextFor(cond.currentChildren)
          unregisterReactiveConditionalsFor(cond.currentChildren)

        # Remove old children
        for child in cond.currentChildren:
          if child != nil:
            kryon_component_remove_child(cond.parent, child)
        cond.currentChildren.setLen(0)

        # Add new children based on condition
        var newChildren: seq[ptr IRComponent] = @[]
        if currentCondition:
          if cond.thenProc != nil:
            newChildren = cond.thenProc()
            # Capture IDs for then-branch (for serialization)
            cond.thenChildrenIds.setLen(0)
            for child in newChildren:
              if child != nil:
                cond.thenChildrenIds.add(child.id)
        else:
          if cond.elseProc != nil:
            newChildren = cond.elseProc()
            # Capture IDs for else-branch (for serialization)
            cond.elseChildrenIds.setLen(0)
            for child in newChildren:
              if child != nil:
                cond.elseChildrenIds.add(child.id)

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

# ============================================================================
# C Bridge for Desktop Renderer Main Loop
# ============================================================================

proc nimProcessReactiveUpdates*() {.exportc: "nimProcessReactiveUpdates", cdecl, dynlib.} =
  ## C-callable bridge function that processes reactive updates every frame
  ## Called from the desktop renderer's main event loop
  processReactiveUpdates()

# ============================================================================
# Cleanup Functions for Tab Panel Removal
# ============================================================================

proc cleanupReactiveConditionalsFor*(removedComponent: ptr IRComponent) =
  ## Remove all reactive conditionals that have this component as parent
  ## Called when a component is removed from the tree
  if removedComponent == nil:
    return

  var kept: seq[ReactiveConditional] = @[]
  for cond in reactiveConditionals:
    if cond != nil and cond.parent != removedComponent:
      kept.add(cond)
    else:
      # Remove children before deleting conditional
      if cond != nil and cond.parent != nil:
        for child in cond.currentChildren:
          if child != nil:
            kryon_component_remove_child(cond.parent, child)
        cond.currentChildren.setLen(0)
      echo "[kryon][reactive] Cleaned up conditional for removed component"
  reactiveConditionals = kept

proc cleanupReactiveTextFor*(removedComponent: ptr IRComponent) =
  ## Remove reactive text expressions for a component
  if removedComponent == nil:
    return

  var kept: seq[ReactiveTextExpression] = @[]
  for expr in reactiveTextExpressions:
    if expr != nil and expr.component != removedComponent:
      kept.add(expr)
    else:
      echo "[kryon][reactive] Cleaned up text expression for removed component"
  reactiveTextExpressions = kept

proc isComponentInSubtree(component: ptr IRComponent, root: ptr IRComponent): bool =
  ## Check if a component is part of a subtree rooted at `root`
  if component == nil or root == nil:
    return false
  if component == root:
    return true
  # Walk up the parent chain with depth limit to prevent infinite loops
  var current = component
  var depth = 0
  const maxDepth = 100
  while current != nil and depth < maxDepth:
    if current == root:
      return true
    current = current.parent
    depth += 1
  return false

# Remove reactive text bindings for a set of components (used when rebuilding loops)
proc unregisterReactiveTextFor*(components: seq[ptr IRComponent]) =
  if components.len == 0: return
  var filtered: seq[ReactiveTextExpression] = @[]
  for expr in reactiveTextExpressions:
    var keep = true
    if expr != nil and expr.component != nil:
      for comp in components:
        # Check if expression's component is the root OR is a descendant of root
        if comp != nil and (comp == expr.component or isComponentInSubtree(expr.component, comp)):
          echo "[kryon][reactive] Unregistering text expression for component in subtree: ", expr.expression
          keep = false
          break
    if keep:
      filtered.add(expr)
  reactiveTextExpressions = filtered

# Remove reactive conditionals for a set of root components (removes nested conditionals)
proc unregisterReactiveConditionalsFor*(components: seq[ptr IRComponent]) =
  if components.len == 0: return
  var filtered: seq[ReactiveConditional] = @[]
  for cond in reactiveConditionals:
    var keep = true
    if cond != nil and cond.parent != nil:
      for comp in components:
        # Check if conditional's parent is the root OR is a descendant of root
        if comp != nil and (comp == cond.parent or isComponentInSubtree(cond.parent, comp)):
          echo "[kryon][reactive] Unregistering nested conditional in subtree"
          keep = false
          break
    if keep:
      filtered.add(cond)
  reactiveConditionals = filtered

proc isComponentInVisibleTree*(component: ptr IRComponent): bool =
  ## Check if a component is currently part of the rendered tree
  ## Returns true if walking up the parent chain reaches the root
  if component == nil:
    return false
  let root = ir_get_root()
  if root == nil:
    echo "[kryon][reactive] Warning: ir_get_root() returned nil - visibility check will fail"
    return false
  # Walk up the parent chain with depth limit to prevent infinite loops
  var current = component
  var depth = 0
  const maxDepth = 100
  while current != nil and depth < maxDepth:
    if current == root:
      return true
    current = current.parent
    depth += 1
  return false

proc cleanupReactiveConditionalsForSubtree*(root: ptr IRComponent) =
  ## Temporarily disable reactive conditionals in a component subtree
  ## Called when a tab panel is removed from the tree
  ## NOTE: We DON'T remove conditionals from the list or reset initialized
  ## We just mark them as suspended so they are skipped until re-activated
  if root == nil:
    return

  echo "[kryon][reactive] Suspending reactive state for subtree id=", root.id

  # Suspend reactive conditionals where parent is in the subtree (but don't remove from list)
  for cond in reactiveConditionals:
    if cond == nil:
      continue
    # Check if this conditional's parent is in the subtree being hidden
    if isComponentInSubtree(cond.parent, root):
      # Mark as suspended - DON'T reset initialized or clear children
      # This preserves the state so we don't re-create everything when shown again
      cond.suspended = true
      echo "[kryon][reactive] Suspended conditional (parent in subtree)"
  # NOTE: We keep all conditionals in reactiveConditionals - don't filter!

  # Suspend text expressions in subtree - same approach
  for expr in reactiveTextExpressions:
    if expr == nil:
      continue
    if isComponentInSubtree(expr.component, root):
      expr.suspended = true
      echo "[kryon][reactive] Suspended text expression (component in subtree)"
  # NOTE: We keep all text expressions in reactiveTextExpressions - don't filter!

proc nimOnComponentRemoved*(component: ptr IRComponent) {.exportc: "nimOnComponentRemoved", cdecl, dynlib.} =
  ## C-callable bridge function to clean up reactive state when a component is removed
  ## Called from ir_tabgroup_select before removing panels
  if component == nil:
    return

  let componentId = component.id
  echo "[kryon][reactive] Component removed callback for id=", componentId

  # Check if this component was already cleaned up - prevent double cleanup crash
  if componentId in cleanedUpComponentIds:
    echo "[kryon][reactive] Skipping cleanup for id=", componentId, " (already cleaned up)"
    return

  # Mark as cleaned up BEFORE doing the cleanup
  cleanedUpComponentIds.incl(componentId)

  cleanupReactiveConditionalsForSubtree(component)
  # Note: suspendReactiveForLoopsFor disabled until ReactiveForLoops are actually used

proc nimOnComponentAdded*(component: ptr IRComponent) {.exportc: "nimOnComponentAdded", cdecl, dynlib.} =
  ## C-callable bridge function called when a component is added to the tree
  ## This resets the cleaned-up status and resumes suspended conditionals
  if component == nil:
    return

  let componentId = component.id
  if componentId in cleanedUpComponentIds:
    echo "[kryon][reactive] Resetting cleanup status for id=", componentId, " (panel re-added)"
    cleanedUpComponentIds.excl(componentId)

  # Resume suspended conditionals for this subtree
  echo "[kryon][reactive] Resuming suspended state for subtree id=", componentId
  for cond in reactiveConditionals:
    if cond == nil:
      continue
    if cond.suspended and isComponentInSubtree(cond.parent, component):
      cond.suspended = false
      echo "[kryon][reactive] Resumed conditional (parent in subtree)"

  # Resume suspended text expressions for this subtree
  for expr in reactiveTextExpressions:
    if expr == nil:
      continue
    if expr.suspended and isComponentInSubtree(expr.component, component):
      expr.suspended = false
      echo "[kryon][reactive] Resumed text expression (component in subtree)"

  # Note: resumeReactiveForLoopsFor disabled until ReactiveForLoops are actually used

# ============================================================================
# Reactive For Loop System
# ============================================================================

type
  ReactiveForLoop* = ref object
    parent*: ptr IRComponent           # Container to add children to
    currentChildren*: seq[ptr IRComponent]  # Currently rendered components
    lastLength*: int                   # Cached length for change detection
    suspended*: bool                   # For tab switching
    # Type-erased update proc - captures the generic logic in a closure
    updateProc*: proc() {.closure.}

var reactiveForLoops*: seq[ReactiveForLoop] = @[]

# Forward declare the cleanup proc from runtime.nim
# This will be resolved at link time since runtime.nim exports it
proc nimCleanupHandlersForComponent*(component: ptr IRComponent) {.cdecl, importc: "nimCleanupHandlersForComponent".}

proc createReactiveForLoopImpl*[T](
    parent: ptr IRComponent,
    collectionProc: proc(): seq[T] {.closure.},
    itemProc: proc(item: T, index: int): ptr IRComponent {.closure.}
): ReactiveForLoop =
  ## Create and register a reactive for loop (implementation)
  var loop = ReactiveForLoop(
    parent: parent,
    currentChildren: @[],
    lastLength: -1,  # -1 means not initialized
    suspended: false
  )

  # Create the type-erased update closure
  loop.updateProc = proc() {.closure.} =
    if loop.suspended:
      return
    if loop.parent == nil or collectionProc == nil or itemProc == nil:
      return
    if not isComponentInVisibleTree(loop.parent):
      return

    let newCollection = collectionProc()
    let newLength = newCollection.len

    # Check if collection changed (simple length comparison for now)
    if newLength == loop.lastLength:
      return

    echo "[kryon][reactive] For loop collection changed: ", loop.lastLength, " -> ", newLength

    # Remove old children from IR tree
    for child in loop.currentChildren:
      if child != nil:
        # Clean up handlers for this component
        nimCleanupHandlersForComponent(child)
        # Call the removed callback
        nimOnComponentRemoved(child)
        # Remove from parent
        kryon_component_remove_child(loop.parent, child)

    # Clear and unregister old reactive bindings
    let oldChildren = loop.currentChildren
    loop.currentChildren.setLen(0)
    unregisterReactiveTextFor(oldChildren)
    unregisterReactiveBindingsFor(oldChildren)

    # Create new children
    for i, item in newCollection:
      let child = itemProc(item, i)
      if child != nil:
        loop.currentChildren.add(child)
        discard kryon_component_add_child(loop.parent, child)
        # Call the added callback
        nimOnComponentAdded(child)
        echo "[kryon][reactive] Added child for item ", i

    # Update cache and invalidate layout
    loop.lastLength = newLength
    kryon_component_mark_dirty(loop.parent)

    # Notify hook that parent's children changed (for TabGroup resync)
    if loop.parent != nil and onReactiveForLoopParentChanged != nil:
      onReactiveForLoopParentChanged(loop.parent)

  reactiveForLoops.add(loop)
  echo "[kryon][reactive] Created reactive for loop"
  result = loop

template createReactiveForLoop*[T](
    parent: ptr IRComponent,
    collectionProc: proc(): seq[T] {.closure.},
    itemProc: proc(item: T, index: int): ptr IRComponent {.closure.}
): ReactiveForLoop =
  ## Create and register a reactive for loop (template wrapper for type inference)
  createReactiveForLoopImpl[T](parent, collectionProc, itemProc)

proc updateAllReactiveForLoops*() =
  ## Update all reactive for loops
  for loop in reactiveForLoops:
    if loop == nil or loop.updateProc == nil:
      continue
    try:
      loop.updateProc()
    except Exception as e:
      echo "[kryon][reactive] Error updating for loop: ", e.msg

proc suspendReactiveForLoopsFor*(root: ptr IRComponent) =
  ## Suspend reactive for loops in a subtree
  for loop in reactiveForLoops:
    if loop == nil:
      continue
    if isComponentInSubtree(loop.parent, root):
      loop.suspended = true
      echo "[kryon][reactive] Suspended for loop (parent in subtree)"

proc resumeReactiveForLoopsFor*(root: ptr IRComponent) =
  ## Resume reactive for loops in a subtree
  for loop in reactiveForLoops:
    if loop == nil:
      continue
    if loop.suspended and isComponentInSubtree(loop.parent, root):
      loop.suspended = false
      echo "[kryon][reactive] Resumed for loop (parent in subtree)"

# ============================================================================
# Reactive State Export to IR Manifest
# ============================================================================

type
  # Wrapper to store reactive variables with type information
  ReactiveVarEntry* = ref object
    name*: string
    varType*: IRReactiveVarType
    getValue*: proc(): IRReactiveValue {.closure.}
    componentBindings*: seq[ptr IRComponent]  # Components bound to this var

# Global registry of named reactive variables for serialization
var namedReactiveVars*: Table[string, ReactiveVarEntry] = initTable[string, ReactiveVarEntry]()

# Register a reactive variable with a name for export
proc registerNamedReactiveVar*[T](name: string, rv: ReactiveVar[T]) =
  ## Register a reactive variable by name so it can be exported to manifest

  # Determine type and create value getter
  var varType: IRReactiveVarType
  var getValue: proc(): IRReactiveValue {.closure.}

  when T is int:
    varType = IR_REACTIVE_TYPE_INT
    getValue = proc(): IRReactiveValue {.closure.} =
      result.as_int = int32(rv.value)
  elif T is float or T is float32 or T is float64:
    varType = IR_REACTIVE_TYPE_FLOAT
    getValue = proc(): IRReactiveValue {.closure.} =
      result.as_float = float64(rv.value)
  elif T is string:
    varType = IR_REACTIVE_TYPE_STRING
    getValue = proc(): IRReactiveValue {.closure.} =
      result.as_string = cstring(rv.value)
  elif T is bool:
    varType = IR_REACTIVE_TYPE_BOOL
    getValue = proc(): IRReactiveValue {.closure.} =
      result.as_bool = rv.value
  else:
    # For other types, convert to JSON string (custom type)
    varType = IR_REACTIVE_TYPE_CUSTOM
    getValue = proc(): IRReactiveValue {.closure.} =
      result.as_string = cstring($rv.value)

  # Extract component bindings
  var components: seq[ptr IRComponent] = @[]
  for binding in rv.bindings:
    if binding != nil and binding.component != nil:
      components.add(binding.component)

  let entry = ReactiveVarEntry(
    name: name,
    varType: varType,
    getValue: getValue,
    componentBindings: components
  )

  namedReactiveVars[name] = entry
  echo "[kryon][reactive] Registered named reactive var: ", name

# Export all reactive state to an IR manifest
proc exportReactiveManifest*(): ptr IRReactiveManifest =
  ## Export all registered reactive state to an IRReactiveManifest
  ## This manifest can then be serialized along with the component tree

  result = ir_reactive_manifest_create()
  if result == nil:
    echo "[kryon][reactive] Failed to create manifest"
    return nil

  echo "[kryon][reactive] Exporting reactive state to manifest..."

  # Table to map variable names to their IDs
  var varNameToId: Table[string, uint32] = initTable[string, uint32]()

  # Export reactive variables
  for name, entry in namedReactiveVars:
    let value = entry.getValue()
    let varId = ir_reactive_manifest_add_var(result, cstring(name), entry.varType, value)
    varNameToId[name] = varId
    echo "[kryon][reactive] Exported var: ", name, " (id=", varId, ")"

    # Add metadata for v2.1 format (POC)
    let typeString = case entry.varType
      of IR_REACTIVE_TYPE_INT: "int"
      of IR_REACTIVE_TYPE_FLOAT: "float"
      of IR_REACTIVE_TYPE_STRING: "string"
      of IR_REACTIVE_TYPE_BOOL: "bool"
      else: "unknown"

    # Simple JSON serialization of initial value
    let initialValueJson = case entry.varType
      of IR_REACTIVE_TYPE_INT: $value.as_int
      of IR_REACTIVE_TYPE_FLOAT: $value.as_float
      of IR_REACTIVE_TYPE_STRING: "\"" & (if value.as_string != nil: $value.as_string else: "") & "\""
      of IR_REACTIVE_TYPE_BOOL: (if value.as_bool: "true" else: "false")
      else: "null"

    ir_reactive_manifest_set_var_metadata(
      result,
      varId,
      cstring(typeString),
      cstring(initialValueJson),
      cstring("global")  # All named vars are global scope for now
    )

    # Add bindings for this variable
    for component in entry.componentBindings:
      if component != nil and component.id > 0:
        ir_reactive_manifest_add_binding(
          result,
          component.id,
          varId,
          IR_BINDING_TEXT,  # Most bindings are text updates
          cstring(name)
        )

  # Export reactive text expressions
  for expr in reactiveTextExpressions:
    if expr == nil or expr.component == nil or expr.varNames.len == 0:
      continue

    # Find variable IDs for this expression
    var depVarIds: seq[uint32] = @[]
    for varName in expr.varNames:
      if varNameToId.hasKey(varName):
        depVarIds.add(varNameToId[varName])

    if depVarIds.len > 0:
      # Add as a conditional binding (text expressions are similar to conditionals)
      ir_reactive_manifest_add_conditional(
        result,
        expr.component.id,
        cstring(expr.expression),
        addr depVarIds[0],
        uint32(depVarIds.len)
      )

  # Export reactive conditionals
  for cond in reactiveConditionals:
    if cond == nil or cond.parent == nil:
      continue

    # Use the stored condition expression if available, otherwise placeholder
    let condExpr = if cond.conditionExpr.len > 0: cond.conditionExpr else: "<conditional>"
    ir_reactive_manifest_add_conditional(
      result,
      cond.parent.id,
      cstring(condExpr),
      nil,
      0
    )

    # Set branch children IDs (for serialization round-trip)
    let thenIdsPtr = if cond.thenChildrenIds.len > 0: addr cond.thenChildrenIds[0] else: nil
    let elseIdsPtr = if cond.elseChildrenIds.len > 0: addr cond.elseChildrenIds[0] else: nil
    ir_reactive_manifest_set_conditional_branches(
      result,
      cond.parent.id,
      thenIdsPtr,
      uint32(cond.thenChildrenIds.len),
      elseIdsPtr,
      uint32(cond.elseChildrenIds.len)
    )

  # Export reactive for loops
  for loop in reactiveForLoops:
    if loop == nil or loop.parent == nil:
      continue

    # Track the for loop and its parent
    ir_reactive_manifest_add_for_loop(
      result,
      loop.parent.id,
      cstring("<collection>"),  # Placeholder
      0  # No collection var ID available
    )

  echo "[kryon][reactive] Exported ", result.variable_count, " variables, ",
       result.binding_count, " bindings, ",
       result.conditional_count, " conditionals, ",
       result.for_loop_count, " for-loops"

  return result

# Helper to create a named reactive variable (combines creation + registration)
proc namedReactiveVar*[T](name: string, initialValue: T): ReactiveVar[T] =
  ## Creates a reactive variable and registers it by name for export
  result = reactiveVar[T](initialValue)
  registerNamedReactiveVar(name, result)
