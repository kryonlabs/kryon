/*
 * Simple Direct Render Test
 */
#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    Memimage *screen;
    Rectangle screen_rect, button_rect;

    printf("Creating 800x600 screen...\n");
    screen_rect = Rect(0, 0, 800, 600);
    screen = memimage_alloc(screen_rect, RGBA32);

    if (!screen) {
        fprintf(stderr, "Failed to create screen\n");
        return 1;
    }

    /* Fill background with white */
    printf("Filling background white...\n");
    memfillcolor(screen, 0xFFFFFFFF);

    /* Draw a green rectangle */
    printf("Drawing green button at (50, 50) size 200x60...\n");
    button_rect.min.x = 50;
    button_rect.min.y = 50;
    button_rect.max.x = 250;
    button_rect.max.y = 110;
    memfillcolor_rect(screen, button_rect, 0xFF00FF00);  /* Green */

    /* Draw a gray rectangle */
    printf("Drawing gray button at (50, 130) size 200x60...\n");
    button_rect.min.x = 50;
    button_rect.min.y = 130;
    button_rect.max.x = 250;
    button_rect.max.y = 190;
    memfillcolor_rect(screen, button_rect, 0xFFCCCCCC);  /* Gray */

    /* Draw a red rectangle */
    printf("Drawing red rectangle at (50, 210) size 400x50...\n");
    button_rect.min.x = 50;
    button_rect.min.y = 210;
    button_rect.max.x = 450;
    button_rect.max.y = 260;
    memfillcolor_rect(screen, button_rect, 0xFF0000FF);  /* Red */

    /* Save as PPM */
    printf("Saving to direct_test.ppm...\n");
    FILE *fp = fopen("direct_test.ppm", "wb");
    if (fp) {
        unsigned char *pixels = screen->data->bdata;
        int i;
        int width = Dx(screen->r);
        int height = Dy(screen->r);

        fprintf(fp, "P6\n%d %d\n255\n", width, height);

        for (i = 0; i < width * height; i++) {
            fputc(pixels[i * 4 + 0], fp);  /* R */
            fputc(pixels[i * 4 + 1], fp);  /* G */
            fputc(pixels[i * 4 + 2], fp);  /* B */
        }

        fclose(fp);
        printf("Saved!\n");
    }

    /* Check pixel values */
    printf("\n=== Verification ===\n");
    {
        unsigned char *p;

        /* Green button */
        p = screen->data->bdata + (75 * 800 + 75) * 4;
        printf("Green button center (75, 75): R=%d G=%d B=%d A=%d\n",
               p[0], p[1], p[2], p[3]);

        /* Gray button */
        p = screen->data->bdata + (160 * 800 + 75) * 4;
        printf("Gray button center (75, 160): R=%d G=%d B=%d A=%d\n",
               p[0], p[1], p[2], p[3]);

        /* Red rectangle */
        p = screen->data->bdata + (235 * 800 + 75) * 4;
        printf("Red rectangle center (75, 235): R=%d G=%d B=%d A=%d\n",
               p[0], p[1], p[2], p[3]);

        /* White background */
        p = screen->data->bdata + (500 * 800 + 500) * 4;
        printf("White background (500, 500): R=%d G=%d B=%d A=%d\n",
               p[0], p[1], p[2], p[3]);
    }

    printf("\n✅ Done! View with: display direct_test.ppm\n");

    memimage_free(screen);
    return 0;
}
