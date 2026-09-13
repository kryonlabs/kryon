#ifndef KRYON_INSPECT_H
#define KRYON_INSPECT_H

#include "kryon_compat.generated.h"

typedef struct InspectSelection {
    char id[96];
    char kind[32];
    char action[64];
    char source_path[512];
    Rectangle bounds;
    int flags;
    int kind_index;
    int source_line;
    int valid;
} InspectSelection;

typedef struct InspectNode {
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
} InspectNode;

void BeginInspectFrame(const char *project_root);
void EndInspectFrame(void);
void SetInspectEnabled(int enabled);
void SetInspectVisible(int visible);
int InspectEnabled(void);
int InspectWidgetCount(void);
int InspectNodeCount(void);
int InspectGetNode(int index, InspectNode *node);
int InspectFindNode(const char *selector, InspectNode *node);
InspectSelection InspectGetSelection(void);
int InspectSelectAt(Vector2 point);
void SetInspectCanvasBounds(Rectangle bounds);
int PushInspectTransform(Camera2D camera);
void PopInspectTransform(int token);
int PushInspectChrome(int enabled);
void PopInspectChrome(int token);
int InspectInputCapturesClick(Vector2 point);
void PushInspectSource(const char *path, int line);
void PopInspectSource(void);

#endif
