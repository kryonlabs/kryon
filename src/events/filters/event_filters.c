/**
 * @file event_filters.c
 * @brief Kryon Event Filters Implementation
 */

#include "internal/events.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// EVENT FILTER TYPES
// =============================================================================

typedef struct KryonEventFilter {
    KryonEventFilterType type;
    KryonEventType event_type;
    uint32_t element_id;
    KryonEventFilterCallback callback;
    void* user_data;
    bool enabled;
    
    // Filter-specific data
    union {
        struct {
            double debounce_time;
            double last_event_time;
        } debounce;
        
        struct {
            double throttle_time;
            double last_event_time;
        } throttle;
        
        struct {
            float min_distance;
            float last_x, last_y;
        } spatial;
        
        struct {
            double time_window;
            size_t max_events;
            double* event_times;
            size_t event_count;
            size_t event_index;
        } rate_limit;
        
        struct {
            float deadzone_radius;
            float center_x, center_y;
        } deadzone;
    } data;
    
    struct KryonEventFilter* next;
} KryonEventFilter;

// =============================================================================
// FILTER MANAGER STATE
// =============================================================================

typedef struct {
    KryonEventFilter* filters;
    size_t filter_count;
    
    // Statistics
    uint64_t events_filtered;
    uint64_t events_passed;
    double total_filter_time;
    
    // Configuration
    bool global_filtering_enabled;
    
} KryonEventFilterManager;

static KryonEventFilterManager g_filter_manager = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static KryonEventFilter* create_filter(KryonEventFilterType type, KryonEventType event_type,
                                      uint32_t element_id, KryonEventFilterCallback callback,
                                      void* user_data) {
    KryonEventFilter* filter = kryon_alloc(sizeof(KryonEventFilter));
    if (!filter) return NULL;
    
    memset(filter, 0, sizeof(KryonEventFilter));
    filter->type = type;
    filter->event_type = event_type;
    filter->element_id = element_id;
    filter->callback = callback;
    filter->user_data = user_data;
    filter->enabled = true;
    
    return filter;
}

static void destroy_filter(KryonEventFilter* filter) {
    if (!filter) return;
    
    // Clean up filter-specific data
    if (filter->type == KRYON_EVENT_FILTER_RATE_LIMIT && filter->data.rate_limit.event_times) {
        kryon_free(filter->data.rate_limit.event_times);
    }
    
    kryon_free(filter);
}

static bool apply_debounce_filter(KryonEventFilter* filter, const KryonEvent* event) {
    double current_time = event->timestamp;
    double time_since_last = current_time - filter->data.debounce.last_event_time;
    
    if (time_since_last >= filter->data.debounce.debounce_time) {
        filter->data.debounce.last_event_time = current_time;
        return true; // Pass the event
    }
    
    return false; // Filter out the event
}

static bool apply_throttle_filter(KryonEventFilter* filter, const KryonEvent* event) {
    double current_time = event->timestamp;
    double time_since_last = current_time - filter->data.throttle.last_event_time;
    
    if (time_since_last >= filter->data.throttle.throttle_time) {
        filter->data.throttle.last_event_time = current_time;
        return true; // Pass the event
    }
    
    return false; // Filter out the event
}

static bool apply_spatial_filter(KryonEventFilter* filter, const KryonEvent* event) {
    // Only apply to events with spatial coordinates
    if (event->type != KRYON_EVENT_MOUSE_MOVED && 
        event->type != KRYON_EVENT_TOUCH_MOVE) {
        return true; // Pass non-spatial events
    }
    
    float x, y;
    if (event->type == KRYON_EVENT_MOUSE_MOVED) {
        x = event->data.mouse.x;
        y = event->data.mouse.y;
    } else {
        x = event->data.touch.x;
        y = event->data.touch.y;
    }
    
    float distance = sqrtf(powf(x - filter->data.spatial.last_x, 2) + 
                          powf(y - filter->data.spatial.last_y, 2));
    
    if (distance >= filter->data.spatial.min_distance) {
        filter->data.spatial.last_x = x;
        filter->data.spatial.last_y = y;
        return true; // Pass the event
    }
    
    return false; // Filter out the event
}

static bool apply_rate_limit_filter(KryonEventFilter* filter, const KryonEvent* event) {
    double current_time = event->timestamp;
    
    // Add current event time
    if (filter->data.rate_limit.event_times) {
        filter->data.rate_limit.event_times[filter->data.rate_limit.event_index] = current_time;
        filter->data.rate_limit.event_index = (filter->data.rate_limit.event_index + 1) % 
                                             filter->data.rate_limit.max_events;
        
        if (filter->data.rate_limit.event_count < filter->data.rate_limit.max_events) {
            filter->data.rate_limit.event_count++;
        }
    }
    
    // Count events within time window
    size_t events_in_window = 0;
    for (size_t i = 0; i < filter->data.rate_limit.event_count; i++) {
        double event_time = filter->data.rate_limit.event_times[i];
        if (current_time - event_time <= filter->data.rate_limit.time_window) {
            events_in_window++;
        }
    }
    
    return events_in_window <= filter->data.rate_limit.max_events;
}

