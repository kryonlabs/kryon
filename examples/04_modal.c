#include "example_ui_font.h"
#include "ui_scaling.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui.h"
#include "kryon.h"
#include <stdio.h>

static void
load_theme(void)
{
    SetCurrentTheme(THEME_SKY, 0);
}

int
main(void)
{
    int modal_open = 0;
    int background_clicks = 0;
    int last_result = 0;
    const char *last_label = "none";

    InitWindow(800, 600, "Flint Modal Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    load_theme();
    InitUI(GetScreenWidth(), GetScreenHeight(), 1.0f);

    printf("Flint Modal Example\n");
    printf("Click Open Modal. While it is open, background clicks are blocked.\n");

    while(!WindowShouldClose()) {
        int hover = 0;

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        DrawUIText("Modal capture example", 32, 32, 28, GetThemeText());
        DrawUIText("Try clicking the background counter while the modal is open.",
                 32, 72, 18, DarkenUIColor(GetThemeText(), 30));

        if(DrawUIGenericButton(32, 120, 220, 42, "Background Button",
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            background_clicks++;
        }
        DrawUIText(TextFormat("Background clicks: %d", background_clicks),
                 32, 176, 18, GetThemeText());
        DrawUIText(TextFormat("Last modal result: %s", last_label),
                 32, 204, 18, GetThemeText());

        if(DrawUIGenericButton(32, 230, 220, 42, "Open Modal",
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            modal_open = 1;
        }

        if(modal_open) {
            UIModalAction actions[3] = {
                { "Cancel", UI_BUTTON_STYLE_SECONDARY, 0 },
                { "Confirm and continue with a longer label", UI_BUTTON_STYLE_PRIMARY, 0 },
                { "Delete", UI_BUTTON_STYLE_DANGER, 0 }
            };

            last_result = DrawUIActionModal((UIModalSpec){
                .title = "Adaptive Action Modal",
                .message = "Flint measures action labels, fits text inside buttons, and wraps the action row when the dialog is narrow.",
                .actions = actions,
                .action_count = 3,
                .max_width = 420
            });

            if(last_result == 1) {
                modal_open = 0;
                last_label = "cancel";
            } else if(last_result == 2) {
                modal_open = 0;
                last_label = "confirm";
                printf("Modal confirmed\n");
            } else if(last_result == 3) {
                modal_open = 0;
                last_label = "delete";
                printf("Modal delete action\n");
            }
        }

        if(IsKeyPressed(KEY_ESCAPE))
            break;
        EndDrawing();
    }
    UnloadExampleUIFont();

    CloseWindow();
    return 0;
}
