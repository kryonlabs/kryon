#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include "app_host.h"

typedef struct App {
    void *app;
    const AppRouteInfo *routes;
    int route_count;
    int selected_route;
} App;

typedef AppRouteInfo AppScreen;

void BindAppHost(App *app, AppHost *host);
int GetAppScreenCount(AppHost *host);
AppScreenInfo GetAppScreen(AppHost *host, int index);
void SetAppScreen(AppHost *host, int index);
int SetAppScreenById(AppHost *host, const char *id);
int SetAppScreenBySourcePath(AppHost *host, const char *source_path);
int SetAppScreenFromRoute(AppHost *host);
void PushAppScreenRoute(AppHost *host, int index);
void ReplaceAppScreenRoute(AppHost *host, int index);
void DrawAppScreen(AppHost *host, Rectangle viewport);

#endif
