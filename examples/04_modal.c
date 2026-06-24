#include "flint_example_font.h"
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

int
main(void)
{
    int modal_open = 0;
    int background_clicks = 0;

    InitWindow(800, 600, "Flint Modal Example");
    SetTargetFPS(60);
    flint_example_load_font();
    load_theme();

    printf("Flint Modal Example\n");
    printf("Click Open Modal. While it is open, background clicks are blocked.\n");

    while(!WindowShouldClose()) {
        int hover = 0;
        int result;

        BeginDrawing();
        ClearBackground(flint_theme_get_bg());
        flint_set_view_size(GetScreenWidth(), GetScreenHeight());
        ui_set_input_blocked(0);

        flint_text_draw("Modal capture example", 32, 32, 28, flint_theme_get_text());
        flint_text_draw("Try clicking the background counter while the modal is open.",
                 32, 72, 18, flint_darken(flint_theme_get_text(), 30));

        if(ui_draw_generic_button(32, 120, 220, 42, "Background Button",
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            background_clicks++;
        }
        flint_text_draw(TextFormat("Background clicks: %d", background_clicks),
                 32, 176, 18, flint_theme_get_text());

        if(ui_draw_generic_button(32, 230, 220, 42, "Open Modal",
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            modal_open = 1;
        }

        if(modal_open) {
            result = ui_draw_modal("Confirm Action",
                                   "This modal owns input. Nothing outside it is clickable.",
                                   "Cancel", "Confirm");
            if(result == 1) {
                modal_open = 0;
            } else if(result == 2) {
                modal_open = 0;
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
