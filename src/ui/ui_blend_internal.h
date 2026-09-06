#ifndef KRYON_UI_BLEND_INTERNAL_H
#define KRYON_UI_BLEND_INTERNAL_H

/* Private raylib backend snapshot: cached mode, custom factors, pending-change
 * bit, and effective GPU factors/equations. Preserve both configured and active
 * state: changing custom factors need not activate them immediately. */
typedef struct UIBlendState { int values[17]; } UIBlendState;
#if defined(KRYON_BACKEND_RAYLIB)
void kryon_rl_blend_save(int *values);
void kryon_rl_blend_restore(const int *values);
void kryon_rl_blend_capture(void);
#endif

static inline void ui_blend_capture(void)
{
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_blend_capture();
#endif
}

static inline UIBlendState ui_blend_save(void)
{
    UIBlendState state = {{0}};
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_blend_save(state.values);
#endif
    return state;
}

static inline void ui_blend_restore(UIBlendState state)
{
#if defined(KRYON_BACKEND_RAYLIB)
    kryon_rl_blend_restore(state.values);
#else
    (void)state;
#endif
}
#endif
