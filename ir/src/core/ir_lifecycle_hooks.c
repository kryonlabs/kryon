#include "ir_lifecycle_hooks.h"
#include <stdlib.h>
#include <stdbool.h>

#define MAX_HOOKS_PER_TYPE 16

typedef struct {
    kryon_lifecycle_hook_fn fn;
    void* user_data;
    bool used;
} HookEntry;

static HookEntry g_hooks[KRYON_HOOK_COUNT][MAX_HOOKS_PER_TYPE] = {0};
static int g_hook_counts[KRYON_HOOK_COUNT] = {0};

bool ir_register_lifecycle_hook(KryonLifecycleHook type, kryon_lifecycle_hook_fn fn, void* user_data) {
    if (type >= KRYON_HOOK_COUNT) {
        return false;
    }

    if (!fn) {
        return false;
    }

    if (g_hook_counts[type] >= MAX_HOOKS_PER_TYPE) {
        return false;
    }

    int idx = g_hook_counts[type];
    g_hooks[type][idx].fn = fn;
    g_hooks[type][idx].user_data = user_data;
    g_hooks[type][idx].used = true;
    g_hook_counts[type]++;

    return true;
}

void ir_invoke_lifecycle_hooks(KryonLifecycleHook type, void* root, float delta_time) {
    if (type >= KRYON_HOOK_COUNT) {
        return;
    }

    for (int i = 0; i < g_hook_counts[type]; i++) {
        if (g_hooks[type][i].used && g_hooks[type][i].fn) {
            g_hooks[type][i].fn(root, delta_time, g_hooks[type][i].user_data);
        }
    }
}
