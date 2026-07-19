#include "example_ui_font.h"
#include "flint.h"

int
main(void)
{
    const char *tabs[] = {"Files", "Search", "Output"};
    int tab = 0;
    int split = 260;
    int open = 1;

    InitWindow(800, 600, "Flint Containers");
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

        DrawUINotebook((UINotebook){{40, 50, 680, 400}, tabs, 3, &tab});
        DrawUIPanedView((UIPanedView){{40, 100, 680, 280}, 501, 1, &split, 120, 160});
        DrawUICollapsible((UICollapsible){{60, 410, 320, 120}, "Details", &open});
        if(open)
            DrawUIText("Collapsible content", 80, 455, 16, GetThemeText());

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
