#include "ki_internal.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

static int
draw_start_card(int x, int y, int w, int card_h, const char *title,
                const char *subtitle)
{
    Rectangle card = {(float)x, (float)y, (float)w, (float)card_h};
    int hovered = 0;
    int clicked = UIHandleClick(card, 0, &hovered);

    DrawRectangleRec(card, hovered ? GetThemeButtonHover() : GetThemeSurface());
    DrawRectangleLinesEx(card, 1, DarkenUIColor(GetThemeSurface(), 42));
    DrawUIText(title, x + ScaleUIPx(12), y + ScaleUIPx(12), UI_TEXT_16,
               GetThemeText());
    if(subtitle != NULL && subtitle[0] != '\0')
        DrawUIText(subtitle, x + ScaleUIPx(12), y + ScaleUIPx(38),
                   UI_TEXT_12, GetThemeIcon());
    return clicked;
}

static int
draw_recent_project_card(int x, int y, int w, int card_h, const char *title,
                         const char *subtitle, int index)
{
    Rectangle card = {(float)x, (float)y, (float)w, (float)card_h};
    Rectangle remove = {
        (float)(x + w - ScaleUIPx(26)),
        (float)(y + ScaleUIPx(8)),
        (float)ScaleUIPx(18),
        (float)ScaleUIPx(18)
    };
    int remove_hovered = 0;
    int hovered = 0;
    int removed = UIHandleClick(remove, 0, &remove_hovered);
    int clicked = 0;

    if(!remove_hovered)
        clicked = UIHandleClick(card, 0, &hovered);

    DrawRectangleRec(card, hovered ? GetThemeButtonHover() : GetThemeSurface());
    DrawRectangleLinesEx(card, 1, DarkenUIColor(GetThemeSurface(), 42));
    DrawUIText(title, x + ScaleUIPx(12), y + ScaleUIPx(12), UI_TEXT_16,
               GetThemeText());
    if(subtitle != NULL && subtitle[0] != '\0')
        DrawUIText(subtitle, x + ScaleUIPx(12), y + ScaleUIPx(38),
                   UI_TEXT_12, GetThemeIcon());
    DrawRectangleRec(remove,
                     remove_hovered ? GetThemeButtonHover()
                                    : DarkenUIColor(GetThemeSurface(), 8));
    DrawRectangleLinesEx(remove, 1, DarkenUIColor(GetThemeSurface(), 48));
    DrawCenteredUIControlText("x", (int)(remove.x + remove.width * 0.5f),
                              (int)(remove.y + remove.height * 0.5f),
                              UI_TEXT_12, GetThemeText());
    if(removed)
        return -1;
    (void)index;
    return clicked;
}

void
draw_start_page(Rectangle content, EditorProject *project,
                EditorRecentProjects *recent, EditorSidebarState *sidebar,
                int *new_project_requested,
                char *status, size_t status_size)
{
    int pad = ScaleUIPx(34);
    int available_w = (int)content.width - pad * 2;
    int col_w = available_w < ScaleUIPx(720) ? available_w : ScaleUIPx(720);
    int x0 = (int)content.x + ((int)content.width - col_w) / 2;
    int y = (int)content.y + pad;
    int gap = ScaleUIPx(14);
    int card_h = ScaleUIPx(64);
    int bottom_limit = (int)(content.y + content.height - pad);
    int cards_cols;
    int cards_col_w;

    DrawRectangleRec(content, GetThemeBackground());

    DrawUIText("Kryon IDE", x0, y, UI_TEXT_24, GetThemeText());
    y += ScaleUIPx(34);
    DrawUIText("Choose a project to begin, or start something new.",
               x0, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(28);

    if(new_project_requested != NULL &&
       DrawUIGenericButton(x0, y, col_w, ScaleUIPx(44), "New Project",
                           UI_BUTTON_STYLE_PRIMARY, 0, NULL))
        *new_project_requested = 1;
    y += ScaleUIPx(44) + ScaleUIPx(22);

    cards_cols = col_w / (ScaleUIPx(200) + gap);
    if(cards_cols < 1)
        cards_cols = 1;
    if(cards_cols > 3)
        cards_cols = 3;
    cards_col_w = (col_w - gap * (cards_cols - 1)) / cards_cols;

    DrawUIText("Recent Projects", x0, y, UI_TEXT_16, GetThemeIcon());
    y += ScaleUIPx(26);
    if(recent == NULL || recent->count == 0) {
        DrawUIText("No recent projects yet - open or create one.",
                   x0, y, UI_TEXT_12, GetThemeIcon());
        y += ScaleUIPx(24);
    } else {
        for(int i = 0; i < recent->count; i++) {
            int col = i % cards_cols;
            int row = i / cards_cols;
            int card_x = x0 + col * (cards_col_w + gap);
            int card_y = y + row * (card_h + gap);
            int action;

            if(card_y + card_h > bottom_limit)
                break;
            action = draw_recent_project_card(card_x, card_y, cards_col_w,
                                              card_h,
                                              path_basename(recent->paths[i]),
                                              recent->paths[i], i);
            if(action < 0) {
                if(editor_remove_recent_project(recent, i)) {
                    snprintf(status, status_size, "Removed recent project");
                    i--;
                }
            } else if(action > 0) {
                editor_open_project(project, recent->paths[i], recent,
                                    status, status_size);
                if(sidebar != NULL)
                    memset(sidebar, 0, sizeof(*sidebar));
            }
        }
        y += ((recent->count + cards_cols - 1) / cards_cols) *
             (card_h + gap);
    }

    y += ScaleUIPx(10);
    {
        char examples_dir[EDITOR_PATH_CAP];
        char ex_paths[EDITOR_MAX_EXAMPLES][EDITOR_PATH_CAP];
        char ex_titles[EDITOR_MAX_EXAMPLES][EDITOR_EXAMPLE_TITLE_CAP];
        int ex_count = 0;

        DrawUIText("Examples", x0, y, UI_TEXT_16, GetThemeIcon());
        y += ScaleUIPx(26);
        if(!editor_examples_dir(examples_dir, sizeof(examples_dir))) {
            DrawUIText("Examples not found - open the kryon repo to browse them.",
                       x0, y, UI_TEXT_12, GetThemeIcon());
            return;
        }
        ex_count = editor_collect_examples(examples_dir, ex_paths, ex_titles,
                                           EDITOR_MAX_EXAMPLES);
        if(ex_count == 0) {
            DrawUIText("No examples found.", x0, y, UI_TEXT_12,
                       GetThemeIcon());
            return;
        }
        for(int i = 0; i < ex_count; i++) {
            int col = i % cards_cols;
            int row = i / cards_cols;
            int card_x = x0 + col * (cards_col_w + gap);
            int card_y = y + row * (card_h + gap);

            if(card_y + card_h > bottom_limit)
                break;
            if(draw_start_card(card_x, card_y, cards_col_w, card_h,
                               ex_titles[i], ex_paths[i])) {
                editor_open_project(project, examples_dir, recent,
                                    status, status_size);
                if(project != NULL && project->loaded) {
                    editor_select_file(project, ex_paths[i]);
                    editor_select_source_path(project, ex_paths[i]);
                }
                if(sidebar != NULL)
                    memset(sidebar, 0, sizeof(*sidebar));
            }
        }
    }
}
