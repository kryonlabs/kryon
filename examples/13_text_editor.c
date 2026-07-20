#include "example_ui_font.h"
#include "kryon.h"
#include <stdio.h>
#include <string.h>

int
main(void)
{
    char text[512] = "Edit this text.";
    int cursor = 15;
    int focused = 0;
    int scroll = 0;

    InitWindow(800, 600, "Kryon Text Editor");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        DrawUITextArea((UITextArea){{40, 70, 520, 180}, text, sizeof(text), &cursor,
                                   &focused, &scroll, 500, 16, 6, 401,
                                   "Write here", {0}, NULL, NULL});
        DrawUIText("Ctrl/Cmd+C, X, and V work in the text area.", 40, 260, 16, GetThemeText());
        if(DrawUIGenericButton(40, 300, 110, 34, "Copy", UI_BUTTON_STYLE_SECONDARY, 0, NULL))
            SetUIClipboardTextValue(text);
        if(DrawUIGenericButton(170, 300, 110, 34, "Paste", UI_BUTTON_STYLE_SECONDARY, 0, NULL)) {
            const char *clip = GetUIClipboardTextValue();
            snprintf(text, sizeof(text), "%s", clip);
            cursor = (int)strlen(text);
        }

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
