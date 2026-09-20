#ifndef ACCESSIBILITY_INTERNAL_H
#define ACCESSIBILITY_INTERNAL_H

#include "ui_accessibility_node.generated.h"

void ui_accessibility_platform_start(const char *title);
void ui_accessibility_platform_close(void);
void ui_accessibility_platform_pump(void);
int ui_accessibility_platform_active(void);
void ui_accessibility_platform_publish(const AccessibilityNode *nodes, int count);
int ui_accessibility_publish_requested(void);
void ui_accessibility_dispatch_snapshot(const AccessibilityNode *nodes, int count);

#endif
