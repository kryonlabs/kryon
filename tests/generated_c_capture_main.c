#include "kryon.h"
#include "kryon_example_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AppHost *CreateAppHost(int abi_version, const char *project_path);
void DestroyAppHost(AppHost *host);

static int app_token;

void *
CreateApp(const char *project_path)
{
    (void)project_path;
    return &app_token;
}

void
DestroyApp(void *app)
{
    (void)app;
}

void
ApplyRoute(void *app, const AppRouteInfo *route)
{
    (void)app;
    (void)route;
}

void
BeginScreenDraw(void *app, Rectangle viewport)
{
    (void)app;
    (void)viewport;
}

static void
usage(void)
{
    fprintf(stderr,
            "usage: generated-c-capture [--png out.png] [--w W] [--h H] "
            "[--source path] [--frames N] [--hold-before-capture-ms N]\n");
}

int
main(int argc, char **argv)
{
    const char *png_path = NULL;
    const char *source_path = NULL;
    int w = 480;
    int h = 640;
    int frames = 2;
    int hold_before_capture_ms = 0;
    int i;
    AppHost *host;
    Image shot;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            png_path = argv[++i];
        else if(strcmp(argv[i], "--w") == 0 && i + 1 < argc)
            w = atoi(argv[++i]);
        else if(strcmp(argv[i], "--h") == 0 && i + 1 < argc)
            h = atoi(argv[++i]);
        else if(strcmp(argv[i], "--source") == 0 && i + 1 < argc)
            source_path = argv[++i];
        else if(strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if(strcmp(argv[i], "--hold-before-capture-ms") == 0 && i + 1 < argc)
            hold_before_capture_ms = atoi(argv[++i]);
        else {
            usage();
            return 2;
        }
    }
    if(png_path == NULL) {
        usage();
        return 2;
    }
    if(w <= 0 || h <= 0 || frames <= 0) {
        fprintf(stderr, "generated-c-capture: invalid dimensions or frames\n");
        return 2;
    }

    InitWindow(w, h, "Kryon generated C capture");
    if(!IsWindowReady()) {
        fprintf(stderr, "generated-c-capture: window not ready\n");
        return 1;
    }
    SetTargetFPS(60);
    InitUI(w, h, GetUIScale());
    LoadExampleUIFont();

    host = CreateAppHost(APP_HOST_ABI_VERSION, ".");
    if(host == NULL) {
        fprintf(stderr, "generated-c-capture: CreateAppHost failed\n");
        CloseWindow();
        return 1;
    }
    if(source_path != NULL && !SetAppScreenBySourcePath(host, source_path)) {
        fprintf(stderr,
                "generated-c-capture: source route not found: %s\n",
                source_path);
        DestroyAppHost(host);
        CloseWindow();
        return 1;
    }

    for(i = 0; i < frames && !WindowShouldClose(); i++) {
        BeginDrawing();
        ClearBackground(BLANK);
        BeginFrame();
        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());
        BeginTree(Key("generated-c-capture"));
        DrawAppScreen(host, (Rectangle){0, 0, (float)GetScreenWidth(),
                                        (float)GetScreenHeight()});
        EndTree();
        EndUIFrame();
        EndFrame();
        EndDrawing();
    }
    if(hold_before_capture_ms > 0)
        WaitTime((double)hold_before_capture_ms / 1000.0);

    shot = LoadImageFromScreen();
    if(shot.data == NULL) {
        fprintf(stderr, "generated-c-capture: screen capture failed\n");
        DestroyAppHost(host);
        CloseWindow();
        return 1;
    }
    if(!ExportImage(shot, png_path)) {
        fprintf(stderr, "generated-c-capture: cannot write %s\n", png_path);
        UnloadImage(shot);
        DestroyAppHost(host);
        CloseWindow();
        return 1;
    }

    UnloadImage(shot);
    DestroyAppHost(host);
    UnloadExampleUIFont();
    CloseWindow();
    return 0;
}
