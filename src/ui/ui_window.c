#include "ui_window.h"

#include <stddef.h>

/*
 * Desktop implementation notes.
 *
 * Linux/FreeBSD talk to the X server directly through dlopen'd libX11 on a
 * private Display connection: creating a second SDL window in this process
 * makes some Mesa stacks (llvmpipe reproducibly) crash on the next
 * glReadPixels-scissor-batch combination, and routing our window through
 * SDL's event queue would need raylib patches. A private connection keeps
 * the extra window, its events, and its blits completely out of SDL's way
 * and adds no build-time dependency.
 *
 * Windows/macOS keep the plain SDL window path.
 */

#if (defined(__linux__) || defined(__FreeBSD__)) && !defined(__ANDROID__) && !defined(ANDROID_BUILD)

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ui_core.h"

#define UI_WINDOW_MAX 8

/* Minimal X declarations for the dlopen'd calls; types mirror the X headers
 * (XID/Window/Atom are unsigned long everywhere we ship, Display is opaque). */
typedef struct _XDisplay Display;
typedef struct _XImage XImage;
typedef unsigned long XID;
typedef XID Window;
typedef XID Atom;
typedef XID Drawable;

enum {
    InbeLSBFirst = 0,
    InbeMSBFirst = 1,
    InbeZPixmap = 2,
    InbeXA_CARDINAL = 6,
    InbePropModeReplace = 0,
    InbeExposureMask = 1 << 15,
    InbeButtonPressMask = 1 << 2
};

/* Prefix of XImage up to the fields the blit needs; the real object comes
 * from XCreateImage, we only read its scalars. */
typedef struct {
    int width, height;
    int xoffset;
    int format;
    char *data;
    int byte_order;
    int bitmap_unit;
    int bitmap_bit_order;
    int bitmap_pad;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
} InbeXImageInfo;

/* Padded stand-in for the XEvent union: big enough for every event we poll. */
typedef union {
    int type;
    char pad[256];
} InbeXEvent;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    int time_dummy;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int button;
    int same_screen;
    int pad1;
    unsigned long pad2, pad3;
} InbeXButtonEvent;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display *display;
    Window window;
    int pad1;
    int x, y, width, height, count;
    int pad2;
} InbeXExposeEvent;

/* Mirror of XSetWindowAttributes (LP64 layout) for XChangeWindowAttributes. */
typedef struct {
    unsigned long background_pixmap;
    unsigned long background_pixel;
    unsigned long border_pixmap;
    unsigned long border_pixel;
    int win_gravity;
    int bit_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    int save_under;
    long event_mask;
    long do_not_propagate_mask;
    int override_redirect;
    unsigned long colormap;
    unsigned long cursor;
} InbeXSetWindowAttributes;

typedef Display *(*InbeXOpenDisplay)(const char *);
typedef Window (*InbeXDefaultRootWindow)(Display *);
typedef int (*InbeXDefaultScreen)(Display *);
typedef unsigned long (*InbeXDefaultVisual)(Display *, int);
typedef int (*InbeXDefaultDepth)(Display *, int);
typedef int (*InbeXDisplayWidth)(Display *, int);
typedef int (*InbeXDisplayHeight)(Display *, int);
typedef Atom (*InbeXInternAtom)(Display *, const char *, int);
typedef Window (*InbeXCreateSimpleWindow)(Display *, Window, int, int,
                                          unsigned int, unsigned int,
                                          unsigned int, unsigned long,
                                          unsigned long);
typedef int (*InbeXDestroyWindow)(Display *, Window);
typedef int (*InbeXMapWindow)(Display *, Window);
typedef int (*InbeXStoreName)(Display *, Window, const char *);
typedef int (*InbeXSelectInput)(Display *, Window, long);
typedef int (*InbeXChangeWindowAttributes)(Display *, Window, unsigned long,
                                           const InbeXSetWindowAttributes *);
typedef void *(*InbeXCreateGC)(Display *, Drawable, unsigned long, void *);
typedef int (*InbeXFreeGC)(Display *, void *);
typedef int (*InbeXPutImage)(Display *, Drawable, void *, XImage *,
                             int, int, int, int, unsigned int, unsigned int);
