#include "flint.h"

#include <stdio.h>

static int failures = 0;

static void
check_true(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void
use_types(void)
{
    Color color = RED;
    Rectangle rect = {10.0f, 20.0f, 30.0f, 40.0f};
    Vector2 point = {1.0f, 2.0f};
    Image image = {0};
    Texture2D texture = {0};
    Font font = {0};

    check_true("color fields", color.r == 230 && color.a == 255);
    check_true("rectangle fields", rect.x == 10.0f && rect.height == 40.0f);
    check_true("vector fields", point.x == 1.0f && point.y == 2.0f);
    check_true("zero image", image.data == 0 && image.width == 0);
    check_true("zero texture", texture.id == 0 && texture.width == 0);
    check_true("zero font", font.baseSize == 0 && font.texture.id == 0);
}

static void
use_functions(int argc)
{
    if(argc == 12345) {
        InitWindow(800, 600, "compat");
        SetTargetFPS(60);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangle(10, 20, 30, 40, RAYWHITE);
        DrawRectangleRec((Rectangle){50.0f, 60.0f, 70.0f, 80.0f}, BLUE);
        DrawText(TextFormat("mouse %.1f", GetMousePosition().x), 10, 10, 20, WHITE);
        DrawTexture((Texture2D){0}, 0, 0, WHITE);
        EndDrawing();
        if(WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || GetMouseWheelMove() != 0.0f)
            TraceLog(LOG_INFO, "input path linked");
        CloseWindow();
    }
}

int
main(int argc, char **argv)
{
    (void)argv;

    use_types();
    use_functions(argc);

    if(failures != 0)
        return 1;
    printf("raylib compatibility tests passed\n");
    return 0;
}
