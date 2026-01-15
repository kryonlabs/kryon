/**
 * Kryon Reactive State System - Implementation
 *
 * Implementation of signals, computed values, and effects for C.
 * Provides automatic dependency tracking similar to Vue 3 / SolidJS.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "kryon_reactive.h"
#include "../ir/ir_constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>

// ============================================================================
// Constants
// ============================================================================

#define KRYON_SIGNAL_TYPE_FLOAT   0
#define KRYON_SIGNAL_TYPE_STRING  1
#define KRYON_SIGNAL_TYPE_BOOL    2

#define INITIAL_SUBSCRIBER_CAPACITY 8
#define MAX_EFFECT_STACK_DEPTH 64

// ============================================================================
// Signal Internal Structure
// ============================================================================

struct KryonSignal {
    uint32_t id;
    uint8_t type;
    bool dirty;
    pthread_mutex_t mutex;

    // Value storage
    union {
        float float_value;
        char* string_value;
        bool bool_value;
    } value;

    // Subscribers
    struct {
        void** callbacks;
        void** user_data;
        uint32_t* types;  // Callback type (0=float, 1=string, 2=bool)
        uint32_t count;
        uint32_t capacity;
    } subscribers;

    // Link tracking for computed values
    KryonComputed* computed_owner;
};

// ============================================================================
// Computed Value Internal Structure
// ============================================================================

struct KryonComputed {
    uint32_t id;
    bool is_string;
    bool dirty;

    // Computation
    union {
        KryonComputedFn float_fn;
        KryonComputedStringFn string_fn;
    } fn;

    // Dependencies
    KryonSignal** inputs;
    size_t input_count;

    // Cached result
    union {
        float float_value;
        char* string_value;
    } cached_value;

    void* user_data;

    // For cleanup
    bool owns_string_value;
};

// ============================================================================
// Effect Internal Structure
// ============================================================================

struct KryonEffect {
    uint32_t id;
    KryonEffectFn callback;
    void* user_data;
    bool paused;
    bool running;

    // Tracked dependencies
    KryonSignal** tracked_signals;
    size_t tracked_count;
    uint32_t* subscription_ids;  // IDs to unsubscribe on destroy
};

// ============================================================================
// Global State
// ============================================================================

typedef struct {
    KryonSignal** signals;
    uint32_t signal_count;
    uint32_t signal_capacity;

    KryonComputed** computeds;
    uint32_t computed_count;
    uint32_t computed_capacity;

    KryonEffect** effects;
    uint32_t effect_count;
    uint32_t effect_capacity;

    // Effect stack for automatic dependency tracking
    KryonEffect* effect_stack[MAX_EFFECT_STACK_DEPTH];
    uint32_t effect_stack_depth;

    // Batch mode
    uint32_t batch_depth;

    // Pending updates (during batch mode)
    struct {
        KryonSignal** signals;
        uint32_t count;
        uint32_t capacity;
    } pending;

    // Debug
    bool debug_mode;

    // Next ID
    uint32_t next_id;

    pthread_mutex_t global_mutex;
} KryonReactiveState;

static KryonReactiveState g_reactive = {0};

// ============================================================================
// Helper Macros
// ============================================================================

#define MUTEX_LOCK(mutex) pthread_mutex_lock(&(mutex))
#define MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&(mutex))
#define GLOBAL_LOCK MUTEX_LOCK(g_reactive.global_mutex)
#define GLOBAL_UNLOCK MUTEX_UNLOCK(g_reactive.global_mutex)

// ============================================================================
// Forward Declarations
// ============================================================================

static void signal_notify_subscribers(KryonSignal* signal);
static void computed_mark_dirty(KryonComputed* computed);
static void effect_track_signal(KryonEffect* effect, KryonSignal* signal);
static uint32_t signal_subscribe_internal(KryonSignal* signal, void* cb, void* user_data, uint32_t type);

// ============================================================================
// Static Callback Functions
// ============================================================================

/* Callback for computed value to mark itself dirty on input change */
static void computed_dirty_callback(float value, void* user_data) {
    (void)value;
    KryonComputed* c = (KryonComputed*)user_data;
    c->dirty = true;
    if (g_reactive.debug_mode) {
        printf("[Reactive] Computed %p marked dirty\n", (void*)c);
    }
}

