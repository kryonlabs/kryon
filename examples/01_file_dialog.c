#include "example_ui_font.h"
#include "file_dialog.h"
#include "theme.h"
#include "theme_meta.h"
#include <stdio.h>
#include "flint.h"

int main(void) {
    // Window setup
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint File Dialog Example");
    SetTargetFPS(60);
    LoadExampleUIFont();

    SetCurrentTheme(THEME_SKY, 0);
    SetFileDialogThemeScope(GetThemeScopeName(THEME_SKY, false));

    FileDialog dlg;
    InitFileDialog(&dlg);

    printf("Flint File Dialog Example\n");
    printf("Press [L] to test Load dialog\n");
    printf("Press [S] to test Save dialog\n");
    printf("Press [ESC] to exit\n\n");

    char last_file[512] = {0};

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());

        // Draw simple UI
        DrawUIText("Flint File Dialog Example", 20, 20, 20, GetThemeText());
        DrawUIText("Press L for Load, S for Save, ESC to exit", 20, 50, 16, GetThemeText());

        if(last_file[0] != 0) {
            DrawUIText("Last selected file:", 20, 80, 16, GetThemeText());
            DrawUIText(last_file, 20, 100, 14, GetThemeText());
        }

        // Handle keyboard input
        if(IsKeyPressed(KEY_L)) {
            printf("Opening load dialog...\n");
            if(LoadFileDialog(&dlg, "Load File")) {
                const char *path = GetFileDialogPath(&dlg);
                if(path) {
                    printf("Selected file: %s\n", path);
                    snprintf(last_file, sizeof(last_file), "%s", path);
                }
            } else {
                printf("Load dialog cancelled\n");
            }
            WaitTime(0.3); // Debounce
        }

        if(IsKeyPressed(KEY_S)) {
            printf("Opening save dialog...\n");
            if(SaveFileDialog(&dlg, "Save File", "example.txt")) {
                const char *path = GetFileDialogPath(&dlg);
                if(path) {
                    printf("Save to: %s\n", path);
                    snprintf(last_file, sizeof(last_file), "%s", path);
                }
            } else {
                printf("Save dialog cancelled\n");
            }
            WaitTime(0.3); // Debounce
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
