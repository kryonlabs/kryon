#include "example_ui_font.h"
#include "kryon.h"

int
main(void)
{
    Rectangle shapes[] = {{100, 100, 90, 60}, {260, 140, 120, 80}};
    int selected = -1;
    int sx = 0;
    int sy = 0;
    float zoom = 1.0f;

    InitWindow(800, 600, "Kryon Canvas");
    SetTargetFPS(60);
    LoadExampleUIFont();
    InitUI(800, 600, GetUIScale());
    SetCurrentTheme(THEME_SKY, 0);

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());

        UICanvasResult canvas = BeginUICanvas((UICanvas){{40, 50, 620, 420}, &sx, &sy, &zoom});
        DrawUICanvasGrid((Rectangle){40, 50, 620, 420}, 24, GetThemeButton());
        for(int i = 0; i < 2; i++) {
            DrawRectangleRec(shapes[i], i == selected ? GetThemeButtonHover() : GetThemeSurface());
            DrawRectangleLinesEx(shapes[i], 2.0f, GetThemeIcon());
        }
        if(canvas.active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            selected = UICanvasHitTest(canvas.world, shapes, 2);
        EndUICanvas((UICanvas){{40, 50, 620, 420}, &sx, &sy, &zoom});

        EndDrawing();
    }

    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
