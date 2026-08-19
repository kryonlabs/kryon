#include "kryon.h"

int main(void)
{
    int frames = 0;

    InitWindow(320, 240, "canvas smoke");
    SetTargetFPS(60);
    while(!WindowShouldClose() && frames < 3) {
        frames++;
        BeginDrawing();
        ClearBackground((Color){16, 16, 20, 255});
        DrawRectangle(10, 10, 100, 40, (Color){45, 77, 123, 255});
        DrawRectangleLines(10, 10, 100, 40, (Color){200, 200, 210, 255});
        DrawRectangleLinesEx((Rectangle){12, 58, 96, 34}, 4.0f,
                             (Color){220, 160, 80, 255});
        DrawCircle(200, 120, 30, (Color){0, 228, 48, 255});
        DrawLine(0, 0, 320, 240, (Color){255, 0, 0, 255});
        DrawRectangleRounded((Rectangle){130, 60, 90, 40, }, 0.5f, 8,
                             (Color){120, 80, 200, 255});
        DrawRectangleRoundedLines((Rectangle){130, 110, 90, 40}, 0.5f, 8,
                                  (Color){240, 200, 60, 255});
        DrawRing((Vector2){60, 150}, 20, 32, 10.0f, 300.0f, 24,
                 (Color){255, 109, 194, 255});
        DrawRectangleGradientV(240, 60, 60, 90,
                               (Color){30, 30, 60, 255},
                               (Color){120, 30, 30, 255});
        DrawText("hello canvas", 12, 200, 16, (Color){240, 240, 242, 255});
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
