#include "app_runtime.h"

#include <stdio.h>
#include <string.h>

const char *GetRoutePath(void);
const char *GetRouteHash(void);
void PushRoute(const char *path);
void ReplaceRoute(const char *path);

static int
app_screen_count(void *userdata)
{
    App *app = userdata;

    if(app == 0 || app->routes == 0 || app->route_count < 0)
        return 0;
    return app->route_count;
}

static AppScreenInfo
app_screen(void *userdata, int index)
{
    App *app = userdata;
    AppScreenInfo screen = {0};

    if(app == 0 || app->routes == 0 ||
       index < 0 || index >= app->route_count)
        return screen;

    screen = app->routes[index];
    return screen;
}

static void
app_select_screen(void *userdata, int index)
{
    App *app = userdata;

    if(app == 0 || app->routes == 0 ||
       index < 0 || index >= app->route_count)
        return;

    if(app->selected_route == index)
        return;

    app->selected_route = index;
    if(app->routes[index].enter != 0)
        app->routes[index].enter(app->app, index);
}

static int
app_select_source_path(void *userdata, const char *source_path)
{
    App *app = userdata;

    if(app == 0 || app->routes == 0 || source_path == 0)
        return 0;
    for(int i = 0; i < app->route_count; i++) {
        if(app->routes[i].source_path != 0 &&
           strcmp(app->routes[i].source_path, source_path) == 0) {
            app_select_screen(userdata, i);
            return 1;
        }
    }
    return 0;
}

static void
app_draw(void *userdata, Rectangle viewport)
{
    App *app = userdata;
    int index;

    if(app == 0 || app->routes == 0 || app->route_count <= 0)
        return;
    index = app->selected_route;
    if(index < 0 || index >= app->route_count)
        index = 0;
    if(app->routes[index].draw != 0)
        app->routes[index].draw(app->app, viewport);
}

void
BindAppHost(App *app, AppHost *host)
{
    if(app == 0 || host == 0)
        return;

    host->userdata = app;
    host->screen_count = app_screen_count;
    host->screen = app_screen;
    host->select_screen = app_select_screen;
    host->select_source_path = app_select_source_path;
    host->draw = app_draw;
}

int
GetAppScreenCount(AppHost *host)
{
    if(host == 0 || host->screen_count == 0)
        return 0;
    return host->screen_count(host->userdata);
}

AppScreenInfo
GetAppScreen(AppHost *host, int index)
{
    AppScreenInfo screen = {0};

    if(host == 0 || host->screen == 0)
        return screen;
    return host->screen(host->userdata, index);
}

void
SetAppScreen(AppHost *host, int index)
{
    if(host == 0 || host->select_screen == 0)
        return;
    host->select_screen(host->userdata, index);
}

int
SetAppScreenById(AppHost *host, const char *id)
{
    int count;

    if(host == 0 || id == 0 || id[0] == '\0')
        return 0;
    count = GetAppScreenCount(host);
    for(int i = 0; i < count; i++) {
        AppScreenInfo screen = GetAppScreen(host, i);

        if(screen.id != 0 && strcmp(screen.id, id) == 0) {
            SetAppScreen(host, i);
            return 1;
        }
    }
    return 0;
}

int
SetAppScreenBySourcePath(AppHost *host, const char *source_path)
{
    if(host == 0 || host->select_source_path == 0)
        return 0;
    return host->select_source_path(host->userdata, source_path);
}

static void
app_route_copy_id(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    if(dst == 0 || dst_size == 0)
        return;
    dst[0] = '\0';
    if(src == 0)
        return;
    while(*src == ' ' || *src == '\t' || *src == '#')
        src++;
    if(*src == '/')
        src++;
    while(src[n] != '\0' && src[n] != '/' && src[n] != '?' &&
          src[n] != '&' && n + 1 < dst_size) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

static int
app_route_current_id(char *dst, size_t dst_size)
{
    const char *hash;

    if(dst == 0 || dst_size == 0)
        return 0;
    dst[0] = '\0';
    hash = GetRouteHash();
    if(hash == 0 || hash[0] == '\0')
        return 0;
    app_route_copy_id(dst, dst_size, hash);
    return dst[0] != '\0';
}

int
SetAppScreenFromRoute(AppHost *host)
{
    char id[256];
    int count;

    if(host == 0)
        return 0;
    count = GetAppScreenCount(host);
    if(count <= 0)
        return 0;
    if(app_route_current_id(id, sizeof(id)) && SetAppScreenById(host, id))
        return 1;
    SetAppScreen(host, 0);
    return 0;
}

static void
app_set_screen_route(AppHost *host, int index, int replace)
{
    AppScreenInfo screen;
    char route[320];
    const char *path;

    if(host == 0 || index < 0 || index >= GetAppScreenCount(host))
        return;
    screen = GetAppScreen(host, index);
    if(screen.id == 0 || screen.id[0] == '\0')
        return;
    path = GetRoutePath();
    if(path == 0 || path[0] == '\0')
        path = "/";
    snprintf(route, sizeof(route), "%s#/%s", path, screen.id);
    if(replace)
        ReplaceRoute(route);
    else
        PushRoute(route);
    SetAppScreen(host, index);
}

void
PushAppScreenRoute(AppHost *host, int index)
{
    app_set_screen_route(host, index, 0);
}

void
ReplaceAppScreenRoute(AppHost *host, int index)
{
    app_set_screen_route(host, index, 1);
}

void
DrawAppScreen(AppHost *host, Rectangle viewport)
{
    if(host == 0 || host->draw == 0)
        return;
    host->draw(host->userdata, viewport);
}

void
ResizeAppHost(AppHost *host, int width, int height)
{
    if(host == 0 || host->resize == 0)
        return;
    host->resize(host->userdata, width, height);
}

void
SetAppHostFocused(AppHost *host, int focused)
{
    if(host == 0 || host->set_focused == 0)
        return;
    host->set_focused(host->userdata, focused);
}
