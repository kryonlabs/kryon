#include "flint_example_font.h"
#include "flint_scaling.h"
#include "flint_theme.h"
#include "flint_theme_meta.h"
#include "flint_ui.h"
#include <raylib.h>
#include <stdio.h>

static void
load_theme(void)
{
    flint_theme_set_current(FLINT_THEME_SKY, 0);
}

static void
apply_ui_theme(void)
{
    ui_set_colors(flint_theme_get_text(),
                  flint_theme_get_bg(),
                  flint_theme_get_surface(),
                  flint_theme_get_circle(),
                  flint_theme_get_button(),
                  flint_theme_get_button_hover(),
                  flint_theme_get_icon());
}

int
main(void)
{
    int modal_open = 0;
    int background_clicks = 0;
    int modal_clicks = 0;
    int last_result = 0;

    InitWindow(800, 600, "Flint Modal Example");
    SetTargetFPS(60);
    flint_example_load_font();
    load_theme();
    ui_init(GetScreenWidth(), GetScreenHeight(), 1.0f);
    apply_ui_theme();

    printf("Flint Modal Example\n");
    printf("Click Open Modal. While it is open, background clicks are blocked.\n");

    while(!WindowShouldClose()) {
        int hover = 0;

        BeginDrawing();
        ClearBackground(flint_theme_get_bg());
        apply_ui_theme();
        flint_ui_begin_frame(GetScreenWidth(), GetScreenHeight(), flint_get_dpi_scale());

        flint_text_draw("Modal capture example", 32, 32, 28, flint_theme_get_text());
        flint_text_draw("Try clicking the background counter while the modal is open.",
                 32, 72, 18, flint_darken(flint_theme_get_text(), 30));

        if(ui_draw_generic_button(32, 120, 220, 42, "Background Button",
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            background_clicks++;
        }
        flint_text_draw(TextFormat("Background clicks: %d", background_clicks),
                 32, 176, 18, flint_theme_get_text());
        flint_text_draw(TextFormat("Last modal result: %s",
                                   last_result == 1 ? "cancel" :
                                   last_result == 2 ? "confirm" : "none"),
                 32, 204, 18, flint_theme_get_text());

        if(ui_draw_generic_button(32, 230, 220, 42, "Open Modal",
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            modal_open = 1;
        }

        if(modal_open) {
            FlintUIPanelFrame frame;
            int x;
            int y;
            int button_w;
            int button_h;
            int gap;

            frame = ui_draw_modal_frame(flint_px(380), flint_px(270),
                                        "Real Modal", (Texture2D){0}, (Texture2D){0});
            x = frame.content_x;
            y = frame.content_y;

            flint_text_draw("This is an actual modal frame.", x, y,
                            20, flint_theme_get_text());
            y += flint_px(34);
            flint_text_draw("Clicking outside is blocked while it is open.",
                            x, y, 18, flint_darken(flint_theme_get_text(), 30));
            y += flint_px(30);
            flint_text_draw(TextFormat("Modal-only clicks: %d", modal_clicks),
                            x, y, 18, flint_theme_get_text());
            y += flint_px(38);

            if(ui_draw_generic_button(x, y, frame.content_w, flint_px(40),
                                      "Button inside modal", UI_BUTTON_STYLE_SECONDARY,
                                      0, &hover)) {
                modal_clicks++;
            }

            button_h = flint_px(40);
            button_w = (frame.content_w - flint_px(12)) / 2;
            gap = flint_px(12);
            y = frame.y + frame.h - button_h - flint_px(18);

            if(ui_draw_generic_button(x, y, button_w, button_h, "Cancel",
                                      UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                modal_open = 0;
                last_result = 1;
            }
            if(ui_draw_generic_button(x + button_w + gap, y, button_w, button_h,
                                      "Confirm", UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
                modal_open = 0;
                last_result = 2;
                printf("Modal confirmed\n");
            }
        }

        if(IsKeyPressed(KEY_ESCAPE))
            break;
        EndDrawing();
    }
    flint_example_unload_font();

    CloseWindow();
    return 0;
}
