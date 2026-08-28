/*
 * canvas_internal.h — shared declarations for the canvas backend sources.
 *
 * The Tier A HTML5 Canvas2D backend is split across the canvas_*.c files
 * in this directory: window/frame (canvas_window.c), input (canvas_input.c),
 * drawing (canvas_draw.c), images & textures (canvas_texture.c), text
 * (canvas_text.c), OS services (canvas_os.c), and null-grade audio stubs
 * (canvas_audio.c). This header carries the include set they share and
 * declares the EM_JS glue used from more than one of them; wrappers with a
 * single consumer stay next to their users, undeclared.
 *
 * Every canvas_*.c wraps its whole body in #ifdef __EMSCRIPTEN__ and
 * compiles to an empty translation unit otherwise: vendoring apps sweep
 * kryon's src/ tree into native builds where <emscripten.h> does not
 * exist. This header is therefore only ever included from inside that
 * guard.
 */

#ifndef KRY_CANVAS_INTERNAL_H
#define KRY_CANVAS_INTERNAL_H

#include "kryon_compat.generated.h"
#include "kry_input.h"
#include "kry_sw_png.h"

#include <emscripten.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* EM_JS glue shared across the canvas_*.c sources. */
void js_canvas_boot(int w, int h, const char *title);
void js_canvas_resize(int w, int h);
void js_ctx_call(int op, double a, double b, double c, double d,
                 double e, double f, double g2,
                 int r, int gg, int bb, int aa);
int js_input_query(int which, int code);
void js_input_end_frame(void);
int js_texture_from_rgba(int ptr, int w, int h);
void js_texture_free(int id);

#endif /* KRY_CANVAS_INTERNAL_H */
