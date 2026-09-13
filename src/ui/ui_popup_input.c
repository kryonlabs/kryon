#include "ui_popup_input_internal.h"
#include <stdlib.h>
#include <limits.h>

typedef struct PopupPanel {
    struct PopupPanel *next, *parent;
    Rectangle bounds;
    unsigned long seen, order;
    int owner, alive;
    int restore_focus, last_focus, has_last_focus, autofocus;
} PopupPanel;

typedef struct PopupFocus {
    struct PopupFocus *next;
    PopupInputToken token;
    int id;
} PopupFocus;

struct PopupInput {
    struct PopupInput *next;
    PopupPanel *panels, *active;
    PopupFocus *focus;
    unsigned long frame, order;
    int finished, keyboard_captured;
};

static PopupInput *bound_context;
static PopupInput *contexts;
static unsigned long generation;

static void clear_focus(PopupInput *context)
{
    while(context->focus) {
        PopupFocus *entry = context->focus;
        context->focus = entry->next;
        free(entry);
    }
}

static int descends(PopupPanel *panel, PopupPanel *ancestor)
{
    for(; panel; panel = panel->parent) if(panel == ancestor) return 1;
    return 0;
}

static int above(PopupPanel *a, PopupPanel *b)
{
    if(descends(a,b)) return a != b;
    if(descends(b,a)) return 0;
    int ad = 0, bd = 0;
    for(PopupPanel *p = a; p; p = p->parent) ad++;
    for(PopupPanel *p = b; p; p = p->parent) bd++;
    while(ad > bd) { a = a->parent; ad--; }
    while(bd > ad) { b = b->parent; bd--; }
    while(a->parent != b->parent) { a = a->parent; b = b->parent; }
    return a->order > b->order;
}

static PopupPanel *top_panel(PopupInput *context)
{
    PopupPanel *top = NULL;
    if(!context) return NULL;
    for(PopupPanel *panel = context->panels; panel; panel = panel->next)
        if(panel->alive && (!top || above(panel,top))) top = panel;
    return top;
}

PopupInput *ui_popup_input_create(void)
{
    PopupInput *context = calloc(1,sizeof(*context));
    if(!context) abort();
    context->finished = 1;
    context->next = contexts;
    contexts = context;
    return context;
}

PopupInput *ui_popup_input_bind(PopupInput *context)
{
    PopupInput *previous = bound_context;
    bound_context = context;
    return previous;
}

PopupInput *ui_popup_input_bound(void) { return bound_context; }

void ui_popup_input_destroy(PopupInput *context)
{
    if(!context) return;
    if(context->active) abort();
    clear_focus(context);
    if(bound_context == context) bound_context = NULL;
    PopupInput **link = &contexts;
    while(*link && *link != context) link = &(*link)->next;
    if(*link) *link = context->next;
    while(context->panels) {
        PopupPanel *panel = context->panels;
        context->panels = panel->next;
        free(panel);
    }
    free(context);
}

void ui_popup_input_frame(PopupInput *context)
{
    if(!context || context->active || !context->finished || generation == ULONG_MAX) abort();
    context->frame = ++generation;
    clear_focus(context);
    context->finished = 0;
    context->keyboard_captured = 0;
}

PopupInputToken ui_popup_input_begin(PopupInput *context, int owner, Rectangle bounds)
{
    if(!context || context->finished || context->order == ULONG_MAX) abort();
    PopupPanel *panel = context->panels;
    while(panel && panel->owner != owner) panel = panel->next;
    if(panel && descends(context->active,panel)) abort();
    if(!panel) {
        panel = calloc(1,sizeof(*panel));
        if(!panel) abort();
        panel->next = context->panels;
        context->panels = panel;
        panel->owner = owner;
        panel->restore_focus = GetFocus();
        panel->autofocus = 1;
    }
    panel->parent = context->active;
    panel->bounds = bounds;
    panel->seen = context->frame;
    panel->order = ++context->order;
    panel->alive = !panel->parent || panel->parent->alive;
    context->active = panel;
    return (PopupInputToken){context,context->frame,panel->order,owner};
}

void ui_popup_input_end(PopupInputToken token)
{
    PopupInput *context = token.context;
    if(!context || context->frame != token.generation || !context->active ||
       context->active->owner != token.owner || context->active->order != token.order) abort();
    context->active = context->active->parent;
}

void ui_popup_input_close(PopupInput *context, int owner)
{
    if(!context) abort();
    for(PopupPanel *panel = context->panels; panel; panel = panel->next) {
        if(panel->owner != owner) continue;
        int focused = GetFocus(), restore = 0;
        for(PopupFocus *entry = context->focus; entry; entry = entry->next) {
            if(entry->id != focused || !entry->token.order) continue;
            for(PopupPanel *child = context->panels; child; child = child->next)
                if(child->owner == entry->token.owner &&
                   child->order == entry->token.order && descends(child,panel))
                    restore = 1;
        }
        for(PopupPanel *child = context->panels; child; child = child->next) {
            if(!descends(child,panel)) continue;
            if(child->has_last_focus && child->last_focus == focused) restore = 1;
            child->alive = 0;
        }
        if(restore) SetFocus(panel->restore_focus);
        return;
    }
}

void ui_popup_input_retire_missing(PopupInput *context)
{
    if(!context || context->finished) return;
    for(PopupPanel *panel = context->panels; panel; panel = panel->next)
        if(panel->seen != context->frame) ui_popup_input_close(context,panel->owner);
}

