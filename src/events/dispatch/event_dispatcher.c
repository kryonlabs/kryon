/**
 * @file event_dispatcher.c
 * @brief Kryon Event Dispatcher Implementation
 */

#include "internal/events.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// EVENT DISPATCHER STATE
// =============================================================================

typedef struct KryonEventListener {
    KryonEventType event_type;
    uint32_t element_id;          // 0 for global listeners
    KryonEventCallback callback;
    void* user_data;
    int priority;                 // Higher priority = called first
    bool once;                    // Remove after first call
    struct KryonEventListener* next;
} KryonEventListener;

typedef struct {
    // Event listeners organized by type
    KryonEventListener* listeners[KRYON_EVENT_TYPE_COUNT];
    
    // Event queue for deferred processing
    KryonEvent* event_queue;
    size_t queue_capacity;
    size_t queue_size;
    size_t queue_head;
    size_t queue_tail;
    
    // Global event listeners (listen to all events)
    KryonEventListener* global_listeners;
    
    // Event propagation control
    bool propagation_stopped;
    bool immediate_propagation_stopped;
    
    // Statistics
    uint64_t events_dispatched;
    uint64_t listeners_called;
    double total_dispatch_time;
    
    // Configuration
    bool enable_event_bubbling;
    bool enable_event_capturing;
    bool async_dispatch;
    size_t max_event_depth;
    
} KryonEventDispatcher;

static KryonEventDispatcher g_event_dispatcher = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static KryonEventListener* create_listener(KryonEventType event_type, uint32_t element_id,
                                          KryonEventCallback callback, void* user_data,
                                          int priority, bool once) {
    KryonEventListener* listener = kryon_alloc(sizeof(KryonEventListener));
    if (!listener) return NULL;
    
    listener->event_type = event_type;
    listener->element_id = element_id;
    listener->callback = callback;
    listener->user_data = user_data;
    listener->priority = priority;
    listener->once = once;
    listener->next = NULL;
    
    return listener;
}

static void insert_listener_sorted(KryonEventListener** head, KryonEventListener* new_listener) {
    if (!*head || new_listener->priority > (*head)->priority) {
        new_listener->next = *head;
        *head = new_listener;
        return;
    }
    
    KryonEventListener* current = *head;
    while (current->next && current->next->priority >= new_listener->priority) {
        current = current->next;
    }
    
    new_listener->next = current->next;
    current->next = new_listener;
}

