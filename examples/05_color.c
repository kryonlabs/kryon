#include "example_ui_font.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui_color.h"
#include "ui_scaling.h"
#include <stdio.h>
#include "flint.h"

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint Color Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screenWidth, screenHeight, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    printf("Flint Color Utilities Example\n");
    printf("===========================\n\n");
    printf("This example demonstrates Flint color manipulation functions:\n");
    printf("  - LightenUIColor(color, amount)\n");
    printf("  - DarkenUIColor(color, amount)\n\n");
    printf("Press SPACE to cycle through color examples\n");
    printf("Press ESC to exit\n\n");

    int color_index = 0;
    int num_colors = 5;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        Color test_colors[] = {
            GetThemeButton(),
            GetThemeButtonHover(),
            GetThemeCircle(),
            GetThemeSurface(),
            GetThemeLink(),
        };
        Color base = test_colors[color_index];

        // Display original color
        DrawRectangleRec((Rectangle){50, 50, 200, 50}, base);
        DrawUIText("Original", 55, 55, 16, GetThemeText());

        // Display lightened color
        Color light = LightenUIColor(base, 40);
        DrawRectangleRec((Rectangle){300, 50, 200, 50}, light);
        DrawUIText("Lightened (+40)", 305, 55, 16, GetThemeText());

        // Display darkened color
        Color dark = DarkenUIColor(base, 40);
        DrawRectangleRec((Rectangle){550, 50, 200, 50}, dark);
        DrawUIText("Darkened (-40)", 555, 55, 16, GetThemeText());

        // Show current color index
        char info[64];
        snprintf(info, sizeof(info), "Color %d/%d (Press SPACE to cycle)", color_index + 1, num_colors);
        DrawUIText(info, 50, 120, 20, GetThemeText());

        // Instructions
        DrawUIText("Top row: Original theme color", 50, 150, 16, GetThemeText());
        DrawUIText("Middle: Lightened by 40", 50, 170, 16, GetThemeText());
        DrawUIText("Bottom: Darkened by 40", 50, 190, 16, GetThemeText());
        DrawUIText("", 50, 220, 16, GetThemeText());

        DrawUIText("Color RGB values:", 50, 250, 20, GetThemeText());
        char rgb[64];
        snprintf(rgb, sizeof(rgb), "R:%d G:%d B:%d", base.r, base.g, base.b);
        DrawUIText(rgb, 50, 270, 16, GetThemeText());

        // Handle input
        if(IsKeyPressed(KEY_SPACE)) {
            color_index = (color_index + 1) % num_colors;
            WaitTime(0.2);
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
