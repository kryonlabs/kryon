#include "example_ui_font.h"
#include "ui_scaling.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui.h"
#include <raylib.h>
#include <stdio.h>

static void
load_theme(void)
{
    SetCurrentTheme(THEME_SKY, 0);
}

static void
apply_ui_theme(void)
{
    SetUIColors(GetThemeText(),
                  GetThemeBackground(),
                  GetThemeSurface(),
                  GetThemeCircle(),
                  GetThemeButton(),
                  GetThemeButtonHover(),
                  GetThemeIcon());
}

int
main(void)
{
    int modal_open = 0;
    int background_clicks = 0;
    int modal_clicks = 0;
    int last_result = 0;

    InitWindow(800, 600, "File UI Toolkit Modal Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    load_theme();
    InitUI(GetScreenWidth(), GetScreenHeight(), 1.0f);
    apply_ui_theme();

    printf("File UI Toolkit Modal Example\n");
    printf("Click Open Modal. While it is open, background clicks are blocked.\n");

    while(!WindowShouldClose()) {
        int hover = 0;

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        apply_ui_theme();
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
        DrawUIText(TextFormat("Last modal result: %s",
                                   last_result == 1 ? "cancel" :
                                   last_result == 2 ? "confirm" : "none"),
                 32, 204, 18, GetThemeText());

        if(DrawUIGenericButton(32, 230, 220, 42, "Open Modal",
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            modal_open = 1;
        }

        if(modal_open) {
            UIPanelFrame frame;
            int x;
            int y;
            int button_w;
            int button_h;
            int gap;

            frame = DrawUIModalFrame(ScaleUIPx(380), ScaleUIPx(270),
                                        "Real Modal", (Texture2D){0}, (Texture2D){0});
            x = frame.content_x;
            y = frame.content_y;

            DrawUIText("This is an actual modal frame.", x, y,
                            20, GetThemeText());
            y += ScaleUIPx(34);
            DrawUIText("Clicking outside is blocked while it is open.",
                            x, y, 18, DarkenUIColor(GetThemeText(), 30));
            y += ScaleUIPx(30);
            DrawUIText(TextFormat("Modal-only clicks: %d", modal_clicks),
                            x, y, 18, GetThemeText());
            y += ScaleUIPx(38);

            if(DrawUIGenericButton(x, y, frame.content_w, ScaleUIPx(40),
                                      "Button inside modal", UI_BUTTON_STYLE_SECONDARY,
                                      0, &hover)) {
                modal_clicks++;
            }

            button_h = ScaleUIPx(40);
            button_w = (frame.content_w - ScaleUIPx(12)) / 2;
            gap = ScaleUIPx(12);
            y = frame.y + frame.h - button_h - ScaleUIPx(18);

            if(DrawUIGenericButton(x, y, button_w, button_h, "Cancel",
                                      UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                modal_open = 0;
                last_result = 1;
            }
            if(DrawUIGenericButton(x + button_w + gap, y, button_w, button_h,
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
    UnloadExampleUIFont();

    CloseWindow();
    return 0;
}
