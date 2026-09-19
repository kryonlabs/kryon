#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../../src/ui/ui_internal.h"
#include "lawcheck.h"
#include "runtime/input.h"
#include "runtime/layout.h"
#include "runtime/paned_view.h"
#include "runtime/style_sheet.h"

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

static int
abs_i32(int value)
{
    return value < 0 ? -value : value;
}

enum { TREE_SNAPSHOT_MAX = 512 };

typedef struct TreeSnapshot {
    Rectangle bounds[TREE_SNAPSHOT_MAX];
    int parent[TREE_SNAPSHOT_MAX];
    int first_child[TREE_SNAPSHOT_MAX];
    int next_sibling[TREE_SNAPSHOT_MAX];
    int kind[TREE_SNAPSHOT_MAX];
    int count;
} TreeSnapshot;

static void
snapshot_tree(TreeSnapshot *snapshot)
{
    const TreeNode *nodes = GetTreeNodes(&snapshot->count);
    if(snapshot->count > TREE_SNAPSHOT_MAX)
        snapshot->count = TREE_SNAPSHOT_MAX;
    for(int i = 0; i < snapshot->count; i++) {
        snapshot->bounds[i] = nodes[i].bounds;
        snapshot->parent[i] = nodes[i].parent;
        snapshot->first_child[i] = nodes[i].first_child;
        snapshot->next_sibling[i] = nodes[i].next_sibling;
        snapshot->kind[i] = nodes[i].kind;
    }
}

static int
snapshots_equal(const TreeSnapshot *a, const TreeSnapshot *b)
{
    if(a->count != b->count)
        return 0;
    for(int i = 0; i < a->count; i++) {
        if(a->parent[i] != b->parent[i] ||
           a->first_child[i] != b->first_child[i] ||
           a->next_sibling[i] != b->next_sibling[i] ||
           a->kind[i] != b->kind[i])
            return 0;
        if(a->bounds[i].x != b->bounds[i].x ||
           a->bounds[i].y != b->bounds[i].y ||
           a->bounds[i].width != b->bounds[i].width ||
           a->bounds[i].height != b->bounds[i].height)
            return 0;
    }
    return 1;
}

static void build_container(uint32_t *state, KeyID *next_key, Rectangle bounds,
                            int depth);

static void
build_leaf(uint32_t *state, KeyID *next_key, Rectangle bounds)
{
    if((law_next_u32(state) & 1u) == 0u) {
        ButtonScope((ButtonProps){
            .bounds = bounds,
            .label = "law",
            .id = (int)(*next_key)++
        });
        End();
    } else {
        Text((TextProps){.text = "law", .wrap = TextWrapNone, .bounds = bounds});
    }
}

static void
build_children(uint32_t *state, KeyID *next_key, int depth)
{
    int count = 1 + (int)(law_next_u32(state) % 4u);
    for(int i = 0; i < count; i++) {
        Rectangle child = {0.0f, 0.0f,
                           law_float_between(state, 12.0f, 280.0f),
                           law_float_between(state, 12.0f, 200.0f)};
        /* Explicit positions exercise the declared-bounds escape hatch. */
        if(depth > 0 && (law_next_u32(state) % 8u) == 0u) {
            child.x = law_float_between(state, 4.0f, 60.0f);
            child.y = law_float_between(state, 4.0f, 60.0f);
        }
        if(depth <= 0 || (law_next_u32(state) & 1u) == 0u)
            build_leaf(state, next_key, child);
        else
            build_container(state, next_key, child, depth - 1);
    }
}

static void
build_container(uint32_t *state, KeyID *next_key, Rectangle bounds, int depth)
{
    uint32_t kind = law_next_u32(state) % 4u;
    ColumnProps props = {0};
    props.bounds = bounds;
    props.gap = (int)(law_next_u32(state) % 12u);
    props.padding = (int)(law_next_u32(state) % 16u);
    props.key = (*next_key)++;
    if(kind == 0u) {
        Column(props);
        build_children(state, next_key, depth);
    } else if(kind == 1u) {
        Row(props);
        build_children(state, next_key, depth);
    } else if(kind == 2u) {
        Stack(props);
        build_children(state, next_key, depth);
    } else {
        GridProps grid = {0};
        grid.bounds = bounds;
        grid.columns = 1 + (int)(law_next_u32(state) % 4u);
        grid.min_item_width = 40;
        grid.max_columns = 4;
        grid.gap = props.gap;
        grid.padding = props.padding;
        grid.key = props.key;
        Grid(grid);
        build_children(state, next_key, depth);
    }
    End();
}

