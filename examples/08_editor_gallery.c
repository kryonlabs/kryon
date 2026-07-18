#include "example_ui_font.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui.h"
#include "flint.h"
#include <stdio.h>

int
main(void)
{
    const int screen_w = 1000;
    const int screen_h = 720;
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
    char text_value[128] = "Flint";
    char area_value[256] = "Edit this text area.\nThen drag the editor handles.";
    const char *options[] = {"Canvas", "Panels", "Controls"};

    InitWindow(screen_w, screen_h, "Flint Editor Gallery");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screen_w, screen_h, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);
    SetUIEditorEnabled(1);

    while(!WindowShouldClose()) {
        int view_w = GetScreenWidth();
        int view_h = GetScreenHeight();
        int x = ScaleUIPx(36);
        int y = ScaleUIPx(36);
        int w = ScaleUIPx(280);

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(view_w, view_h, GetUIScale());
        BeginUIEditorFrame(".");
        SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(),
                    GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(),
                    GetThemeIcon());

        DrawUIText("Flint Editor Gallery", x, y, 28, GetThemeText());
        y += ScaleUIPx(44);
        DrawUIText("Core widgets", x, y, 18, GetThemeText());
        y += ScaleUIPx(30);

        DrawUIGenericButton(x, y, w, ScaleUIPx(40), "Primary Button",
                            UI_BUTTON_STYLE_PRIMARY, 0, NULL);
        y += ScaleUIPx(52);
        DrawUIGenericButton(x, y, w, ScaleUIPx(40), "Secondary Button",
                            UI_BUTTON_STYLE_SECONDARY, 0, NULL);
        y += ScaleUIPx(56);

        DrawUITextField((UITextField){
            .bounds = {(float)x, (float)y, (float)w, (float)ScaleUIPx(42)},
            .text = text_value,
            .text_size = sizeof(text_value),
            .cursor_position = &text_cursor,
            .focused = &text_focused,
            .focus_id = 101,
            .style = {
                .background = GetThemeSurface(),
                .border = DarkenUIColor(GetThemeSurface(), 40),
                .text = GetThemeText(),
                .cursor = GetThemeText(),
                .radius = 0.08f
            }
        });
        y += ScaleUIPx(58);

        DrawUIDropdownButton(201, x, y, w, ScaleUIPx(38),
                             options, 3, &dropdown_index);
        y += ScaleUIPx(58);

        DrawUISlider(301, x, y, w, "Horizontal slider", 0, 100,
                     &slider_value, "%");
        y += ScaleUIPx(76);

        DrawUIToggleSwitch(x, y, ScaleUIPx(180), ScaleUIPx(38),
                           &toggle_value, "Off", "On");
        y += ScaleUIPx(56);
        DrawUICheckboxToggle(x, y, "Checkbox", &checkbox_value);

        x = view_w / 2;
        y = ScaleUIPx(86);
        DrawUITextArea((UITextArea){
            .bounds = {(float)x, (float)y, (float)ScaleUIPx(360), (float)ScaleUIPx(150)},
            .text = area_value,
            .text_size = sizeof(area_value),
            .cursor_position = &area_cursor,
            .focused = &area_focused,
            .scroll_y = &area_scroll,
            .focus_id = 102,
            .placeholder = "Text area",
            .style = {
                .background = GetThemeSurface(),
                .border = DarkenUIColor(GetThemeSurface(), 40),
                .text = GetThemeText(),
                .cursor = GetThemeText(),
                .radius = 0.08f
            }
        });
        y += ScaleUIPx(176);

        DrawUIReadonlyTextBox((UIReadonlyTextBox){
            .bounds = {(float)x, (float)y, (float)ScaleUIPx(360), (float)ScaleUIPx(96)},
            .text = "Readonly boxes, buttons, dropdowns, sliders, toggles, and text inputs are all visible to the editor overlay.",
            .style = {
                .background = GetThemeSurface(),
                .border = DarkenUIColor(GetThemeSurface(), 40),
                .text = GetThemeText(),
                .radius = 0.08f
            },
            .line_gap = ScaleUIPx(3)
        });
        y += ScaleUIPx(136);

        DrawUIVerticalSlider(401, x + ScaleUIPx(24), y,
                             ScaleUIPx(160), 0, 100, &vertical_value);
        DrawUIText("Vertical slider", x + ScaleUIPx(56), y + ScaleUIPx(64),
                   GetUIFontSize(), GetThemeText());

        DrawUIDropdownMenu(201);
        DrawUIEditorOverlay();
        EndUIFocus();
        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
