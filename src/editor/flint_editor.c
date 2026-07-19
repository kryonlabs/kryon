#include "file_dialog.h"
#include "flint.h"
#include "theme.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

enum {
    EDITOR_PATH_CAP = FILE_DIALOG_PATH_MAX,
};

typedef struct EditorProject {
    char path[EDITOR_PATH_CAP];
    char name[96];
    int loaded;
    int selected_screen;
    FlintEditorHost *host;
} EditorProject;

static void
load_editor_font(void)
{
    Font font;

    font = LoadUIFontAsset("fonts/noto/NotoSans-Regular.ttf",
                           UI_TEXT_BASE_SIZE);
    if(font.texture.id == 0)
        font = LoadUIFontAsset("../fonts/noto/NotoSans-Regular.ttf",
                               UI_TEXT_BASE_SIZE);
    if(font.texture.id != 0) {
        RegisterUIFont("default", font);
        UseUIFont("default");
    }
}

static const char *
path_basename(const char *path)
{
    const char *base = path;

    if(path == NULL || path[0] == '\0')
        return "";
    for(const char *p = path; *p != '\0'; p++) {
        if(*p == '/' && p[1] != '\0')
            base = p + 1;
    }
    return base;
}

static UITextInputStyle
editor_input_style(void)
{
    return (UITextInputStyle){
        .background = GetThemeSurface(),
        .border = DarkenUIColor(GetThemeSurface(), 40),
        .focus_border = GetThemeButtonHover(),
        .text = GetThemeText(),
        .cursor = GetThemeText(),
        .radius = 0.08f
    };
}

static void
editor_set_project(EditorProject *project, const char *path)
{
    if(project == NULL || path == NULL || path[0] == '\0')
        return;

    memset(project, 0, sizeof(*project));
    snprintf(project->path, sizeof(project->path), "%s", path);
    snprintf(project->name, sizeof(project->name), "%s", path_basename(path));
    project->loaded = 1;
}

static void
draw_panel(Rectangle bounds, const char *title)
{
    DrawRectangleRec(bounds, GetThemeSurface());
    DrawRectangleLinesEx(bounds, 1, DarkenUIColor(GetThemeSurface(), 36));
    if(title != NULL && title[0] != '\0')
        DrawUIText(title, (int)bounds.x + ScaleUIPx(14),
                   (int)bounds.y + ScaleUIPx(12), UI_TEXT_16,
                   GetThemeText());
}

static int
draw_menu_button(int x, int y, const char *label, int active)
{
    return DrawUIGenericButton(x, y, ScaleUIPx(78), ScaleUIPx(28), label,
                               active ? UI_BUTTON_STYLE_PRIMARY
                                      : UI_BUTTON_STYLE_SECONDARY,
                               0, NULL);
}

static void
draw_project_menu(int x, int y, int *open, FileDialog *project_dialog)
{
    int w = ScaleUIPx(188);
    int item_h = ScaleUIPx(32);

    if(open == NULL || !*open)
        return;
    DrawRectangle(x, y, w, item_h * 3 + ScaleUIPx(10), GetThemeSurface());
    DrawRectangleLines(x, y, w, item_h * 3 + ScaleUIPx(10),
                       DarkenUIColor(GetThemeSurface(), 38));
    y += ScaleUIPx(5);
    if(DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                           "Open Project", UI_BUTTON_STYLE_PRIMARY, 0, NULL)) {
        BeginSelectFileDialogFolder(project_dialog, "Open Flint Project");
        *open = 0;
    }
    y += item_h;
    DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                        "New Project", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    y += item_h;
    DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                        "Save", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
}