typedef XImage *(*InbeXCreateImage)(Display *, void *, unsigned int,
                                            int, int, char *, unsigned int,
                                            unsigned int, int, int);
typedef int (*InbeXDestroyImage)(XImage *);
typedef int (*InbeXPending)(Display *);
typedef int (*InbeXNextEvent)(Display *, InbeXEvent *);
typedef int (*InbeXFlush)(Display *);
typedef int (*InbeXFree)(void *);
typedef int (*InbeXGetWindowProperty)(Display *, Window, Atom, long, long, int,
                                      Atom, Atom *, int *, unsigned long *,
                                      unsigned long *, unsigned char **);

/* XCreateWindow attributes we set through XChangeWindowAttributes. */
enum { InbeCWOverrideRedirect = 1 << 9 };

struct UIWindow {
    Window window;
    int width;
    int height;
    float scale;
    Color background;
    RenderTexture2D target;
    int clicked;
};

static Display *ui_display;
static void *ui_x11;
static int ui_x11_tried;
static void *ui_gc;
static XImage *ui_ximage;
static unsigned char *ui_pixels;
static UIWindow *ui_windows[UI_WINDOW_MAX];
static int ui_window_count;
static UIWindow *ui_window_active;

static InbeXOpenDisplay ui_open_display;
static InbeXDefaultRootWindow ui_root_window;
static InbeXDefaultScreen ui_default_screen;
static InbeXDefaultVisual ui_default_visual;
static InbeXDefaultDepth ui_default_depth;
static InbeXDisplayWidth ui_display_width;
static InbeXDisplayHeight ui_display_height;
static InbeXInternAtom ui_intern_atom;
static InbeXCreateSimpleWindow ui_create_simple_window;
static InbeXDestroyWindow ui_destroy_window;
static InbeXMapWindow ui_map_window;
static InbeXStoreName ui_store_name;
static InbeXSelectInput ui_select_input;
static InbeXChangeWindowAttributes ui_change_attributes;
static InbeXCreateGC ui_create_gc;
static InbeXPutImage ui_put_image;
static InbeXCreateImage ui_create_image;
static InbeXDestroyImage ui_destroy_image;
static InbeXPending ui_pending;
static InbeXNextEvent ui_next_event;
static InbeXFlush ui_x11_flush;
static InbeXFree ui_x11_free;
static InbeXGetWindowProperty ui_get_window_property;

static void *
ui_resolve(void *handle, const char *name)
{
    return handle != NULL ? dlsym(handle, name) : NULL;
}

