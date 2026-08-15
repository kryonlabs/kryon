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

typedef struct UIInspectNode {
    char name[96];
    char role[32];
    char text[64];
    char value[128];
    char source_path[512];
    Rectangle bounds;
    int flags;
    int order;
    int parent;
    int source_line;
    int valid;
} UIInspectNode;

void BeginUIInspectFrame(const char *project_root);
void EndUIInspectFrame(void);
void SetUIInspectEnabled(int enabled);
void SetUIInspectVisible(int visible);
int UIInspectEnabled(void);
int UIInspectWidgetCount(void);
int UIInspectNodeCount(void);
int UIInspectGetNode(int index, UIInspectNode *node);
int UIInspectFindNode(const char *selector, UIInspectNode *node);
UIInspectSelection UIInspectGetSelection(void);
int UIInspectSelectAt(Vector2 point);
void SetUIInspectCanvasBounds(Rectangle bounds);
int PushUIInspectTransform(Camera2D camera);
void PopUIInspectTransform(int token);
int PushUIInspectChrome(int enabled);
void PopUIInspectChrome(int token);
int UIInspectInputCapturesClick(Vector2 point);
void PushUIInspectSource(const char *path, int line);
void PopUIInspectSource(void);

#endif
