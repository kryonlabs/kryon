#ifndef IR_LIFECYCLE_HOOKS_H
#define IR_LIFECYCLE_HOOKS_H

#include <stdbool.h>
#include "../include/kryon/capability.h"

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
