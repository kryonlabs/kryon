#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include "app_host.h"

typedef struct AppScreen {
    const char *id;
    const char *group;
    const char *title;
    const char *source_path;
    void (*enter)(void *app, int screen_index);
    void (*draw)(void *app, Rectangle viewport);
} AppScreen;

typedef struct App {
    void *app;
    const AppScreen *screens;
    int screen_count;
    int selected_screen;
} App;

void BindAppHost(App *app, AppHost *host);
int GetAppScreenCount(AppHost *host);
AppScreenInfo GetAppScreen(AppHost *host, int index);
void SetAppScreen(AppHost *host, int index);
int SetAppScreenBySourcePath(AppHost *host, const char *source_path);
void DrawAppScreen(AppHost *host, Rectangle viewport);

#endif
