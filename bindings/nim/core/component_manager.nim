# 0BSD
## Component Manager - Component Lifecycle Management
## Phase 1, Week 2: Thin wrapper over IR component functions
##
## This module provides a high-level Nim API over IR component functions.
## ALL LOGIC IS IN IR - this is just a convenience wrapper.
##
## Design Goals:
## - Thin wrapper - no state duplication
## - Coordinate cleanup with EventDispatcher
## - Type-safe Nim API over C functions
## - All actual state lives in IR layer

import ../ir_core
import event_dispatcher

type
  ComponentManager* = ref object
    ## Component lifecycle coordinator - holds NO state
    ## All component state lives in IRContext
    irContext*: ptr IRContext
    eventDispatcher*: EventDispatcher  # For cleanup coordination

# ============================================================================
# Component Manager Creation
# ============================================================================

proc createComponentManager*(ctx: ptr IRContext, dispatcher: EventDispatcher): ComponentManager =
  ## Creates a new component manager
  ## Note: Holds references to IR context and event dispatcher only
  result = ComponentManager(
    irContext: ctx,
    eventDispatcher: dispatcher
  )

# ============================================================================
# Component Creation (Direct IR Wrappers)
# ============================================================================

proc createComponent*(manager: ComponentManager,
                     componentType: IRComponentType): ptr IRComponent =
  ## Creates a new component with automatic ID generation (IR manages IDs)
  ## Returns: Pointer to IR component (managed by IR layer)
  result = ir_create_component(componentType)

proc createComponentWithId*(manager: ComponentManager,
                           componentType: IRComponentType,
                           id: uint32): ptr IRComponent =
  ## Creates a component with specific ID (for deserialization/hot reload)
  result = ir_create_component_with_id(componentType, id)

# ============================================================================
# Component Tree Operations (Direct IR Wrappers)
# ============================================================================

proc addChild*(manager: ComponentManager,
               parent: ptr IRComponent,
               child: ptr IRComponent) =
  ## Adds child to parent (IR manages parent-child relationships)
  ir_add_child(parent, child)

proc removeChild*(manager: ComponentManager,
                  parent: ptr IRComponent,
                  child: ptr IRComponent) =
  ## Removes child from parent
  ir_remove_child(parent, child)

proc insertChild*(manager: ComponentManager,
                  parent: ptr IRComponent,
                  child: ptr IRComponent,
                  index: uint32) =
  ## Inserts child at specific index
  ir_insert_child(parent, child, index)

proc getChild*(manager: ComponentManager,
               parent: ptr IRComponent,
               index: uint32): ptr IRComponent =
  ## Gets child at index
  result = ir_get_child(parent, index)

# ============================================================================
# Component Lookup (Direct IR Wrappers)
# ============================================================================

proc findComponentById*(manager: ComponentManager,
                       root: ptr IRComponent,
                       componentId: uint32): ptr IRComponent =
  ## Finds component by ID (IR performs tree traversal)
  ## Returns: Component if found, nil otherwise
  result = ir_find_component_by_id(root, componentId)

proc findComponentAtPoint*(manager: ComponentManager,
                          root: ptr IRComponent,
                          x: float,
                          y: float): ptr IRComponent =
  ## Finds component at screen coordinates (for hit testing)
  result = ir_find_component_at_point(root, x.cfloat, y.cfloat)

# ============================================================================
# Component Destruction with Cleanup Coordination
# ============================================================================

proc destroyComponent*(manager: ComponentManager,
                      component: ptr IRComponent) =
  ## Destroys a component and cleans up all associated resources
  ## Coordinates cleanup with event dispatcher
  if component.isNil:
    return

  # Step 1: Clean up event handlers for this component
  if not manager.eventDispatcher.isNil:
    manager.eventDispatcher.cleanupHandlers(component.id)

  # Step 2: Recursively clean up event handlers for all children
  if component.child_count > 0:
    for i in 0..<component.child_count:
      let child = ir_get_child(component, i)
      if not child.isNil:
        manager.destroyComponent(child)  # Recursive cleanup

  # Step 3: Destroy component in IR (IR frees memory)
  ir_destroy_component(component)

proc destroySubtree*(manager: ComponentManager,
                    root: ptr IRComponent) =
  ## Destroys entire subtree rooted at component
  ## Alias for destroyComponent (already handles children)
  manager.destroyComponent(root)

# ============================================================================
# Component Validation and Debugging (Direct IR Wrappers)
# ============================================================================

proc validateComponent*(manager: ComponentManager,
                       component: ptr IRComponent): bool =
  ## Validates component structure (IR validation logic)
  result = ir_validate_component(component)

proc printComponentInfo*(manager: ComponentManager,
                        component: ptr IRComponent,
                        depth: int = 0) =
  ## Prints component debug info (IR handles formatting)
  ir_print_component_info(component, depth.cint)

proc optimizeComponent*(manager: ComponentManager,
                       component: ptr IRComponent) =
  ## Optimizes component tree (IR optimization pass)
  ir_optimize_component(component)

# ============================================================================
# Bulk Operations
# ============================================================================

proc finalizeSubtree*(manager: ComponentManager,
                     component: ptr IRComponent) =
  ## Finalizes entire subtree (post-construction processing)
  ## Used after building component tree to trigger IR finalization
  ir_component_finalize_subtree(component)

# ============================================================================
# Component Convenience Factories
# ============================================================================

proc createContainer*(manager: ComponentManager): ptr IRComponent =
  ## Creates a container component
  result = ir_container()

proc createText*(manager: ComponentManager, content: string): ptr IRComponent =
  ## Creates a text component
  result = ir_text(cstring(content))

proc createButton*(manager: ComponentManager, content: string): ptr IRComponent =
  ## Creates a button component
  result = ir_button(cstring(content))

proc createInput*(manager: ComponentManager): ptr IRComponent =
  ## Creates an input component
  result = ir_input()

proc createCheckbox*(manager: ComponentManager, label: string): ptr IRComponent =
  ## Creates a checkbox component
  result = ir_checkbox(cstring(label))

proc createRow*(manager: ComponentManager): ptr IRComponent =
  ## Creates a row layout component
  result = ir_row()

proc createColumn*(manager: ComponentManager): ptr IRComponent =
  ## Creates a column layout component
  result = ir_column()

proc createCenter*(manager: ComponentManager): ptr IRComponent =
  ## Creates a center layout component
  result = ir_center()

proc createDropdown*(manager: ComponentManager,
                    placeholder: string,
                    options: seq[string]): ptr IRComponent =
  ## Creates a dropdown component
  # Convert seq[string] to ptr cstring array
  var cOptions: seq[cstring]
  for opt in options:
    cOptions.add(cstring(opt))

  if cOptions.len > 0:
    result = ir_dropdown(cstring(placeholder), cast[ptr cstring](addr cOptions[0]), cOptions.len.uint32)
  else:
    result = ir_dropdown(cstring(placeholder), nil, 0)
