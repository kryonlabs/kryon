#include "kryon.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;

static void
check(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "libdraw hierarchy failed: %s\n", name);
        failures++;
    }
}

static unsigned char *
image_pixel(Image *image, int x, int y)
{
    if(image == NULL || image->data == NULL || x < 0 || y < 0 ||
       x >= image->width || y >= image->height)
        return NULL;
    return (unsigned char *)image->data + ((size_t)y * image->width + x) * 4;
}

static int
pixel_near(unsigned char *pixel, Color color, int tolerance)
{
    if(pixel == NULL)
        return 0;
    return abs((int)pixel[0] - (int)color.r) <= tolerance &&
           abs((int)pixel[1] - (int)color.g) <= tolerance &&
           abs((int)pixel[2] - (int)color.b) <= tolerance;
}

static int
region_has_light_pixel(Image *image, int x, int y, int w, int h)
{
    int ix;
    int iy;

    for(iy = y; iy < y + h; iy++) {
        for(ix = x; ix < x + w; ix++) {
            unsigned char *p = image_pixel(image, ix, iy);

            if(p != NULL && p[0] > 180 && p[1] > 180 && p[2] > 180)
                return 1;
        }
    }
    return 0;
}

static int
region_has_red_leak(Image *image, int x, int y, int w, int h)
{
    int ix;
    int iy;

    for(iy = y; iy < y + h; iy++) {
        for(ix = x; ix < x + w; ix++) {
            unsigned char *p = image_pixel(image, ix, iy);

            if(p != NULL && p[0] > 150 && p[1] < 90 && p[2] < 100)
                return 1;
        }
    }
    return 0;
}

int
main(void)
{
    const Color bg = {12, 14, 18, 255};
    const Color lower = {235, 28, 44, 255};
    const Color cover = {22, 166, 118, 255};
    Image shot;
    unsigned char *covered;
    const char *out;

    InitWindow(260, 180, "Kryon libdraw hierarchy");
    check("window ready", IsWindowReady());
    if(!IsWindowReady())
        return 2;

    BeginDrawing();
    ClearBackground(bg);
    BeginUIFrame(260, 180, 1.0f);
    BeginTree(Key("libdraw-hierarchy"));
    Text("HHHHHHHHHHHH", 54, 68, 32, lower);
    DrawRectangle(44, 54, 160, 84, cover);
    Text("TOP", 72, 86, 24, WHITE);
    Button((ButtonProps){.bounds = {54, 134, 110, 28},
                         .label = "BUTTON-LEAK",
                         .style = ButtonStyleSecondary,
                         .font = Text12,
                         .id = 901});
    DrawRectangle(44, 128, 160, 42, cover);
    EndTree();
    EndUIFrame();
    EndDrawing();

    shot = LoadImageFromScreen();
    out = getenv("KRYON_LIBDRAW_HIERARCHY_OUT");
    if(out != NULL && out[0] != '\0')
        ExportImage(shot, out);
    check("screen capture data", shot.data != NULL);
    covered = image_pixel(&shot, 92, 76);
    check("opaque later rect covers earlier text",
          pixel_near(covered, cover, 8));
    check("opaque later rect has no lower text leak",
          !region_has_red_leak(&shot, 48, 58, 150, 76));
    check("later text remains visible",
          region_has_light_pixel(&shot, 68, 82, 70, 34));
    check("opaque later rect covers earlier button",
          pixel_near(image_pixel(&shot, 92, 146), cover, 8));
    check("opaque later rect has no button replay",
          !region_has_light_pixel(&shot, 54, 134, 110, 28));

    UnloadImage(shot);
    CloseWindow();
    return failures == 0 ? 0 : 1;
}
