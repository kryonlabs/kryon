#include "flint_example_font.h"
#include "flint_file_dialog.h"
#include "flint_theme.h"
#include "flint_theme_meta.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    // Window setup
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint File Dialog Example");
    SetTargetFPS(60);
    flint_example_load_font();

    flint_theme_set_current(FLINT_THEME_SKY, 0);
    flint_file_dialog_set_theme_scope(flint_theme_scope_for(FLINT_THEME_SKY, false));

    FlintFileDialog dlg;
    flint_file_dialog_init(&dlg);

    printf("Flint File Dialog Example\n");
    printf("Press [L] to test Load dialog\n");
    printf("Press [S] to test Save dialog\n");
    printf("Press [ESC] to exit\n\n");

    char last_file[512] = {0};

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(flint_theme_get_bg());

        // Draw simple UI
        flint_text_draw("Flint File Dialog Example", 20, 20, 20, flint_theme_get_text());
        flint_text_draw("Press L for Load, S for Save, ESC to exit", 20, 50, 16, flint_theme_get_text());

        if(last_file[0] != 0) {
            flint_text_draw("Last selected file:", 20, 80, 16, flint_theme_get_text());
            flint_text_draw(last_file, 20, 100, 14, flint_theme_get_text());
        }

        // Handle keyboard input
        if(IsKeyPressed(KEY_L)) {
            printf("Opening load dialog...\n");
            if(flint_file_dialog_load(&dlg, "Load File")) {
                const char *path = flint_file_dialog_get_path(&dlg);
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
            if(flint_file_dialog_save(&dlg, "Save File", "example.txt")) {
                const char *path = flint_file_dialog_get_path(&dlg);
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

    flint_file_dialog_cleanup(&dlg);
    flint_example_unload_font();
    CloseWindow();

    return 0;
}
