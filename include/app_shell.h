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

#ifdef __cplusplus
}
#endif

#endif /* KRYON_APP_SHELL_H */
