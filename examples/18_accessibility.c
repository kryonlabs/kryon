#include "example_ui_font.h"
#include "flint.h"

int
main(void)
{
    UIAccessibilityNode nodes[] = {
        {{40, 80, 180, 36}, "button", "Save", 1, 0, 0},
        {{40, 130, 180, 36}, "checkbox", "Enabled", 0, 0, 1}
    };
    int enabled = 1;

    InitWindow(800, 600, "Flint Accessibility");
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

        DrawUIGenericButton(40, 80, 180, 36, "Save", UI_BUTTON_STYLE_PRIMARY, 0, NULL);
        DrawUICheckboxToggle(40, 134, "Enabled", &enabled);
        nodes[1].checked = enabled;
        DrawUIFocusDebugOverlay(nodes, 2);

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
