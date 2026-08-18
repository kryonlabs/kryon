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
        DrawCircle(200, 120, 30, (Color){0, 228, 48, 255});
        DrawLine(0, 0, 320, 240, (Color){255, 0, 0, 255});
        DrawText("hello canvas", 12, 200, 16, (Color){240, 240, 242, 255});
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
