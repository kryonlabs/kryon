#include "kryon.h"
#include "../src/ui/ui_paint_internal.h"
#include "../src/ui/ui_blend_internal.h"
#include <stdio.h>
#include <stdlib.h>

static Image
capture(RenderTexture2D target, SurfaceDrawing command, int cached)
{
    BeginDrawing();
    BeginTextureMode(target);
    ui_blend_capture();
    ClearBackground((Color){31, 52, 87, 255});
    BeginScissorMode(13, 11, 224, 120);
    if(cached)
        ui_draw_surface(command);
    else
        ui_draw_surface_direct(command);
    EndScissorMode();
    EndTextureMode();
    EndDrawing();
    return LoadImageFromTexture(target.texture);
}

int
main(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(256, 160, "Surface cache verification");
    SetTargetFPS(0);
    RenderTexture2D target = LoadRenderTexture(256, 160);
    int errors = 0;
    for(int sample = 0; sample < 24; sample++) {
        MaterialPaint paint = {
            .bounds = {20.25f + sample % 3, 20.5f, 190, 70},
            .value = {.background = 0x086bffcc, .border = 0x47a4ffdd,
                .focus = 0x0055ffff, .radius = 12, .border_width = 1,
                .opacity = 0.8f, .material = sample % 2 ? MaterialGlass : MaterialLightfield},
            .ambient = sample % 2 ? 0xf8fbffff : 0x021c33ff,
            .light = 0x0088ffff, .hover = 0.65f, .focus = 1,
            .scale = sample % 3 == 0 ? 1.5f : 1
        };
        paint = PrepareMaterial(paint);
        SurfaceDrawing command = PaintMaterialLayer(paint, sample % 8);
        if(sample % 4 == 0) {
            command.segment.x += 30;
            command.segment.width -= 30;
        }
        Image reference = capture(target, command, 0);
        for(int repeat = 0; repeat < 2; repeat++) {
            Image actual = capture(target, command, 1);
            Color *a = LoadImageColors(actual);
            Color *b = LoadImageColors(reference);
            for(int p = 0; p < actual.width * actual.height; p++) {
                if(abs(a[p].r - b[p].r) > 1 || abs(a[p].g - b[p].g) > 1 ||
                   abs(a[p].b - b[p].b) > 1 || abs(a[p].a - b[p].a) > 1) {
                    fprintf(stderr, "sample %d repeat %d differs at pixel %d\n", sample, repeat, p);
                    errors++;
                    break;
                }
            }
            UnloadImageColors(a);
            UnloadImageColors(b);
            UnloadImage(actual);
        }
        UnloadImage(reference);
    }
    /* Exercise LRU eviction with changing colors, then compare the restored
     * original entry against direct rendering again. */
    SurfaceDrawing probe = {.visible = true, .scale = 1,
        .bounds = {20, 20, 190, 70}, .area = {14, 14, 202, 82},
        .surface = {20, 20, 190, 70}, .segment = {20, 20, 190, 70},
        .layer = {.width = 190, .height = 70, .radius = 12, .blur = 6,
                  .color = 0x3388ff80}};
    BeginDrawing();
    BeginTextureMode(target);
    ui_blend_capture();
    for(int i = 0; i < 140; i++) {
        probe.layer.color = 0x33880080U | ((unsigned)i << 8);
        ui_draw_surface(probe);
    }
    probe.layer.color = 0x3388ff80;
    ui_draw_surface(probe);
    double start = GetTime();
    for(int i = 0; i < 100; i++)
        ui_draw_surface_direct(probe);
    double direct_ms = (GetTime() - start) * 1000;
    start = GetTime();
    for(int i = 0; i < 100; i++)
        ui_draw_surface(probe);
    double cached_ms = (GetTime() - start) * 1000;
    EndTextureMode();
    EndDrawing();
    printf("surface CPU submission, 100 repeated layers: direct %.3f ms, cached %.3f ms\n",
        direct_ms, cached_ms);
    Image reference = capture(target, probe, 0);
    Image actual = capture(target, probe, 1);
    Color *expected = LoadImageColors(reference);
    Color *restored = LoadImageColors(actual);
    for(int p = 0; p < actual.width * actual.height; p++) {
        if(abs(expected[p].r - restored[p].r) > 1 ||
           abs(expected[p].g - restored[p].g) > 1 ||
           abs(expected[p].b - restored[p].b) > 1 ||
           abs(expected[p].a - restored[p].a) > 1) {
            fputs("surface differs after eviction\n", stderr);
            errors++;
            break;
        }
    }
    UnloadImageColors(expected);
    UnloadImageColors(restored);
    UnloadImage(reference);
    UnloadImage(actual);
    ui_surface_cache_shutdown();
    UnloadRenderTexture(target);
    CloseWindow();
    if(!errors)
        puts("surface cache: cold and warm pixels match direct painting");
    return errors ? 1 : 0;
}
