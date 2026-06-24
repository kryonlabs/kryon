#include "flint_scaling.h"
#include <stdio.h>
#include <raylib.h>

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Flint Scaling Example");
    SetTargetFPS(60);

    // Test different DPI scales
    float scales[] = {1.0f, 1.5f, 2.0f, 2.5f};
    int num_scales = 4;
    int scale_index = 0;

    printf("Flint DPI Scaling Example\n");
    printf("=======================\n\n");
    printf("This example demonstrates Flint DPI scaling functions:\n");
    printf("  - flint_px(value) - Scale pixels by DPI factor\n");
    printf("  - flint_clamp_px(value, min, max) - Scale and clamp pixels\n\n");
    printf("Press SPACE to cycle DPI scales\n");
    printf("Use arrow keys to move test box\n");
    printf("Press ESC to exit\n\n");

    Vector2 box_pos = {100, 100};
    int box_size_base = 100;

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Set current DPI scale
        flint_set_dpi_scale(scales[scale_index]);

        // Scale box size
        int box_size = flint_px(box_size_base);

        // Draw info
        char info[128];
        snprintf(info, sizeof(info), "DPI Scale: %.1f (Press SPACE to cycle)", scales[scale_index]);
        DrawText(info, 50, 20, 20, BLACK);

        // Draw box with scaled size
        DrawRectangleRec((Rectangle){box_pos.x, box_pos.y, box_size, box_size}, SKYBLUE);
        DrawText("Scaled Box", box_pos.x + 10, box_pos.y + 10, 16, WHITE);

        // Show base vs scaled
        char size_info[128];
        snprintf(size_info, sizeof(size_info), "Base: %dpx, Scaled: %dpx", box_size_base, box_size);
        DrawText(size_info, 50, 60, 16, DARKGRAY);

        // Instructions
        DrawText("Arrow keys to move box", 50, 90, 16, DARKGRAY);
        DrawText("Box size scales with DPI", 50, 110, 16, DARKGRAY);

        // Handle box movement
        if(IsKeyDown(KEY_RIGHT)) box_pos.x += 3;
        if(IsKeyDown(KEY_LEFT)) box_pos.x -= 3;
        if(IsKeyDown(KEY_DOWN)) box_pos.y += 3;
        if(IsKeyDown(KEY_UP)) box_pos.y -= 3;

        // Handle DPI scale changes
        if(IsKeyPressed(KEY_SPACE)) {
            scale_index = (scale_index + 1) % num_scales;
            WaitTime(0.2);
        }

        if(IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
