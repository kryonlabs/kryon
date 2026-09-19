#ifndef KRY_INPUT_INTERNAL_H
#define KRY_INPUT_INTERNAL_H

#include "kryon_compat.generated.h"

#ifdef KRYON_BACKEND_RAYLIB
void kry_sdl_prepare_input_poll(void);
void kry_sdl_finish_input_poll(void);
#endif
Vector2 kry_mouse_press_position(Vector2 fallback);
#if ANDROID_BUILD
int kry_android_touch_down(void);
Vector2 kry_android_touch_position(void);
int kry_android_frame_drag(Vector2 *start, Vector2 *current);
void kry_android_consume_frame_drag(void);
#endif

#endif
