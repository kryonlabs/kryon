#ifndef FLINT_EDITOR_HOST_H
#define FLINT_EDITOR_HOST_H

#include "flint_compat.generated.h"

#define FLINT_EDITOR_HOST_ABI_VERSION 1

typedef struct FlintEditorScreen {
    const char *id;
    const char *group;
    const char *title;
} FlintEditorScreen;

typedef struct FlintEditorHost {
    void *userdata;
    int (*screen_count)(void *userdata);
    FlintEditorScreen (*screen)(void *userdata, int index);
    void (*select_screen)(void *userdata, int index);
    void (*draw)(void *userdata, Rectangle viewport);
} FlintEditorHost;

typedef FlintEditorHost *(*FlintEditorHostCreateFunc)(int abi_version,
                                                      const char *project_path);
typedef void (*FlintEditorHostDestroyFunc)(FlintEditorHost *host);

#endif
