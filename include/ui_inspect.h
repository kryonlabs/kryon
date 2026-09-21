#ifndef KRYON_INSPECT_H
#define KRYON_INSPECT_H

#include "kryon_compat.generated.h"
#include "ui_inspect_props.generated.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