static void
build_generated_tree(uint32_t seed, KeyID screen_key)
{
    uint32_t state = seed;
    KeyID next_key = screen_key * 1000u + 100u;
    int roots = 1 + (int)(law_next_u32(&state) % 3u);
    BeginTree(screen_key);
    for(int i = 0; i < roots; i++) {
        Rectangle root = {0.0f, 0.0f,
                          law_float_between(&state, 200.0f, 780.0f),
                          law_float_between(&state, 150.0f, 580.0f)};
        build_container(&state, &next_key, root, 2);
    }
    EndTree();
}

int
main(void)
{
    LawCheck law = {0};

    LAW_BEGIN(&law, "layout.metrics.content.nonnegative");
    FOR_INT(width, -8, 64) {
        FOR_INT(height, -8, 64) {
            FOR_INT(padding, -4, 40) {
                Rectangle bounds = {3.0f, 5.0f, (float)width, (float)height};
                LayoutMetrics metrics = LayoutMetricsFor(bounds, -2, padding);
                REQUIRE(&law, metrics.gap >= 0);
                REQUIRE(&law, metrics.padding >= 0);
                REQUIRE(&law, metrics.content.width >= 0.0f);
                REQUIRE(&law, metrics.content.height >= 0.0f);
            }
        }
    }

    LAW_BEGIN(&law, "input.drag.monotonic.with.distance");
    FOR_INT(threshold, -4, 12) {
        FOR_INT(dx, -16, 16) {
            FOR_INT(dy, -16, 16) {
                int starts = InputPointerDragShouldStart(dx, dy, threshold);
                int farther_x = dx < 0 ? dx - 1 : dx + 1;
                int farther_y = dy < 0 ? dy - 1 : dy + 1;
                if(starts && abs_i32(dx) >= abs_i32(dy))
                    REQUIRE(&law, InputPointerDragShouldStart(farther_x, dy, threshold));
                if(starts && abs_i32(dy) >= abs_i32(dx))
                    REQUIRE(&law, InputPointerDragShouldStart(dx, farther_y, threshold));
            }
        }
    }

    LAW_BEGIN(&law, "paned_view.split.clamped");
    FOR_INT(min_first, -8, 32) {
        FOR_INT(limit, min_first, 64) {
            FOR_INT(split, -16, 80) {
                int clamped = PanedViewClampSplit(split, min_first, limit);
                REQUIRE(&law, clamped >= min_first);
                REQUIRE(&law, clamped <= limit);
            }
        }
    }

    LAW_BEGIN(&law, "style.priority.deterministic.ordering");
    FOR_INT(layer_a, -2, 2) {
        FOR_INT(layer_b, -2, 2) {
            FOR_INT(spec_a, 0, 4) {
                FOR_INT(spec_b, 0, 4) {
                    FOR_INT(order_a, 0, 4) {
                        FOR_INT(order_b, 0, 4) {
                            StylePriority a = {true, layer_a, spec_a, order_a};
                            StylePriority b = {true, layer_b, spec_b, order_b};
                            int a_wins = StylePriorityWins(a, b);
                            int b_wins = StylePriorityWins(b, a);
                            if(layer_a > layer_b ||
                               (layer_a == layer_b && spec_a > spec_b) ||
                               (layer_a == layer_b && spec_a == spec_b &&
                                order_a > order_b)) {
                                REQUIRE(&law, a_wins);
                                REQUIRE(&law, !b_wins);
                            }
                            if(layer_a == layer_b && spec_a == spec_b &&
                               order_a == order_b) {
                                REQUIRE(&law, a_wins);
                                REQUIRE(&law, b_wins);
                            }
                        }
                    }
                }
            }
        }
    }

    LAW_BEGIN(&law, "style.priority.absent.never.wins");
    {
        StylePriority absent = {false, INT_MAX, INT_MAX, INT_MAX};
        StylePriority present = {true, INT_MIN, 0, 0};
        REQUIRE(&law, !StylePriorityWins(absent, present));
        REQUIRE(&law, StylePriorityWins(present, absent));
    }

    LAW_BEGIN(&law, "layout.tree.deterministic");
    {
        TreeSnapshot first = {0};
        TreeSnapshot second = {0};
        build_generated_tree(0x5eed1234u, 2100);
        snapshot_tree(&first);
        /* Same seed and keys: the second build reconciles onto retained
         * nodes instead of recreating the tree. */
        build_generated_tree(0x5eed1234u, 2100);
        snapshot_tree(&second);
        REQUIRE(&law, first.count > 8);
        REQUIRE(&law, second.count == first.count);
        REQUIRE(&law, snapshots_equal(&first, &second));
    }

    LAW_BEGIN(&law, "layout.tree.idempotent");
    {
        TreeSnapshot before = {0};
        TreeSnapshot after = {0};
        build_generated_tree(0x1d3a5e77u, 2200);
        snapshot_tree(&before);
        InvalidateTree(INVALIDATE_LAYOUT);
        LayoutTree();
        snapshot_tree(&after);
        REQUIRE(&law, after.count == before.count);
        REQUIRE(&law, snapshots_equal(&before, &after));
    }

    LAW_BEGIN(&law, "layout.tree.children.start.in.content");
    for(int run = 0; run < 16; run++) {
        const TreeNode *nodes;
        int count = 0;
        build_generated_tree(0xC0FFEE00u + (uint32_t)run, (KeyID)(2300 + run));
        nodes = GetTreeNodes(&count);
        for(int i = 0; i < count; i++) {
            const TreeNode *parent = &nodes[i];
            LayoutMetrics metrics;
            float eps = 0.01f;
            float prev_end = 0.0f;
            int have_prev = 0;
            int child;
            if(parent->kind != WidgetKindColumn &&
               parent->kind != WidgetKindRow &&
               parent->kind != WidgetKindStack)
                continue;
            metrics = LayoutMetricsFor(parent->bounds, parent->data.layout.gap,
                                       parent->data.layout.padding);
            for(child = parent->first_child; child >= 0;
                child = nodes[child].next_sibling) {
                const TreeNode *node = &nodes[child];
                /* Children with explicit positions skip the auto cursor. */
                if(node->declared_bounds.x != 0.0f ||
                   node->declared_bounds.y != 0.0f)
                    continue;
                REQUIRE(&law, node->bounds.x >= metrics.content.x - eps);
                REQUIRE(&law, node->bounds.y >= metrics.content.y - eps);
                if(parent->kind == WidgetKindColumn) {
                    REQUIRE(&law, node->bounds.x <= metrics.content.x + eps);
                    if(have_prev)
                        REQUIRE(&law, node->bounds.y >= prev_end - eps);
                    prev_end = node->bounds.y + node->bounds.height;
                } else if(parent->kind == WidgetKindRow) {
                    REQUIRE(&law, node->bounds.y <= metrics.content.y + eps);
                    if(have_prev)
                        REQUIRE(&law, node->bounds.x >= prev_end - eps);
                    prev_end = node->bounds.x + node->bounds.width;
                } else {
                    REQUIRE(&law, node->bounds.x <= metrics.content.x + eps);
                    REQUIRE(&law, node->bounds.y <= metrics.content.y + eps);
                }
                have_prev = 1;
            }
        }
    }

    LAW_BEGIN(&law, "layout.tree.bounds.finite");
    for(int run = 0; run < 8; run++) {
        const TreeNode *nodes;
        int count = 0;
        build_generated_tree(0xFACE0000u + (uint32_t)run, (KeyID)(2400 + run));
        nodes = GetTreeNodes(&count);
        REQUIRE(&law, count > 0);
        for(int i = 0; i < count; i++) {
            Rectangle bounds = nodes[i].bounds;
            REQUIRE(&law, isfinite(bounds.x) && isfinite(bounds.y));
            REQUIRE(&law, isfinite(bounds.width) && isfinite(bounds.height));
            REQUIRE(&law, bounds.width >= 0.0f && bounds.height >= 0.0f);
        }
    }

    return lawcheck_finish(&law);
}
