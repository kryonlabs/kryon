/**
 * Kryon Reactive UI Binding
 *
 * Connects reactive state to UI components automatically.
 * When a signal changes, bound UI components update automatically.
 *
 * Usage:
 *   KryonSignal* count = kryon_signal_create(0);
 *   IRComponent* text = kryon_text("Count: 0");
 *
 *   KryonTextBinding* binding = kryon_bind_text(text, count);
 *
 *   // Later:
 *   kryon_signal_set(count, 42);  // Text automatically updates to "Count: 42"
 *
 *   // Cleanup:
 *   kryon_unbind_text(binding);
 */

#ifndef KRYON_REACTIVE_UI_H
#define KRYON_REACTIVE_UI_H

#include "kryon_reactive.h"
#include "../../ir/include/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Text Binding
// ============================================================================

/**
 * Binding between a text component and a signal
 */
typedef struct KryonTextBinding KryonTextBinding;

/**
 * Bind a signal to a text component's content
 * When the signal changes, the text component updates automatically.
 *
 * @param component The text component
 * @param signal The signal to bind (float or string)
 * @param format Optional format string for float signals (e.g., "%.2f")
 *                If NULL, defaults to "%g" for float, direct for string
 * @return Binding handle, or NULL on failure
 */
KryonTextBinding* kryon_bind_text(IRComponent* component,
                                   KryonSignal* signal,
                                   const char* format);

/**
 * Bind a string signal with a prefix/suffix template
 * Example: signal="value", prefix="Count: ", suffix="" -> "Count: value"
 *
 * @param component The text component
 * @param signal The string signal to bind
 * @param prefix Text before the signal value (can be NULL)
 * @param suffix Text after the signal value (can be NULL)
 * @return Binding handle, or NULL on failure
 */
KryonTextBinding* kryon_bind_text_template(IRComponent* component,
                                            KryonSignal* signal,
                                            const char* prefix,
                                            const char* suffix);

/**
 * Unbind a text binding
 * @param binding The binding to unbind
 */
void kryon_unbind_text(KryonTextBinding* binding);

// ============================================================================
// Visibility Binding
// ============================================================================

/**
 * Binding between component visibility and a boolean signal
 */
typedef struct KryonVisibilityBinding KryonVisibilityBinding;

/**
 * Bind a boolean signal to component visibility
 * When true, component is visible; when false, hidden.
 *
 * @param component The component
 * @param signal Boolean signal
 * @return Binding handle, or NULL on failure
 */
KryonVisibilityBinding* kryon_bind_visible(IRComponent* component,
                                            KryonSignal* signal);

/**
 * Unbind a visibility binding
 */
void kryon_unbind_visible(KryonVisibilityBinding* binding);

// ============================================================================
// Property Binding (Generic)
// ============================================================================

/**
 * Function type for setting a component property from a signal value
 */
typedef void (*KryonPropertySetter)(IRComponent*, float value);

/**
 * Bind a signal to any component property
 * Useful for custom bindings like width, height, opacity, etc.
 *
 * @param component The component
 * @param signal Float signal
 * @param setter Function to apply the value to the component
 * @return Binding handle, or NULL on failure
 */
KryonTextBinding* kryon_bind_property(IRComponent* component,
                                       KryonSignal* signal,
                                       KryonPropertySetter setter);

// ============================================================================
// Color Binding
// ============================================================================

/**
 * Bind a signal to background color
 * Signal value is interpreted as 0xRRGGBB or 0xRRGGBBAA
 */
KryonVisibilityBinding* kryon_bind_background(IRComponent* component,
                                               KryonSignal* signal);

/**
 * Bind a signal to text color
 */
KryonVisibilityBinding* kryon_bind_color(IRComponent* component,
                                          KryonSignal* signal);

// ============================================================================
// Enabled/Disabled Binding
// ============================================================================

/**
 * Bind boolean signal to component enabled/disabled state
 * Uses opacity (0.5 when disabled, 1.0 when enabled)
 */
KryonVisibilityBinding* kryon_bind_enabled(IRComponent* component,
                                            KryonSignal* signal);

// ============================================================================
// Two-Way Binding (Input Components)
// ============================================================================

