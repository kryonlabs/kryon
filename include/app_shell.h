#ifndef KRYON_APP_SHELL_H
#define KRYON_APP_SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*KryRouteAllowedFn)(int route, void *user);

typedef struct KryRouteList {
    int *routes;
    int count;
    int capacity;
    int empty_route;
} KryRouteList;

typedef struct KryRouteStack {
    int *routes;
    int count;
    int capacity;
    int root_route;
} KryRouteStack;

typedef struct KryAppShellLayoutSpec {
    int view_width;
    int view_height;
    int safe_left;
    int safe_top;
    int safe_right;
    int safe_bottom;
    int padding;
    int nav_height;
    int sidebar_breakpoint;
    int sidebar_width;
    int min_content_width;
    int max_content_width;
} KryAppShellLayoutSpec;

typedef struct KryAppShellLayout {
    int compact;
    int sidebar_width;
    int content_x;
    int content_y;
    int content_width;
    int content_height;
    int nav_x;
    int nav_y;
    int nav_width;
    int nav_height;
} KryAppShellLayout;

int KryRouteListContains(const int *routes, int count, int route);
int KryRouteAllowedInSet(int route, const int *allowed_routes,
                         int allowed_count);
int KryRouteListSanitize(KryRouteList list, KryRouteAllowedFn allowed,
                         void *user);
int KryRouteListSanitizeSet(KryRouteList list, const int *allowed_routes,
                            int allowed_count);
int KryRouteListMove(KryRouteList list, int from_index, int to_index);
int KryRouteListFirstUnused(const int *routes, int count,
                            const int *candidates, int candidate_count,
                            int fallback);
void KryRouteStackInit(KryRouteStack *stack, int *routes, int capacity,
                       int root_route);
int KryRouteStackCurrent(const KryRouteStack *stack);
int KryRouteStackPush(KryRouteStack *stack, int route);
int KryRouteStackPop(KryRouteStack *stack);
void KryRouteStackReset(KryRouteStack *stack, int root_route);
KryAppShellLayout KryAppShellMeasure(KryAppShellLayoutSpec spec);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_APP_SHELL_H */
