/*
 * krb-web — native web host for KRB cartridges (plan 11, phase 3).
 *
 * Compiles the KRB runtime + kry_sw software rasterizer to wasm with
 * emscripten. Pixel-identical to the native headless renderer: the frame
 * is blitted into an ImageData every tick. Cartridge is preloaded at
 * /app.krb (--preload-file); input comes from canvas events through the
 * exported krb_web_mouse / krb_web_button.
 *
 * Build: make krb-web KRB=examples/02_buttons.kry
 */

#include "krb.h"
#include "kry_sw.h"

#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#ifndef KRB_WEB_CARTRIDGE
#define KRB_WEB_CARTRIDGE "/app.krb"
#endif

static KrbImage g_img;
static KrySw g_sw;
static int g_running;

#ifdef KRB_WEB_ONESHOT
static int g_frames;
#endif

static void
frame(void)
{
    KrySwAdvance(&g_sw, 1.0f / 60.0f);
    KrbDraw(&g_img, 0, 0, g_sw.w, g_sw.h);
    EM_ASM({
        var ptr = $0;
        var w = $1;
        var h = $2;
        var cv = Module.canvas;
        var ctx = cv.getContext('2d');
        if (!Module.krbImg || Module.krbImg.width !== w ||
            Module.krbImg.height !== h)
            Module.krbImg = ctx.createImageData(w, h);
        Module.krbImg.data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
        ctx.putImageData(Module.krbImg, 0, 0);
    }, g_sw.pixels, g_sw.w, g_sw.h);
#ifdef KRB_WEB_ONESHOT
    if(++g_frames >= 1)
        emscripten_cancel_main_loop();
#endif
}

int
main(void)
{
    /* The player waits for a cartridge: the shell writes the picked or
     * dropped file to KRB_WEB_CARTRIDGE in MEMFS and calls krb_web_start. */
    return 0;
}

int
krb_web_start(void)
{
    memset(&g_img, 0, sizeof(g_img));
    if(g_sw.pixels != NULL)
        KrySwFree(&g_sw);
    if(KrbLoadFile(&g_img, KRB_WEB_CARTRIDGE) != 0) {
        fprintf(stderr, "krb-player: load failed: %s\n", KRB_WEB_CARTRIDGE);
        return 1;
    }
    if(KrySwInit(&g_sw, NULL, 800, 600) != 0)
        return 1;
    {
        const unsigned char *ad = NULL;
        unsigned al = 0;
        unsigned ak = 0;

        if(KrbAssetFind(&g_img, "@atlas", &ad, &al, &ak, NULL, NULL) == 0 &&
           ak == 1)
            KrySwSetAtlas(&g_sw, ad, al);
    }
    KryBackendSelect(KrySwBackend(&g_sw));
    if(!g_running)
        emscripten_set_main_loop(frame, 0, 1);
    g_running = 1;
    return 0;
}

void
krb_web_mouse(int x, int y)
{
    KrySwMouse(&g_sw, x, y);
}

void
krb_web_wheel(int dy)
{
    KrySwWheel(&g_sw, dy);
}

void
krb_web_button(int button, int down)
{
    if(down)
        KrySwButtonDown(&g_sw, button);
    else
        KrySwButtonUp(&g_sw, button);
}
