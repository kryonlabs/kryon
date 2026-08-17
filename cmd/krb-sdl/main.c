/*
 * krb-sdl — SDL2 desktop host for KRB cartridges (plan 11, phase 7).
 *
 * Same shape as the web host: the kry_sw rasterizer produces the frame,
 * SDL2 owns the window, input, and presentation (streaming texture).
 * This is the template for the Android EGL host.
 *
 * Usage: krb-sdl [--png out.png] [--w W] [--h H] file.krb
 *   --png renders exactly one frame, dumps it, and exits (for tests;
 *   runs fine under SDL_VIDEODRIVER=dummy).
 */

#include "krb.h"
#include "kry_sw.h"
#include "png_write.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    const char *png_path = NULL;
    const char *krb_path = NULL;
    int w = 800;
    int h = 600;
    int i;
    KrbImage img;
    KrySw sw;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            png_path = argv[++i];
        else if(strcmp(argv[i], "--w") == 0 && i + 1 < argc)
            w = atoi(argv[++i]);
        else if(strcmp(argv[i], "--h") == 0 && i + 1 < argc)
            h = atoi(argv[++i]);
        else if(argv[i][0] != '-' && krb_path == NULL)
            krb_path = argv[i];
        else {
            fprintf(stderr,
                    "usage: krb-sdl [--png out.png] [--w W] [--h H]"
                    " file.krb\n");
            return 2;
        }
    }
    if(krb_path == NULL) {
        fprintf(stderr, "krb-sdl: no cartridge given\n");
        return 2;
    }
    memset(&img, 0, sizeof(img));
    if(KrbLoadFile(&img, krb_path) != 0) {
        fprintf(stderr, "krb-sdl: load failed: %s\n", krb_path);
        return 1;
    }
    if(KrySwInit(&sw, NULL, w, h) != 0) {
        fprintf(stderr, "krb-sdl: rasterizer init failed\n");
        return 1;
    }
    {
        const unsigned char *ad = NULL;
        unsigned al = 0;
        unsigned ak = 0;

        if(KrbAssetFind(&img, "@atlas", &ad, &al, &ak, NULL, NULL) == 0 &&
           ak == 1)
            KrySwSetAtlas(&sw, ad, al);
    }
    KryBackendSelect(KrySwBackend(&sw));

    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "krb-sdl: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if(SDL_CreateWindowAndRenderer(w, h, SDL_WINDOW_RESIZABLE, &window,
                                   &renderer) != 0) {
        fprintf(stderr, "krb-sdl: window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowTitle(window, "krb-sdl");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
    if(texture == NULL) {
        fprintf(stderr, "krb-sdl: texture: %s\n", SDL_GetError());
        return 1;
    }

    for(;;) {
        SDL_Event ev;
        int running = 1;

        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_QUIT)
                running = 0;
            else if(ev.type == SDL_MOUSEMOTION)
                KrySwMouse(&sw, ev.motion.x, ev.motion.y);
            else if(ev.type == SDL_MOUSEBUTTONDOWN)
                KrySwButtonDown(&sw, 0);
            else if(ev.type == SDL_MOUSEBUTTONUP)
                KrySwButtonUp(&sw, 0);
            else if(ev.type == SDL_MOUSEWHEEL)
                KrySwWheel(&sw, ev.wheel.y * 50);
        }
        if(!running)
            break;

        KrySwAdvance(&sw, 1.0f / 60.0f);
        KrbDraw(&img, 0, 0, w, h);

        if(SDL_UpdateTexture(texture, NULL, sw.pixels, sw.stride) != 0) {
            fprintf(stderr, "krb-sdl: update: %s\n", SDL_GetError());
            return 1;
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        if(png_path != NULL) {
            /* Test mode: read back through the SDL renderer — the same
             * pixels that reached the screen — and dump them. */
            unsigned char *buf = malloc((size_t)w * h * 4);
            SDL_Rect r = {0, 0, w, h};
            int ok;
            if(buf == NULL)
                return 1;
            ok = SDL_RenderReadPixels(renderer, &r,
                                      SDL_PIXELFORMAT_RGBA32, buf,
                                      w * 4) == 0;
            if(ok)
                ok = krb_png_write(png_path, buf, w, h) == 0;
            free(buf);
            if(!ok) {
                fprintf(stderr, "krb-sdl: frame dump failed\n");
                return 1;
            }
            break;
        }
        SDL_Delay(16);
    }

    printf("krb-sdl ok %s %dx%d\n", krb_path, w, h);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    KrbFree(&img);
    KrySwFree(&sw);
    return 0;
}