static bool apply_deadzone_filter(KryonEventFilter* filter, const KryonEvent* event) {
    // Only apply to events with spatial coordinates
    if (event->type != KRYON_EVENT_MOUSE_MOVED && 
        event->type != KRYON_EVENT_TOUCH_MOVE) {
        return true; // Pass non-spatial events
    }
    
    float x, y;
    if (event->type == KRYON_EVENT_MOUSE_MOVED) {
        x = event->data.mouse.x;
        y = event->data.mouse.y;
    } else {
        x = event->data.touch.x;
        y = event->data.touch.y;
    }
    
    float distance = sqrtf(powf(x - filter->data.deadzone.center_x, 2) + 
                          powf(y - filter->data.deadzone.center_y, 2));
    
    return distance > filter->data.deadzone.deadzone_radius;
}

static bool apply_filter(KryonEventFilter* filter, const KryonEvent* event) {
    if (!filter->enabled) return true;
    
    // Check if filter applies to this event
    if (filter->event_type != KRYON_EVENT_TYPE_COUNT && filter->event_type != event->type) {
        return true; // Filter doesn't apply to this event type
    }
    
    if (filter->element_id != 0 && filter->element_id != event->target_id) {
        return true; // Filter doesn't apply to this element
    }
    
    // Apply the appropriate filter
    bool result = true;
    
    switch (filter->type) {
        case KRYON_EVENT_FILTER_DEBOUNCE:
            result = apply_debounce_filter(filter, event);
            break;
            
        case KRYON_EVENT_FILTER_THROTTLE:
            result = apply_throttle_filter(filter, event);
            break;
            
        case KRYON_EVENT_FILTER_SPATIAL:
            result = apply_spatial_filter(filter, event);
            break;
            
        case KRYON_EVENT_FILTER_RATE_LIMIT:
            result = apply_rate_limit_filter(filter, event);
            break;
            
        case KRYON_EVENT_FILTER_DEADZONE:
            result = apply_deadzone_filter(filter, event);
            break;
            
        case KRYON_EVENT_FILTER_CUSTOM:
            if (filter->callback) {
                result = filter->callback(event, filter->user_data);
            }
            break;
            
        default:
            result = true;
            break;
    }
    
    return result;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_event_filter_init(void) {
    memset(&g_filter_manager, 0, sizeof(g_filter_manager));
    g_filter_manager.global_filtering_enabled = true;
    return true;
}

void kryon_event_filter_shutdown(void) {
    // Clean up all filters
    KryonEventFilter* current = g_filter_manager.filters;
    while (current) {
        KryonEventFilter* next = current->next;
        destroy_filter(current);
        current = next;
    }
    
    memset(&g_filter_manager, 0, sizeof(g_filter_manager));
}

KryonEventFilterId kryon_event_filter_add_debounce(KryonEventType event_type, uint32_t element_id,
                                                   double debounce_time) {
    KryonEventFilter* filter = create_filter(KRYON_EVENT_FILTER_DEBOUNCE, event_type, element_id, NULL, NULL);
    if (!filter) return 0;
    
    filter->data.debounce.debounce_time = debounce_time;
    filter->data.debounce.last_event_time = 0.0;
    
    // Add to filter list
    filter->next = g_filter_manager.filters;
    g_filter_manager.filters = filter;
    g_filter_manager.filter_count++;
    
    return (KryonEventFilterId)filter;
}

KryonEventFilterId kryon_event_filter_add_throttle(KryonEventType event_type, uint32_t element_id,
                                                   double throttle_time) {
    KryonEventFilter* filter = create_filter(KRYON_EVENT_FILTER_THROTTLE, event_type, element_id, NULL, NULL);
    if (!filter) return 0;
    
    filter->data.throttle.throttle_time = throttle_time;
    filter->data.throttle.last_event_time = 0.0;
    
    // Add to filter list
    filter->next = g_filter_manager.filters;
    g_filter_manager.filters = filter;
    g_filter_manager.filter_count++;
    
    return (KryonEventFilterId)filter;
}

KryonEventFilterId kryon_event_filter_add_spatial(KryonEventType event_type, uint32_t element_id,
                                                  float min_distance) {
    KryonEventFilter* filter = create_filter(KRYON_EVENT_FILTER_SPATIAL, event_type, element_id, NULL, NULL);
    if (!filter) return 0;
    
    filter->data.spatial.min_distance = min_distance;
    filter->data.spatial.last_x = 0.0f;
    filter->data.spatial.last_y = 0.0f;
    
    // Add to filter list
    filter->next = g_filter_manager.filters;
    g_filter_manager.filters = filter;
    g_filter_manager.filter_count++;
    
    return (KryonEventFilterId)filter;
}

KryonEventFilterId kryon_event_filter_add_rate_limit(KryonEventType event_type, uint32_t element_id,
                                                     double time_window, size_t max_events) {
    KryonEventFilter* filter = create_filter(KRYON_EVENT_FILTER_RATE_LIMIT, event_type, element_id, NULL, NULL);
    if (!filter) return 0;
    
    filter->data.rate_limit.time_window = time_window;
    filter->data.rate_limit.max_events = max_events;
    filter->data.rate_limit.event_times = kryon_alloc(sizeof(double) * max_events);
    filter->data.rate_limit.event_count = 0;
    filter->data.rate_limit.event_index = 0;
    
    if (!filter->data.rate_limit.event_times) {
        destroy_filter(filter);
        return 0;
    }
    
    // Add to filter list
    filter->next = g_filter_manager.filters;
    g_filter_manager.filters = filter;
    g_filter_manager.filter_count++;
    
    return (KryonEventFilterId)filter;
}

KryonEventFilterId kryon_event_filter_add_deadzone(KryonEventType event_type, uint32_t element_id,
                                                   float center_x, float center_y, float radius) {
    KryonEventFilter* filter = create_filter(KRYON_EVENT_FILTER_DEADZONE, event_type, element_id, NULL, NULL);
    if (!filter) return 0;
    
    filter->data.deadzone.center_x = center_x;
    filter->data.deadzone.center_y = center_y;
    filter->data.deadzone.deadzone_radius = radius;
    
    // Add to filter list
    filter->next = g_filter_manager.filters;
    g_filter_manager.filters = filter;
    g_filter_manager.filter_count++;
    
    return (KryonEventFilterId)filter;
}

KryonEventFilterId kryon_event_filter_add_custom(KryonEventType event_type, uint32_t element_id,
                                                 KryonEventFilterCallback callback, void* user_data) {
    if (!callback) return 0;
    
    KryonEventFilter* filter = create_filter(KRYON_EVENT_FILTER_CUSTOM, event_type, element_id, callback, user_data);
    if (!filter) return 0;
    
    // Add to filter list
    filter->next = g_filter_manager.filters;
    g_filter_manager.filters = filter;
    g_filter_manager.filter_count++;
    
    return (KryonEventFilterId)filter;
}

bool kryon_event_filter_remove(KryonEventFilterId filter_id) {
    if (filter_id == 0) return false;
    
    KryonEventFilter* target = (KryonEventFilter*)filter_id;
    KryonEventFilter* current = g_filter_manager.filters;
    KryonEventFilter* prev = NULL;
    
    while (current) {
        if (current == target) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_filter_manager.filters = current->next;
            }
            
            destroy_filter(current);
            g_filter_manager.filter_count--;
            return true;
        }
        
        prev = current;
        current = current->next;
    }
    
    return false;
}

