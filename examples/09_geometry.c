#include "example_ui_font.h"
#include "flint.h"

int
main(void)
{
    InitWindow(800, 600, "Flint Geometry");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());
        SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(),
                    GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(),
                    GetThemeIcon());

        UIFrame frame = BeginUIFrameBox((Rectangle){40, 40, 720, 220}, 16, 16, 10);
        Rectangle title = UIFramePack(&frame, UI_SIDE_TOP, 34);
        Rectangle tools = UIFramePack(&frame, UI_SIDE_LEFT, 180);
        Rectangle status = UIFramePack(&frame, UI_SIDE_BOTTOM, 30);
        UIGrid grid = {{260, 300, 320, 180}, 2, 2, 10, 10, 0, 0};

        DrawUIText("Packed frame", (int)title.x, (int)title.y, 24, GetThemeText());
        DrawUILabelFrame((UILabelFrame){tools, "Tools"});
        DrawUIText("Left packed area", (int)tools.x + 14, (int)tools.y + 24, 16, GetThemeText());
        DrawUISeparator(status, 0);
        DrawUIText("Bottom status area", (int)status.x, (int)status.y + 8, 14, GetThemeIcon());

        for(int row = 0; row < 2; row++) {
            for(int col = 0; col < 2; col++) {
                Rectangle cell = UIGridCell(grid, row, col, 1, 1);
                DrawRectangleRec(cell, GetThemeSurface());
                DrawRectangleLinesEx(cell, 1.0f, GetThemeButton());
            }
        }

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
