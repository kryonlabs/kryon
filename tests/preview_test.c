#include "kryon.h"

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
    Rectangle fit;
    Rectangle rects[4];
    PreviewLine lines[4];
    Vector2 points[4];
    PreviewObject objects[4];
    char rect_path[256];
    char line_path[256];
    char point_path[256];
    char object_path[256];

    fit = PreviewFitRect((Rectangle){0, 0, 1000, 700}, 640, 480,
                         PREVIEW_SCALE_FIT);
    check_int("fit width", (int)fit.width, 933);
    check_int("fit height", (int)fit.height, 700);
    check_int("fit x", (int)fit.x, 33);
    check_int("fit y", (int)fit.y, 0);

    fit = PreviewFitRect((Rectangle){10, 20, 1000, 700}, 640, 480,
                         PREVIEW_SCALE_50);
    check_int("50 width", (int)fit.width, 320);
    check_int("50 height", (int)fit.height, 240);
    check_int("50 x", (int)fit.x, 350);
    check_int("50 y", (int)fit.y, 250);

    snprintf(rect_path, sizeof(rect_path), "%s/kryon-preview-rects.txt",
             getenv("TMPDIR") != 0 ? getenv("TMPDIR") : "/tmp");
    snprintf(line_path, sizeof(line_path), "%s/kryon-preview-lines.txt",
             getenv("TMPDIR") != 0 ? getenv("TMPDIR") : "/tmp");
    snprintf(point_path, sizeof(point_path), "%s/kryon-preview-points.txt",
             getenv("TMPDIR") != 0 ? getenv("TMPDIR") : "/tmp");
    snprintf(object_path, sizeof(object_path), "%s/kryon-preview-objects.txt",
             getenv("TMPDIR") != 0 ? getenv("TMPDIR") : "/tmp");

    rects[0] = (Rectangle){10, 20, -30, 40};
    rects[1] = (Rectangle){50, 60, 70, 80};
    if(!SavePreviewRectLayer(rect_path, rects, 2))
        return 1;
    check_int("rect load", LoadPreviewRectLayer(rect_path, rects, 4), 2);
    check_int("rect normalized x", (int)rects[0].x, -20);
    check_int("rect normalized w", (int)rects[0].width, 30);

    lines[0] = (PreviewLine){{1, 2}, {3, 4}};
    if(!SavePreviewLineLayer(line_path, lines, 1))
        return 1;
    check_int("line load", LoadPreviewLineLayer(line_path, lines, 4), 1);
    check_int("line b y", (int)lines[0].b.y, 4);

    points[0] = (Vector2){11, 12};
    if(!SavePreviewPointLayer(point_path, points, 1))
        return 1;
    check_int("point load", LoadPreviewPointLayer(point_path, points, 4), 1);
    check_int("point y", (int)points[0].y, 12);

    objects[0] = (PreviewObject){7, {1, 2, 3, 4}};
    if(!SavePreviewObjectLayer(object_path, objects, 1))
        return 1;
    check_int("object load", LoadPreviewObjectLayer(object_path, objects, 4),
              1);
    check_int("object type", objects[0].type, 7);
    check_int("object h", (int)objects[0].bounds.height, 4);

    return 0;
}