static int
ui_x11_init(void)
{
    static const char *const names[] = { "libX11.so.6", "libX11.so", NULL };
    int i;

    if(ui_x11_tried)
        return ui_display != NULL;
    ui_x11_tried = 1;

    for(i = 0; names[i] != NULL && ui_x11 == NULL; i++)
        ui_x11 = dlopen(names[i], RTLD_LAZY | RTLD_LOCAL);
    if(ui_x11 == NULL)
        return 0;

    ui_open_display = (InbeXOpenDisplay)ui_resolve(ui_x11, "XOpenDisplay");
    ui_root_window = (InbeXDefaultRootWindow)ui_resolve(ui_x11, "XDefaultRootWindow");
    ui_default_screen = (InbeXDefaultScreen)ui_resolve(ui_x11, "XDefaultScreen");
    ui_default_visual = (InbeXDefaultVisual)ui_resolve(ui_x11, "XDefaultVisual");
    ui_default_depth = (InbeXDefaultDepth)ui_resolve(ui_x11, "XDefaultDepth");
    ui_display_width = (InbeXDisplayWidth)ui_resolve(ui_x11, "XDisplayWidth");
    ui_display_height = (InbeXDisplayHeight)ui_resolve(ui_x11, "XDisplayHeight");
    ui_intern_atom = (InbeXInternAtom)ui_resolve(ui_x11, "XInternAtom");
    ui_create_simple_window = (InbeXCreateSimpleWindow)ui_resolve(ui_x11, "XCreateSimpleWindow");
    ui_destroy_window = (InbeXDestroyWindow)ui_resolve(ui_x11, "XDestroyWindow");
    ui_map_window = (InbeXMapWindow)ui_resolve(ui_x11, "XMapWindow");
    ui_store_name = (InbeXStoreName)ui_resolve(ui_x11, "XStoreName");
    ui_select_input = (InbeXSelectInput)ui_resolve(ui_x11, "XSelectInput");
    ui_change_attributes = (InbeXChangeWindowAttributes)ui_resolve(ui_x11, "XChangeWindowAttributes");
    ui_create_gc = (InbeXCreateGC)ui_resolve(ui_x11, "XCreateGC");
    ui_put_image = (InbeXPutImage)ui_resolve(ui_x11, "XPutImage");
    ui_create_image = (InbeXCreateImage)ui_resolve(ui_x11, "XCreateImage");
    ui_destroy_image = (InbeXDestroyImage)ui_resolve(ui_x11, "XDestroyImage");
    ui_pending = (InbeXPending)ui_resolve(ui_x11, "XPending");
    ui_next_event = (InbeXNextEvent)ui_resolve(ui_x11, "XNextEvent");
    ui_x11_flush = (InbeXFlush)ui_resolve(ui_x11, "XFlush");
    ui_x11_free = (InbeXFree)ui_resolve(ui_x11, "XFree");
    ui_get_window_property = (InbeXGetWindowProperty)ui_resolve(ui_x11, "XGetWindowProperty");

    if(ui_open_display == NULL || ui_root_window == NULL ||
       ui_create_simple_window == NULL || ui_put_image == NULL ||
       ui_create_image == NULL) {
        ui_x11 = NULL;
        return 0;
    }

    ui_display = ui_open_display(NULL);
    return ui_display != NULL;
}

static int
ui_window_register(UIWindow *win)
{
    if(ui_window_count >= UI_WINDOW_MAX)
        return 0;
    ui_windows[ui_window_count++] = win;
    return 1;
}

static void
ui_window_unregister(UIWindow *win)
{
    int i, j;

    for(i = 0; i < ui_window_count; i++) {
        if(ui_windows[i] == win) {
            for(j = i; j < ui_window_count - 1; j++)
                ui_windows[j] = ui_windows[j + 1];
            ui_window_count--;
            return;
        }
    }
}

/* Work area of the primary screen; falls back to the full screen when the
 * window manager does not publish _NET_WORKAREA. */
static void
ui_primary_workarea(int *x, int *y, int *w, int *h)
{
    int screen = ui_default_screen(ui_display);
    unsigned char *data = NULL;
    Atom workarea, type = 0;
    int format = 0;
    unsigned long n = 0, left = 0;

    *x = 0;
    *y = 0;
    *w = ui_display_width(ui_display, screen);
    *h = ui_display_height(ui_display, screen);

    if(ui_intern_atom == NULL || ui_get_window_property == NULL)
        return;
    workarea = ui_intern_atom(ui_display, "_NET_WORKAREA", 1);
    if(workarea == 0)
        return;
    if(ui_get_window_property(ui_display, ui_root_window(ui_display), workarea,
                              0, 4, 0, InbeXA_CARDINAL, &type, &format,
                              &n, &left, &data) != 0 || n < 4 || data == NULL) {
        if(data != NULL)
            ui_x11_free(data);
        return;
    }
    *x = (int)((unsigned long *)data)[0];
    *y = (int)((unsigned long *)data)[1];
    *w = (int)((unsigned long *)data)[2];
    *h = (int)((unsigned long *)data)[3];
    ui_x11_free(data);
}