void kryon_event_filter_remove_all(uint32_t element_id) {
    KryonEventFilter* current = g_filter_manager.filters;
    KryonEventFilter* prev = NULL;
    
    while (current) {
        if (current->element_id == element_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_filter_manager.filters = current->next;
            }
            
            KryonEventFilter* to_remove = current;
            current = current->next;
            destroy_filter(to_remove);
            g_filter_manager.filter_count--;
        } else {
            prev = current;
            current = current->next;
        }
    }
}

bool kryon_event_filter_enable(KryonEventFilterId filter_id, bool enabled) {
    if (filter_id == 0) return false;
    
    KryonEventFilter* filter = (KryonEventFilter*)filter_id;
    filter->enabled = enabled;
    return true;
}

bool kryon_event_filter_process(KryonEvent* event) {
    if (!g_filter_manager.global_filtering_enabled || !event) {
        return true;
    }
    
    double start_time = kryon_platform_get_time();
    bool result = true;
    
    // Apply all applicable filters
    KryonEventFilter* current = g_filter_manager.filters;
    while (current && result) {
        result = apply_filter(current, event);
        current = current->next;
    }
    
    // Update statistics
    double end_time = kryon_platform_get_time();
    g_filter_manager.total_filter_time += (end_time - start_time);
    
    if (result) {
        g_filter_manager.events_passed++;
    } else {
        g_filter_manager.events_filtered++;
    }
    
    return result;
}

void kryon_event_filter_enable_global(bool enabled) {
    g_filter_manager.global_filtering_enabled = enabled;
}

bool kryon_event_filter_is_global_enabled(void) {
    return g_filter_manager.global_filtering_enabled;
}

KryonEventFilterStats kryon_event_filter_get_stats(void) {
    KryonEventFilterStats stats = {0};
    stats.active_filters = g_filter_manager.filter_count;
    stats.events_filtered = g_filter_manager.events_filtered;
    stats.events_passed = g_filter_manager.events_passed;
    stats.total_filter_time = g_filter_manager.total_filter_time;
    
    uint64_t total_events = stats.events_filtered + stats.events_passed;
    stats.filter_ratio = total_events > 0 ? (double)stats.events_filtered / total_events : 0.0;
    stats.average_filter_time = total_events > 0 ? stats.total_filter_time / total_events : 0.0;
    
    return stats;
}

void kryon_event_filter_clear_stats(void) {
    g_filter_manager.events_filtered = 0;
    g_filter_manager.events_passed = 0;
    g_filter_manager.total_filter_time = 0.0;
}