#include "example_ui_font.h"
#include "theme_meta.h"
#include "ui_scaling.h"
#include <stdio.h>
#include "flint.h"

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "File UI Toolkit Theme Example");
    SetTargetFPS(60);
    LoadExampleUIFont();

    printf("File UI Toolkit Theme System Example\n");
    printf("=========================\n\n");
    printf("This example demonstrates File UI Toolkit theme catalog:\n");
    printf("  - GetThemeCatalogColor() - Get theme colors\n");
    printf("  - GetThemeScopeName() - Get theme scope name\n");
    printf("  - ThemeId enum - All available themes\n\n");
    printf("Press SPACE to cycle through themes\n");
    printf("Press D to toggle light/dark mode\n");
    printf("Press ESC to exit\n\n");

    // Available themes
    ThemeId themes[] = {
        THEME_SKY,
        THEME_OCEAN,
        THEME_FOREST,
        THEME_SUNSET,
        THEME_LAVENDER,
        THEME_CHERRY,
    };
    int num_themes = 6;
    int theme_index = 0;
    int dark_mode = 0;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        ThemeId current_theme = themes[theme_index];

        // Get theme scope for proper color access
        const char* scope = GetThemeScopeName(current_theme, dark_mode);

        // Get theme colors using catalog (works without theme files)
        Color background, text, button, button_hover, circle;
        GetThemeCatalogColor(current_theme, dark_mode, "background", &background);
        GetThemeCatalogColor(current_theme, dark_mode, "text", &text);
        GetThemeCatalogColor(current_theme, dark_mode, "button", &button);
        GetThemeCatalogColor(current_theme, dark_mode, "button_hover", &button_hover);
        GetThemeCatalogColor(current_theme, dark_mode, "circle", &circle);

        // Fill background with theme color
        ClearBackground(background);

        // Draw title with theme text color
        const char *theme_name = GetThemeLabel(current_theme);
        char title[128];
        snprintf(title, sizeof(title), "Theme: %s (%s mode)", theme_name, dark_mode ? "Dark" : "Light");
        DrawUIText(title, ScaleUIPx(20), ScaleUIPx(20), 24, text);

        // Draw color swatches
        int y = ScaleUIPx(70);
        int x = ScaleUIPx(20);
        int swatch_size = ScaleUIPx(60);

        // Background
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, background);
        DrawRectangleLinesEx((Rectangle){x, y, swatch_size, swatch_size}, 1, text);
        DrawUIText("Background", x + swatch_size + 10, y + 10, 16, text);

        // Text
        y += swatch_size + ScaleUIPx(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, text);
        DrawUIText("Text", x + swatch_size + 10, y + 10, 16, text);

        // Button
        y += swatch_size + ScaleUIPx(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, button);
        DrawUIText("Button", x + swatch_size + 10, y + 10, 16, text);

        // Button Hover
        y += swatch_size + ScaleUIPx(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, button_hover);
        DrawUIText("Button Hover", x + swatch_size + 10, y + 10, 16, text);

        // Circle
        y += swatch_size + ScaleUIPx(20);
        DrawRectangleRec((Rectangle){x, y, swatch_size, swatch_size}, circle);
        DrawUIText("Circle", x + swatch_size + 10, y + 10, 16, text);

        // Draw theme info
        y += swatch_size + ScaleUIPx(40);
        char info[128];
        snprintf(info, sizeof(info), "Scope: %s", scope);
        DrawUIText(info, x, y, 16, text);

        y += ScaleUIPx(30);
        snprintf(info, sizeof(info), "Theme ID: %d", current_theme);
        DrawUIText(info, x, y, 16, text);

        // Draw RGB values
        y += ScaleUIPx(40);
        DrawUIText("Color Values (R,G,B):", x, y, 18, text);
        y += ScaleUIPx(25);
        snprintf(info, sizeof(info), "Background: %d,%d,%d", background.r, background.g, background.b);
        DrawUIText(info, x, y, 14, text);
        y += ScaleUIPx(20);
        snprintf(info, sizeof(info), "Text: %d,%d,%d", text.r, text.g, text.b);
        DrawUIText(info, x, y, 14, text);
        y += ScaleUIPx(20);
        snprintf(info, sizeof(info), "Button: %d,%d,%d", button.r, button.g, button.b);
        DrawUIText(info, x, y, 14, text);

        // Instructions
        y = ScaleUIPx(20);
        x = ScaleUIPx(400);
        DrawUIText("Controls:", x, y, 18, text);
        y += ScaleUIPx(30);
        DrawUIText("SPACE - Next theme", x, y, 16, text);
        y += ScaleUIPx(25);
        DrawUIText("D - Toggle dark/light mode", x, y, 16, text);
        y += ScaleUIPx(25);
        DrawUIText("ESC - Exit", x, y, 16, text);

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
    UnloadExampleUIFont();

    CloseWindow();
    return 0;
}
