#include "app_runtime.h"

#include <string.h>

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
SetAppScreenBySourcePath(AppHost *host, const char *source_path)
{
    if(host == 0 || host->select_source_path == 0)
        return 0;
    return host->select_source_path(host->userdata, source_path);
}

void
DrawAppScreen(AppHost *host, Rectangle viewport)
{
    if(host == 0 || host->draw == 0)
        return;
    host->draw(host->userdata, viewport);
}
