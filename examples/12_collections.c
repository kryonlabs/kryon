#include "example_ui_font.h"
#include "flint.h"

int
main(void)
{
    const char *items[] = {"One", "Two", "Three", "Four"};
    UITreeItem tree[] = {
        {"Project", 0, 1, 1, 1},
        {"src", 1, 2, 1, 1},
        {"main.c", 2, 3, 0, 1},
        {"README.md", 1, 4, 0, 1}
    };
    const char *cols[] = {"Name", "Kind"};
    const char *row0[] = {"main.c", "C source"};
    const char *row1[] = {"README.md", "Markdown"};
    UITableRow rows[] = {{row0, 2}, {row1, 2}};
    int list_sel = 0;
    int tree_sel = 1;
    int row_sel = 0;
    int sort_col = 0;
    int scroll = 0;

    InitWindow(900, 620, "Flint Collections");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(900, 620, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());
        SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(),
                    GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(),
                    GetThemeIcon());

        DrawUIListBox((UIListBox){{40, 60, 180, 180}, 301, items, 4, &list_sel, &scroll, 30});
        DrawUITreeView((UITreeView){{260, 60, 220, 180}, 302, tree, 4, &tree_sel, &scroll, 28});
        DrawUITableView((UITableView){{520, 60, 320, 180}, 303, cols, 2, rows, 2, NULL,
                                      &row_sel, &sort_col, &scroll, 28});

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
