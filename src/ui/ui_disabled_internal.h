#ifndef KRYON_UI_DISABLED_INTERNAL_H
#define KRYON_UI_DISABLED_INTERNAL_H
typedef struct DisabledScopeState { int depth, start, floor; } DisabledScopeState;
DisabledScopeState ui_disabled_suspend(void);
void ui_disabled_resume(DisabledScopeState scope);
#endif
