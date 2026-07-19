#include "example_ui_font.h"
#include "flint.h"
#include <stdio.h>

int
main(void)
{
    UIAccelerator keys[] = {
        {KEY_N, 1, 0, 0, 1},
        {KEY_S, 1, 0, 0, 2}
    };
    char status[96] = "Press Ctrl+N or Ctrl+S";

    InitWindow(800, 600, "Flint Keyboard");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        int action;
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        action = DispatchUIAccelerators(keys, 2);
        if(action != 0)
            snprintf(status, sizeof(status), "Accelerator %d", action);
        if(DrawUIGenericButton(40, 120, 160, 34, "Copy Status", UI_BUTTON_STYLE_SECONDARY, 0, NULL))
            SetUIClipboardTextValue(status);
        DrawUIText(status, 40, 70, 20, GetThemeText());

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
