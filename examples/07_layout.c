#include "flint_example_font.h"
#include "flint_layout.h"
#include "flint_scaling.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint Layout Example");
    SetTargetFPS(60);
    flint_example_load_font();

    // Initialize view size
    flint_set_view_size(screenWidth, screenHeight);

    printf("Flint Layout Utilities Example\n");
    printf("==============================\n\n");
    printf("This example demonstrates Flint layout functions:\n");
    printf("  - flint_page_side_padding() - Get side padding for content\n");
    printf("  - flint_centered_column() - Calculate centered column\n");
    printf("  - flint_view_width() - Get available content width\n\n");
    printf("Press SPACE to show layout guides\n");
    printf("Press ESC to exit\n\n");

    int show_guides = 0;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        int screen_w = GetScreenWidth();
        int screen_h = GetScreenHeight();

        // Calculate layout
        int side_padding = flint_page_side_padding();
        int content_x = side_padding;
        int content_w = flint_view_width();
        int content_h = screen_h - flint_px(64);

        // Draw main content area
        Rectangle content_area = {content_x, flint_px(32), content_w, content_h};
        DrawRectangleRec(content_area, LIGHTGRAY);
        DrawRectangleLinesEx(content_area, 1, DARKGRAY);

        // Draw centered column example
        int col_x, col_w;
        flint_centered_column(200, side_padding, &col_x, &col_w);
        DrawRectangle(col_x, flint_px(32), col_w, content_h, BLUE);

        if(show_guides) {
            // Draw layout guides
            DrawLine(side_padding, 0, side_padding, screen_h, RED);
            DrawLine(screen_w - side_padding, 0, screen_w - side_padding, screen_h, RED);

            char label[64];
            snprintf(label, sizeof(label), "Side padding: %d", side_padding);
            flint_text_draw(label, side_padding + 10, 10, 12, RED);

            snprintf(label, sizeof(label), "Content width: %d", content_w);
            flint_text_draw(label, content_x + 10, content_area.y - 20, 12, RED);

            snprintf(label, sizeof(label), "Center column X: %d, W: %d", col_x, col_w);
            flint_text_draw(label, col_x + 10, content_area.y - 20, 12, BLUE);

            // Draw center line
            int center_x = screen_w / 2;
            DrawLine(center_x, 0, center_x, screen_h, GREEN);
        }

        // Draw title
        flint_text_draw("Flint Layout Example", side_padding, flint_px(20), 24, BLACK);
        flint_text_draw("Press SPACE for layout guides", side_padding, flint_px(48), 16, DARKGRAY);

        // Handle input
        if(IsKeyPressed(KEY_SPACE)) {
            show_guides = !show_guides;
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