static void
draw_left_sidebar(Rectangle bounds, EditorProject *project)
{
    int x = (int)bounds.x + ScaleUIPx(14);
    int y = (int)bounds.y + ScaleUIPx(48);
    const char *last_group = NULL;
    int screen_count;

    draw_panel(bounds, project->loaded ? project->name : "Project");
    DrawUIText("Screens", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(22);

    if(!project->loaded) {
        DrawUIText("Open a Flint project.", x, y, UI_TEXT_16, GetThemeIcon());
        return;
    }
    if(project->host == NULL || project->host->screen_count == NULL ||
       project->host->screen == NULL) {
        DrawUIText("No Flint editor host found.", x, y, UI_TEXT_16,
                   GetThemeIcon());
        y += ScaleUIPx(30);
        DrawUIText("The app must expose screens to the editor.", x, y,
                   UI_TEXT_12, GetThemeIcon());
        return;
    }

    screen_count = project->host->screen_count(project->host->userdata);
    for(int i = 0; i < screen_count; i++) {
        FlintEditorScreen screen = project->host->screen(project->host->userdata, i);
        const char *group = screen.group != NULL ? screen.group : "";
        const char *title = screen.title != NULL ? screen.title : screen.id;
        if(title == NULL)
            title = "Screen";
        if(last_group == NULL || strcmp(last_group, group) != 0) {
            DrawUIText(group, x, y, UI_TEXT_12, GetThemeText());
            y += ScaleUIPx(20);
            last_group = group;
        }
        if(DrawUIGenericButton(x, y, (int)bounds.width - ScaleUIPx(28),
                               ScaleUIPx(34), title,
                               project->selected_screen == i
                                   ? UI_BUTTON_STYLE_PRIMARY
                                   : UI_BUTTON_STYLE_SECONDARY,
                               0, NULL)) {
            project->selected_screen = i;
            if(project->host->select_screen != NULL)
                project->host->select_screen(project->host->userdata, i);
        }
        y += ScaleUIPx(42);
    }
}

static void
draw_canvas(Rectangle canvas, const EditorProject *project)
{
    int x = (int)canvas.x + ScaleUIPx(34);
    int y = (int)canvas.y + ScaleUIPx(34);

    DrawRectangleRec(canvas, DarkenUIColor(GetThemeBackground(), 4));
    DrawRectangleLinesEx(canvas, 1, DarkenUIColor(GetThemeBackground(), 38));
    if(!project->loaded) {
        DrawUIText("Open a Flint project", x, y, UI_TEXT_24, GetThemeText());
        y += ScaleUIPx(40);
        DrawUIText("Use Project / Open Project to choose an app folder.",
                   x, y, UI_TEXT_16, GetThemeIcon());
        return;
    }
    if(project->host == NULL || project->host->draw == NULL) {
        DrawUIText("Editor host required", x, y, UI_TEXT_24, GetThemeText());
        y += ScaleUIPx(40);
        DrawUIText("Flint will render the real app here after the project exposes its editor host.",
                   x, y, UI_TEXT_16, GetThemeIcon());
        return;
    }
    project->host->draw(project->host->userdata, canvas);
}

static void
draw_top_bar(int view_w, int *project_menu_open, int *edit_menu_open,
             int *view_menu_open, FileDialog *project_dialog,
             const EditorProject *project)
{
    int top_h = ScaleUIPx(54);
    int path_w = view_w - ScaleUIPx(874);

    if(path_w < ScaleUIPx(180))
        path_w = ScaleUIPx(180);

    DrawRectangle(0, 0, view_w, top_h, DarkenUIColor(GetThemeBackground(), 12));
    DrawLine(0, top_h - 1, view_w, top_h - 1,
             DarkenUIColor(GetThemeBackground(), 42));
    DrawUIText("Flint Editor", ScaleUIPx(14), ScaleUIPx(14), UI_TEXT_24,
               GetThemeText());
    if(draw_menu_button(ScaleUIPx(150), ScaleUIPx(13), "Project", *project_menu_open)) {
        *project_menu_open = !*project_menu_open;
        *edit_menu_open = 0;
        *view_menu_open = 0;
    }
    if(draw_menu_button(ScaleUIPx(234), ScaleUIPx(13), "Edit", *edit_menu_open)) {
        *edit_menu_open = !*edit_menu_open;
        *project_menu_open = 0;
        *view_menu_open = 0;
    }
    if(draw_menu_button(ScaleUIPx(318), ScaleUIPx(13), "View", *view_menu_open)) {
        *view_menu_open = !*view_menu_open;
        *project_menu_open = 0;
        *edit_menu_open = 0;
    }
    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)ScaleUIPx(410), (float)ScaleUIPx(9),
                   (float)path_w, (float)ScaleUIPx(36)},
        .text = project->loaded ? project->path : "",
        .style = editor_input_style()
    });
    if(DrawUIGenericButton(view_w - ScaleUIPx(452), ScaleUIPx(10),
                           ScaleUIPx(116), ScaleUIPx(34), "Open Project",
                           UI_BUTTON_STYLE_PRIMARY, 0, NULL))
        BeginSelectFileDialogFolder(project_dialog, "Open Flint Project");
    DrawUIGenericButton(view_w - ScaleUIPx(156), ScaleUIPx(10),
                        ScaleUIPx(64), ScaleUIPx(34), "Run",
                        UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    DrawUIGenericButton(view_w - ScaleUIPx(84), ScaleUIPx(10),
                        ScaleUIPx(64), ScaleUIPx(34), "Save",
                        UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    draw_project_menu(ScaleUIPx(150), top_h - ScaleUIPx(2),
                      project_menu_open, project_dialog);
}

static void
draw_chrome(int view_w, int view_h, Rectangle canvas, EditorProject *project,
            int *preview_enabled, int *project_menu_open, int *edit_menu_open,
            int *view_menu_open, FileDialog *project_dialog,
            const char *status_text)
{
    int chrome = PushUIEditorChrome(1);
    int top_h = ScaleUIPx(54);
    int side_w = ScaleUIPx(240);
    int inspector_w = ScaleUIPx(270);
    int bottom_h = ScaleUIPx(34);
    int x;
    int y;
    char detail[128];

    draw_top_bar(view_w, project_menu_open, edit_menu_open, view_menu_open,
                 project_dialog, project);
    draw_left_sidebar((Rectangle){0, (float)top_h, (float)side_w,
                                  (float)(view_h - top_h - bottom_h)},
                      project);

    draw_panel((Rectangle){(float)(view_w - inspector_w), (float)top_h,
                           (float)inspector_w,
                           (float)(view_h - top_h - bottom_h)}, "Inspector");
    x = view_w - inspector_w + ScaleUIPx(14);
    y = top_h + ScaleUIPx(48);
    DrawUIText("Selection", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(24);
    DrawUIText(project->host != NULL ? "App screen" : "No screen",
               x, y, UI_TEXT_16, GetThemeText());
    y += ScaleUIPx(28);
    snprintf(detail, sizeof(detail), "Canvas %.0fx%.0f",
             canvas.width, canvas.height);
    DrawUIText(detail, x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(28);
    DrawUICheckboxToggle(x, y, "Preview interactions", preview_enabled);

    DrawRectangle(0, view_h - bottom_h, view_w, bottom_h,
                  DarkenUIColor(GetThemeBackground(), 14));
    DrawLine(0, view_h - bottom_h, view_w, view_h - bottom_h,
             DarkenUIColor(GetThemeBackground(), 42));
    DrawUIText(status_text, ScaleUIPx(14), view_h - bottom_h + ScaleUIPx(9),
               UI_TEXT_12, GetThemeIcon());
    DrawUIText("Root Flint editor. Real app rendering requires a Flint editor host.",
               side_w + ScaleUIPx(18), view_h - bottom_h + ScaleUIPx(9),
               UI_TEXT_12, GetThemeIcon());
    PopUIEditorChrome(chrome);
}

int
main(int argc, char **argv)
{
    const int screen_w = 1180;
    const int screen_h = 760;
    int preview_enabled = 0;
    int project_menu_open = 0;
    int edit_menu_open = 0;
    int view_menu_open = 0;
    FileDialog project_dialog;
    EditorProject project = {0};
    char status_text[160] = "Ready";

    InitWindow(screen_w, screen_h, "Flint Editor");
    SetTargetFPS(60);
    load_editor_font();
    InitUI(screen_w, screen_h, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);
    SetUIEditorEnabled(1);
    InitFileDialog(&project_dialog);

    if(argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        editor_set_project(&project, argv[1]);
        snprintf(status_text, sizeof(status_text), "Opened %s", project.name);
    }
    SetFileDialogCurrentDir(&project_dialog,
                            project.loaded ? project.path : GetWorkingDirectory());

    while(!WindowShouldClose()) {
        int view_w = GetScreenWidth();
        int view_h = GetScreenHeight();
        int top_h = ScaleUIPx(54);
        int left_w = ScaleUIPx(240);
        int right_w = ScaleUIPx(270);
        int bottom_h = ScaleUIPx(34);
        int dialog_result;
        Rectangle canvas = {
            (float)left_w,
            (float)top_h,
            (float)(view_w - left_w - right_w),
            (float)(view_h - top_h - bottom_h)
        };

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(view_w, view_h, GetUIScale());
        BeginUIEditorFrame(project.loaded ? project.path : ".");
        SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(),
                    GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(),
                    GetThemeIcon());
        dialog_result = UpdateFileDialog(&project_dialog);
        if(dialog_result == 1) {
            editor_set_project(&project, GetFileDialogPath(&project_dialog));
            SetFileDialogCurrentDir(&project_dialog, project.path);
            snprintf(status_text, sizeof(status_text), "Opened %s", project.name);
        }

        SetUIEditorCanvasBounds(canvas);
        draw_canvas(canvas, &project);
        DrawUIEditorOverlay();
        draw_chrome(view_w, view_h, canvas, &project, &preview_enabled,
                    &project_menu_open, &edit_menu_open, &view_menu_open,
                    &project_dialog, status_text);

        EndUIFocus();
        EndDrawing();
    }

    ClearUIFonts();
    CloseFileDialog(&project_dialog);
    CloseWindow();
    return 0;
}
