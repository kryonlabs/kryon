#include "flint_theme_meta.h"
#include "flint_scaling.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint Theme Example");
    SetTargetFPS(60);

    printf("Flint Theme System Example\n");
    printf("=========================\n\n");
    printf("This example demonstrates Flint theme catalog:\n");
    printf("  - flint_theme_catalog_color() - Get theme colors\n");
    printf("  - flint_theme_scope_for() - Get theme scope name\n");
    printf("  - FlintThemeId enum - All available themes\n\n");
    printf("Press SPACE to cycle through themes\n");
    printf("Press D to toggle light/dark mode\n");
    printf("Press ESC to exit\n\n");

    // Available themes
    FlintThemeId themes[] = {
        FLINT_THEME_SKY,
        FLINT_THEME_OCEAN,
        FLINT_THEME_FOREST,
        FLINT_THEME_SUNSET,
        FLINT_THEME_LAVENDER,
        FLINT_THEME_CHERRY,
    };
    int num_themes = 6;
    int theme_index = 0;
    int dark_mode = 0;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        FlintThemeId current_theme = themes[theme_index];

        // Get theme scope for proper color access
        const char* scope = flint_theme_scope_for(current_theme, dark_mode);

        // Get theme colors using catalog (works without theme files)
        Color background, text, button, button_hover, circle;
        flint_theme_catalog_color(current_theme, dark_mode, "background", &background);
        flint_theme_catalog_color(current_theme, dark_mode, "text", &text);
        flint_theme_catalog_color(current_theme, dark_mode, "button", &button);
        flint_theme_catalog_color(current_theme, dark_mode, "button_hover", &button_hover);
        flint_theme_catalog_color(current_theme, dark_mode, "circle", &circle);

        // Fill background with theme color
        ClearBackground(background);

        // Draw title with theme text color
        const char *theme_name = flint_theme_label(current_theme);
        char title[128];
        snprintf(title, sizeof(title), "Theme: %s (%s mode)", theme_name, dark_mode ? "Dark" : "Light");
        DrawText(title, flint_px(20), flint_px(20), 24, text);

        // Draw color swatches
        int y = flint_px(70);
        int x = flint_px(20);
        int swatch_size = flint_px(60);

        // Background
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, background);
        DrawRectangleLinesEx((Rectangle){x, y, swatch_size, swatch_size}, 1, text);
        DrawText("Background", x + swatch_size + 10, y + 10, 16, text);

        // Text
        y += swatch_size + flint_px(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, text);
        DrawText("Text", x + swatch_size + 10, y + 10, 16, text);

        // Button
        y += swatch_size + flint_px(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, button);
        DrawText("Button", x + swatch_size + 10, y + 10, 16, text);

        // Button Hover
        y += swatch_size + flint_px(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, button_hover);
        DrawText("Button Hover", x + swatch_size + 10, y + 10, 16, text);

        // Circle
        y += swatch_size + flint_px(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, circle);
        DrawText("Circle", x + swatch_size + 10, y + 10, 16, text);

        // Draw theme info
        y += swatch_size + flint_px(40);
        char info[128];
        snprintf(info, sizeof(info), "Scope: %s", scope);
        DrawText(info, x, y, 16, text);

        y += flint_px(30);
        snprintf(info, sizeof(info), "Theme ID: %d", current_theme);
        DrawText(info, x, y, 16, text);

        // Draw RGB values
        y += flint_px(40);
        DrawText("Color Values (R,G,B):", x, y, 18, text);
        y += flint_px(25);
        snprintf(info, sizeof(info), "Background: %d,%d,%d", background.r, background.g, background.b);
        DrawText(info, x, y, 14, text);
        y += flint_px(20);
        snprintf(info, sizeof(info), "Text: %d,%d,%d", text.r, text.g, text.b);
        DrawText(info, x, y, 14, text);
        y += flint_px(20);
        snprintf(info, sizeof(info), "Button: %d,%d,%d", button.r, button.g, button.b);
        DrawText(info, x, y, 14, text);

        // Instructions
        y = flint_px(20);
        x = flint_px(400);
        DrawText("Controls:", x, y, 18, text);
        y += flint_px(30);
        DrawText("SPACE - Next theme", x, y, 16, text);
        y += flint_px(25);
        DrawText("D - Toggle dark/light mode", x, y, 16, text);
        y += flint_px(25);
        DrawText("ESC - Exit", x, y, 16, text);

        // Handle input
        if(IsKeyPressed(KEY_SPACE)) {
            theme_index = (theme_index + 1) % num_themes;
        }

        if(IsKeyPressed(KEY_D)) {
            dark_mode = !dark_mode;
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
