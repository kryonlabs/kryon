#include "ui_internal.h"

#include <stdlib.h>

typedef struct UITabBarScope {
    struct UITabBarScope *previous;
    int count;
    int selected;
    int item_open;
} UITabBarScope;

static UITabBarScope *tab_bar_scope;

void
ui_tab_scope_finish_frame(void)
{
    if(tab_bar_scope != NULL)
        abort();
}

int
BeginTabBar(TabBarProps bar, int *selected_index)
{
    UITabBarScope *scope;
    int clicked;
    int selected;

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 ||
       bar.bounds.height <= 0)
        return 0;

    selected = selected_index != NULL ? *selected_index : bar.selected_index;
    if(selected < 0 || selected >= bar.count)
        selected = 0;
    bar.selected_index = selected;
    clicked = IsWindowReady() ? TabBar(bar) : ui_tab_bar_keyboard_input(bar);
    if(clicked >= 0) {
        selected = clicked;
        if(selected_index != NULL)
            *selected_index = clicked;
    } else if(selected_index != NULL && *selected_index != selected) {
        *selected_index = selected;
    }

    scope = calloc(1, sizeof(*scope));
    if(scope == NULL)
        abort();
    scope->previous = tab_bar_scope;
    scope->count = bar.count;
    scope->selected = selected;
    tab_bar_scope = scope;
    return 1;
}

int
BeginTabItem(int index)
{
    if(tab_bar_scope == NULL || tab_bar_scope->item_open)
        abort();
    if(index < 0 || index >= tab_bar_scope->count ||
       index != tab_bar_scope->selected)
        return 0;
    tab_bar_scope->item_open = 1;
    return 1;
}

void
EndTabItem(void)
{
    if(tab_bar_scope == NULL || !tab_bar_scope->item_open)
        abort();
    tab_bar_scope->item_open = 0;
}

void
EndTabBar(void)
{
    UITabBarScope *scope = tab_bar_scope;

    if(scope == NULL || scope->item_open)
        abort();
    tab_bar_scope = scope->previous;
    free(scope);
}
