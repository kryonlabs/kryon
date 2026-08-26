#include "kryon.h"

int
main(void)
{
    SetSingleInstance(0);
    InitWindow(640, 360, "termi smoke");
    SetTargetFPS(60);
    for(int frame = 0; frame < 120 && !WindowShouldClose(); frame++) {
        BeginDrawing();
        ClearBackground((Color){8, 12, 18, 255});
        DrawRectangle(8, 8, 240, 48, (Color){24, 88, 136, 255});
        DrawRectangleLines(8, 8, 240, 48, (Color){230, 235, 240, 255});
        DrawText("Termi backend", 24, 24, 16, (Color){255, 255, 255, 255});
        Button((ButtonProps){
            .bounds = (Rectangle){24, 88, 180, 44},
            .label = "Button",
            .font = Text16,
            .id = 1001,
        });
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
