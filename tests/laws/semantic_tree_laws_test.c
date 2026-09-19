#include <string.h>

#include "../../src/ui/ui_internal.h"
#include "lawcheck.h"

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
    (void)rec;
    (void)color;
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

static const TreeNode *
find_node(const TreeNode *nodes, int count, int kind, int id)
{
    for(int i = 0; i < count; i++) {
        if(nodes[i].kind == kind && nodes[i].id == id)
            return &nodes[i];
    }
    return NULL;
}

int
main(void)
{
    LawCheck law = {0};
    const TreeNode *nodes;
    const TreeNode *label_button;
    const TreeNode *child_button;
    const TreeNode *label_text = NULL;
    const TreeNode *child_text = NULL;
    int count = 0;

    LAW_BEGIN(&law, "semantic.button.label.equals.text.child");

    BeginTree(9100);
    ButtonScope((ButtonProps){
        .bounds = {10, 10, 120, 40},
        .label = "Save",
        .id = 9101
    });
    End();
    ButtonScope((ButtonProps){
        .bounds = {150, 10, 120, 40},
        .id = 9102
    });
    Text((TextProps){.text = "Save", .wrap = TextWrapNone});
    End();
    EndTree();

    nodes = GetTreeNodes(&count);
    label_button = find_node(nodes, count, WidgetKindButton, 9101);
    child_button = find_node(nodes, count, WidgetKindButton, 9102);
    REQUIRE(&law, label_button != NULL);
    REQUIRE(&law, child_button != NULL);

    if(label_button != NULL && label_button->first_child >= 0)
        label_text = &nodes[label_button->first_child];
    if(child_button != NULL && child_button->first_child >= 0)
        child_text = &nodes[child_button->first_child];

    REQUIRE(&law, label_text != NULL);
    REQUIRE(&law, child_text != NULL);
    if(label_text != NULL && child_text != NULL) {
        REQUIRE(&law, label_text->kind == WidgetKindText);
        REQUIRE(&law, child_text->kind == WidgetKindText);
        REQUIRE(&law, label_text->parent == (int)(label_button - nodes));
        REQUIRE(&law, child_text->parent == (int)(child_button - nodes));
        REQUIRE(&law, strcmp(label_text->owned_text, child_text->owned_text) == 0);
        REQUIRE(&law, label_text->data.primitive.font ==
                      child_text->data.primitive.font);
        REQUIRE(&law, ColorToInt(label_text->data.primitive.color) ==
                      ColorToInt(child_text->data.primitive.color));
        REQUIRE(&law, (int)label_text->bounds.width ==
                      (int)child_text->bounds.width);
        REQUIRE(&law, (int)label_text->bounds.height ==
                      (int)child_text->bounds.height);
    }

    return lawcheck_finish(&law);
}
