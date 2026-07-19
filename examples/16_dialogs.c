#include "example_ui_font.h"
#include "flint.h"

int
main(void)
{
    int dialog = 0;
    char prompt[128] = "value";
    int cursor = 5;
    int focused = 0;
    Color color = {120, 160, 220, 255};

    InitWindow(800, 600, "Flint Dialogs");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        if(DrawUIGenericButton(40, 60, 150, 34, "Message", UI_BUTTON_STYLE_PRIMARY, 0, NULL)) dialog = 1;
        if(DrawUIGenericButton(40, 104, 150, 34, "Confirm", UI_BUTTON_STYLE_PRIMARY, 0, NULL)) dialog = 2;
        if(DrawUIGenericButton(40, 148, 150, 34, "Prompt", UI_BUTTON_STYLE_PRIMARY, 0, NULL)) dialog = 3;
        DrawUIColorPicker((Rectangle){240, 60, 320, 160}, &color);

        if(dialog == 1 && DrawUIMessageDialog((UIMessageDialog){"Message", "Hello from Flint.", "OK"}) > 0) dialog = 0;
        if(dialog == 2 && DrawUIConfirmDialog((UIConfirmDialog){"Confirm", "Continue?", "No", "Yes"}) != 0) dialog = 0;
        if(dialog == 3 && DrawUIPromptDialog((UIPromptDialog){"Prompt", prompt, sizeof(prompt), &cursor, &focused, "Cancel", "Save"}) != 0) dialog = 0;

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
