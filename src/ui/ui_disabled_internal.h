#ifndef KRYON_UI_DISABLED_INTERNAL_H
#define KRYON_UI_DISABLED_INTERNAL_H
typedef struct UIDisabledScope { int depth, start, floor; } UIDisabledScope;
UIDisabledScope ui_disabled_suspend(void);
void ui_disabled_resume(UIDisabledScope scope);
#endif
