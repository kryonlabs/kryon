#include "flint_example_font.h"
#include "flint_color.h"
#include "flint_scaling.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint Color Example");
    SetTargetFPS(60);
    flint_example_load_font();

    printf("Flint Color Utilities Example\n");
    printf("===========================\n\n");
    printf("This example demonstrates Flint color manipulation functions:\n");
    printf("  - flint_lighten(color, amount)\n");
    printf("  - flint_darken(color, amount)\n\n");
    printf("Press SPACE to cycle through color examples\n");
    printf("Press ESC to exit\n\n");

    // Test colors
    Color test_colors[] = {
        (Color){200, 100, 50, 255},    // Brown
        (Color){50, 150, 200, 255},    // Blue
        (Color){100, 200, 100, 255},  // Green
        (Color){200, 100, 200, 255},  // Purple
        (Color){200, 200, 100, 255},   // Yellow
    };
    int color_index = 0;
    int num_colors = 5;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        Color base = test_colors[color_index];

        // Display original color
        DrawRectangleRec((Rectangle){50, 50, 200, 50}, base);
        flint_text_draw("Original", 55, 55, 16, BLACK);

        // Display lightened color
        Color light = flint_lighten(base, 40);
        DrawRectangleRec((Rectangle){300, 50, 200, 50}, light);
        flint_text_draw("Lightened (+40)", 305, 55, 16, BLACK);

        // Display darkened color
        Color dark = flint_darken(base, 40);
        DrawRectangleRec((Rectangle){550, 50, 200, 50}, dark);
        flint_text_draw("Darkened (-40)", 555, 55, 16, BLACK);

        // Show current color index
        char info[64];
        snprintf(info, sizeof(info), "Color %d/%d (Press SPACE to cycle)", color_index + 1, num_colors);
        flint_text_draw(info, 50, 120, 20, DARKGRAY);

        // Instructions
        flint_text_draw("Top row: Original color", 50, 150, 16, DARKGRAY);
        flint_text_draw("Middle: Lightened by 40", 50, 170, 16, DARKGRAY);
        flint_text_draw("Bottom: Darkened by 40", 50, 190, 16, DARKGRAY);
        flint_text_draw("", 50, 220, 16, DARKGRAY);

        flint_text_draw("Color RGB values:", 50, 250, 20, BLACK);
        char rgb[64];
        snprintf(rgb, sizeof(rgb), "R:%d G:%d B:%d", base.r, base.g, base.b);
        flint_text_draw(rgb, 50, 270, 16, DARKGRAY);

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
    flint_example_unload_font();

    CloseWindow();
    return 0;
}