UIWindow *
OpenUIWindow(const char *title, int x, int y, int width, int height,
             int flags, Color background, float ui_scale)
{
    UIWindow *win;

    if(width <= 0 || height <= 0 || !ui_x11_init())
        return NULL;

    if((flags & UI_WINDOW_TOP_RIGHT) != 0) {
        int wx, wy, ww, wh;
        ui_primary_workarea(&wx, &wy, &ww, &wh);
        x = wx + ww - width - x;
        y = wy + y;
    }

    win = (UIWindow *)calloc(1, sizeof(UIWindow));
    if(win == NULL)
        return NULL;
    win->window = ui_create_simple_window(ui_display, ui_root_window(ui_display),
                                          x, y, (unsigned int)width,
                                          (unsigned int)height, 0, 0, 0);
    if(win->window == 0) {
        free(win);
        return NULL;
    }
    win->width = width;
    win->height = height;
    win->scale = ui_scale > 0.0f ? ui_scale : 1.0f;
    win->background = background;

    if(ui_store_name != NULL)
        ui_store_name(ui_display, win->window, title != NULL ? title : "kryon");

    /* Borderless windows are override-redirect: no decorations, no taskbar
     * entry, no window manager stacking. Managed windows get input events
     * only, which is all the API polls for. */
    if((flags & UI_WINDOW_BORDERLESS) != 0 && ui_change_attributes != NULL) {
        InbeXSetWindowAttributes attributes;
        memset(&attributes, 0, sizeof(attributes));
        attributes.override_redirect = 1; /* True */
        ui_change_attributes(ui_display, win->window, InbeCWOverrideRedirect, &attributes);
    }
    if(ui_select_input != NULL)
        ui_select_input(ui_display, win->window,
                        InbeExposureMask | InbeButtonPressMask);

    win->target = LoadRenderTexture(width, height);
    if(win->target.id == 0 || !ui_window_register(win)) {
        if(win->target.id != 0)
            UnloadRenderTexture(win->target);
        ui_destroy_window(ui_display, win->window);
        free(win);
        return NULL;
    }
    if(ui_gc == NULL && ui_create_gc != NULL)
        ui_gc = ui_create_gc(ui_display, win->window, 0, NULL);

    ui_map_window(ui_display, win->window);
    ui_x11_flush(ui_display);
    return win;
}

void
CloseUIWindow(UIWindow *window)
{
    if(window == NULL)
        return;
    ui_window_unregister(window);
    if(ui_window_active == window)
        ui_window_active = NULL;
    UnloadRenderTexture(window->target);
    ui_destroy_window(ui_display, window->window);
    ui_x11_flush(ui_display);
    free(window);
}

void
BeginUIWindow(UIWindow *window)
{
    if(window == NULL)
        return;
    ui_window_active = window;
    BeginTextureMode(window->target);
    ClearBackground(window->background);
    BeginUIFrame(window->width, window->height, window->scale);
}

