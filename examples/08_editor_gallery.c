#include "example_ui_font.h"
#include "file_dialog.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui.h"
#include "kryon.h"
#include <stdio.h>
#include <string.h>

typedef enum EditorTool {
    EDITOR_TOOL_SELECT,
    EDITOR_TOOL_BUTTON,
    EDITOR_TOOL_INPUT,
    EDITOR_TOOL_SLIDER,
    EDITOR_TOOL_TOGGLE,
    EDITOR_TOOL_TEXT
} EditorTool;

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
draw_palette_button(int x, int *y, int w, const char *label, int active)
{
    UIButtonStyle style = active ? UI_BUTTON_STYLE_PRIMARY
                                 : UI_BUTTON_STYLE_SECONDARY;
    int clicked = DrawUIGenericButton(x, *y, w, ScaleUIPx(34), label,
                                      style, 0, NULL);
    *y += ScaleUIPx(42);
    return clicked;
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
    DrawRectangle(x, y, w, item_h * 4 + ScaleUIPx(10), GetThemeSurface());
    DrawRectangleLines(x, y, w, item_h * 4 + ScaleUIPx(10),
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
                        "Save Layout", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    y += item_h;
    DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                        "Reveal Folder", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
}

static void
draw_editor_chrome(int view_w, int view_h, Rectangle canvas, EditorTool *tool,
                   int *view_mode, int *preview_enabled, int *project_menu_open,
                   int *edit_menu_open, int *view_menu_open,
                   FileDialog *project_dialog, const char *project_path)
{
    int chrome = PushUIInspectChrome(1);
    int top_h = ScaleUIPx(54);
    int side_w = ScaleUIPx(228);
    int inspector_w = ScaleUIPx(270);
    int bottom_h = ScaleUIPx(34);
    int y;
    int x;
    char detail[128];
    const char *modes[] = {"Design", "Preview", "Assets"};

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
                   (float)(view_w - ScaleUIPx(874)), (float)ScaleUIPx(36)},
        .text = project_path,
        .style = editor_input_style()
    });
    if(DrawUIGenericButton(view_w - ScaleUIPx(452), ScaleUIPx(10),
                           ScaleUIPx(116), ScaleUIPx(34), "Open Project",
                           UI_BUTTON_STYLE_PRIMARY, 0, NULL))
        BeginSelectFileDialogFolder(project_dialog, "Open Flint Project");
    DrawUIDropdownButton(801, view_w - ScaleUIPx(326), ScaleUIPx(10),
                         ScaleUIPx(160), ScaleUIPx(34),
                         modes, 3, view_mode);
    DrawUIGenericButton(view_w - ScaleUIPx(156), ScaleUIPx(10),
                        ScaleUIPx(64), ScaleUIPx(34), "Run",
                        UI_BUTTON_STYLE_PRIMARY, 0, NULL);
    DrawUIGenericButton(view_w - ScaleUIPx(84), ScaleUIPx(10),
                        ScaleUIPx(64), ScaleUIPx(34), "Save",
                        UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    draw_project_menu(ScaleUIPx(150), top_h - ScaleUIPx(2),
                      project_menu_open, project_dialog);

    draw_panel((Rectangle){0, (float)top_h, (float)side_w,
                           (float)(view_h - top_h - bottom_h)}, "Palette");
    y = top_h + ScaleUIPx(48);
    x = ScaleUIPx(14);
    DrawUIText("Tools", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(22);
    if(draw_palette_button(x, &y, side_w - ScaleUIPx(28), "Select", *tool == EDITOR_TOOL_SELECT))
        *tool = EDITOR_TOOL_SELECT;
    if(draw_palette_button(x, &y, side_w - ScaleUIPx(28), "Button", *tool == EDITOR_TOOL_BUTTON))
        *tool = EDITOR_TOOL_BUTTON;
    if(draw_palette_button(x, &y, side_w - ScaleUIPx(28), "Text Field", *tool == EDITOR_TOOL_INPUT))
        *tool = EDITOR_TOOL_INPUT;
    if(draw_palette_button(x, &y, side_w - ScaleUIPx(28), "Slider", *tool == EDITOR_TOOL_SLIDER))
        *tool = EDITOR_TOOL_SLIDER;
    if(draw_palette_button(x, &y, side_w - ScaleUIPx(28), "Toggle", *tool == EDITOR_TOOL_TOGGLE))
        *tool = EDITOR_TOOL_TOGGLE;
    if(draw_palette_button(x, &y, side_w - ScaleUIPx(28), "Text Box", *tool == EDITOR_TOOL_TEXT))
        *tool = EDITOR_TOOL_TEXT;

    y += ScaleUIPx(16);
    DrawUIText("Project", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(22);
    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)x, (float)y, (float)(side_w - ScaleUIPx(28)), (float)ScaleUIPx(94)},
        .text = project_path,
        .style = editor_input_style(),
        .line_gap = ScaleUIPx(3)
    });

    draw_panel((Rectangle){(float)(view_w - inspector_w), (float)top_h,
                           (float)inspector_w,
                           (float)(view_h - top_h - bottom_h)}, "Inspector");
    x = view_w - inspector_w + ScaleUIPx(14);
    y = top_h + ScaleUIPx(48);
    DrawUIText("Selection", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(24);
    DrawUIText("Canvas widget", x, y, UI_TEXT_16, GetThemeText());
    y += ScaleUIPx(28);
    snprintf(detail, sizeof(detail), "Canvas %.0fx%.0f", canvas.width, canvas.height);
    DrawUIText(detail, x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(28);
    DrawUICheckboxToggle(x, y, "Preview interactions", preview_enabled);
    y += ScaleUIPx(46);
    DrawUIText("Position", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(24);
    DrawUIGenericButton(x, y, ScaleUIPx(112), ScaleUIPx(34), "Align Left",
                        UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    DrawUIGenericButton(x + ScaleUIPx(124), y, ScaleUIPx(112), ScaleUIPx(34),
                        "Center", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    y += ScaleUIPx(50);
    DrawUIText("Theme", x, y, UI_TEXT_12, GetThemeIcon());
    y += ScaleUIPx(24);
    DrawUIGenericButton(x, y, ScaleUIPx(112), ScaleUIPx(34), "Light",
                        UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    DrawUIGenericButton(x + ScaleUIPx(124), y, ScaleUIPx(112), ScaleUIPx(34),
                        "Dark", UI_BUTTON_STYLE_SECONDARY, 0, NULL);

    DrawRectangle(0, view_h - bottom_h, view_w, bottom_h,
                  DarkenUIColor(GetThemeBackground(), 14));
    DrawLine(0, view_h - bottom_h, view_w, view_h - bottom_h,
             DarkenUIColor(GetThemeBackground(), 42));
    DrawUIText("Ready", ScaleUIPx(14), view_h - bottom_h + ScaleUIPx(9),
               UI_TEXT_12, GetThemeIcon());
    DrawUIText("Drag widgets in the canvas. Ctrl+S saves .flint/editor_layout.ini.",
               side_w + ScaleUIPx(18), view_h - bottom_h + ScaleUIPx(9),
               UI_TEXT_12, GetThemeIcon());

    DrawUIDropdownMenu(801);
    PopUIInspectChrome(chrome);
}

static void
draw_canvas(Rectangle canvas, int *slider_value, int *vertical_value,
            int *toggle_value, int *checkbox_value, int *dropdown_index,
            char *text_value, size_t text_size, int *text_cursor,
            int *text_focused, char *area_value, size_t area_size,
            int *area_cursor, int *area_focused, int *area_scroll)
{
    int x = (int)canvas.x + ScaleUIPx(34);
    int y = (int)canvas.y + ScaleUIPx(54);
    int w = ScaleUIPx(280);
    const char *options[] = {"Canvas", "Panels", "Controls"};

    DrawRectangleRec(canvas, DarkenUIColor(GetThemeBackground(), 4));
    DrawRectangleLinesEx(canvas, 1, DarkenUIColor(GetThemeBackground(), 38));
    DrawUIText("Home Screen", x, (int)canvas.y + ScaleUIPx(20), UI_TEXT_24,
               GetThemeText());

    DrawUIGenericButton(x, y, w, ScaleUIPx(40), "Primary Button",
                        UI_BUTTON_STYLE_PRIMARY, 0, NULL);
    y += ScaleUIPx(52);
    DrawUIGenericButton(x, y, w, ScaleUIPx(40), "Secondary Button",
                        UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    y += ScaleUIPx(56);

    DrawUITextField((UITextField){
        .bounds = {(float)x, (float)y, (float)w, (float)ScaleUIPx(42)},
        .text = text_value,
        .text_size = text_size,
        .cursor_position = text_cursor,
        .focused = text_focused,
        .focus_id = 101,
        .style = editor_input_style()
    });
    y += ScaleUIPx(58);

    DrawUIDropdownButton(201, x, y, w, ScaleUIPx(38),
                         options, 3, dropdown_index);
    y += ScaleUIPx(58);

    DrawUISlider(301, x, y, w, "Horizontal slider", 0, 100,
                 slider_value, "%");
    y += ScaleUIPx(76);

    DrawUIToggleSwitch(x, y, ScaleUIPx(180), ScaleUIPx(38),
                       toggle_value, "Off", "On");
    y += ScaleUIPx(56);
    DrawUICheckboxToggle(x, y, "Checkbox", checkbox_value);

    x = (int)(canvas.x + canvas.width * 0.52f);
    y = (int)canvas.y + ScaleUIPx(82);
    DrawUITextArea((UITextArea){
        .bounds = {(float)x, (float)y, (float)ScaleUIPx(360), (float)ScaleUIPx(150)},
        .text = area_value,
        .text_size = area_size,
        .cursor_position = area_cursor,
        .focused = area_focused,
        .scroll_y = area_scroll,
        .focus_id = 102,
        .placeholder = "Text area",
        .style = editor_input_style()
    });
    y += ScaleUIPx(176);

    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)x, (float)y, (float)ScaleUIPx(360), (float)ScaleUIPx(96)},
        .text = "This is the editable design canvas. Flint widgets register themselves with the editor overlay.",
        .style = editor_input_style(),
        .line_gap = ScaleUIPx(3)
    });
    y += ScaleUIPx(136);

    DrawUIVerticalSlider(401, x + ScaleUIPx(24), y,
                         ScaleUIPx(150), 0, 100, vertical_value);
    DrawUIText("Vertical slider", x + ScaleUIPx(56), y + ScaleUIPx(60),
               GetUIFontSize(), GetThemeText());
    DrawUIDropdownMenu(201);
}

int
main(void)
{
    const int screen_w = 1180;
    const int screen_h = 760;
    EditorTool tool = EDITOR_TOOL_SELECT;
    int view_mode = 0;
    int preview_enabled = 0;
    int slider_value = 42;
    int vertical_value = 60;
    int toggle_value = 1;
    int checkbox_value = 1;
    int dropdown_index = 0;
    int text_cursor = 5;
    int text_focused = 0;
    int area_cursor = 12;
    int area_focused = 0;
    int area_scroll = 0;
    int project_menu_open = 0;
    int edit_menu_open = 0;
    int view_menu_open = 0;
    FileDialog project_dialog;
    char project_path[FILE_DIALOG_PATH_MAX] = "/home/wao/src/flint";
    char text_value[128] = "Flint";
    char area_value[256] = "Edit this text area.\nThen drag the editor handles.";

    InitWindow(screen_w, screen_h, "Flint Editor");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screen_w, screen_h, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);
    SetUIInspectEnabled(1);
    InitFileDialog(&project_dialog);
    SetFileDialogCurrentDir(&project_dialog, project_path);

    while(!WindowShouldClose()) {
        int view_w = GetScreenWidth();
        int view_h = GetScreenHeight();
        int top_h = ScaleUIPx(54);
        int left_w = ScaleUIPx(228);
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
        BeginUIInspectFrame(".");
        dialog_result = UpdateFileDialog(&project_dialog);
        if(dialog_result == 1) {
            snprintf(project_path, sizeof(project_path), "%s",
                     GetFileDialogPath(&project_dialog));
            SetFileDialogCurrentDir(&project_dialog, project_path);
        }
        SetUIInspectCanvasBounds(canvas);

        draw_canvas(canvas, &slider_value, &vertical_value, &toggle_value,
                    &checkbox_value, &dropdown_index,
                    text_value, sizeof(text_value), &text_cursor,
                    &text_focused, area_value, sizeof(area_value),
                    &area_cursor, &area_focused, &area_scroll);
        DrawUIInspectOverlay();
        draw_editor_chrome(view_w, view_h, canvas, &tool, &view_mode,
                           &preview_enabled, &project_menu_open,
                           &edit_menu_open, &view_menu_open,
                           &project_dialog, project_path);

        EndUIFocus();
        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseFileDialog(&project_dialog);
    CloseWindow();
    return 0;
}
