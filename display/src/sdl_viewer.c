/*
 * SDL2 Window Viewer for Kryon
 * This connects to the Kryon server and displays the graphics in a real SDL2 window
 * C89/C90 compliant
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "p9client.h"

/*
 * usleep prototype for C89 compatibility
 */
extern int usleep(unsigned int usec);

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600
#define SERVER_PORT   17019
#define SERVER_HOST   "127.0.0.1"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint8_t *pixels;
    P9Client *p9;
    int screen_fd;
    int running;
} KryonViewer;

/*
 * Connect to Kryon server via 9P
 */
static int connect_to_server(KryonViewer *viewer, const char *host, int port)
{
    /* Connect to Kryon server */
    viewer->p9 = p9_client_connect(host, port);
    if (viewer->p9 == NULL) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        return -1;
    }

    /* Attach to server */
    if (p9_client_attach(viewer->p9, "") < 0) {
        fprintf(stderr, "Failed to attach: %s\n", p9_client_error(viewer->p9));
        p9_client_disconnect(viewer->p9);
        viewer->p9 = NULL;
        return -1;
    }

    /* Open screen device */
    viewer->screen_fd = p9_client_open_path(viewer->p9, "/dev/screen", P9_OREAD);
    if (viewer->screen_fd < 0) {
        fprintf(stderr, "Failed to open /dev/screen: %s\n", p9_client_error(viewer->p9));
        p9_client_disconnect(viewer->p9);
        viewer->p9 = NULL;
        return -1;
    }

    printf("Connected to Kryon server at %s:%d\n", host, port);
    return 0;
}

/*
 * Read screen from server
 */
static int read_screen(KryonViewer *viewer)
{
    ssize_t nread;
    size_t total;
    size_t to_read;

    total = 0;
    to_read = SCREEN_WIDTH * SCREEN_HEIGHT * 4;

    while (total < to_read) {
        nread = p9_client_read(viewer->p9, viewer->screen_fd,
                               (char *)(viewer->pixels + total),
                               to_read - total);
        if (nread < 0) {
            fprintf(stderr, "Error reading screen\n");
            return -1;
        }
        if (nread == 0) {
            break;
        }
        total += nread;
    }

    return 0;
}

/*
 * Initialize SDL viewer
 */
static KryonViewer* viewer_create(const char *host, int port)
{
    KryonViewer *viewer;

    viewer = (KryonViewer *)calloc(1, sizeof(KryonViewer));
    if (!viewer) return NULL;

    /* Connect to server first */
    if (connect_to_server(viewer, host, port) < 0) {
        free(viewer);
        return NULL;
    }

    /* Allocate screen buffer (RGBA32) */
    viewer->pixels = (uint8_t *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    if (!viewer->pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        p9_client_clunk(viewer->p9, viewer->screen_fd);
        p9_client_disconnect(viewer->p9);
        free(viewer);
        return NULL;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free(viewer->pixels);
        p9_client_clunk(viewer->p9, viewer->screen_fd);
        p9_client_disconnect(viewer->p9);
        free(viewer);
        return NULL;
    }

    viewer->window = SDL_CreateWindow(
        "Kryon Display - /dev/screen",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!viewer->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(viewer->pixels);
        p9_client_clunk(viewer->p9, viewer->screen_fd);
        p9_client_disconnect(viewer->p9);
        free(viewer);
        return NULL;
    }

    viewer->renderer = SDL_CreateRenderer(viewer->window, -1, SDL_RENDERER_ACCELERATED);
    if (!viewer->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(viewer->window);
        SDL_Quit();
        free(viewer->pixels);
        p9_client_clunk(viewer->p9, viewer->screen_fd);
        p9_client_disconnect(viewer->p9);
        free(viewer);
        return NULL;
    }

    viewer->texture = SDL_CreateTexture(
        viewer->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT
    );

    if (!viewer->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(viewer->renderer);
        SDL_DestroyWindow(viewer->window);
        SDL_Quit();
        free(viewer->pixels);
        p9_client_clunk(viewer->p9, viewer->screen_fd);
        p9_client_disconnect(viewer->p9);
        free(viewer);
        return NULL;
    }

    viewer->running = 1;

    return viewer;
}

/*
 * Destroy viewer
 */
static void viewer_destroy(KryonViewer *viewer)
{
    if (!viewer) return;

    if (viewer->texture) SDL_DestroyTexture(viewer->texture);
    if (viewer->renderer) SDL_DestroyRenderer(viewer->renderer);
    if (viewer->window) SDL_DestroyWindow(viewer->window);
    SDL_Quit();

    if (viewer->pixels) free(viewer->pixels);

    if (viewer->screen_fd >= 0) {
        p9_client_clunk(viewer->p9, viewer->screen_fd);
    }

    if (viewer->p9) {
        p9_client_disconnect(viewer->p9);
    }

    free(viewer);
}

/*
 * Update the display
 */
static void viewer_update(KryonViewer *viewer)
{
    SDL_UpdateTexture(viewer->texture, NULL, viewer->pixels, SCREEN_WIDTH * 4);
    SDL_RenderClear(viewer->renderer);
    SDL_RenderCopy(viewer->renderer, viewer->texture, NULL, NULL);
    SDL_RenderPresent(viewer->renderer);
}

/*
 * Main loop
 */
int main(void)
{
    KryonViewer *viewer;
    SDL_Event event;
    int frame_count = 0;

    printf("=== Kryon SDL2 Viewer ===\n\n");

    /* Create viewer (connects to server) */
    viewer = viewer_create(SERVER_HOST, SERVER_PORT);
    if (!viewer) {
        return 1;
    }

    printf("Viewer running! Press ESC or close window to exit.\n");
    printf("Displaying real-time graphics from Kryon server.\n\n");

    /* Event loop */
    while (viewer->running) {
        /* Read screen at ~60 FPS */
        if (read_screen(viewer) < 0) {
            break;
        }

        /* Update display */
        viewer_update(viewer);

        /* Process SDL events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                viewer->running = 0;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    viewer->running = 0;
                }
            }
        }

        /* Sleep for ~60 FPS */
        usleep(16667);

        frame_count++;
    }

    /* Cleanup */
    viewer_destroy(viewer);

    printf("Viewer closed.\n");
    return 0;
}
