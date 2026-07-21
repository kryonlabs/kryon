#include "kryon.h"
#include "ui_inspect.h"

#include <stdio.h>
#include <stdlib.h>

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

int
main(void)
{
    Rectangle parent = {10, 20, 200, 120};
    UIFrame frame;
    UIGrid grid;
    Rectangle r;
    Rectangle hits[3] = {
        {0, 0, 20, 20},
        {10, 10, 20, 20},
        {100, 100, 10, 10}
    };

    SetUIScale(1.0f);

    frame = BeginUIFrameBox(parent, 10, 10, 4);
    r = UIFramePack(&frame, UI_SIDE_TOP, 30);
    check_int("pack x", (int)r.x, 20);
    check_int("pack y", (int)r.y, 30);
    check_int("pack width", (int)r.width, 180);
    check_int("pack height", (int)r.height, 30);

    grid = (UIGrid){parent, 2, 2, 10, 10, 0, 0};
    r = UIGridCell(grid, 1, 1, 1, 1);
    check_int("grid x", (int)r.x, 115);
    check_int("grid y", (int)r.y, 85);
    check_int("grid width", (int)r.width, 95);
    check_int("grid height", (int)r.height, 55);

    check_int("topmost hit", UICanvasHitTest((Vector2){15, 15}, hits, 3), 1);
    check_int("miss", UICanvasHitTest((Vector2){80, 80}, hits, 3), -1);

    {
        int sx = 10;
        int sy = 20;
        float zoom = 2.0f;
        UICanvas canvas = {{40, 50, 200, 100}, &sx, &sy, &zoom};
        Vector2 p = UICanvasToScreen(canvas, (Vector2){50, 70});
        Rectangle rr = UICanvasRectToScreen(canvas, (Rectangle){50, 70, 20, 10});
        check_int("canvas screen x", (int)p.x, 40);
        check_int("canvas screen y", (int)p.y, 50);
        check_int("canvas rect w", (int)rr.width, 40);
        check_int("canvas rect h", (int)rr.height, 20);
    }

    {
        Camera2D camera = {0};
        UIWidget widget;
        UIInspectSelection selection;
        int token;

        SetUIInspectEnabled(1);
        BeginUIInspectFrame(".");
        SetUIInspectCanvasBounds((Rectangle){40, 50, 200, 120});
        camera.offset = (Vector2){40, 50};
        camera.zoom = 2.0f;
        token = PushUIInspectTransform(camera);
        BeginUIInspectFrame(NULL);
        widget = BeginUIWidget("test", "inspect-transform",
                               (Rectangle){10, 20, 30, 15}, 0);
        EndUIWidget(&widget);
        check_int("inspect transformed count", UIInspectWidgetCount(), 1);
        check_int("inspect transformed hit",
                  UIInspectSelectAt((Vector2){65, 95}), 1);
        selection = UIInspectGetSelection();
        check_int("inspect selected x", (int)selection.bounds.x, 10);
        check_int("inspect transformed miss",
                  UIInspectSelectAt((Vector2){20, 20}), 0);
        PopUIInspectTransform(token);
    }

    return 0;
}
