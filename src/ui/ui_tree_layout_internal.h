#ifndef KRYON_UI_TREE_LAYOUT_INTERNAL_H
#define KRYON_UI_TREE_LAYOUT_INTERNAL_H

#include "kryon.h"
#define UI_TREE_LAYOUT_DEPTH 128
typedef struct UITreeLayoutScope {
    NodeId stack[UI_TREE_LAYOUT_DEPTH];
    int depth, building;
    unsigned long declaration;
} UITreeLayoutScope;

UITreeLayoutScope ui_tree_layout_suspend(void);
void ui_tree_layout_resume(UITreeLayoutScope scope);
#endif
