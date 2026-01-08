/**
 * Kryon Reactive State System for C
 *
 * A Vue 3 / SolidJS-inspired reactive state management system for C.
 * Provides signals, computed values, and effects with automatic dependency tracking.
 *
 * Features:
 * - Signals: Reactive primitive values
 * - Computed: Derived values that auto-recalculate
 * - Effects: Side effects that run when tracked signals change
 * - Batch updates: Queue multiple updates and flush once
 *
 * Usage:
 *   KryonSignal* count = kryon_signal_create(0);
 *
 *   KryonSignal* doubled = kryon_signal_derive(count, [](float v) { return v * 2; });
 *
 *   KryonEffect* effect = kryon_effect_create((KryonEffectFn)[] {
 *       printf("Count: %f, Doubled: %f\n",
 *              kryon_signal_get(count),
 *              kryon_signal_get(doubled));
 *   }, 2, (KryonSignal*[]){count, doubled});
 *
 *   kryon_signal_set(count, 5);  // Prints: Count: 5.0, Doubled: 10.0
 */

#ifndef KRYON_REACTIVE_H
#define KRYON_REACTIVE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct KryonSignal KryonSignal;
typedef struct KryonComputed KryonComputed;
typedef struct KryonEffect KryonEffect;

// ============================================================================
// Signal - Reactive Primitive Values
// ============================================================================

/**
 * Create a reactive signal with an initial value
 * @param initial_value The starting value
 * @return New signal, or NULL on allocation failure
 */
KryonSignal* kryon_signal_create(float initial_value);

/**
 * Create a reactive signal for string values
 * @param initial_value The starting string (copied)
 * @return New signal, or NULL on allocation failure
 */
KryonSignal* kryon_signal_create_string(const char* initial_value);

/**
 * Create a reactive signal for boolean values
 * @param initial_value The starting boolean
 * @return New signal, or NULL on allocation failure
 */
KryonSignal* kryon_signal_create_bool(bool initial_value);

/**
 * Get the current value of a signal
 * @param signal The signal to read
 * @return Current value (as float; use type-specific getters for other types)
 */
float kryon_signal_get(KryonSignal* signal);

/**
 * Get the string value of a signal
 * @param signal The signal (must be created with kryon_signal_create_string)
 * @return Current string value, or NULL if not a string signal
 */
const char* kryon_signal_get_string(KryonSignal* signal);

/**
 * Get the boolean value of a signal
 * @param signal The signal (must be created with kryon_signal_create_bool)
 * @return Current boolean value
 */
bool kryon_signal_get_bool(KryonSignal* signal);

/**
 * Set a new value for the signal
 * If the value changed, all subscribers are notified
 * @param signal The signal to update
 * @param value New value
 */
void kryon_signal_set(KryonSignal* signal, float value);

/**
 * Set a new string value for the signal
 * @param signal The signal (must be created with kryon_signal_create_string)
 * @param value New string value (copied)
 */
void kryon_signal_set_string(KryonSignal* signal, const char* value);

/**
 * Set a new boolean value for the signal
 * @param signal The signal (must be created with kryon_signal_create_bool)
 * @param value New boolean value
 */
void kryon_signal_set_bool(KryonSignal* signal, bool value);

/**
 * Check if signal is of a specific type
 */
bool kryon_signal_is_string(KryonSignal* signal);
bool kryon_signal_is_bool(KryonSignal* signal);

// ============================================================================
// Signal Subscriptions
// ============================================================================

/**
 * Callback function type for float signal changes
 * @param value The new value
 * @param user_data User-provided context
 */
typedef void (*KryonSignalCallback)(float value, void* user_data);

/**
 * Callback function type for string signal changes
 */
typedef void (*KryonStringCallback)(const char* value, void* user_data);

/**
 * Callback function type for bool signal changes
 */
typedef void (*KryonBoolCallback)(bool value, void* user_data);

/**
 * Subscribe to signal value changes
 * @param signal The signal to observe
 * @param cb Callback function (must match signal type)
 * @param user_data User context passed to callback
 * @return Subscription ID for unsubscribing, or 0 on failure
 */
uint32_t kryon_signal_subscribe(KryonSignal* signal, KryonSignalCallback cb, void* user_data);
uint32_t kryon_signal_subscribe_string(KryonSignal* signal, KryonStringCallback cb, void* user_data);
uint32_t kryon_signal_subscribe_bool(KryonSignal* signal, KryonBoolCallback cb, void* user_data);

/**
 * Unsubscribe from a signal
 * @param signal The signal
 * @param sub_id Subscription ID returned from subscribe
 */
void kryon_signal_unsubscribe(KryonSignal* signal, uint32_t sub_id);

/**
 * Destroy a signal and free all resources
 * @param signal The signal to destroy
 */
void kryon_signal_destroy(KryonSignal* signal);

// ============================================================================
// Computed Values - Derived State
// ============================================================================

/**
 * Function type for computed value calculation
 * @param inputs Array of input signal values
 * @param input_count Number of inputs
 * @param user_data User-provided context
 * @return Computed value
 */
