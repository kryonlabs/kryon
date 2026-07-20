#include "example_ui_font.h"
#include "kryon.h"
#include <stdio.h>

int
main(void)
{
    static const UIMenuItem file_items[] = {
        {UI_MENU_COMMAND, "New", "Ctrl+N", 1, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Open", "Ctrl+O", 2, 0, 0, NULL, 0},
        {UI_MENU_SEPARATOR, NULL, NULL, 0, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Quit", "Esc", 3, 0, 0, NULL, 0}
    };
    static const UIMenuItem view_items[] = {
        {UI_MENU_CHECK, "Show Grid", "G", 4, 0, 1, NULL, 0},
        {UI_MENU_RADIO, "Compact", NULL, 5, 0, 0, NULL, 0},
        {UI_MENU_RADIO, "Comfortable", NULL, 6, 0, 1, NULL, 0}
    };
    static const UIMenuItem export_items[] = {
        {UI_MENU_COMMAND, "PNG", NULL, 8, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "SVG", NULL, 9, 0, 0, NULL, 0}
    };
    static const UIMenuItem tool_items[] = {
        {UI_MENU_COMMAND, "Run", "F5", 7, 0, 0, NULL, 0},
        {UI_MENU_SUBMENU, "Export", NULL, 10, 0, 0, export_items, 2}
    };
    static const UIMenu menus[] = {
        {{0}, "File", file_items, 4},
        {{0}, "View", view_items, 3},
        {{0}, "Tools", tool_items, 2}
    };
    int open = -1;
    char status[64] = "No menu command";

    InitWindow(800, 600, "Kryon Menus");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        UIMenuBarResult menu = DrawUIMenuBar(100, (Rectangle){0, 0, GetScreenWidth(), 34}, menus, 3, &open);
        if(menu.activated_id != 0)
            snprintf(status, sizeof(status), "Command %d", menu.activated_id);
        DrawUIText(status, 40, 80, 20, GetThemeText());

        if(IsKeyPressed(KEY_ESCAPE))
            break;
        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
