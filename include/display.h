/*
 * Kryon Display Client
 * C89/C90 compliant
 *
 * Displays Kryon server's screen to a native window and captures input
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include <stdint.h>

/*
 * C89 compatibility
 */
#ifdef _WIN32
typedef long ssize_t;
#else
#include <sys/types.h>
#endif

/*
 * Forward declarations
 */
typedef struct DisplayClient DisplayClient;

/*
 * Display client configuration
 */
typedef struct {
    const char *host;
    int port;
    int width;
    int height;
    int verbose;
} DisplayConfig;

/*
 * Display client state
 */
struct DisplayClient {
    /* 9P client connection */
    void *p9;               /* Opaque P9Client pointer */

    /* Native display (SDL2) */
#ifdef HAVE_SDL2
    void *sdl_window;       /* SDL_Window* */
    void *sdl_renderer;     /* SDL_Renderer* */
    void *sdl_texture;      /* SDL_Texture* */
#endif

    /* File descriptors */
    int screen_fd;
    int mouse_fd;
    int refresh_needed;

    /* Screen buffer */
    uint8_t *screen_buf;      /* Converted RGBA data for SDL */
    uint8_t *temp_buf;        /* Temporary buffer for raw BGRA data from server */
    int screen_width;
    int screen_height;

    /* Main loop */
    int running;
};

/*
 * Create display client
 */
DisplayClient *display_client_create(const DisplayConfig *config);

/*
 * Destroy display client
 */
void display_client_destroy(DisplayClient *dc);

/*
 * Run display client main loop
 */
int display_client_run(DisplayClient *dc);

/*
 * Request screen refresh
 */
void display_client_request_refresh(DisplayClient *dc);

/*
 * Send mouse event to server
 */
int display_client_send_mouse(DisplayClient *dc, int x, int y, int buttons);

/*
 * Send key event to server
 */
int display_client_send_key(DisplayClient *dc, int keycode, int pressed);

#endif /* DISPLAY_H */