typedef float (*KryonComputedFn)(const float* inputs, size_t input_count, void* user_data);

/**
 * Function type for string computed value calculation
 */
typedef const char* (*KryonComputedStringFn)(const char* const* inputs, size_t input_count, void* user_data);

/**
 * Create a computed value that derives from input signals
 * @param fn Function to compute the value
 * @param signals Array of input signals
 * @param signal_count Number of input signals
 * @param user_data User context
 * @return New computed value, or NULL on failure
 */
KryonComputed* kryon_computed_create(KryonComputedFn fn,
                                      KryonSignal** signals,
                                      size_t signal_count,
                                      void* user_data);

/**
 * Create a computed value for strings
 */
KryonComputed* kryon_computed_create_string(KryonComputedStringFn fn,
                                              KryonSignal** signals,
                                              size_t signal_count,
                                              void* user_data);

/**
 * Get the current computed value
 * @param computed The computed value
 * @return Current value
 */
float kryon_computed_get(KryonComputed* computed);

/**
 * Get the current computed string value
 */
const char* kryon_computed_get_string(KryonComputed* computed);

/**
 * Check if computed is a string computed
 */
bool kryon_computed_is_string(KryonComputed* computed);

/**
 * Manually mark computed as dirty (force recalculation on next get)
 * Usually not needed - inputs automatically trigger this
 */
void kryon_computed_mark_dirty(KryonComputed* computed);

/**
 * Destroy a computed value
 */
void kryon_computed_destroy(KryonComputed* computed);

// ============================================================================
// Effects - Side Effects on State Changes
// ============================================================================

/**
 * Function type for effect callbacks
 * @param user_data User-provided context
 */
typedef void (*KryonEffectFn)(void* user_data);

/**
 * Create an effect that runs when tracked signals change
 * The effect runs immediately on creation, then again when any tracked signal changes
 *
 * @param fn The effect function
 * @param user_data User context
 * @return New effect, or NULL on failure
 */
KryonEffect* kryon_effect_create(KryonEffectFn fn, void* user_data);

/**
 * Create an effect with explicit signal dependencies
 * The effect runs when any of the specified signals change
 *
 * @param fn The effect function
 * @param signals Array of signals to track
 * @param signal_count Number of signals
 * @param user_data User context
 * @return New effect, or NULL on failure
 */
KryonEffect* kryon_effect_create_tracked(KryonEffectFn fn,
                                          KryonSignal** signals,
                                          size_t signal_count,
                                          void* user_data);

/**
 * Manually trigger the effect
 * Usually not needed - signals automatically trigger this
 */
void kryon_effect_trigger(KryonEffect* effect);

/**
 * Disable the effect (won't run on signal changes)
 */
void kryon_effect_pause(KryonEffect* effect);

/**
 * Enable the effect
 */
void kryon_effect_resume(KryonEffect* effect);

/**
 * Destroy an effect
 */
void kryon_effect_destroy(KryonEffect* effect);

// ============================================================================
// Batch Updates
// ============================================================================

/**
 * Begin batch mode
 * Signal changes are queued and not propagated until batch_end()
 * Nested batches are supported
 */
void kryon_batch_begin(void);

/**
 * End batch mode and flush all queued updates
 * Subscribers are notified once per signal, even if changed multiple times
 */
void kryon_batch_end(void);

/**
 * Check if currently in batch mode
 */
bool kryon_batch_active(void);

// ============================================================================
// Reactive Context (Internal)
// ============================================================================

/**
 * Get the currently executing effect (for automatic dependency tracking)
 * This is used internally by signals during effect execution
 */
KryonEffect* kryon_reactive_get_current_effect(void);

/**
 * Set the current effect (used by effect execution)
 */
void kryon_reactive_set_current_effect(KryonEffect* effect);

// ============================================================================
// String Signal Helpers
// ============================================================================

/**
 * Helper to create a formatted string signal
 * Like sprintf but reactive - updates when format arguments change
 *
 * @param format Format string
 * @param args Signals to use as arguments
 * @param arg_count Number of arguments
 * @return New computed signal, or NULL on failure
 */
KryonComputed* kryon_signal_format(const char* format,
                                    KryonSignal** args,
                                    size_t arg_count);

/**
 * Helper to concatenate string signals
 */
KryonComputed* kryon_signal_concat(KryonSignal** strings,
                                    size_t count,
                                    const char* separator);

// ============================================================================
// Debugging
// ============================================================================

/**
 * Enable/disable debug mode
 * When enabled, tracks dependency changes and effect triggers
 */
void kryon_reactive_set_debug(bool enabled);

/**
 * Get statistics about reactive state
 */
typedef struct {
    uint32_t signal_count;
    uint32_t computed_count;
    uint32_t effect_count;
    uint32_t total_subscriptions;
    uint32_t batch_depth;
} KryonReactiveStats;

void kryon_reactive_get_stats(KryonReactiveStats* out_stats);

/**
 * Cleanup all reactive state (useful for shutdown)
 */
void kryon_reactive_cleanup_all(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_REACTIVE_H
