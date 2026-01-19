#ifndef IR_HASHMAP_H
#define IR_HASHMAP_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct IRComponent IRComponent;
typedef struct IREvent IREvent;

// IREventType is defined in ir_core.h
#include "../include/ir_core.h"

// ============================================================================
// Component ID Hash Map (for fast lookups)
// ============================================================================

typedef struct IRComponentMap IRComponentMap;

// Create a new component hash map
IRComponentMap* ir_map_create(uint32_t initial_capacity);

// Destroy the map (doesn't destroy components)
void ir_map_destroy(IRComponentMap* map);

// Insert a component into the map
bool ir_map_insert(IRComponentMap* map, IRComponent* component);

// Lookup a component by ID (O(1) average case)
IRComponent* ir_map_lookup(IRComponentMap* map, uint32_t id);

// Remove a component from the map
bool ir_map_remove(IRComponentMap* map, uint32_t id);

// Get the number of components in the map
uint32_t ir_map_get_count(IRComponentMap* map);

// Clear all components from the map
void ir_map_clear(IRComponentMap* map);

// ============================================================================
// Event Hash Map (for fast event dispatch)
// ============================================================================

typedef struct IREventMap IREventMap;

// Create a new event hash map
IREventMap* ir_event_map_create(void);

// Destroy the event map and all events
void ir_event_map_destroy(IREventMap* map);

// Add an event to the map
void ir_event_map_add(IREventMap* map, IREvent* event);

// Find all events of a given type
IREvent* ir_event_map_find(IREventMap* map, IREventType type);

// Remove an event from the map
bool ir_event_map_remove(IREventMap* map, IREventType type);

#endif // IR_HASHMAP_H
