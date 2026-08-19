#include "app_shell.h"

int
KryRouteListContains(const int *routes, int count, int route)
{
    if(routes == 0 || count <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        if(routes[i] == route)
            return 1;
    }
    return 0;
}

int
KryRouteAllowedInSet(int route, const int *allowed_routes, int allowed_count)
{
    return KryRouteListContains(allowed_routes, allowed_count, route);
}

typedef struct KryRouteAllowedSet {
    const int *routes;
    int count;
} KryRouteAllowedSet;

static int
route_allowed_set_cb(int route, void *user)
{
    KryRouteAllowedSet *set = user;

    return set != 0 && KryRouteAllowedInSet(route, set->routes, set->count);
}

int
KryRouteListSanitize(KryRouteList list, KryRouteAllowedFn allowed, void *user)
{
    int write = 0;

    if(list.routes == 0 || list.capacity <= 0)
        return 0;
    if(list.count < 0 || list.count > list.capacity)
        list.count = list.capacity;
    for(int i = 0; i < list.count; i++) {
        int route = list.routes[i];

        if(allowed != 0 && !allowed(route, user))
            continue;
        if(KryRouteListContains(list.routes, write, route))
            continue;
        list.routes[write++] = route;
    }
    for(int i = write; i < list.capacity; i++)
        list.routes[i] = list.empty_route;
    return write;
}

int
KryRouteListSanitizeSet(KryRouteList list, const int *allowed_routes,
                        int allowed_count)
{
    KryRouteAllowedSet set = {allowed_routes, allowed_count};

    return KryRouteListSanitize(list, route_allowed_set_cb, &set);
}

int
KryRouteListMove(KryRouteList list, int from_index, int to_index)
{
    int route;

    if(list.routes == 0 || list.count <= 0)
        return 0;
    if(from_index < 0 || to_index < 0 ||
       from_index >= list.count || to_index >= list.count ||
       from_index == to_index)
        return 0;
    route = list.routes[from_index];
    if(from_index < to_index) {
        for(int i = from_index; i < to_index; i++)
            list.routes[i] = list.routes[i + 1];
    } else {
        for(int i = from_index; i > to_index; i--)
            list.routes[i] = list.routes[i - 1];
    }
    list.routes[to_index] = route;
    return 1;
}

int
KryRouteListFirstUnused(const int *routes, int count,
                        const int *candidates, int candidate_count,
                        int fallback)
{
    if(candidates == 0 || candidate_count <= 0)
        return fallback;
    for(int i = 0; i < candidate_count; i++) {
        if(!KryRouteListContains(routes, count, candidates[i]))
            return candidates[i];
    }
    return fallback;
}
