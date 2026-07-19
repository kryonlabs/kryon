#ifndef FLINT_APP_H
#define FLINT_APP_H

#include "flint_editor_host.h"

typedef struct FlintAppScreen {
    const char *id;
    const char *group;
    const char *title;
    void (*enter)(void *app, int screen_index);
    void (*draw)(void *app, Rectangle viewport);
} FlintAppScreen;

typedef struct FlintAppDescriptor {
    void *app;
    const FlintAppScreen *screens;
    int screen_count;
    int selected_screen;
} FlintAppDescriptor;

void FlintBindAppEditorHost(FlintAppDescriptor *app, FlintEditorHost *host);

#endif
