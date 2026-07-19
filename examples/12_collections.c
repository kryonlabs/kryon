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
    UICascadingTreeItem cascade[] = {
        {"app", 0, 11, 1, 0},
        {"scenes", 1, 12, 1, 0},
        {"start.kry", 2, 13, 0, 1},
        {"settings.kry", 2, 14, 0, 1},
        {"src", 1, 15, 1, 0},
        {"main.c", 2, 16, 0, 1},
        {"very_long_file_name_that_must_clip_inside_the_tree_row.c", 2, 17, 0, 1},
        {"assets", 1, 18, 1, 0},
        {"background.png", 2, 19, 0, 1}
    };
    const char *cols[] = {"Name", "Kind"};
    const char *row0[] = {"main.c", "C source"};
    const char *row1[] = {"README.md", "Markdown"};
    UITableRow rows[] = {{row0, 2}, {row1, 2}};
    int expanded_ids[8] = {11, 12, 15};
    int expanded_count = 3;
    int list_sel = 0;
    int tree_sel = 1;
    int cascade_sel = 13;
    int cascade_activated = 0;
    int row_sel = 0;
    int sort_col = 0;
    int list_scroll = 0;
    int tree_scroll = 0;
    int cascade_scroll = 0;
    int table_scroll = 0;

    InitWindow(900, 620, "Flint Collections");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(900, 620, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        DrawUIListBox((UIListBox){{40, 60, 180, 180}, 301, items, 4,
                                  &list_sel, &list_scroll, 30});
        DrawUITreeView((UITreeView){{260, 60, 220, 180}, 302, tree, 4,
                                    &tree_sel, &tree_scroll, 28});
        DrawUITableView((UITableView){{520, 60, 320, 180}, 303, cols, 2,
                                      rows, 2, NULL, &row_sel, &sort_col,
                                      &table_scroll, 28});

        DrawUIText("Cascading file browser", 40, 275, UI_TEXT_16,
                   GetThemeText());
        cascade_activated = 0;
        DrawUICascadingTreeView((UICascadingTreeView){
            .bounds = {40, 305, 360, 240},
            .id = 304,
            .items = cascade,
            .item_count = 9,
            .selected_id = &cascade_sel,
            .activated_id = &cascade_activated,
            .expanded = {expanded_ids, &expanded_count, 8},
            .scroll_offset = &cascade_scroll,
            .row_height = 28
        });
        if(cascade_activated != 0) {
            DrawUIText(TextFormat("Selected id: %d", cascade_activated),
                       430, 315, UI_TEXT_16, GetThemeText());
        }
        DrawUIText("Folders use +/- and toggle open. Files select.",
                   430, 345, UI_TEXT_16, GetThemeIcon());

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
