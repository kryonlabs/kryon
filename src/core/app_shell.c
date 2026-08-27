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

void
KryRouteStackInit(KryRouteStack *stack, int *routes, int capacity,
                  int root_route)
{
    if(stack == 0)
        return;
    stack->routes = routes;
    stack->capacity = capacity;
    stack->root_route = root_route;
    stack->count = 0;
    if(routes != 0 && capacity > 0) {
        routes[0] = root_route;
        stack->count = 1;
    }
}

int
KryRouteStackCurrent(const KryRouteStack *stack)
{
    if(stack == 0 || stack->routes == 0 || stack->count <= 0)
        return stack != 0 ? stack->root_route : 0;
    return stack->routes[stack->count - 1];
}

int
KryRouteStackPush(KryRouteStack *stack, int route)
{
    if(stack == 0 || stack->routes == 0 || stack->capacity <= 0)
        return 0;
    if(stack->count <= 0) {
        stack->routes[0] = stack->root_route;
        stack->count = 1;
    }
    if(KryRouteStackCurrent(stack) == route)
        return 1;
    if(stack->count >= stack->capacity)
        return 0;
    stack->routes[stack->count++] = route;
    return 1;
}

int
KryRouteStackPop(KryRouteStack *stack)
{
    if(stack == 0 || stack->routes == 0)
        return 0;
    if(stack->count <= 1)
        return KryRouteStackCurrent(stack);
    stack->count--;
    return KryRouteStackCurrent(stack);
}

void
KryRouteStackReset(KryRouteStack *stack, int root_route)
{
    if(stack == 0)
        return;
    stack->root_route = root_route;
    stack->count = 0;
    if(stack->routes != 0 && stack->capacity > 0) {
        stack->routes[0] = root_route;
        stack->count = 1;
    }
}

KryAppShellLayout
KryAppShellMeasure(KryAppShellLayoutSpec spec)
{
    KryAppShellLayout layout = {0};
    int padding = spec.padding >= 0 ? spec.padding : 0;
    int nav_h = spec.nav_height > 0 ? spec.nav_height : 0;
    int breakpoint = spec.sidebar_breakpoint > 0 ? spec.sidebar_breakpoint : 480;
    int side_w = spec.sidebar_width > 0 ? spec.sidebar_width : 0;
    int available_w;

    layout.compact = spec.view_width <= breakpoint;
    layout.sidebar_width = layout.compact ? 0 : side_w;

    layout.nav_x = spec.safe_left;
    layout.nav_y = spec.view_height - spec.safe_bottom - nav_h;
    layout.nav_width = spec.view_width - spec.safe_left - spec.safe_right;
    layout.nav_height = nav_h;
    if(layout.nav_y < spec.safe_top)
        layout.nav_y = spec.safe_top;
    if(layout.nav_width < 0)
        layout.nav_width = 0;

    layout.content_x = spec.safe_left + layout.sidebar_width + padding;
    layout.content_y = spec.safe_top + padding;
    available_w = spec.view_width - spec.safe_left - spec.safe_right -
                  layout.sidebar_width - padding * 2;
    layout.content_width = available_w;
    if(spec.max_content_width > 0 && layout.content_width > spec.max_content_width)
        layout.content_width = spec.max_content_width;
    if(spec.min_content_width > 0 && layout.content_width < spec.min_content_width)
        layout.content_width = spec.min_content_width;
    layout.content_height = layout.nav_y - layout.content_y - padding;
    if(layout.content_height < 0)
        layout.content_height = 0;
    return layout;
}
