#ifndef KRY_INPUT_INTERNAL_H
#define KRY_INPUT_INTERNAL_H

#include "kryon_compat.generated.h"

#ifdef KRYON_BACKEND_RAYLIB
void kry_sdl_prepare_input_poll(void);
void kry_sdl_finish_input_poll(void);
#endif
Vector2 kry_mouse_press_position(Vector2 fallback);

#endif
