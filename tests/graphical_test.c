/*
 * Kryon Graphical Test
 * Generates a visual proof that the rendering works
 */

#include "graphics.h"
#include "window.h"
#include "widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void render_all(void);
extern void mark_dirty(KryonWindow *win);

int main(void)
{
    Memimage *screen;
    Rectangle screen_rect;
    KryonWindow *win;
    KryonWidget *btn1, *btn2, *lbl;
    FILE *fp;
    int i;

    /* Initialize registries */
    window_registry_init();
    widget_registry_init();

    /* Create 800x600 screen */
    printf("Creating 800x600 screen...\n");
    screen_rect = Rect(0, 0, 800, 600);
    screen = memimage_alloc(screen_rect, RGBA32);
    if (screen == NULL) {
        fprintf(stderr, "Failed to allocate screen\n");
        return 1;
    }

    /* Set screen for rendering */
    render_set_screen(screen);

    /* Clear to white */
    printf("Clearing screen...\n");
    memfillcolor(screen, 0xFFFFFFFF);

    /* Create a window */
    printf("Creating window...\n");
    win = window_create("Test Window", 800, 600);
    if (win == NULL) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    /* Add a green button (value=1, so it should be green) */
    printf("Creating buttons...\n");
    btn1 = widget_create(WIDGET_BUTTON, "btn_green", win);
    if (btn1) {
        free(btn1->prop_rect);
        btn1->prop_rect = strdup("50 50 200 60");
        free(btn1->prop_text);
        btn1->prop_text = strdup("Green Button");
        free(btn1->prop_value);
        btn1->prop_value = strdup("1");  /* Active = green */
    }

    /* Add a gray button (value=0, so it should be gray) */
    btn2 = widget_create(WIDGET_BUTTON, "btn_gray", win);
    if (btn2) {
        free(btn2->prop_rect);
        btn2->prop_rect = strdup("50 130 200 60");
        free(btn2->prop_text);
        btn2->prop_text = strdup("Gray Button");
        free(btn2->prop_value);
        btn2->prop_value = strdup("0");  /* Inactive = gray */
    }

    /* Add a label */
    printf("Creating label...\n");
    lbl = widget_create(WIDGET_LABEL, "lbl_test", win);
    if (lbl) {
        free(lbl->prop_rect);
        lbl->prop_rect = strdup("50 210 400 50");
        free(lbl->prop_text);
        lbl->prop_text = strdup("Kryon Graphics Test - Phase 3");
    }

    /* Add widgets to window */
    window_add_widget(win, btn1);
    window_add_widget(win, btn2);
    window_add_widget(win, lbl);

    /* Render everything */
    printf("Rendering...\n");
    render_all();

    /* Save as PPM file (simple image format) */
    printf("Saving screenshot to test_output.ppm...\n");
    fp = fopen("test_output.ppm", "wb");
    if (fp) {
        unsigned char *pixels = screen->data->bdata;
        int width = Dx(screen->r);
        int height = Dy(screen->r);

        /* PPM header: P6 width height 255\n */
        fprintf(fp, "P6\n%d %d\n255\n", width, height);

        /* Write pixels (convert RGBA to RGB) */
        for (i = 0; i < width * height; i++) {
            unsigned char r = pixels[i * 4 + 0];
            unsigned char g = pixels[i * 4 + 1];
            unsigned char b = pixels[i * 4 + 2];
            fputc(r, fp);
            fputc(g, fp);
            fputc(b, fp);
        }

        fclose(fp);
        printf("Saved!\n");
    }

    /* Also save raw RGBA for inspection */
    printf("Saving raw RGBA to test_output.raw...\n");
    fp = fopen("test_output.raw", "wb");
    if (fp) {
        fwrite(screen->data->bdata, 1, Dx(screen->r) * Dy(screen->r) * 4, fp);
        fclose(fp);
        printf("Saved!\n");
    }

    /* Print some pixel values to verify */
    printf("\n=== Pixel Verification ===\n");
    printf("Pixel at (75, 75) - should be inside green button:\n");
    {
        unsigned char *p = screen->data->bdata + (75 * 800 + 75) * 4;
        printf("  R=%d G=%d B=%d A=%d\n", p[0], p[1], p[2], p[3]);
    }
    printf("Pixel at (75, 155) - should be inside gray button:\n");
    {
        unsigned char *p = screen->data->bdata + (155 * 800 + 75) * 4;
        printf("  R=%d G=%d B=%d A=%d\n", p[0], p[1], p[2], p[3]);
    }
    printf("Pixel at (75, 230) - should be inside label (white):\n");
    {
        unsigned char *p = screen->data->bdata + (230 * 800 + 75) * 4;
        printf("  R=%d G=%d B=%d A=%d\n", p[0], p[1], p[2], p[3]);
    }
    printf("Pixel at (500, 500) - should be background (white):\n");
    {
        unsigned char *p = screen->data->bdata + (500 * 800 + 500) * 4;
        printf("  R=%d G=%d B=%d A=%d\n", p[0], p[1], p[2], p[3]);
    }

    printf("\n✅ Test complete!\n");
    printf("View with: display test_output.ppm\n");
    printf("Or convert: convert test_output.ppm test_output.png\n");

    /* Cleanup */
    memimage_free(screen);

    return 0;
}