static bool remove_listener_from_list(KryonEventListener** head, KryonEventCallback callback,
                                     uint32_t element_id) {
    KryonEventListener* current = *head;
    KryonEventListener* prev = NULL;
    
    while (current) {
        if (current->callback == callback && current->element_id == element_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                *head = current->next;
            }
            kryon_free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    
    return false;
}

static void cleanup_once_listeners(KryonEventListener** head) {
    KryonEventListener* current = *head;
    KryonEventListener* prev = NULL;
    
    while (current) {
        if (current->once) {
            if (prev) {
                prev->next = current->next;
            } else {
                *head = current->next;
            }
            
            KryonEventListener* to_free = current;
            current = current->next;
            kryon_free(to_free);
        } else {
            prev = current;
            current = current->next;
        }
    }
}

static bool expand_event_queue(void) {
    if (g_event_dispatcher.queue_capacity == 0) {
        g_event_dispatcher.queue_capacity = 256;
        g_event_dispatcher.event_queue = kryon_alloc(sizeof(KryonEvent) * g_event_dispatcher.queue_capacity);
        return g_event_dispatcher.event_queue != NULL;
    }
    
    size_t new_capacity = g_event_dispatcher.queue_capacity * 2;
    KryonEvent* new_queue = kryon_alloc(sizeof(KryonEvent) * new_capacity);
    if (!new_queue) return false;
    
    // Copy existing events
    if (g_event_dispatcher.queue_size > 0) {
        if (g_event_dispatcher.queue_head <= g_event_dispatcher.queue_tail) {
            // Contiguous data
            memcpy(new_queue, 
                   &g_event_dispatcher.event_queue[g_event_dispatcher.queue_head],
                   sizeof(KryonEvent) * g_event_dispatcher.queue_size);
        } else {
            // Wrapped data
            size_t tail_count = g_event_dispatcher.queue_capacity - g_event_dispatcher.queue_head;
            memcpy(new_queue,
                   &g_event_dispatcher.event_queue[g_event_dispatcher.queue_head],
                   sizeof(KryonEvent) * tail_count);
            memcpy(&new_queue[tail_count],
                   g_event_dispatcher.event_queue,
                   sizeof(KryonEvent) * g_event_dispatcher.queue_tail);
        }
    }
    
    kryon_free(g_event_dispatcher.event_queue);
    g_event_dispatcher.event_queue = new_queue;
    g_event_dispatcher.queue_capacity = new_capacity;
    g_event_dispatcher.queue_head = 0;
    g_event_dispatcher.queue_tail = g_event_dispatcher.queue_size;
    
    return true;
}

static void call_listeners(KryonEventListener* listeners, KryonEvent* event) {
    KryonEventListener* current = listeners;
    KryonEventListener* prev = NULL;
    
    while (current && !g_event_dispatcher.immediate_propagation_stopped) {
        bool should_call = false;
        
        // Check if this listener should be called for this event
        if (current->element_id == 0 || current->element_id == event->target_id) {
            should_call = true;
        }
        
        if (should_call && current->callback) {
            double start_time = kryon_platform_get_time();
            current->callback(event, current->user_data);
            double end_time = kryon_platform_get_time();
            
            g_event_dispatcher.total_dispatch_time += (end_time - start_time);
            g_event_dispatcher.listeners_called++;
        }
        
        // Remove "once" listeners after calling
        if (current->once && should_call) {
            if (prev) {
                prev->next = current->next;
            } else {
                // This affects the head of the list, but we can't modify it here
                // Mark for later cleanup
            }
            KryonEventListener* to_free = current;
            current = current->next;
            kryon_free(to_free);
        } else {
            prev = current;
            current = current->next;
        }
        
        if (g_event_dispatcher.propagation_stopped) {
            break;
        }
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_event_dispatcher_init(const KryonEventDispatcherConfig* config) {
    memset(&g_event_dispatcher, 0, sizeof(g_event_dispatcher));
    
    if (config) {
        g_event_dispatcher.enable_event_bubbling = config->enable_bubbling;
        g_event_dispatcher.enable_event_capturing = config->enable_capturing;
        g_event_dispatcher.async_dispatch = config->async_dispatch;
        g_event_dispatcher.max_event_depth = config->max_event_depth;
    } else {
        // Default configuration
        g_event_dispatcher.enable_event_bubbling = true;
        g_event_dispatcher.enable_event_capturing = false;
        g_event_dispatcher.async_dispatch = false;
        g_event_dispatcher.max_event_depth = 32;
    }
    
    return expand_event_queue();
}

void kryon_event_dispatcher_shutdown(void) {
    // Clean up all listeners
    for (int i = 0; i < KRYON_EVENT_TYPE_COUNT; i++) {
        KryonEventListener* current = g_event_dispatcher.listeners[i];
        while (current) {
            KryonEventListener* next = current->next;
            kryon_free(current);
            current = next;
        }
    }
    
    // Clean up global listeners
    KryonEventListener* current = g_event_dispatcher.global_listeners;
    while (current) {
        KryonEventListener* next = current->next;
        kryon_free(current);
        current = next;
    }
    
    // Free event queue
    kryon_free(g_event_dispatcher.event_queue);
    
    memset(&g_event_dispatcher, 0, sizeof(g_event_dispatcher));
}

bool kryon_event_add_listener(KryonEventType event_type, uint32_t element_id,
                             KryonEventCallback callback, void* user_data, int priority) {
    if (event_type >= KRYON_EVENT_TYPE_COUNT || !callback) {
        return false;
    }
    
    KryonEventListener* listener = create_listener(event_type, element_id, callback, 
                                                  user_data, priority, false);
    if (!listener) return false;
    
    insert_listener_sorted(&g_event_dispatcher.listeners[event_type], listener);
    return true;
}

bool kryon_event_add_listener_once(KryonEventType event_type, uint32_t element_id,
                                  KryonEventCallback callback, void* user_data, int priority) {
    if (event_type >= KRYON_EVENT_TYPE_COUNT || !callback) {
        return false;
    }
    
    KryonEventListener* listener = create_listener(event_type, element_id, callback,
                                                  user_data, priority, true);
    if (!listener) return false;
    
    insert_listener_sorted(&g_event_dispatcher.listeners[event_type], listener);
    return true;
}

bool kryon_event_remove_listener(KryonEventType event_type, uint32_t element_id,
                                KryonEventCallback callback) {
    if (event_type >= KRYON_EVENT_TYPE_COUNT) {
        return false;
    }
    
    return remove_listener_from_list(&g_event_dispatcher.listeners[event_type], callback, element_id);
}

void kryon_event_remove_all_listeners(uint32_t element_id) {
    for (int i = 0; i < KRYON_EVENT_TYPE_COUNT; i++) {
        KryonEventListener** head = &g_event_dispatcher.listeners[i];
        KryonEventListener* current = *head;
        KryonEventListener* prev = NULL;
        
        while (current) {
            if (current->element_id == element_id) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    *head = current->next;
                }
                
                KryonEventListener* to_free = current;
                current = current->next;
                kryon_free(to_free);
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
}

bool kryon_event_add_global_listener(KryonEventCallback callback, void* user_data, int priority) {
    if (!callback) return false;
    
    KryonEventListener* listener = create_listener(KRYON_EVENT_TYPE_COUNT, 0, callback,
                                                  user_data, priority, false);
    if (!listener) return false;
    
    insert_listener_sorted(&g_event_dispatcher.global_listeners, listener);
    return true;
}

bool kryon_event_remove_global_listener(KryonEventCallback callback) {
    return remove_listener_from_list(&g_event_dispatcher.global_listeners, callback, 0);
}

bool kryon_event_dispatch(KryonEvent* event) {
    if (!event || event->type >= KRYON_EVENT_TYPE_COUNT) {
        return false;
    }
    
    if (g_event_dispatcher.async_dispatch) {
        return kryon_event_queue_event(event);
    }
    
    return kryon_event_dispatch_immediate(event);
}

bool kryon_event_dispatch_immediate(KryonEvent* event) {
    if (!event || event->type >= KRYON_EVENT_TYPE_COUNT) {
        return false;
    }
    
    double start_time = kryon_platform_get_time();
    
    // Reset propagation flags
    g_event_dispatcher.propagation_stopped = false;
    g_event_dispatcher.immediate_propagation_stopped = false;
    
    // Set event phase
    event->phase = KRYON_EVENT_PHASE_TARGET;
    event->timestamp = start_time;
    
    // Call global listeners first
    call_listeners(g_event_dispatcher.global_listeners, event);
    
    // Call specific event type listeners
    if (!g_event_dispatcher.immediate_propagation_stopped) {
        call_listeners(g_event_dispatcher.listeners[event->type], event);
    }
    
    // Clean up "once" listeners
    cleanup_once_listeners(&g_event_dispatcher.listeners[event->type]);
    cleanup_once_listeners(&g_event_dispatcher.global_listeners);
    
    // Update statistics
    g_event_dispatcher.events_dispatched++;
    
    double end_time = kryon_platform_get_time();
    g_event_dispatcher.total_dispatch_time += (end_time - start_time);
    
    return true;
}

bool kryon_event_queue_event(const KryonEvent* event) {
    if (!event || event->type >= KRYON_EVENT_TYPE_COUNT) {
        return false;
    }
    
    // Expand queue if needed
    if (g_event_dispatcher.queue_size >= g_event_dispatcher.queue_capacity) {
        if (!expand_event_queue()) {
            return false;
        }
    }
    
    // Add event to queue
    g_event_dispatcher.event_queue[g_event_dispatcher.queue_tail] = *event;
    g_event_dispatcher.queue_tail = (g_event_dispatcher.queue_tail + 1) % g_event_dispatcher.queue_capacity;
    g_event_dispatcher.queue_size++;
    
    return true;
}

void kryon_event_process_queue(void) {
    while (g_event_dispatcher.queue_size > 0) {
        KryonEvent event = g_event_dispatcher.event_queue[g_event_dispatcher.queue_head];
        g_event_dispatcher.queue_head = (g_event_dispatcher.queue_head + 1) % g_event_dispatcher.queue_capacity;
        g_event_dispatcher.queue_size--;
        
        kryon_event_dispatch_immediate(&event);
    }
}

void kryon_event_stop_propagation(void) {
    g_event_dispatcher.propagation_stopped = true;
}

void kryon_event_stop_immediate_propagation(void) {
    g_event_dispatcher.immediate_propagation_stopped = true;
    g_event_dispatcher.propagation_stopped = true;
}

KryonEventDispatcherStats kryon_event_get_stats(void) {
    KryonEventDispatcherStats stats = {0};
    stats.events_dispatched = g_event_dispatcher.events_dispatched;
    stats.listeners_called = g_event_dispatcher.listeners_called;
    stats.total_dispatch_time = g_event_dispatcher.total_dispatch_time;
    stats.queued_events = g_event_dispatcher.queue_size;
    stats.average_dispatch_time = g_event_dispatcher.events_dispatched > 0 ?
        g_event_dispatcher.total_dispatch_time / g_event_dispatcher.events_dispatched : 0.0;
    
    return stats;
}

void kryon_event_clear_stats(void) {
    g_event_dispatcher.events_dispatched = 0;
    g_event_dispatcher.listeners_called = 0;
    g_event_dispatcher.total_dispatch_time = 0.0;
}

size_t kryon_event_get_listener_count(KryonEventType event_type) {
    if (event_type >= KRYON_EVENT_TYPE_COUNT) {
        return 0;
    }
    
    size_t count = 0;
    KryonEventListener* current = g_event_dispatcher.listeners[event_type];
    
    while (current) {
        count++;
        current = current->next;
    }
    
    return count;
}

void kryon_event_clear_queue(void) {
    g_event_dispatcher.queue_size = 0;
    g_event_dispatcher.queue_head = 0;
    g_event_dispatcher.queue_tail = 0;
}