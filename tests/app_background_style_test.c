#include "../src/ui/ui_internal.h"
#include "ui_style_sheet.h"
#include "ui_tree_props.generated.h"

#include <stdio.h>

static int failures;
static Color last_rectangle_rec_color;

static void
check_color(const char *name, Color got, Color want)
{
    if(ColorToInt(got) == ColorToInt(want))
        return;
    fprintf(stderr, "FAIL: %s got 0x%08x want 0x%08x\n", name,
            (unsigned)ColorToInt(got), (unsigned)ColorToInt(want));
    failures++;
}

void __wrap_DrawRectangle(int posX, int posY, int width, int height,
                          Color color);
void __wrap_DrawRectangleRec(Rectangle rec, Color color);
void __wrap_DrawRectangleLinesEx(Rectangle rec, float lineThick,
                                 Color color);
void __wrap_DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                                 Color color);
void __wrap_DrawLine(int startPosX, int startPosY, int endPosX, int endPosY,
                     Color color);
void __wrap_BeginScissorMode(int x, int y, int width, int height);
void __wrap_EndScissorMode(void);

int
main(void)
{
    const TreeNode *nodes;
    int count;

    ClearStylePacks();
    last_rectangle_rec_color = RED;
    BeginTree(6900);
    AppBackground();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_color("unstyled app background node",
                count > 1 ? nodes[1].data.primitive.color : RED, BLANK);
    check_color("unstyled app background draw", last_rectangle_rec_color,
                BLANK);

    if(!EnsureBuiltInStylePacks()) {
        fprintf(stderr, "FAIL: built-in style packs did not register\n");
        return 1;
    }
    last_rectangle_rec_color = RED;
    BeginTree(6901);
    AppBackground();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_color("material app background node",
                count > 1 ? nodes[1].data.primitive.color : RED,
                (Color){0x10, 0x12, 0x17, 0xff});
    check_color("material app background draw", last_rectangle_rec_color,
                (Color){0x10, 0x12, 0x17, 0xff});

    ClearStylePacks();
    return failures == 0 ? 0 : 1;
}

void
__wrap_DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    (void)posX;
    (void)posY;
    (void)width;
    (void)height;
    (void)color;
}

void
__wrap_DrawRectangleRec(Rectangle rec, Color color)
{
    last_rectangle_rec_color = color;
    (void)rec;
}

void
__wrap_DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    (void)rec;
    (void)lineThick;
    (void)color;
}

void
__wrap_DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                            Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
}

void
__wrap_DrawLine(int startPosX, int startPosY, int endPosX, int endPosY,
                Color color)
{
    (void)startPosX;
    (void)startPosY;
    (void)endPosX;
    (void)endPosY;
    (void)color;
}

void
__wrap_BeginScissorMode(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void
__wrap_EndScissorMode(void)
{
}
