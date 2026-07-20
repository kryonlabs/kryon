#ifndef APP_HOST_H
#define APP_HOST_H

#include "kryon_compat.generated.h"

#define APP_HOST_ABI_VERSION 3

typedef struct AppScreenInfo {
    const char *id;
    const char *group;
    const char *title;
    const char *source_path;
} AppScreenInfo;

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

int IsUIInspectActive(void);

#endif
