#ifndef APP_HOST_H
#define APP_HOST_H

#include "kryon_compat.generated.h"

#define APP_HOST_ABI_VERSION 3

typedef struct KryonInputOverride {
    int enabled;
    int mouse_inside;
    int pass_buttons;
    Vector2 mouse_position;
    Vector2 mouse_delta;
} KryonInputOverride;

typedef struct AppRouteInfo {
    const char *id;
    const char *group;
    const char *title;
    const char *source_path;
    void (*enter)(void *app, int route_index);
    void (*draw)(void *app, Rectangle viewport);
} AppRouteInfo;

typedef AppRouteInfo AppScreenInfo;

typedef struct AppHost {
    void *userdata;
    int (*screen_count)(void *userdata);
    AppScreenInfo (*screen)(void *userdata, int index);
    void (*select_screen)(void *userdata, int index);
    int (*select_source_path)(void *userdata, const char *source_path);
    void (*draw)(void *userdata, Rectangle viewport);
} AppHost;

typedef AppHost *(*CreateAppHostCallback)(int abi_version,
                                          const char *project_path);
typedef void (*DestroyAppHostCallback)(AppHost *host);

void BeginKryonInputOverride(KryonInputOverride input);
void EndKryonInputOverride(void);
int IsUIInspectActive(void);

#endif