/* Callback for effects to trigger themselves */
static void effect_trigger_callback(float value, void* user_data) {
    (void)value;
    KryonEffect* e = (KryonEffect*)user_data;
    if (!e->paused) {
        kryon_effect_trigger(e);
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

static void reactive_init_once(void) {
    if (g_reactive.signals != NULL) return;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_reactive.global_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    g_reactive.signal_capacity = 64;
    g_reactive.signals = calloc(g_reactive.signal_capacity, sizeof(KryonSignal*));

    g_reactive.computed_capacity = 32;
    g_reactive.computeds = calloc(g_reactive.computed_capacity, sizeof(KryonComputed*));

    g_reactive.effect_capacity = 32;
    g_reactive.effects = calloc(g_reactive.effect_capacity, sizeof(KryonEffect*));

    g_reactive.pending.capacity = 64;
    g_reactive.pending.signals = calloc(g_reactive.pending.capacity, sizeof(KryonSignal*));

    g_reactive.next_id = 1;
}

static void reactive_register_signal(KryonSignal* signal) {
    reactive_init_once();

    GLOBAL_LOCK;

    if (g_reactive.signal_count >= g_reactive.signal_capacity) {
        if (!ir_grow_capacity(&g_reactive.signal_capacity)) {
            GLOBAL_UNLOCK;
            return;  // Cannot grow further
        }
        KryonSignal** new_signals = realloc(g_reactive.signals,
                                             g_reactive.signal_capacity * sizeof(KryonSignal*));
        if (!new_signals) {
            GLOBAL_UNLOCK;
            return;  // Allocation failed
        }
        g_reactive.signals = new_signals;
    }

    signal->id = g_reactive.next_id++;
    g_reactive.signals[g_reactive.signal_count++] = signal;

    GLOBAL_UNLOCK;
}

static void reactive_unregister_signal(KryonSignal* signal) {
    GLOBAL_LOCK;

    for (uint32_t i = 0; i < g_reactive.signal_count; i++) {
        if (g_reactive.signals[i] == signal) {
            g_reactive.signals[i] = g_reactive.signals[--g_reactive.signal_count];
            break;
        }
    }

    GLOBAL_UNLOCK;
}

// ============================================================================
// Signal Implementation
// ============================================================================

static KryonSignal* signal_create_base(uint8_t type) {
    KryonSignal* signal = calloc(1, sizeof(KryonSignal));
    if (!signal) return NULL;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&signal->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    signal->type = type;
    signal->dirty = false;

    signal->subscribers.capacity = INITIAL_SUBSCRIBER_CAPACITY;
    signal->subscribers.callbacks = calloc(INITIAL_SUBSCRIBER_CAPACITY, sizeof(void*));
    signal->subscribers.user_data = calloc(INITIAL_SUBSCRIBER_CAPACITY, sizeof(void*));
    signal->subscribers.types = calloc(INITIAL_SUBSCRIBER_CAPACITY, sizeof(uint32_t));

    reactive_register_signal(signal);

    if (g_reactive.debug_mode) {
        printf("[Reactive] Created signal %p (type=%u)\n", (void*)signal, type);
    }

    return signal;
}

KryonSignal* kryon_signal_create(float initial_value) {
    KryonSignal* signal = signal_create_base(KRYON_SIGNAL_TYPE_FLOAT);
    if (!signal) return NULL;

    signal->value.float_value = initial_value;
    return signal;
}

KryonSignal* kryon_signal_create_string(const char* initial_value) {
    KryonSignal* signal = signal_create_base(KRYON_SIGNAL_TYPE_STRING);
    if (!signal) return NULL;

    signal->value.string_value = initial_value ? strdup(initial_value) : strdup("");
    return signal;
}

KryonSignal* kryon_signal_create_bool(bool initial_value) {
    KryonSignal* signal = signal_create_base(KRYON_SIGNAL_TYPE_BOOL);
    if (!signal) return NULL;

    signal->value.bool_value = initial_value;
    return signal;
}

float kryon_signal_get(KryonSignal* signal) {
    if (!signal) return 0.0f;

    MUTEX_LOCK(signal->mutex);

    // Track dependency if in effect
    KryonEffect* effect = kryon_reactive_get_current_effect();
    if (effect && !effect->running) {
        effect_track_signal(effect, signal);
    }

    float value = signal->value.float_value;

    MUTEX_UNLOCK(signal->mutex);

    return value;
}

const char* kryon_signal_get_string(KryonSignal* signal) {
    if (!signal || signal->type != KRYON_SIGNAL_TYPE_STRING) return NULL;

    MUTEX_LOCK(signal->mutex);

    // Track dependency if in effect
    KryonEffect* effect = kryon_reactive_get_current_effect();
    if (effect && !effect->running) {
        effect_track_signal(effect, signal);
    }

    const char* value = signal->value.string_value;

    MUTEX_UNLOCK(signal->mutex);

    return value;
}

bool kryon_signal_get_bool(KryonSignal* signal) {
    if (!signal || signal->type != KRYON_SIGNAL_TYPE_BOOL) return false;

    MUTEX_LOCK(signal->mutex);

    // Track dependency if in effect
    KryonEffect* effect = kryon_reactive_get_current_effect();
    if (effect && !effect->running) {
        effect_track_signal(effect, signal);
    }

    bool value = signal->value.bool_value;

    MUTEX_UNLOCK(signal->mutex);

    return value;
}

void kryon_signal_set(KryonSignal* signal, float value) {
    if (!signal || signal->type != KRYON_SIGNAL_TYPE_FLOAT) return;

    MUTEX_LOCK(signal->mutex);

    bool changed = (signal->value.float_value != value);
    if (changed) {
        signal->value.float_value = value;
        signal->dirty = true;

        if (g_reactive.debug_mode) {
            printf("[Reactive] Signal %p set to %f\n", (void*)signal, value);
        }

        MUTEX_UNLOCK(signal->mutex);
        signal_notify_subscribers(signal);
    } else {
        MUTEX_UNLOCK(signal->mutex);
    }
}

void kryon_signal_set_string(KryonSignal* signal, const char* value) {
    if (!signal || signal->type != KRYON_SIGNAL_TYPE_STRING) return;

    MUTEX_LOCK(signal->mutex);

    bool changed = (strcmp(signal->value.string_value, value ? value : "") != 0);
    if (changed) {
        free(signal->value.string_value);
        signal->value.string_value = strdup(value ? value : "");
        signal->dirty = true;

        if (g_reactive.debug_mode) {
            printf("[Reactive] Signal %p set to \"%s\"\n", (void*)signal, signal->value.string_value);
        }

        MUTEX_UNLOCK(signal->mutex);
        signal_notify_subscribers(signal);
    } else {
        MUTEX_UNLOCK(signal->mutex);
    }
}

void kryon_signal_set_bool(KryonSignal* signal, bool value) {
    if (!signal || signal->type != KRYON_SIGNAL_TYPE_BOOL) return;

    MUTEX_LOCK(signal->mutex);

    bool changed = (signal->value.bool_value != value);
    if (changed) {
        signal->value.bool_value = value;
        signal->dirty = true;

        if (g_reactive.debug_mode) {
            printf("[Reactive] Signal %p set to %s\n", (void*)signal, value ? "true" : "false");
        }

        MUTEX_UNLOCK(signal->mutex);
        signal_notify_subscribers(signal);
    } else {
        MUTEX_UNLOCK(signal->mutex);
    }
}

bool kryon_signal_is_string(KryonSignal* signal) {
    return signal && signal->type == KRYON_SIGNAL_TYPE_STRING;
}

bool kryon_signal_is_bool(KryonSignal* signal) {
    return signal && signal->type == KRYON_SIGNAL_TYPE_BOOL;
}

// ============================================================================
// Signal Subscriptions
// ============================================================================

static uint32_t signal_subscribe_internal(KryonSignal* signal, void* cb,
                                           void* user_data, uint32_t type) {
    if (!signal || !cb) return 0;

    MUTEX_LOCK(signal->mutex);

    // Expand capacity if needed
    if (signal->subscribers.count >= signal->subscribers.capacity) {
        if (!ir_grow_capacity(&signal->subscribers.capacity)) {
            MUTEX_UNLOCK(signal->mutex);
            return 0;  // Cannot grow further
        }
        void** new_callbacks = realloc(signal->subscribers.callbacks,
                                        signal->subscribers.capacity * sizeof(void*));
        void** new_user_data = realloc(signal->subscribers.user_data,
                                        signal->subscribers.capacity * sizeof(void*));
        uint32_t* new_types = realloc(signal->subscribers.types,
                                       signal->subscribers.capacity * sizeof(uint32_t));
        if (!new_callbacks || !new_user_data || !new_types) {
            // Rollback or keep old allocations on partial failure
            free(new_callbacks);
            free(new_user_data);
            free(new_types);
            MUTEX_UNLOCK(signal->mutex);
            return 0;  // Allocation failed
        }
        signal->subscribers.callbacks = new_callbacks;
        signal->subscribers.user_data = new_user_data;
        signal->subscribers.types = new_types;
    }

    uint32_t index = signal->subscribers.count++;
    signal->subscribers.callbacks[index] = cb;
    signal->subscribers.user_data[index] = user_data;
    signal->subscribers.types[index] = type;

    // Generate subscription ID (signal_id << 16 | index)
    uint32_t sub_id = (signal->id << 16) | index;

    MUTEX_UNLOCK(signal->mutex);

    if (g_reactive.debug_mode) {
        printf("[Reactive] Subscribed to signal %p, sub_id=%u\n", (void*)signal, sub_id);
    }

    return sub_id;
}

uint32_t kryon_signal_subscribe(KryonSignal* signal, KryonSignalCallback cb, void* user_data) {
    return signal_subscribe_internal(signal, (void*)cb, user_data, 0);
}

uint32_t kryon_signal_subscribe_string(KryonSignal* signal, KryonStringCallback cb, void* user_data) {
    return signal_subscribe_internal(signal, (void*)cb, user_data, 1);
}

uint32_t kryon_signal_subscribe_bool(KryonSignal* signal, KryonBoolCallback cb, void* user_data) {
    return signal_subscribe_internal(signal, (void*)cb, user_data, 2);
}

void kryon_signal_unsubscribe(KryonSignal* signal, uint32_t sub_id) {
    if (!signal || sub_id == 0) return;

    uint32_t index = sub_id & 0xFFFF;

    MUTEX_LOCK(signal->mutex);

    if (index < signal->subscribers.count) {
        // Swap with last to fill gap
        uint32_t last = signal->subscribers.count - 1;
        if (index != last) {
            signal->subscribers.callbacks[index] = signal->subscribers.callbacks[last];
            signal->subscribers.user_data[index] = signal->subscribers.user_data[last];
            signal->subscribers.types[index] = signal->subscribers.types[last];
        }
        signal->subscribers.count--;
    }

    MUTEX_UNLOCK(signal->mutex);
}

static void signal_notify_subscribers(KryonSignal* signal) {
    // Add to pending updates if in batch mode
    if (g_reactive.batch_depth > 0) {
        GLOBAL_LOCK;

        if (g_reactive.pending.count >= g_reactive.pending.capacity) {
            if (!ir_grow_capacity(&g_reactive.pending.capacity)) {
                GLOBAL_UNLOCK;
                return;  // Cannot grow further
            }
            KryonSignal** new_signals = realloc(g_reactive.pending.signals,
                                                 g_reactive.pending.capacity * sizeof(KryonSignal*));
            if (!new_signals) {
                GLOBAL_UNLOCK;
                return;  // Allocation failed
            }
            g_reactive.pending.signals = new_signals;
        }

        // Check if already pending
        for (uint32_t i = 0; i < g_reactive.pending.count; i++) {
            if (g_reactive.pending.signals[i] == signal) {
                GLOBAL_UNLOCK;
                return;
            }
        }

        g_reactive.pending.signals[g_reactive.pending.count++] = signal;

        GLOBAL_UNLOCK;
        return;
    }

    // Direct notification
    MUTEX_LOCK(signal->mutex);

    for (uint32_t i = 0; i < signal->subscribers.count; i++) {
        void* cb = signal->subscribers.callbacks[i];
        void* user_data = signal->subscribers.user_data[i];
        uint32_t type = signal->subscribers.types[i];

        MUTEX_UNLOCK(signal->mutex);

        switch (type) {
            case 0:  // Float callback
                ((KryonSignalCallback)cb)(signal->value.float_value, user_data);
                break;
            case 1:  // String callback
                ((KryonStringCallback)cb)(signal->value.string_value, user_data);
                break;
            case 2:  // Bool callback
                ((KryonBoolCallback)cb)(signal->value.bool_value, user_data);
                break;
        }

        MUTEX_LOCK(signal->mutex);
    }

    MUTEX_UNLOCK(signal->mutex);
}

void kryon_signal_destroy(KryonSignal* signal) {
    if (!signal) return;

    MUTEX_LOCK(signal->mutex);

    // Free string value
    if (signal->type == KRYON_SIGNAL_TYPE_STRING) {
        free(signal->value.string_value);
    }

    MUTEX_UNLOCK(signal->mutex);

    // Free subscribers
    free(signal->subscribers.callbacks);
    free(signal->subscribers.user_data);
    free(signal->subscribers.types);

    pthread_mutex_destroy(&signal->mutex);

    reactive_unregister_signal(signal);

    if (g_reactive.debug_mode) {
        printf("[Reactive] Destroyed signal %p\n", (void*)signal);
    }

    free(signal);
}

// ============================================================================
// Computed Value Implementation
// ============================================================================

KryonComputed* kryon_computed_create(KryonComputedFn fn,
                                      KryonSignal** signals,
                                      size_t signal_count,
                                      void* user_data) {
    if (!fn || signal_count == 0) return NULL;

    KryonComputed* computed = calloc(1, sizeof(KryonComputed));
    if (!computed) return NULL;

    computed->id = g_reactive.next_id++;
    computed->is_string = false;
    computed->dirty = true;
    computed->fn.float_fn = fn;
    computed->user_data = user_data;
    computed->input_count = signal_count;

    // Copy input signals
    computed->inputs = calloc(signal_count, sizeof(KryonSignal*));
    for (size_t i = 0; i < signal_count; i++) {
        computed->inputs[i] = signals[i];
        // Subscribe to changes
        uint32_t sub_id = kryon_signal_subscribe(signals[i],
            computed_dirty_callback, computed);

        // Store subscription ID for cleanup
        (void)sub_id;  // TODO: track for cleanup
    }

    // Register globally
    GLOBAL_LOCK;
    if (g_reactive.computed_count >= g_reactive.computed_capacity) {
        if (!ir_grow_capacity(&g_reactive.computed_capacity)) {
            GLOBAL_UNLOCK;
            kryon_computed_destroy(computed);
            return NULL;  // Cannot grow further
        }
        KryonComputed** new_computeds = realloc(g_reactive.computeds,
                                                g_reactive.computed_capacity * sizeof(KryonComputed*));
        if (!new_computeds) {
            GLOBAL_UNLOCK;
            kryon_computed_destroy(computed);
            return NULL;  // Allocation failed
        }
        g_reactive.computeds = new_computeds;
    }
    g_reactive.computeds[g_reactive.computed_count++] = computed;
    GLOBAL_UNLOCK;

    // Mark signals as having a computed owner
    for (size_t i = 0; i < signal_count; i++) {
        MUTEX_LOCK(signals[i]->mutex);
        signals[i]->computed_owner = computed;
        MUTEX_UNLOCK(signals[i]->mutex);
    }

    if (g_reactive.debug_mode) {
        printf("[Reactive] Created computed %p with %zu inputs\n", (void*)computed, signal_count);
    }

    return computed;
}

KryonComputed* kryon_computed_create_string(KryonComputedStringFn fn,
                                              KryonSignal** signals,
                                              size_t signal_count,
                                              void* user_data) {
    KryonComputed* computed = kryon_computed_create(
        (KryonComputedFn)fn, signals, signal_count, user_data);
    if (computed) {
        computed->is_string = true;
    }
    return computed;
}

float kryon_computed_get(KryonComputed* computed) {
    if (!computed || computed->is_string) return 0.0f;

    if (computed->dirty) {
        // Gather input values
        float* inputs = calloc(computed->input_count, sizeof(float));
        for (size_t i = 0; i < computed->input_count; i++) {
            inputs[i] = kryon_signal_get(computed->inputs[i]);
        }

        // Recompute
        computed->cached_value.float_value = computed->fn.float_fn(inputs, computed->input_count, computed->user_data);
        computed->dirty = false;

        free(inputs);

        if (g_reactive.debug_mode) {
            printf("[Reactive] Computed %p recomputed to %f\n", (void*)computed, computed->cached_value.float_value);
        }
    }

    return computed->cached_value.float_value;
}

const char* kryon_computed_get_string(KryonComputed* computed) {
    if (!computed || !computed->is_string) return NULL;

    if (computed->dirty) {
        // Gather input values
        const char** inputs = calloc(computed->input_count, sizeof(const char*));
        for (size_t i = 0; i < computed->input_count; i++) {
            inputs[i] = kryon_signal_get_string(computed->inputs[i]);
        }

        // Recompute
        const char* result = computed->fn.string_fn(inputs, computed->input_count, computed->user_data);

        // Free old value if we own it
        if (computed->owns_string_value) {
            free(computed->cached_value.string_value);
        }

        computed->cached_value.string_value = strdup(result ? result : "");
        computed->owns_string_value = true;
        computed->dirty = false;

        free(inputs);

        if (g_reactive.debug_mode) {
            printf("[Reactive] Computed %p recomputed to \"%s\"\n", (void*)computed, computed->cached_value.string_value);
        }
    }

    return computed->cached_value.string_value;
}

bool kryon_computed_is_string(KryonComputed* computed) {
    return computed && computed->is_string;
}

void kryon_computed_mark_dirty(KryonComputed* computed) {
    if (computed) {
        computed->dirty = true;
    }
}

void kryon_computed_destroy(KryonComputed* computed) {
    if (!computed) return;

    // Unsubscribe from inputs
    for (size_t i = 0; i < computed->input_count; i++) {
        // TODO: properly unsubscribe using stored IDs
    }

    free(computed->inputs);

    if (computed->is_string && computed->owns_string_value) {
        free(computed->cached_value.string_value);
    }

    // Unregister globally
    GLOBAL_LOCK;
    for (uint32_t i = 0; i < g_reactive.computed_count; i++) {
        if (g_reactive.computeds[i] == computed) {
            g_reactive.computeds[i] = g_reactive.computeds[--g_reactive.computed_count];
            break;
        }
    }
    GLOBAL_UNLOCK;

    if (g_reactive.debug_mode) {
        printf("[Reactive] Destroyed computed %p\n", (void*)computed);
    }

    free(computed);
}

// ============================================================================
// Effect Implementation
// ============================================================================

static void effect_track_signal(KryonEffect* effect, KryonSignal* signal) {
    if (!effect || !signal) return;

    // Check if already tracking
    for (size_t i = 0; i < effect->tracked_count; i++) {
        if (effect->tracked_signals[i] == signal) {
            return;
        }
    }

    // Add to tracked signals
    effect->tracked_signals = realloc(effect->tracked_signals,
                                      (effect->tracked_count + 1) * sizeof(KryonSignal*));
    effect->tracked_signals[effect->tracked_count] = signal;
    effect->tracked_count++;

    // Subscribe to signal changes
    uint32_t sub_id = kryon_signal_subscribe(signal,
        effect_trigger_callback, effect);

    // Store subscription ID
    effect->subscription_ids = realloc(effect->subscription_ids,
                                        effect->tracked_count * sizeof(uint32_t));
    effect->subscription_ids[effect->tracked_count - 1] = sub_id;
}

KryonEffect* kryon_effect_create(KryonEffectFn fn, void* user_data) {
    if (!fn) return NULL;

    KryonEffect* effect = calloc(1, sizeof(KryonEffect));
    if (!effect) return NULL;

    effect->id = g_reactive.next_id++;
    effect->callback = fn;
    effect->user_data = user_data;
    effect->paused = false;

    // Register globally
    GLOBAL_LOCK;
    if (g_reactive.effect_count >= g_reactive.effect_capacity) {
        if (!ir_grow_capacity(&g_reactive.effect_capacity)) {
            GLOBAL_UNLOCK;
            kryon_effect_destroy(effect);
            return NULL;  // Cannot grow further
        }
        KryonEffect** new_effects = realloc(g_reactive.effects,
                                            g_reactive.effect_capacity * sizeof(KryonEffect*));
        if (!new_effects) {
            GLOBAL_UNLOCK;
            kryon_effect_destroy(effect);
            return NULL;  // Allocation failed
        }
        g_reactive.effects = new_effects;
    }
    g_reactive.effects[g_reactive.effect_count++] = effect;
    GLOBAL_UNLOCK;

    // Run immediately
    kryon_effect_trigger(effect);

    if (g_reactive.debug_mode) {
        printf("[Reactive] Created effect %p\n", (void*)effect);
    }

    return effect;
}

KryonEffect* kryon_effect_create_tracked(KryonEffectFn fn,
                                          KryonSignal** signals,
                                          size_t signal_count,
                                          void* user_data) {
    KryonEffect* effect = kryon_effect_create(fn, user_data);
    if (!effect) return NULL;

    // Manually track signals
    for (size_t i = 0; i < signal_count; i++) {
        effect_track_signal(effect, signals[i]);
    }

    return effect;
}

void kryon_effect_trigger(KryonEffect* effect) {
    if (!effect || effect->paused || effect->running) return;

    effect->running = true;

    // Push onto effect stack
    kryon_reactive_set_current_effect(effect);

    if (g_reactive.debug_mode) {
        printf("[Reactive] Triggering effect %p\n", (void*)effect);
    }

    // Execute callback
    effect->callback(effect->user_data);

    // Pop from effect stack
    kryon_reactive_set_current_effect(NULL);

    effect->running = false;
}

void kryon_effect_pause(KryonEffect* effect) {
    if (effect) {
        effect->paused = true;
    }
}

void kryon_effect_resume(KryonEffect* effect) {
    if (effect) {
        effect->paused = false;
    }
}

void kryon_effect_destroy(KryonEffect* effect) {
    if (!effect) return;

    // Unsubscribe from tracked signals
    for (size_t i = 0; i < effect->tracked_count; i++) {
        kryon_signal_unsubscribe(effect->tracked_signals[i], effect->subscription_ids[i]);
    }

    free(effect->tracked_signals);
    free(effect->subscription_ids);

    // Unregister globally
    GLOBAL_LOCK;
    for (uint32_t i = 0; i < g_reactive.effect_count; i++) {
        if (g_reactive.effects[i] == effect) {
            g_reactive.effects[i] = g_reactive.effects[--g_reactive.effect_count];
            break;
        }
    }
    GLOBAL_UNLOCK;

    if (g_reactive.debug_mode) {
        printf("[Reactive] Destroyed effect %p\n", (void*)effect);
    }

    free(effect);
}

// ============================================================================
// Batch Updates
// ============================================================================

void kryon_batch_begin(void) {
    g_reactive.batch_depth++;

    if (g_reactive.debug_mode) {
        printf("[Reactive] Batch begin (depth=%u)\n", g_reactive.batch_depth);
    }
}

void kryon_batch_end(void) {
    if (g_reactive.batch_depth == 0) return;

    g_reactive.batch_depth--;

    if (g_reactive.debug_mode) {
        printf("[Reactive] Batch end (depth=%u, pending=%u)\n",
               g_reactive.batch_depth, g_reactive.pending.count);
    }

    // Flush pending updates if we're exiting outermost batch
    if (g_reactive.batch_depth == 0 && g_reactive.pending.count > 0) {
        uint32_t count = g_reactive.pending.count;

        // Copy pending list and clear
        KryonSignal** pending = calloc(count, sizeof(KryonSignal*));
        memcpy(pending, g_reactive.pending.signals, count * sizeof(KryonSignal*));
        g_reactive.pending.count = 0;

        GLOBAL_UNLOCK;

        // Notify all pending signals
        for (uint32_t i = 0; i < count; i++) {
            signal_notify_subscribers(pending[i]);
        }

        free(pending);
    }
}

bool kryon_batch_active(void) {
    return g_reactive.batch_depth > 0;
}

// ============================================================================
// Reactive Context
// ============================================================================

KryonEffect* kryon_reactive_get_current_effect(void) {
    if (g_reactive.effect_stack_depth > 0) {
        return g_reactive.effect_stack[g_reactive.effect_stack_depth - 1];
    }
    return NULL;
}

void kryon_reactive_set_current_effect(KryonEffect* effect) {
    if (effect) {
        if (g_reactive.effect_stack_depth < MAX_EFFECT_STACK_DEPTH) {
            g_reactive.effect_stack[g_reactive.effect_stack_depth++] = effect;
        }
    } else if (g_reactive.effect_stack_depth > 0) {
        g_reactive.effect_stack_depth--;
    }
}

// ============================================================================
// String Helpers
// ============================================================================

typedef struct {
    char* buffer;
    size_t capacity;
    KryonSignal** args;
    size_t arg_count;
} FormatContext;

static const char* format_computed_fn(const char* const* inputs, size_t count, void* user_data) {
    FormatContext* ctx = (FormatContext*)user_data;

    // Estimate size (rough approximation)
    size_t size = 256;
    for (size_t i = 0; i < count; i++) {
        size += strlen(inputs[i]) * 2;
    }

    // Reallocate buffer if needed
    if (ctx->capacity < size) {
        ctx->capacity = size;
        ctx->buffer = realloc(ctx->buffer, size);
    }

    // Format using args as signals for value retrieval
    // Actually need to access the signals to get current values
    va_list args;
    va_start(args, count);

    // This is a simplified version - proper implementation would
    // need to build argument list from signal values
    snprintf(ctx->buffer, ctx->capacity, "%s", inputs[0]);

    va_end(args);

    return ctx->buffer;
}

KryonComputed* kryon_signal_format(const char* format,
                                    KryonSignal** args,
                                    size_t arg_count) {
    // TODO: Implement proper formatting
    (void)format;
    (void)args;
    (void)arg_count;
    return NULL;
}

KryonComputed* kryon_signal_concat(KryonSignal** strings,
                                    size_t count,
                                    const char* separator) {
    // TODO: Implement concatenation
    (void)strings;
    (void)count;
    (void)separator;
    return NULL;
}

// ============================================================================
// Debugging & Statistics
// ============================================================================

void kryon_reactive_set_debug(bool enabled) {
    g_reactive.debug_mode = enabled;
}

void kryon_reactive_get_stats(KryonReactiveStats* out_stats) {
    if (!out_stats) return;

    GLOBAL_LOCK;

    out_stats->signal_count = g_reactive.signal_count;
    out_stats->computed_count = g_reactive.computed_count;
    out_stats->effect_count = g_reactive.effect_count;

    uint32_t total_subs = 0;
    for (uint32_t i = 0; i < g_reactive.signal_count; i++) {
        total_subs += g_reactive.signals[i]->subscribers.count;
    }
    out_stats->total_subscriptions = total_subs;

    out_stats->batch_depth = g_reactive.batch_depth;

    GLOBAL_UNLOCK;
}

void kryon_reactive_cleanup_all(void) {
    // Destroy all effects
    while (g_reactive.effect_count > 0) {
        kryon_effect_destroy(g_reactive.effects[0]);
    }
    free(g_reactive.effects);

    // Destroy all computeds
    while (g_reactive.computed_count > 0) {
        kryon_computed_destroy(g_reactive.computeds[0]);
    }
    free(g_reactive.computeds);

    // Destroy all signals
    while (g_reactive.signal_count > 0) {
        kryon_signal_destroy(g_reactive.signals[0]);
    }
    free(g_reactive.signals);

    // Free pending
    free(g_reactive.pending.signals);

    pthread_mutex_destroy(&g_reactive.global_mutex);

    memset(&g_reactive, 0, sizeof(g_reactive));

    if (g_reactive.debug_mode) {
        printf("[Reactive] Cleaned up all reactive state\n");
    }
}
