#include "flint_app.h"

static int
flint_app_screen_count(void *userdata)
{
    FlintAppDescriptor *app = userdata;

    if(app == 0 || app->screens == 0 || app->screen_count < 0)
        return 0;
    return app->screen_count;
}

static FlintEditorScreen
flint_app_screen(void *userdata, int index)
{
    FlintAppDescriptor *app = userdata;
    FlintEditorScreen screen = {0};

    if(app == 0 || app->screens == 0 ||
       index < 0 || index >= app->screen_count)
        return screen;

    screen.id = app->screens[index].id;
    screen.group = app->screens[index].group;
    screen.title = app->screens[index].title;
    return screen;
}

static void
flint_app_select_screen(void *userdata, int index)
{
    FlintAppDescriptor *app = userdata;

    if(app == 0 || app->screens == 0 ||
       index < 0 || index >= app->screen_count)
        return;

    app->selected_screen = index;
    if(app->screens[index].enter != 0)
        app->screens[index].enter(app->app, index);
}

static void
flint_app_draw(void *userdata, Rectangle viewport)
{
    FlintAppDescriptor *app = userdata;
    int index;

    if(app == 0 || app->screens == 0 || app->screen_count <= 0)
        return;
    index = app->selected_screen;
    if(index < 0 || index >= app->screen_count)
        index = 0;
    if(app->screens[index].draw != 0)
        app->screens[index].draw(app->app, viewport);
}

void
FlintBindAppEditorHost(FlintAppDescriptor *app, FlintEditorHost *host)
{
    if(app == 0 || host == 0)
        return;

    host->userdata = app;
    host->screen_count = flint_app_screen_count;
    host->screen = flint_app_screen;
    host->select_screen = flint_app_select_screen;
    host->draw = flint_app_draw;
}