/**
 * Set up two-way binding between an input and a signal
 * - Signal changes update the input value
 * - Input changes update the signal
 *
 * @param component The input component
 * @param signal String signal to bind
 * @return Binding handle, or NULL on failure
 */
KryonTextBinding* kryon_bind_input_two_way(IRComponent* component,
                                             KryonSignal* signal);

// ============================================================================
// List/Array Binding
// ============================================================================

/**
 * Callback type for rendering list items
 * @param parent Container component for the item
 * @param index Item index
 * @param user_data User context
 */
typedef void (*KryonListItemRenderer)(IRComponent* parent, size_t index, void* user_data);

/**
 * Bind an array signal count to a list container
 * Automatically rebuilds the list when count changes.
 *
 * Note: This is a simplified version - full array reactivity would need
 * a dedicated array signal type with add/remove operations.
 *
 * @param container The container component (Column, Row, etc.)
 * @param count_signal Signal containing the item count
 * @param renderer Function to render each item
 * @param user_data Context passed to renderer
 * @return Binding handle, or NULL on failure
 */
KryonTextBinding* kryon_bind_list(IRComponent* container,
                                   KryonSignal* count_signal,
                                   KryonListItemRenderer renderer,
                                   void* user_data);

// ============================================================================
// Binding Groups (for batch cleanup)
// ============================================================================

/**
 * A group of bindings that can be cleaned up together
 */
typedef struct KryonBindingGroup KryonBindingGroup;

/**
 * Create a new binding group
 */
KryonBindingGroup* kryon_binding_group_create(void);

/**
 * Add a text binding to a group
 */
void kryon_binding_group_add_text(KryonBindingGroup* group,
                                    KryonTextBinding* binding);

/**
 * Add a visibility binding to a group
 */
void kryon_binding_group_add_visible(KryonBindingGroup* group,
                                      KryonVisibilityBinding* binding);

/**
 * Destroy all bindings in a group
 */
void kryon_binding_group_destroy(KryonBindingGroup* group);

// ============================================================================
// DSL Macros for Reactive Binding
// ============================================================================

#ifdef KRYON_DSL_H

/**
 * Reactive text that auto-updates
 * Usage: RTEXT(count_signal, COLOR_RED, FONT_SIZE(16))
 */
#define RTEXT(signal_expr, ...) \
    ({ \
        KryonSignal* _sig = (signal_expr); \
        IRComponent* _comp = kryon_text(""); \
        _kryon_add_to_parent(_comp); \
        kryon_bind_text(_comp, _sig, NULL); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

/**
 * Reactive text with format
 * Usage: RTEXT_FMT(count_signal, "Count: %.0f")
 */
#define RTEXT_FMT(signal_expr, fmt, ...) \
    ({ \
        KryonSignal* _sig = (signal_expr); \
        IRComponent* _comp = kryon_text(""); \
        _kryon_add_to_parent(_comp); \
        kryon_bind_text(_comp, _sig, fmt); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

/**
 * Reactive visibility
 * Usage: RVISIBLE(is_loading_signal)
 */
#define RVISIBLE(signal_expr) \
    ({ \
        KryonSignal* _sig = (signal_expr); \
        IRComponent* _comp = _kryon_get_current_parent(); \
        kryon_bind_visible(_comp, _sig); \
        _comp; \
    })

/**
 * Bind existing component's text
 * Usage: TEXT("Hello"), BIND_TEXT(count_signal)
 */
#define BIND_TEXT(signal_expr) \
    kryon_bind_text(_comp, (signal_expr), NULL)

#define BIND_TEXT_FMT(signal_expr, fmt) \
    kryon_bind_text(_comp, (signal_expr), fmt)

#define BIND_VISIBLE(signal_expr) \
    kryon_bind_visible(_comp, (signal_expr))

#define BIND_BACKGROUND(signal_expr) \
    kryon_bind_background(_comp, (signal_expr))

#define BIND_COLOR(signal_expr) \
    kryon_bind_color(_comp, (signal_expr))

#endif // KRYON_DSL_H

#ifdef __cplusplus
}
#endif

#endif // KRYON_REACTIVE_UI_H
