#include "accessibility_internal.h"
#include "ui_tree.h"

static AccessibilitySink accessibility_sink;
static void *accessibility_sink_userdata;

#if defined(__GNUC__) || defined(__clang__)
extern void kry_platform_accessibility_snapshot(
    const AccessibilityNode *nodes, int count) __attribute__((weak));
#else
static void (*kry_platform_accessibility_snapshot)(
    const AccessibilityNode *nodes, int count);
#endif

void
SetAccessibilitySink(AccessibilitySink sink, void *userdata)
{
    accessibility_sink = sink;
    accessibility_sink_userdata = userdata;
}

int
ui_accessibility_publish_requested(void)
{
    return accessibility_sink != NULL ||
           kry_platform_accessibility_snapshot != NULL ||
           ui_accessibility_platform_active();
}

void
ui_accessibility_dispatch_snapshot(const AccessibilityNode *nodes, int count)
{
    ui_accessibility_platform_publish(nodes, count);
    if(kry_platform_accessibility_snapshot != NULL)
        kry_platform_accessibility_snapshot(nodes, count);
    if(accessibility_sink != NULL)
        accessibility_sink(nodes, count, accessibility_sink_userdata);
}
