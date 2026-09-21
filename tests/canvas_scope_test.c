#include "../src/ui/ui_internal.h"
#include "ui_drawing_props.generated.h"
#include <assert.h>
#include <math.h>

static void same_matrix(Matrix a, Matrix b)
{
    const float *left = (const float *)&a;
    const float *right = (const float *)&b;
    for (int i = 0; i < 16; i++) assert(fabsf(left[i] - right[i]) < 0.001f);
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(240, 180, "Canvas scope regression");
    assert(IsWindowReady());
    BeginDrawing();
    Matrix original = rlGetMatrixModelview();
    float zoom = 2;
    int offset = 10;
    Canvas outer = {.bounds = {20, 20, 120, 100}, .scroll_x = &offset, .zoom = &zoom};
    Canvas child = {.bounds = {30, 30, 60, 60}};
    CanvasScope(outer);
    Matrix parent = rlGetMatrixModelview();
    CanvasScope(child);
    CanvasEndScope(child);
    same_matrix(rlGetMatrixModelview(), parent);
    assert(GetClipEffective((Rectangle){0, 0, 240, 180}).width == 120);
    float child_zoom = 3;
    child.zoom = &child_zoom;
    CanvasScope(child);
    assert(fabsf(rlGetMatrixModelview().m0 - 3) < 0.001f);
    CanvasEndScope(child);
    same_matrix(rlGetMatrixModelview(), parent);
    CanvasEndScope(outer);
    same_matrix(rlGetMatrixModelview(), original);
    assert(GetClipEffective((Rectangle){0, 0, 240, 180}).width == 240);
    EndDrawing();
    CloseWindow();
    return 0;
}
