#include "example_ui_font.h"
#include "kryon.h"

int
main(void)
{
    const char *choices[] = {"Alpha", "Beta", "Gamma"};
    int radio = 1;
    int spin = 4;
    int combo = 0;
    int progress = 45;

    InitWindow(800, 600, "Flint Basic Controls");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        DrawUIText("Basic controls", 40, 40, 24, GetThemeText());
        int picked = DrawUIRadioButton((UIRadioButton){{40, 90, 160, 28}, "Choice A", 1, radio == 1, 0});
        if(picked) radio = picked;
        picked = DrawUIRadioButton((UIRadioButton){{40, 124, 160, 28}, "Choice B", 2, radio == 2, 0});
        if(picked) radio = picked;
        DrawUISpinbox((UISpinbox){{240, 90, 140, 34}, 201, 0, 10, 1, &spin, 0});
        DrawUICombobox((UICombobox){{240, 140, 180, 34}, 202, choices, 3, &combo, 0});
        DrawUIProgressBar((UIProgressBar){{40, 210, 380, 28}, 0, 100, progress, "Progress"});
        if(IsKeyPressed(KEY_RIGHT) && progress < 100) progress += 5;
        if(IsKeyPressed(KEY_LEFT) && progress > 0) progress -= 5;

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