void ui_popup_input_finish(PopupInput *context)
{
    if(!context || context->active || context->finished) abort();
    ui_popup_input_retire_missing(context);
    PopupPanel **link = &context->panels;
    while(*link) {
        PopupPanel *panel = *link;
        if(!panel->alive) { *link = panel->next; free(panel); }
        else link = &panel->next;
    }
    context->finished = 1;
}

static int captures(PopupInput *context, PopupPanel *active, Vector2 point)
{
    if(!context) return 0;
    if(active && !active->alive) return 1;
    PopupPanel *top = NULL;
    for(PopupPanel *panel = context->panels; panel; panel = panel->next)
        if(panel->alive && CheckCollisionPointRec(point,panel->bounds) && (!top || above(panel,top))) top = panel;
    return top && top != active;
}

int ui_popup_input_captures(PopupInput *context, Vector2 point)
{
    return captures(context,context ? context->active : NULL,point);
}

PopupInputToken ui_popup_input_snapshot(void)
{
    PopupInput *context = bound_context;
    if(!context) return (PopupInputToken){0};
    PopupPanel *panel = context->active;
    return (PopupInputToken){context,context->frame,
        panel ? panel->order : 0,panel ? panel->owner : 0};
}

static int snapshot_owner(PopupInputToken token, PopupInput **host, PopupPanel **owner)
{
    /* A background declaration may precede lazy creation of the host registry. */
    *owner = NULL;
    if(!token.context) { *host = bound_context; return 1; }
    PopupInput *context = contexts;
    while(context && context != token.context) context = context->next;
    if(!context || context->frame != token.generation) return 0;
    PopupPanel *panel = NULL;
    if(token.order) {
        for(panel = context->panels; panel; panel = panel->next)
            if(panel->owner == token.owner && panel->order == token.order) break;
        if(!panel || !panel->alive) return 0;
    }
    *host = context;
    *owner = panel;
    return 1;
}

int ui_popup_input_snapshot_captures(PopupInputToken token, Vector2 point)
{
    PopupInput *context;
    PopupPanel *panel;
    if(!snapshot_owner(token,&context,&panel)) return 1;
    return captures(context,panel,point);
}

int ui_popup_input_current_captures(Vector2 point)
{
    return ui_popup_input_captures(bound_context,point);
}

int ui_popup_input_snapshot_keyboard_captures(PopupInputToken token)
{
    PopupInput *context;
    PopupPanel *active;
    if(!snapshot_owner(token,&context,&active)) {
        if(bound_context) bound_context->keyboard_captured = 1;
        return 1;
    }
    if(!context) return 0;
    PopupPanel *top = NULL;
    for(PopupPanel *panel = context->panels; panel; panel = panel->next)
        if(panel->alive && (!top || above(panel,top))) top = panel;
    int captured = top && top != active;
    if(captured) context->keyboard_captured = 1;
    return captured;
}

int ui_popup_input_keyboard_captures(void)
{
    return ui_popup_input_snapshot_keyboard_captures(ui_popup_input_snapshot());
}

int ui_popup_input_keyboard_was_captured(void)
{
    return bound_context && bound_context->keyboard_captured;
}

PopupInputOwner ui_popup_input_owner(void)
{
    PopupInputOwner owner = {bound_context,0,0};
    if(bound_context && bound_context->active) {
        owner.owner = bound_context->active->owner;
        owner.has_owner = 1;
    }
    return owner;
}

int ui_popup_input_owner_captures(PopupInputOwner owner)
{
    PopupInput *context = contexts;
    PopupPanel *current;
    PopupPanel *top;

    if(owner.context == NULL)
        return bound_context != NULL;
    while(context && context != owner.context) context = context->next;
    if(!context || bound_context != context) return 1;
    current = context->active;
    if(owner.has_owner) {
        if(!current || current->owner != owner.owner || !current->alive)
            return 1;
    } else if(current) {
        return 1;
    }
    top = top_panel(context);
    return top && (!owner.has_owner || top != current);
}

void ui_popup_input_register_focus(int id, PopupInputToken token, int eligible)
{
    PopupInput *context = token.context ? token.context : bound_context;
    if(!context || id <= 0) return;
    PopupInput *live = contexts;
    while(live && live != context) live = live->next;
    if(!live) return;
    if(!token.context) token = (PopupInputToken){context,context->frame,0,0};
    PopupFocus *entry = context->focus;
    while(entry && entry->id != id) entry = entry->next;
    if(!entry) {
        entry = calloc(1,sizeof(*entry));
        if(!entry) abort();
        entry->next = context->focus;
        context->focus = entry;
        entry->id = id;
    }
    entry->token = token;
    if(token.order) {
        PopupPanel *panel = context->panels;
        while(panel && (panel->owner != token.owner ||
              panel->order != token.order)) panel = panel->next;
        if(panel && panel->alive) {
            if(eligible && panel->autofocus &&
               !ui_popup_input_snapshot_keyboard_captures(token)) {
                SetFocus(id);
                panel->autofocus = 0;
            }
            if(GetFocus() == id) {
                panel->last_focus = id;
                panel->has_last_focus = 1;
            }
        }
    }
}

int ui_popup_input_focus_captures(int id)
{
    PopupInput *context = bound_context;
    if(!context) return 0;
    for(PopupFocus *entry = context->focus; entry; entry = entry->next)
        if(entry->id == id) return ui_popup_input_snapshot_keyboard_captures(entry->token);
    return ui_popup_input_snapshot_keyboard_captures(
        (PopupInputToken){context,context->frame,0,0});
}
