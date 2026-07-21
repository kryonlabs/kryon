#ifndef UI_INSPECT_H
#define UI_INSPECT_H

#include "ui_widget.h"

typedef struct UIInspectSelection {
    char id[96];
    char kind[32];
    char action[64];
    char source_path[512];
    Rectangle bounds;
    int flags;
    int kind_index;
    int source_line;
    int valid;
} UIInspectSelection;

void BeginUIInspectFrame(const char *project_root);
void EndUIInspectFrame(void);
void SetUIInspectEnabled(int enabled);
int UIInspectEnabled(void);
int UIInspectWidgetCount(void);
UIInspectSelection UIInspectGetSelection(void);
int UIInspectSelectAt(Vector2 point);
void SetUIInspectCanvasBounds(Rectangle bounds);
int PushUIInspectChrome(int enabled);
void PopUIInspectChrome(int token);
int UIInspectInputCapturesClick(Vector2 point);
void PushUIInspectSource(const char *path, int line);
void PopUIInspectSource(void);
void DrawUIInspectOverlay(void);

#endif
