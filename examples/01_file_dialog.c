#include "example_ui_font.h"
#include "file_dialog.h"
#include "theme.h"
#include "theme_meta.h"
#include <stdio.h>
#include "kryon.h"

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint File Dialog Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screenWidth, screenHeight, GetUIScale());

    SetCurrentTheme(THEME_SKY, 0);
    SetFileDialogThemeScope(GetThemeScopeName(THEME_SKY, false));

    FileDialog dlg;
    InitFileDialog(&dlg);

    printf("Flint File Dialog Example\n");
    printf("Click the buttons to test load, save, and folder dialogs\n\n");

    char last_path[512] = {0};
    char status[256] = "Choose an action.";

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        int x = ScaleUIPx(32);
        int y = ScaleUIPx(28);
        int button_w = ScaleUIPx(180);
        int button_h = ScaleUIPx(42);
        int gap = ScaleUIPx(12);
        int hover = 0;

        char backend_text[160];
        snprintf(backend_text, sizeof(backend_text), "Backend: %s", GetFileDialogBackendName());

        DrawUIText("Flint File Dialog", x, y, 24, GetThemeText());
        y += ScaleUIPx(38);
        DrawUIText(backend_text, x, y, 15, GetThemeText());
        y += ScaleUIPx(40);

        if(DrawUIGenericButton(x, y, button_w, button_h, "Open File",
                               UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            printf("Opening load dialog...\n");
            if(LoadFileDialog(&dlg, "Load File")) {
                const char *path = GetFileDialogPath(&dlg);
                if(path) {
                    printf("Selected file: %s\n", path);
                    snprintf(last_path, sizeof(last_path), "%s", path);
                    snprintf(status, sizeof(status), "Selected file.");
                }
            } else {
                snprintf(status, sizeof(status), "Open file cancelled or unavailable.");
                printf("%s\n", status);
            }
        }

        hover = 0;
        if(DrawUIGenericButton(x + button_w + gap, y, button_w, button_h, "Save File",
                               UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            printf("Opening save dialog...\n");
            if(SaveFileDialog(&dlg, "Save File", "example.txt")) {
                const char *path = GetFileDialogPath(&dlg);
                if(path) {
                    printf("Save to: %s\n", path);
                    snprintf(last_path, sizeof(last_path), "%s", path);
                    snprintf(status, sizeof(status), "Selected save path.");
                }
            } else {
                snprintf(status, sizeof(status), "Save file cancelled or unavailable.");
                printf("%s\n", status);
            }
        }

        hover = 0;
        if(DrawUIGenericButton(x + (button_w + gap) * 2, y, button_w, button_h,
                               "Select Folder", UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            printf("Opening folder dialog...\n");
            if(SelectFileDialogFolder(&dlg, "Select Folder")) {
                const char *path = GetFileDialogPath(&dlg);
                if(path) {
                    printf("Selected folder: %s\n", path);
                    snprintf(last_path, sizeof(last_path), "%s", path);
                    snprintf(status, sizeof(status), "Selected folder.");
                }
            } else {
                snprintf(status, sizeof(status), "Folder dialog cancelled or unavailable.");
                printf("%s\n", status);
            }
        }

        y += button_h + ScaleUIPx(36);
        DrawUIText("Status", x, y, 18, GetThemeText());
        y += ScaleUIPx(28);
        DrawUIText(status, x, y, 16, GetThemeText());

        if(last_path[0] != '\0') {
            y += ScaleUIPx(34);
            DrawUIText("Last selected path", x, y, 18, GetThemeText());
            y += ScaleUIPx(28);
            DrawUIText(last_path, x, y, 14, GetThemeText());
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        EndDrawing();
    }

    CloseFileDialog(&dlg);
    UnloadExampleUIFont();
    CloseWindow();

    return 0;
}
