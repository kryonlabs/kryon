#include "example_ui_font.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui_layout.h"
#include "ui_scaling.h"
#include <stdio.h>
#include "flint.h"

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint Layout Example");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(screenWidth, screenHeight, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    // Initialize view size
    SetUIViewSize(screenWidth, screenHeight);

    printf("Flint Layout Utilities Example\n");
    printf("==============================\n\n");
    printf("This example demonstrates Flint layout functions:\n");
    printf("  - GetUIPageSidePadding() - Get side padding for content\n");
    printf("  - GetUICenteredColumn() - Calculate centered column\n");
    printf("  - GetUIViewWidth() - Get available content width\n\n");
    printf("Press SPACE to show layout guides\n");
    printf("Press ESC to exit\n\n");

    int show_guides = 0;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        int screen_w = GetScreenWidth();
        int screen_h = GetScreenHeight();

        // Calculate layout
        int side_padding = GetUIPageSidePadding();
        int content_x = side_padding;
        int content_w = GetUIViewWidth();
        int content_h = screen_h - ScaleUIPx(64);

        // Draw main content area
        Rectangle content_area = {content_x, ScaleUIPx(32), content_w, content_h};
        DrawRectangleRec(content_area, GetThemeSurface());
        DrawRectangleLinesEx(content_area, 1, GetThemeIcon());

        // Draw centered column example
        int col_x, col_w;
        GetUICenteredColumn(200, side_padding, &col_x, &col_w);
        DrawRectangle(col_x, ScaleUIPx(32), col_w, content_h, GetThemeButton());

        if(show_guides) {
            // Draw layout guides
            DrawLine(side_padding, 0, side_padding, screen_h, GetThemeLink());
            DrawLine(screen_w - side_padding, 0, screen_w - side_padding, screen_h, GetThemeLink());

            char label[64];
            snprintf(label, sizeof(label), "Side padding: %d", side_padding);
            DrawUIText(label, side_padding + 10, 10, 12, GetThemeLink());

            snprintf(label, sizeof(label), "Content width: %d", content_w);
            DrawUIText(label, content_x + 10, content_area.y - 20, 12, GetThemeLink());

            snprintf(label, sizeof(label), "Center column X: %d, W: %d", col_x, col_w);
            DrawUIText(label, col_x + 10, content_area.y - 20, 12, GetThemeButton());

            // Draw center line
            int center_x = screen_w / 2;
            DrawLine(center_x, 0, center_x, screen_h, GetThemeCircle());
        }

        // Draw title
        DrawUIText("Flint Layout Example", side_padding, ScaleUIPx(20), 24, GetThemeText());
        DrawUIText("Press SPACE for layout guides", side_padding, ScaleUIPx(48), 16, GetThemeText());

        // Handle input
        if(IsKeyPressed(KEY_SPACE)) {
            show_guides = !show_guides;
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
