#ifndef PREVIEW_HOST_H
#define PREVIEW_HOST_H

#include "app_host.h"
#include "preview_layers.h"

#define PREVIEW_HOST_ABI_VERSION 1

typedef struct PreviewSceneInfo {
    const char *id;
    const char *group;
    const char *title;
    const char *source_path;
    int width;
    int height;
} PreviewSceneInfo;

typedef struct PreviewHost {
    void *userdata;
    int (*scene_count)(void *userdata);
    PreviewSceneInfo (*scene)(void *userdata, int index);
    void (*select_scene)(void *userdata, int index);
    void (*draw_scene)(void *userdata, Rectangle viewport);
    int (*layer_count)(void *userdata);
    PreviewLayerInfo (*layer)(void *userdata, int index);
} PreviewHost;

typedef PreviewHost *(*CreatePreviewHostCallback)(int abi_version,
                                                  const char *project_path);
typedef void (*DestroyPreviewHostCallback)(PreviewHost *host);

#endif
