# 0BSD
## Event Dispatcher - Unified Event Handling System
## Phase 1, Week 2: Event System Refactor
##
## This module replaces:
## - g_button_handlers table in runtime.nim
## - g_checkbox_handlers table in runtime.nim
## - Scattered event handling logic
##
## Design Goals:
## - Single event dispatch table per context with O(1) lookup
## - Type-safe event registration
## - Automatic cleanup when components are destroyed
## - Support for all event types (click, hover, focus, etc.)

import ../ir_core
import tables

type
  EventHandlerKey = tuple[componentId: uint32, eventType: IREventType]

  EventHandler* = proc() {.closure.}

  EventStatistics* = object
    ## Statistics for monitoring and debugging
    totalHandlers*: int
    handlersPerType*: array[IREventType, int]
    totalDispatches*: uint64
    failedDispatches*: uint64

  EventDispatcher* = ref object
    ## Central event dispatcher - O(1) hash table based
    initialized*: bool
    handlers: Table[EventHandlerKey, EventHandler]  # Hash table for O(1) lookup
    componentHandlers: Table[uint32, seq[IREventType]]  # Track handlers per component
    stats: EventStatistics  # Runtime statistics

# ============================================================================
# Event Dispatcher Creation
# ============================================================================

proc createEventDispatcher*(): EventDispatcher =
  ## Creates a new event dispatcher with hash tables
  result = EventDispatcher(
    initialized: true,
    handlers: initTable[EventHandlerKey, EventHandler](),
    componentHandlers: initTable[uint32, seq[IREventType]]()
  )

  # Initialize statistics
  result.stats.totalHandlers = 0
  result.stats.totalDispatches = 0
  result.stats.failedDispatches = 0
  for eventType in IREventType:
    result.stats.handlersPerType[eventType] = 0

# ============================================================================
# Event Handler Registration (O(1) operations)
# ============================================================================

proc registerEventHandler*(dispatcher: EventDispatcher,
                           componentId: uint32,
                           eventType: IREventType,
                           handler: EventHandler) =
  ## Registers an event handler for a component (O(1) hash table insert)
  if not dispatcher.initialized:
    return

  let key: EventHandlerKey = (componentId, eventType)

  # Store handler in hash table
  dispatcher.handlers[key] = handler

  # Track handler for component cleanup
  if componentId notin dispatcher.componentHandlers:
    dispatcher.componentHandlers[componentId] = @[]

  if eventType notin dispatcher.componentHandlers[componentId]:
    dispatcher.componentHandlers[componentId].add(eventType)

  # Update statistics
  dispatcher.stats.totalHandlers = dispatcher.handlers.len
  dispatcher.stats.handlersPerType[eventType] += 1

proc unregisterEventHandler*(dispatcher: EventDispatcher,
                             componentId: uint32,
                             eventType: IREventType) =
  ## Unregisters an event handler (O(1) hash table removal)
  if not dispatcher.initialized:
    return

  let key: EventHandlerKey = (componentId, eventType)

  if key in dispatcher.handlers:
    dispatcher.handlers.del(key)
    dispatcher.stats.totalHandlers = dispatcher.handlers.len
    dispatcher.stats.handlersPerType[eventType] -= 1

  # Remove from component tracking
  if componentId in dispatcher.componentHandlers:
    let idx = dispatcher.componentHandlers[componentId].find(eventType)
    if idx >= 0:
      dispatcher.componentHandlers[componentId].delete(idx)

# ============================================================================
# Event Dispatching (O(1) lookup)
# ============================================================================

proc dispatchEvent*(dispatcher: EventDispatcher,
                   componentId: uint32,
                   eventType: IREventType): bool =
  ## Dispatches an event to the registered handler (O(1) hash lookup)
  ## Returns true if handler was found and invoked, false otherwise
  if not dispatcher.initialized:
    dispatcher.stats.failedDispatches += 1
    return false

  let key: EventHandlerKey = (componentId, eventType)

  dispatcher.stats.totalDispatches += 1

  if key in dispatcher.handlers:
    # Handler found - invoke it
    try:
      dispatcher.handlers[key]()
      return true
    except:
      dispatcher.stats.failedDispatches += 1
      return false
  else:
    # No handler registered
    return false

proc hasHandler*(dispatcher: EventDispatcher,
                componentId: uint32,
                eventType: IREventType): bool =
  ## Checks if a handler is registered (O(1) lookup)
  if not dispatcher.initialized:
    return false

  let key: EventHandlerKey = (componentId, eventType)
  return key in dispatcher.handlers

# ============================================================================
# Component Cleanup
# ============================================================================

proc cleanupHandlers*(dispatcher: EventDispatcher, componentId: uint32) =
  ## Cleans up all handlers for a component
  if not dispatcher.initialized:
    return

  if componentId in dispatcher.componentHandlers:
    # Remove all event handlers for this component
    for eventType in dispatcher.componentHandlers[componentId]:
      dispatcher.unregisterEventHandler(componentId, eventType)

    # Remove component from tracking
    dispatcher.componentHandlers.del(componentId)

proc cleanupAll*(dispatcher: EventDispatcher) =
  ## Cleans up all registered handlers
  if not dispatcher.initialized:
    return

  dispatcher.handlers.clear()
  dispatcher.componentHandlers.clear()
  dispatcher.stats.totalHandlers = 0
  for eventType in IREventType:
    dispatcher.stats.handlersPerType[eventType] = 0

# ============================================================================
# Statistics and Debugging
# ============================================================================

proc getStatistics*(dispatcher: EventDispatcher): EventStatistics =
  ## Returns current event dispatcher statistics
  if not dispatcher.initialized:
    return EventStatistics()
  return dispatcher.stats

proc printStatistics*(dispatcher: EventDispatcher) =
  ## Prints event dispatcher statistics for debugging
  if not dispatcher.initialized:
    echo "Event dispatcher not initialized"
    return

  echo "Event Dispatcher Statistics:"
  echo "  Total handlers: ", dispatcher.stats.totalHandlers
  echo "  Total dispatches: ", dispatcher.stats.totalDispatches
  echo "  Failed dispatches: ", dispatcher.stats.failedDispatches
  echo "  Handlers by type:"
  for eventType in IREventType:
    if dispatcher.stats.handlersPerType[eventType] > 0:
      echo "    ", eventType, ": ", dispatcher.stats.handlersPerType[eventType]
