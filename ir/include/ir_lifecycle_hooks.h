#ifndef IR_LIFECYCLE_HOOKS_H
#define IR_LIFECYCLE_HOOKS_H

#include <stdbool.h>

/**
 * Lifecycle hook types
 */
typedef enum {
    KRYON_HOOK_PRE_RENDER = 0,
    KRYON_HOOK_POST_RENDER = 1,
    KRYON_HOOK_PRE_LAYOUT = 2,
    KRYON_HOOK_POST_LAYOUT = 3,
    KRYON_HOOK_COUNT = 4
} KryonLifecycleHook;

/**
 * Lifecycle hook function type
 *
 * @param root Root component of the tree (opaque void* pointer)
 * @param delta_time Time since last frame (seconds)
 * @param user_data Plugin-specific data
 */
typedef void (*kryon_lifecycle_hook_fn)(void* root, float delta_time, void* user_data);

/**
 * Register a lifecycle hook
 *
 * @param type Hook type (PRE_RENDER, POST_RENDER, etc.)
 * @param fn Hook function to call
 * @param user_data User data passed to hook function
 * @return true on success, false on error
 */
bool ir_register_lifecycle_hook(KryonLifecycleHook type, kryon_lifecycle_hook_fn fn, void* user_data);

/**
 * Invoke all registered hooks of a given type
 *
 * @param type Hook type
 * @param root Root component (will be passed to hooks as opaque pointer)
 * @param delta_time Time since last frame
 */
void ir_invoke_lifecycle_hooks(KryonLifecycleHook type, void* root, float delta_time);

#endif /* IR_LIFECYCLE_HOOKS_H */