static void
ui_window_dump(const unsigned char *flipped, int width, int height)
{
    static double last_dump;
    const char *path = getenv("KRYON_UI_WINDOW_DUMP");
    double now;

    if(path == NULL || path[0] == '\0')
        return;
    now = GetTime();
    if(last_dump != 0.0 && now - last_dump < 1.0)
        return;
    last_dump = now;
    Image image = {
        .data = (void *)flipped,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    ExportImage(image, path);
}

/* Convert the GL readback (RGBA byte order, bottom-up rows) into the XImage
 * buffer (server byte order, top-down rows). */
static void
ui_window_convert(const unsigned char *rgba, int width, int height)
{
    InbeXImageInfo *info = (InbeXImageInfo *)ui_ximage;
    int lsb = info->byte_order == InbeLSBFirst;
    int y, x, bpl = info->bytes_per_line;
    int bpp = info->bits_per_pixel / 8;

    for(y = 0; y < height; y++) {
        const unsigned char *src = rgba + (size_t)(height - 1 - y) * width * 4;
        unsigned char *dst = ui_pixels + (size_t)y * bpl;
        for(x = 0; x < width; x++) {
            unsigned char r = src[x * 4], g = src[x * 4 + 1], b = src[x * 4 + 2];
            if(lsb) {
                dst[x * bpp] = b;
                dst[x * bpp + 1] = g;
                dst[x * bpp + 2] = r;
            } else {
                dst[x * bpp] = r;
                dst[x * bpp + 1] = g;
                dst[x * bpp + 2] = b;
            }
            if(bpp == 4)
                dst[x * bpp + 3] = 255;
        }
    }
}

static void
ui_window_blit(UIWindow *window)
{
    if(ui_gc == NULL || ui_put_image == NULL)
        return;
    ui_put_image(ui_display, window->window, ui_gc, ui_ximage,
                 0, 0, 0, 0, (unsigned int)window->width, (unsigned int)window->height);
}

static void
ui_window_poll_events(UIWindow *window)
{
    InbeXEvent event;
    int dirty = 0;

    if(ui_pending == NULL || ui_next_event == NULL)
        return;
    while(ui_pending(ui_display) > 0) {
        ui_next_event(ui_display, &event);
        if(event.type == 12 /* Expose */ &&
           ((InbeXExposeEvent *)&event)->window == window->window)
            dirty = 1;
        else if(event.type == 4 /* ButtonPress */ &&
                ((InbeXButtonEvent *)&event)->window == window->window)
            window->clicked = 1;
    }
    if(dirty)
        ui_window_blit(window);
}

void
EndUIWindow(void)
{
    UIWindow *window = ui_window_active;
    Image image;
    int size;

    if(window == NULL)
        return;
    ui_window_active = NULL;
    EndUIFocus();
    /* EndTextureMode flushes the widget batch into the texture; the readback
     * then picks up finished pixels (kryon-preview uses the same order). */
    EndTextureMode();

    image = LoadImageFromTexture(window->target.texture);
    if(image.data == NULL || ui_create_image == NULL)
        return;

    size = window->width * window->height * 4;
    if(ui_ximage != NULL &&
       (((InbeXImageInfo *)ui_ximage)->width != window->width ||
        ((InbeXImageInfo *)ui_ximage)->height != window->height)) {
        ui_destroy_image(ui_ximage);
        ui_ximage = NULL;
    }
    if(ui_ximage == NULL) {
        if(ui_create_image == NULL)
            return;
        ui_pixels = (unsigned char *)realloc(ui_pixels, size);
        ui_ximage = ui_create_image(ui_display,
                                    (void *)ui_default_visual(ui_display, ui_default_screen(ui_display)),
                                    (unsigned int)ui_default_depth(ui_display, ui_default_screen(ui_display)),
                                    InbeZPixmap, 0, (char *)ui_pixels,
                                    (unsigned int)window->width, (unsigned int)window->height,
                                    32, 0);
        if(ui_ximage == NULL)
            return;
    }
    ui_window_convert((const unsigned char *)image.data, window->width, window->height);
    UnloadImage(image);

    ui_window_blit(window);
    ui_window_dump(ui_pixels, window->width, window->height);
    ui_window_poll_events(window);
    ui_x11_flush(ui_display);
}

int
IsUIWindowClicked(UIWindow *window)
{
    if(window == NULL || !window->clicked)
        return 0;
    window->clicked = 0;
    return 1;
}

#elif defined(UI_WINDOW_HAVE_SDL) /* Windows/macOS plain-SDL path: define
                                   * UI_WINDOW_HAVE_SDL in the build once
                                   * those environments ship SDL headers. */

#include <SDL2/SDL.h>
#include <stdlib.h>

#include "ui_core.h"

#define UI_WINDOW_MAX 8

struct UIWindow {
    SDL_Window *window;
    Uint32 window_id;
    int width;
    int height;
    float scale;
    Color background;
    RenderTexture2D target;
    int clicked;
};

static UIWindow *ui_windows[UI_WINDOW_MAX];
static int ui_window_count;
static UIWindow *ui_window_active;

static int
ui_window_register(UIWindow *win)
{
    if(ui_window_count >= UI_WINDOW_MAX)
        return 0;
    ui_windows[ui_window_count++] = win;
    return 1;
}

static void
ui_window_unregister(UIWindow *win)
{
    int i, j;

    for(i = 0; i < ui_window_count; i++) {
        if(ui_windows[i] == win) {
            for(j = i; j < ui_window_count - 1; j++)
                ui_windows[j] = ui_windows[j + 1];
            ui_window_count--;
            return;
        }
    }
}

UIWindow *
OpenUIWindow(const char *title, int x, int y, int width, int height,
             int flags, Color background, float ui_scale)
{
    UIWindow *win;
    Uint32 sdl_flags = 0;

    if(width <= 0 || height <= 0)
        return NULL;
    if((flags & UI_WINDOW_BORDERLESS) != 0)
        sdl_flags |= SDL_WINDOW_BORDERLESS;
    if((flags & UI_WINDOW_ALWAYS_ON_TOP) != 0)
        sdl_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    if((flags & UI_WINDOW_SKIP_TASKBAR) != 0)
        sdl_flags |= SDL_WINDOW_SKIP_TASKBAR;
    if((flags & UI_WINDOW_TOP_RIGHT) != 0) {
        SDL_Rect usable;
        if(SDL_GetDisplayUsableBounds(0, &usable) == 0) {
            x = usable.x + usable.w - width - x;
            y = usable.y + y;
        }
    }

    win = (UIWindow *)calloc(1, sizeof(UIWindow));
    if(win == NULL)
        return NULL;
    win->window = SDL_CreateWindow(title, x, y, width, height, sdl_flags);
    if(win->window == NULL) {
        free(win);
        return NULL;
    }
    win->window_id = SDL_GetWindowID(win->window);
    win->width = width;
    win->height = height;
    win->scale = ui_scale > 0.0f ? ui_scale : 1.0f;
    win->background = background;
    win->target = LoadRenderTexture(width, height);
    if(win->target.id == 0 || !ui_window_register(win)) {
        if(win->target.id != 0)
            UnloadRenderTexture(win->target);
        SDL_DestroyWindow(win->window);
        free(win);
        return NULL;
    }
    return win;
}

void
CloseUIWindow(UIWindow *window)
{
    if(window == NULL)
        return;
    ui_window_unregister(window);
    if(ui_window_active == window)
        ui_window_active = NULL;
    UnloadRenderTexture(window->target);
    SDL_DestroyWindow(window->window);
    free(window);
}

void
BeginUIWindow(UIWindow *window)
{
    if(window == NULL)
        return;
    ui_window_active = window;
    BeginTextureMode(window->target);
    ClearBackground(window->background);
    BeginUIFrame(window->width, window->height, window->scale);
}

void
EndUIWindow(void)
{
    UIWindow *window = ui_window_active;
    Image image;
    SDL_Surface *surface;

    if(window == NULL)
        return;
    ui_window_active = NULL;
    EndUIFocus();
    /* EndTextureMode flushes the widget batch into the texture; the readback
     * then picks up finished pixels (kryon-preview uses the same order). */
    EndTextureMode();
    image = LoadImageFromTexture(window->target.texture);
    if(image.data == NULL)
        return;
    ImageFlipVertical(&image);

    surface = SDL_GetWindowSurface(window->window);
    if(surface != NULL && surface->pixels != NULL) {
        /* GL RGBA8 readback is R,G,B,A in memory = SDL's ABGR8888. */
        SDL_ConvertPixels(window->width, window->height,
                          SDL_PIXELFORMAT_ABGR8888, image.data,
                          window->width * 4,
                          surface->format->format, surface->pixels,
                          surface->pitch);
        SDL_UpdateWindowSurface(window->window);
    }
    UnloadImage(image);
}

int
IsUIWindowClicked(UIWindow *window)
{
    if(window == NULL || !window->clicked)
        return 0;
    window->clicked = 0;
    return 1;
}

#else /* web/android: no extra windows */

UIWindow *
OpenUIWindow(const char *title, int x, int y, int width, int height,
             int flags, Color background, float ui_scale)
{
    (void)title; (void)x; (void)y; (void)width; (void)height;
    (void)flags; (void)background; (void)ui_scale;
    return NULL;
}

void
CloseUIWindow(UIWindow *window)
{
    (void)window;
}

void
BeginUIWindow(UIWindow *window)
{
    (void)window;
}

void
EndUIWindow(void)
{
}

int
IsUIWindowClicked(UIWindow *window)
{
    (void)window;
    return 0;
}

#endif
