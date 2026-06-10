#include "flint_file_dialog.h"
#include "flint_theme.h"
#include "flint_theme_meta.h"
#include <stdio.h>
#include <raylib.h>

static void populate_theme_from_catalog(FlintThemeId theme_id, bool dark_mode) {
    const char *scope = flint_theme_scope_for(theme_id, dark_mode);

    // Populate all theme colors from catalog
    Color color;
    if(flint_theme_catalog_color(theme_id, dark_mode, "background", &color))
        flint_theme_set_color(scope, "background", color);
    if(flint_theme_catalog_color(theme_id, dark_mode, "text", &color))
        flint_theme_set_color(scope, "text", color);
    if(flint_theme_catalog_color(theme_id, dark_mode, "circle", &color))
        flint_theme_set_color(scope, "circle", color);
    if(flint_theme_catalog_color(theme_id, dark_mode, "button", &color))
        flint_theme_set_color(scope, "button", color);
    if(flint_theme_catalog_color(theme_id, dark_mode, "button_hover", &color))
        flint_theme_set_color(scope, "button_hover", color);
    if(flint_theme_catalog_color(theme_id, dark_mode, "icon", &color))
        flint_theme_set_color(scope, "icon", color);
    if(flint_theme_catalog_color(theme_id, dark_mode, "link", &color))
        flint_theme_set_color(scope, "link", color);
}

int main(void) {
    // Window setup
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint File Dialog Example");
    SetTargetFPS(60);

    // Register theme scopes
    flint_theme_register_defaults("");

    // Populate all themes from catalog
    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        populate_theme_from_catalog((FlintThemeId)i, false);
        populate_theme_from_catalog((FlintThemeId)i, true);
    }

    FlintFileDialog dlg;
    flint_file_dialog_init(&dlg);

    // Set theme parameters (Sky light mode as example)
    dlg.theme_id = FLINT_THEME_SKY;
    dlg.dark_mode = 0;

    printf("Flint File Dialog Example\n");
    printf("Press [L] to test Load dialog\n");
    printf("Press [S] to test Save dialog\n");
    printf("Press [ESC] to exit\n\n");

    char last_file[512] = {0};

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw simple UI
        DrawText("Flint File Dialog Example", 20, 20, 20, BLACK);
        DrawText("Press L for Load, S for Save, ESC to exit", 20, 50, 16, DARKGRAY);

        if(last_file[0] != 0) {
            DrawText("Last selected file:", 20, 80, 16, BLACK);
            DrawText(last_file, 20, 100, 14, DARKGRAY);
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
    CloseWindow();

    return 0;
}
