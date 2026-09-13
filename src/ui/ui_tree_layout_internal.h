#ifndef KRYON_UI_TREE_LAYOUT_INTERNAL_H
#define KRYON_UI_TREE_LAYOUT_INTERNAL_H

#include "kryon.h"
#define TREE_LAYOUT_DEPTH 128
typedef struct TreeLayoutScopeState {
    NodeId stack[TREE_LAYOUT_DEPTH];
    int depth, building;
    unsigned long declaration;
} TreeLayoutScopeState;

TreeLayoutScopeState ui_tree_layout_suspend(void);
void ui_tree_layout_resume(TreeLayoutScopeState scope);
#endif
