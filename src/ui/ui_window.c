#include "ui_window.h"
#include "ui_paint_layers_internal.h"
#include "ui/window_policy.h"

#include <stddef.h>

/*
 * Desktop implementation notes.
 *
 * Linux/FreeBSD talk to the X server directly through dlopend libX11 on a
 * private Display connection: creating a second SDL window in this process
 * makes some Mesa stacks (llvmpipe reproducibly) crash on the next
 * glReadPixels-scissor-batch combination, and routing our window through
 * SDLs event queue would need raylib patches. A private connection keeps
 * the extra window, its events, and its blits completely out of SDLs way
 * and adds no build-time dependency.
 *
 * Windows/macOS keep the plain SDL window path.
 */

#if (defined(__linux__) || defined(__FreeBSD__)) && !defined(__ANDROID__) && !defined(ANDROID_BUILD) && !defined(NATIVE_WINDOW_HAVE_SDL)

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ui_core.h"

#define NATIVE_WINDOW_MAX 8

/* Minimal X declarations for the dlopend calls; types mirror the X headers
 * (XID/Window/Atom are unsigned long everywhere we ship, Display is opaque). */
typedef struct _XDisplay Display;
typedef struct _XImage XImage;
typedef unsigned long XID;
typedef XID Window;
typedef XID Atom;
typedef XID Drawable;

enum {
    KryonX11LSBFirst = 0,
    KryonX11MSBFirst = 1,
    KryonX11ZPixmap = 2,
    KryonX11XA_ATOM = 4,
    KryonX11XA_CARDINAL = 6,
    KryonX11PropModeReplace = 0,
    KryonX11ExposureMask = 1 << 15,
    KryonX11ButtonPressMask = 1 << 2,
    KryonX11ButtonReleaseMask = 1 << 3,
    KryonX11PointerMotionMask = 1 << 6
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
} KryonX11XImageInfo;

/* Padded stand-in for the XEvent union: big enough for every event we poll. */
typedef union {
    int type;
    char pad[256];
} KryonX11XEvent;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    unsigned long time;  /* X11 Time is unsigned long: 8 bytes on LP64 */
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int button;
    int same_screen;
    int pad1;
    unsigned long pad2, pad3;
} KryonX11XButtonEvent;

typedef struct {
    int type;
    unsigned long serial;
    int send_event;
    Display *display;
    Window window;
    int pad1;
    int x, y, width, height, count;
    int pad2;
} KryonX11XExposeEvent;

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
} KryonX11XSetWindowAttributes;

typedef Display *(*KryonX11XOpenDisplay)(const char *);
typedef Window (*KryonX11XDefaultRootWindow)(Display *);
typedef int (*KryonX11XDefaultScreen)(Display *);
typedef unsigned long (*KryonX11XDefaultVisual)(Display *, int);
typedef int (*KryonX11XDefaultDepth)(Display *, int);
typedef int (*KryonX11XDisplayWidth)(Display *, int);
typedef int (*KryonX11XDisplayHeight)(Display *, int);
typedef Atom (*KryonX11XInternAtom)(Display *, const char *, int);
typedef Window (*KryonX11XCreateSimpleWindow)(Display *, Window, int, int,
                                          unsigned int, unsigned int,
                                          unsigned int, unsigned long,
                                          unsigned long);
typedef int (*KryonX11XDestroyWindow)(Display *, Window);
typedef int (*KryonX11XMoveWindow)(Display *, Window, int, int);
typedef int (*KryonX11XMapWindow)(Display *, Window);
typedef int (*KryonX11XStoreName)(Display *, Window, const char *);
typedef int (*KryonX11XSelectInput)(Display *, Window, long);
typedef int (*KryonX11XChangeWindowAttributes)(Display *, Window, unsigned long,
                                           const KryonX11XSetWindowAttributes *);
typedef void *(*KryonX11XCreateGC)(Display *, Drawable, unsigned long, void *);
typedef int (*KryonX11XFreeGC)(Display *, void *);
typedef int (*KryonX11XPutImage)(Display *, Drawable, void *, XImage *,
                             int, int, int, int, unsigned int, unsigned int);
typedef XImage *(*KryonX11XCreateImage)(Display *, void *, unsigned int,
                                            int, int, char *, unsigned int,
                                            unsigned int, int, int);
typedef int (*KryonX11XDestroyImage)(XImage *);
typedef int (*KryonX11XPending)(Display *);
typedef int (*KryonX11XNextEvent)(Display *, KryonX11XEvent *);
typedef int (*KryonX11XFlush)(Display *);
typedef int (*KryonX11XFree)(void *);
typedef int (*KryonX11XGetWindowProperty)(Display *, Window, Atom, long, long, int,
                                      Atom, Atom *, int *, unsigned long *,
                                      unsigned long *, unsigned char **);
typedef int (*KryonX11XChangeProperty)(Display *, Window, Atom, Atom, int, int,
                                   const unsigned char *, int);

/* XCreateWindow attributes we set through XChangeWindowAttributes. */
enum { KryonX11CWOverrideRedirect = 1 << 9 };

#define NATIVE_WINDOW_OWNS_PAINT_LAYERS 1
struct NativeWindow {
    Window window;
    PaintLayers *paint_layers;
    int focus_id;
    int previous_focus_id;
    int width;
    int height;
    float scale;
    Color background;
    RenderTexture2D target;
    int clicked;
    int click_button;           /* 1 = left, 3 = right; valid while clicked */
    int click_x, click_y;       /* window-relative press position */
    int x, y;                   /* current window position */
    int drag_active;
    int drag_last_root_x;
    int drag_last_root_y;
    int dragged;                /* the last press-drag actually moved */
    XImage *ximage;             /* per-window XImage + pixel buffer: the */
    unsigned char *pixels;      /* global single-buffer scheme corrupted */
};                              /* the heap once two windows alternate */

static Display *ui_display;
static void *ui_x11;
static int ui_x11_tried;
static void *ui_gc;
static NativeWindow *ui_windows[NATIVE_WINDOW_MAX];
static int ui_window_count;
static NativeWindow *ui_window_active;

static KryonX11XOpenDisplay ui_open_display;
static KryonX11XDefaultRootWindow ui_root_window;
static KryonX11XDefaultScreen ui_default_screen;
static KryonX11XDefaultVisual ui_default_visual;
static KryonX11XDefaultDepth ui_default_depth;
static KryonX11XDisplayWidth ui_display_width;
static KryonX11XDisplayHeight ui_display_height;
static KryonX11XInternAtom ui_intern_atom;
static KryonX11XCreateSimpleWindow ui_create_simple_window;
static KryonX11XDestroyWindow ui_destroy_window;
static KryonX11XMoveWindow ui_move_window;
static KryonX11XMapWindow ui_map_window;
static KryonX11XStoreName ui_store_name;
static KryonX11XSelectInput ui_select_input;
static KryonX11XChangeWindowAttributes ui_change_attributes;
static KryonX11XCreateGC ui_create_gc;
static KryonX11XPutImage ui_put_image;
static KryonX11XCreateImage ui_create_image;
static KryonX11XDestroyImage ui_destroy_image;
static KryonX11XPending ui_pending;
static KryonX11XNextEvent ui_next_event;
static KryonX11XFlush ui_x11_flush;
static KryonX11XFree ui_x11_free;
static KryonX11XGetWindowProperty ui_get_window_property;
static KryonX11XChangeProperty ui_change_property;

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

    ui_open_display = (KryonX11XOpenDisplay)ui_resolve(ui_x11, "XOpenDisplay");
    ui_root_window = (KryonX11XDefaultRootWindow)ui_resolve(ui_x11, "XDefaultRootWindow");
    ui_default_screen = (KryonX11XDefaultScreen)ui_resolve(ui_x11, "XDefaultScreen");
    ui_default_visual = (KryonX11XDefaultVisual)ui_resolve(ui_x11, "XDefaultVisual");
    ui_default_depth = (KryonX11XDefaultDepth)ui_resolve(ui_x11, "XDefaultDepth");
    ui_display_width = (KryonX11XDisplayWidth)ui_resolve(ui_x11, "XDisplayWidth");
    ui_display_height = (KryonX11XDisplayHeight)ui_resolve(ui_x11, "XDisplayHeight");
    ui_intern_atom = (KryonX11XInternAtom)ui_resolve(ui_x11, "XInternAtom");
    ui_create_simple_window = (KryonX11XCreateSimpleWindow)ui_resolve(ui_x11, "XCreateSimpleWindow");
    ui_destroy_window = (KryonX11XDestroyWindow)ui_resolve(ui_x11, "XDestroyWindow");
    ui_move_window = (KryonX11XMoveWindow)ui_resolve(ui_x11, "XMoveWindow");
    ui_map_window = (KryonX11XMapWindow)ui_resolve(ui_x11, "XMapWindow");
    ui_store_name = (KryonX11XStoreName)ui_resolve(ui_x11, "XStoreName");
    ui_select_input = (KryonX11XSelectInput)ui_resolve(ui_x11, "XSelectInput");
    ui_change_attributes = (KryonX11XChangeWindowAttributes)ui_resolve(ui_x11, "XChangeWindowAttributes");
    ui_create_gc = (KryonX11XCreateGC)ui_resolve(ui_x11, "XCreateGC");
    ui_put_image = (KryonX11XPutImage)ui_resolve(ui_x11, "XPutImage");
    ui_create_image = (KryonX11XCreateImage)ui_resolve(ui_x11, "XCreateImage");
    ui_destroy_image = (KryonX11XDestroyImage)ui_resolve(ui_x11, "XDestroyImage");
    ui_pending = (KryonX11XPending)ui_resolve(ui_x11, "XPending");
    ui_next_event = (KryonX11XNextEvent)ui_resolve(ui_x11, "XNextEvent");
    ui_x11_flush = (KryonX11XFlush)ui_resolve(ui_x11, "XFlush");
    ui_x11_free = (KryonX11XFree)ui_resolve(ui_x11, "XFree");
    ui_get_window_property = (KryonX11XGetWindowProperty)ui_resolve(ui_x11, "XGetWindowProperty");
    ui_change_property = (KryonX11XChangeProperty)ui_resolve(ui_x11, "XChangeProperty");

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
ui_window_register(NativeWindow *win)
{
    if(ui_window_count >= NATIVE_WINDOW_MAX)
        return 0;
    ui_windows[ui_window_count++] = win;
    return 1;
}

static void
ui_window_unregister(NativeWindow *win)
{
    int i, j;

    for(i = 0; i < ui_window_count; i++) {
        if(ui_windows[i] == win) {
            for(j = i; j < ui_window_count - 1; j++)
                ui_windows[j] = ui_windows[j + 1];
            ui_window_count--;
            break;
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
                              0, 4, 0, KryonX11XA_CARDINAL, &type, &format,
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

static void
ui_window_apply_ewmh_hints(Window window, int flags)
{
    Atom states[4];
    int state_count = 0;

    if(ui_intern_atom == NULL || ui_change_property == NULL)
        return;

    if((flags & NATIVE_WINDOW_BORDERLESS) != 0) {
        Atom motif = ui_intern_atom(ui_display, "_MOTIF_WM_HINTS", 0);
        unsigned long hints[5] = { 2, 0, 0, 0, 0 };
        if(motif != 0)
            ui_change_property(ui_display, window, motif, motif, 32,
                               KryonX11PropModeReplace,
                               (const unsigned char *)hints, 5);
    }

    if((flags & NATIVE_WINDOW_ALWAYS_ON_TOP) != 0) {
        Atom above = ui_intern_atom(ui_display, "_NET_WM_STATE_ABOVE", 0);
        if(above != 0)
            states[state_count++] = above;
    }
    if((flags & NATIVE_WINDOW_SKIP_TASKBAR) != 0) {
        Atom skip = ui_intern_atom(ui_display, "_NET_WM_STATE_SKIP_TASKBAR", 0);
        if(skip != 0)
            states[state_count++] = skip;
    }
    if((flags & NATIVE_WINDOW_STICKY) != 0) {
        Atom sticky = ui_intern_atom(ui_display, "_NET_WM_STATE_STICKY", 0);
        Atom desktop = ui_intern_atom(ui_display, "_NET_WM_DESKTOP", 0);
        unsigned long all_desktops = 0xFFFFFFFFUL;
        if(sticky != 0)
            states[state_count++] = sticky;
        if(desktop != 0)
            ui_change_property(ui_display, window, desktop, KryonX11XA_CARDINAL,
                               32, KryonX11PropModeReplace,
                               (const unsigned char *)&all_desktops, 1);
    }
    if(state_count > 0) {
        Atom state = ui_intern_atom(ui_display, "_NET_WM_STATE", 0);
        if(state != 0)
            ui_change_property(ui_display, window, state, KryonX11XA_ATOM, 32,
                               KryonX11PropModeReplace,
                               (const unsigned char *)states, state_count);
    }
}

NativeWindow *
OpenNativeWindow(const char *title, int x, int y, int width, int height,
             int flags, Color background, float ui_scale)
{
    NativeWindow *win;

    /* The render texture below needs a live GL context; without the apps
     * main window there is nothing to share assets with and rlgl is not
     * initialized. */
    if(width <= 0 || height <= 0 || !IsWindowReady() || !ui_x11_init())
        return NULL;

    if((flags & (NATIVE_WINDOW_TOP_RIGHT | NATIVE_WINDOW_CENTER)) != 0) {
        int wx, wy, ww, wh;
        ui_primary_workarea(&wx, &wy, &ww, &wh);
        WindowPoint position = WindowInitialPosition(x, y, width, height,
                                                     flags, wx, wy, ww, wh);
        x = position.x;
        y = position.y;
    }

    win = (NativeWindow *)calloc(1, sizeof(NativeWindow));
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
    win->x = x;
    win->y = y;

    if(ui_store_name != NULL)
        ui_store_name(ui_display, win->window, title != NULL ? title : "kryon");

    /* Non-sticky borderless windows are override-redirect. Sticky windows
     * need the window manager to honor EWMH workspace hints, so they stay
     * managed and drop decorations through _MOTIF_WM_HINTS instead. */
    if((flags & NATIVE_WINDOW_BORDERLESS) != 0 &&
       (flags & NATIVE_WINDOW_STICKY) == 0 && ui_change_attributes != NULL) {
        KryonX11XSetWindowAttributes attributes;
        memset(&attributes, 0, sizeof(attributes));
        attributes.override_redirect = 1; /* True */
        ui_change_attributes(ui_display, win->window, KryonX11CWOverrideRedirect, &attributes);
    }
    ui_window_apply_ewmh_hints(win->window, flags);
    if(ui_select_input != NULL)
        ui_select_input(ui_display, win->window,
                        KryonX11ExposureMask | KryonX11ButtonPressMask |
                        KryonX11ButtonReleaseMask | KryonX11PointerMotionMask);

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
CloseNativeWindow(NativeWindow *window)
{
    if(window == NULL)
        return;
    if(ui_window_active == window) EndNativeWindow();
    ui_window_unregister(window);
    ui_paint_layers_destroy(window->paint_layers);
    if(window->ximage != NULL) {
        ((KryonX11XImageInfo *)window->ximage)->data = NULL;
        ui_destroy_image(window->ximage);
    }
    free(window->pixels);
    UnloadRenderTexture(window->target);
    ui_destroy_window(ui_display, window->window);
    ui_x11_flush(ui_display);
    free(window);
}

void
BeginNativeWindow(NativeWindow *window)
{
    if(window == NULL)
        return;
    window->previous_focus_id = GetFocus();
    SetFocus(window->focus_id);
    ui_window_active = window;
    BeginTextureMode(window->target);
    ClearBackground(window->background);
    BeginInterfaceFrame(window->width, window->height, window->scale);
    ui_window_layers_begin();
}

static void
ui_window_dump(const unsigned char *flipped, int width, int height)
{
    static double last_dump;
    const char *path = getenv("KRYON_NATIVE_WINDOW_DUMP");
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
ui_window_convert(NativeWindow *window, const unsigned char *rgba)
{
    KryonX11XImageInfo *info = (KryonX11XImageInfo *)window->ximage;
    int width = window->width, height = window->height;
    int lsb = info->byte_order == KryonX11LSBFirst;
    int y, x, bpl = info->bytes_per_line;
    int bpp = info->bits_per_pixel / 8;

    for(y = 0; y < height; y++) {
        const unsigned char *src = rgba + (size_t)(height - 1 - y) * width * 4;
        unsigned char *dst = window->pixels + (size_t)y * bpl;
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
ui_window_blit(NativeWindow *window)
{
    if(ui_gc == NULL || ui_put_image == NULL || window->ximage == NULL)
        return;
    ui_put_image(ui_display, window->window, ui_gc, window->ximage,
                 0, 0, 0, 0, (unsigned int)window->width, (unsigned int)window->height);
}

static void
ui_window_poll_events(NativeWindow *window)
{
    KryonX11XEvent event;
    int dirty = 0;

    if(ui_pending == NULL || ui_next_event == NULL)
        return;
    while(ui_pending(ui_display) > 0) {
        ui_next_event(ui_display, &event);
        if(event.type == 12 /* Expose */ &&
           ((KryonX11XExposeEvent *)&event)->window == window->window)
            dirty = 1;
        else if(event.type == 4 /* ButtonPress */ &&
                ((KryonX11XButtonEvent *)&event)->window == window->window) {
            KryonX11XButtonEvent *button = (KryonX11XButtonEvent *)&event;
            window->clicked = 1;
            window->click_button = (int)button->button;
            window->click_x = button->x;
            window->click_y = button->y;
            if(button->button == 1) {
                window->drag_active = 1;
                window->drag_last_root_x = button->x_root;
                window->drag_last_root_y = button->y_root;
                window->dragged = 0;
            }
        } else if(event.type == 5 /* ButtonRelease */ &&
                  ((KryonX11XButtonEvent *)&event)->window == window->window) {
            window->drag_active = 0;
        } else if(event.type == 6 /* MotionNotify */ &&
                  ((KryonX11XButtonEvent *)&event)->window == window->window) {
            KryonX11XButtonEvent *motion = (KryonX11XButtonEvent *)&event;
            int dx, dy;
            if(!window->drag_active || ui_move_window == NULL)
                continue;
            dx = motion->x_root - window->drag_last_root_x;
            dy = motion->y_root - window->drag_last_root_y;
            if(dx == 0 && dy == 0)
                continue;
            window->drag_last_root_x = motion->x_root;
            window->drag_last_root_y = motion->y_root;
            {
                int wx, wy, ww, wh;
                ui_primary_workarea(&wx, &wy, &ww, &wh);
                WindowPoint position = WindowDragPosition(
                    window->x, window->y, dx, dy, wx, wy, ww, wh);
                window->x = position.x;
                window->y = position.y;
            }
            ui_move_window(ui_display, window->window, window->x, window->y);
            if(WindowDragMoved(dx, dy))
                window->dragged = 1;
            dirty = 1;
        }
    }
    if(dirty)
        ui_window_blit(window);
}

void
EndNativeWindow(void)
{
    NativeWindow *window = ui_window_active;
    Image image;

    if(window == NULL)
        return;
    EndInterfaceFrame();
    if(window->paint_layers) ui_paint_layers_composite(window->paint_layers);
    window->focus_id = GetFocus();
    SetFocus(window->previous_focus_id);
    ui_window_active = NULL;
    /* EndTextureMode flushes the widget batch into the texture; the readback
     * then picks up finished pixels (kryon-preview uses the same order). */
    EndTextureMode();

    image = ui_paint_readback(window->target.texture);
    if(image.data == NULL || ui_create_image == NULL)
        return;

    if(window->ximage != NULL &&
       (((KryonX11XImageInfo *)window->ximage)->width != window->width ||
        ((KryonX11XImageInfo *)window->ximage)->height != window->height)) {
        /* XDestroyImage frees the data pointer it was created with; null it
         * first because the buffer belongs to the window, not the image. */
        ((KryonX11XImageInfo *)window->ximage)->data = NULL;
        ui_destroy_image(window->ximage);
        window->ximage = NULL;
    }
    if(window->ximage == NULL) {
        int size = window->width * window->height * 4;
        window->pixels = (unsigned char *)realloc(window->pixels, (size_t)size);
        if(window->pixels == NULL)
            return;
        window->ximage = ui_create_image(ui_display,
                                    (void *)ui_default_visual(ui_display, ui_default_screen(ui_display)),
                                    (unsigned int)ui_default_depth(ui_display, ui_default_screen(ui_display)),
                                    KryonX11ZPixmap, 0, (char *)window->pixels,
                                    (unsigned int)window->width, (unsigned int)window->height,
                                    32, 0);
        if(window->ximage == NULL)
            return;
    }
    ui_window_convert(window, (const unsigned char *)image.data);
    UnloadImage(image);

    ui_window_blit(window);
    ui_window_dump(window->pixels, window->width, window->height);
    ui_window_poll_events(window);
    ui_x11_flush(ui_display);
}

int
IsNativeWindowClicked(NativeWindow *window)
{
    if(window == NULL || !window->clicked)
        return 0;
    window->clicked = 0;
    return 1;
}

int
IsNativeWindowRightClicked(NativeWindow *window)
{
    if(window == NULL || !window->clicked || window->click_button != 3)
        return 0;
    window->clicked = 0;
    return 1;
}

int
IsNativeWindowDragged(NativeWindow *window)
{
    if(window == NULL || !window->dragged)
        return 0;
    window->dragged = 0;
    /* Swallow the click that started the drag so it is not also reported
     * as a plain click. */
    window->clicked = 0;
    return 1;
}

void
GetNativeWindowPosition(NativeWindow *window, int *x, int *y)
{
    if(x != NULL)
        *x = window != NULL ? window->x : 0;
    if(y != NULL)
        *y = window != NULL ? window->y : 0;
}

void
GetNativeWindowClickPosition(NativeWindow *window, int *x, int *y)
{
    if(x != NULL)
        *x = window != NULL ? window->click_x : -1;
    if(y != NULL)
        *y = window != NULL ? window->click_y : -1;
}

/* The Xlib path polls its windows inside EndNativeWindow; nothing to pump. */
void
PumpWindows(void)
{
}

int
StealCoreWindowClose(void)
{
    return 0;
}

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define Rectangle RectangleWin32
#define CloseWindow CloseWindowWin32
#define ShowCursor ShowCursorWin32
#define SetFocus Win32SetFocus
#define GetFocus Win32GetFocus
#include <windows.h>
#undef GetFocus
#undef SetFocus
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#include <stdlib.h>
#include <string.h>
#include "ui_core.h"

#define NATIVE_WINDOW_CLASS_NAME "KryonNativeWindow"
#define NATIVE_WINDOW_APP_ICON 101

#define NATIVE_WINDOW_OWNS_PAINT_LAYERS 1
struct NativeWindow {
    HWND window;
    PaintLayers *paint_layers;
    int focus_id;
    int previous_focus_id;
    int width, height;
    float scale;
    Color background;
    RenderTexture2D target;
    unsigned char *pixels;
    int clicked, right_clicked;
    int click_x, click_y;
    int x, y;
    int drag_active, dragged;
    POINT drag_last;
};

static NativeWindow *ui_window_active;
static ATOM ui_window_class;
static NativeWindow *ui_windows[8];
static int ui_window_count;
static HWND ui_core_window;
static WNDPROC ui_core_window_proc;
static volatile LONG ui_window_core_close_pending;

static LRESULT CALLBACK
ui_core_window_close_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    WNDPROC window_proc = ui_core_window_proc;

    if(message == WM_CLOSE) {
        InterlockedExchange(&ui_window_core_close_pending, 1);
        return 0;
    }
    if(message == WM_NCDESTROY) {
        ui_core_window = NULL;
        ui_core_window_proc = NULL;
    }
    return CallWindowProc(window_proc, hwnd, message, wparam, lparam);
}

static void
ui_window_hook_core_close(void)
{
    HWND hwnd = (HWND)GetWindowHandle();
    WNDPROC previous;

    if(hwnd == NULL)
        return;
    /* Another subsystem may subclass us later; it will still call through
     * this procedure. Reinstalling above it would create a callback cycle. */
    if(hwnd == ui_core_window)
        return;

    SetLastError(0);
    previous = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                                         (LONG_PTR)ui_core_window_close_proc);
    if(previous == NULL && GetLastError() != 0)
        return;
    ui_core_window = hwnd;
    ui_core_window_proc = previous;
}

static LRESULT CALLBACK
ui_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    NativeWindow *window = (NativeWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    POINT point;
    (void)wparam;
    if(message == WM_NCCREATE) {
        window = (NativeWindow *)((CREATESTRUCT *)lparam)->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    if(window == NULL) return DefWindowProc(hwnd, message, wparam, lparam);
    switch(message) {
    case WM_LBUTTONDOWN:
        window->click_x = (short)LOWORD(lparam); window->click_y = (short)HIWORD(lparam);
        window->drag_active = 1; window->dragged = 0;
        GetCursorPos(&window->drag_last); SetCapture(hwnd); return 0;
    case WM_MOUSEMOVE:
        if(window->drag_active) {
            RECT work;
            int dx, dy;
            GetCursorPos(&point);
            dx = point.x - window->drag_last.x;
            dy = point.y - window->drag_last.y;
            if(dx != 0 || dy != 0) {
                if(SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0)) {
                    WindowPoint position = WindowDragPosition(
                        window->x, window->y, dx, dy, work.left, work.top,
                        work.right - work.left, work.bottom - work.top);
                    window->x = position.x;
                    window->y = position.y;
                } else {
                    window->x += dx;
                    window->y += dy;
                }
                SetWindowPos(hwnd, NULL, window->x, window->y, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
                window->drag_last = point;
                if(WindowDragMoved(dx, dy)) window->dragged = 1;
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if(window->drag_active) { window->drag_active = 0; ReleaseCapture(); if(!window->dragged) window->clicked = 1; }
        return 0;
    case WM_RBUTTONUP:
        window->click_x = (short)LOWORD(lparam); window->click_y = (short)HIWORD(lparam);
        window->right_clicked = 1; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: return 0;
    default: return DefWindowProc(hwnd, message, wparam, lparam);
    }
}

static int ui_window_register_class(void)
{
    WNDCLASSEXA wc; HINSTANCE instance;
    if(ui_window_class != 0) return 1;
    instance = GetModuleHandle(NULL); memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = ui_window_proc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(NATIVE_WINDOW_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm = wc.hIcon; wc.lpszClassName = NATIVE_WINDOW_CLASS_NAME;
    ui_window_class = RegisterClassExA(&wc); return ui_window_class != 0;
}

NativeWindow *OpenNativeWindow(const char *title, int x, int y, int width, int height,
                       int flags, Color background, float ui_scale)
{
    NativeWindow *window; DWORD style = WS_POPUP, ex_style = 0; RECT work;
    if(width <= 0 || height <= 0 || !IsWindowReady() || !ui_window_register_class()) return NULL;
    if((flags & (NATIVE_WINDOW_TOP_RIGHT | NATIVE_WINDOW_CENTER)) != 0 &&
       SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0)) {
        WindowPoint position = WindowInitialPosition(
            x, y, width, height, flags, work.left, work.top,
            work.right - work.left, work.bottom - work.top);
        x = position.x;
        y = position.y;
    }
    if(!(flags & NATIVE_WINDOW_BORDERLESS)) style = WS_OVERLAPPEDWINDOW;
    if(flags & NATIVE_WINDOW_ALWAYS_ON_TOP) ex_style |= WS_EX_TOPMOST;
    if(flags & NATIVE_WINDOW_SKIP_TASKBAR) ex_style |= WS_EX_TOOLWINDOW;
    window = (NativeWindow *)calloc(1, sizeof(*window)); if(window == NULL) return NULL;
    window->width=width; window->height=height; window->scale=ui_scale>0?ui_scale:1; window->background=background; window->x=x; window->y=y;
    window->target=LoadRenderTexture(width,height); if(window->target.id==0) { free(window); return NULL; }
    window->window=CreateWindowExA(ex_style,NATIVE_WINDOW_CLASS_NAME,title?title:"Kryon",style,x,y,width,height,NULL,NULL,GetModuleHandle(NULL),window);
    if(window->window==NULL) { UnloadRenderTexture(window->target); free(window); return NULL; }
    if(ui_window_count >= (int)(sizeof(ui_windows)/sizeof(ui_windows[0]))) {
        DestroyWindow(window->window); UnloadRenderTexture(window->target); free(window); return NULL;
    }
    ui_windows[ui_window_count++] = window;
    ShowWindow(window->window,SW_SHOWNOACTIVATE); UpdateWindow(window->window); return window;
}

void CloseNativeWindow(NativeWindow *window)
{
    if(!window) return;
    if(ui_window_active == window) EndNativeWindow();
    ui_paint_layers_destroy(window->paint_layers);
    for(int i = 0; i < ui_window_count; i++) {
        if(ui_windows[i] != window) continue;
        for(; i < ui_window_count-1; i++) ui_windows[i] = ui_windows[i+1];
        ui_window_count--;
        break;
    }
    DestroyWindow(window->window);
    UnloadRenderTexture(window->target);
    free(window->pixels);
    free(window);
}

void BeginNativeWindow(NativeWindow *window)
{
    if(!window)
        return;
    window->previous_focus_id = GetFocus();
    SetFocus(window->focus_id);
    ui_window_active = window;
    BeginTextureMode(window->target);
    ClearBackground(window->background);
    BeginInterfaceFrame(window->width,window->height,window->scale);
    ui_window_layers_begin();
}
void EndNativeWindow(void)
{
    NativeWindow *window = ui_window_active;
    Image image;
    BITMAPINFO info;
    HDC dc;
    size_t count;
    size_t i;

    if(window == NULL)
        return;
    EndInterfaceFrame();
    if(window->paint_layers != NULL)
        ui_paint_layers_composite(window->paint_layers);
    window->focus_id = GetFocus();
    SetFocus(window->previous_focus_id);
    ui_window_active = NULL;
    EndTextureMode();
    image=ui_paint_readback(window->target.texture); if(!image.data)return;
    ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8); count=(size_t)window->width*window->height;
    window->pixels=(unsigned char *)realloc(window->pixels,count*4); if(!window->pixels){UnloadImage(image);return;}
    for(i=0;i<count;i++){const unsigned char *s=(const unsigned char *)image.data+i*4; unsigned char *d=window->pixels+i*4; d[0]=s[2];d[1]=s[1];d[2]=s[0];d[3]=255;}
    UnloadImage(image); memset(&info,0,sizeof(info)); info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=window->width; info.bmiHeader.biHeight=window->height; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    dc=GetDC(window->window); StretchDIBits(dc,0,0,window->width,window->height,0,0,window->width,window->height,window->pixels,&info,DIB_RGB_COLORS,SRCCOPY); ReleaseDC(window->window,dc);
}
int IsNativeWindowClicked(NativeWindow *w){int v=w&&w->clicked;if(w)w->clicked=0;return v;}
int IsNativeWindowRightClicked(NativeWindow *w){int v=w&&w->right_clicked;if(w)w->right_clicked=0;return v;}
int IsNativeWindowDragged(NativeWindow *w){int v=w&&w->dragged;if(w){w->dragged=0;if(v)w->clicked=0;}return v;}
void GetNativeWindowPosition(NativeWindow *w,int*x,int*y){if(x)*x=w?w->x:0;if(y)*y=w?w->y:0;}
void GetNativeWindowClickPosition(NativeWindow *w,int*x,int*y){if(x)*x=w?w->click_x:-1;if(y)*y=w?w->click_y:-1;}
void PumpWindows(void){MSG m;int i;for(i=0;i<ui_window_count;i++)while(PeekMessage(&m,ui_windows[i]->window,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}}
int StealCoreWindowClose(void)
{
    ui_window_hook_core_close();
    return (int)InterlockedExchange(&ui_window_core_close_pending, 0);
}

#elif defined(NATIVE_WINDOW_HAVE_SDL) /* SDL supports additional native windows
                                   * on Wayland, Windows, and macOS. */

#include <SDL2/SDL.h>
#if defined(__linux__) || defined(__FreeBSD__)
#include <GLES2/gl2.h>
#endif
#include <stdlib.h>

#include "ui_core.h"

#define NATIVE_WINDOW_OWNS_PAINT_LAYERS 1
struct NativeWindow {
    SDL_Window *window;
    PaintLayers *paint_layers;
    int focus_id;
    int previous_focus_id;
    SDL_GLContext context;
    Uint32 window_id;
    int width;
    int height;
    float scale;
    Color background;
    RenderTexture2D target;
#if defined(__linux__) || defined(__FreeBSD__)
    GLuint present_program;
    GLuint present_vbo;
    GLint present_position;
    GLint present_texcoord;
    GLint present_texture;
#endif
    int clicked;
    int right_clicked;
    int click_x;
    int click_y;
    int x, y;                   /* current window position (kept by drag) */
    /* Borderless windows have no title bar, so dragging is app-side: the
     * event watch only marks press/release, and PumpWindows follows the
     * global pointer from the frame loop - derived motion events lose the
     * final step to SDL3-style pointer batching, so the position itself is
     * the source of truth. */
    int drag_active;
    int drag_release_seen;
    int drag_last_gx;
    int drag_last_gy;
    int dragged;
};

static NativeWindow **ui_windows;
static int ui_window_count;
static int ui_window_capacity;
static NativeWindow *ui_window_active;
static int ui_window_event_watch_installed;
#if defined(__linux__) || defined(__FreeBSD__)
static GLuint
ui_window_compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint ok = GL_FALSE;

    if(shader == 0)
        return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(ok != GL_TRUE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int
ui_window_init_presenter(NativeWindow *window)
{
    static const char *vertex_source =
        "attribute vec2 position; attribute vec2 texcoord;"
        "varying vec2 uv; void main(){ uv=texcoord;"
        "gl_Position=vec4(position,0.0,1.0); }";
    static const char *fragment_source =
        "precision mediump float; varying vec2 uv; uniform sampler2D image;"
        "void main(){ gl_FragColor=texture2D(image,uv); }";
    static const GLfloat vertices[] = {
        -1, -1, 0, 0,   1, -1, 1, 0,   -1, 1, 0, 1,
        -1,  1, 0, 1,   1, -1, 1, 0,    1, 1, 1, 1
    };
    GLuint vertex = ui_window_compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = ui_window_compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    GLint linked = GL_FALSE;

    if(vertex == 0 || fragment == 0)
        goto fail;
    window->present_program = glCreateProgram();
    if(window->present_program == 0)
        goto fail;
    glAttachShader(window->present_program, vertex);
    glAttachShader(window->present_program, fragment);
    glLinkProgram(window->present_program);
    glGetProgramiv(window->present_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    vertex = 0;
    fragment = 0;
    if(linked != GL_TRUE)
        goto fail_program;
    window->present_position = glGetAttribLocation(window->present_program, "position");
    window->present_texcoord = glGetAttribLocation(window->present_program, "texcoord");
    window->present_texture = glGetUniformLocation(window->present_program, "image");
    glGenBuffers(1, &window->present_vbo);
    if(window->present_vbo == 0)
        goto fail_program;
    glBindBuffer(GL_ARRAY_BUFFER, window->present_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return 1;

fail_program:
    glDeleteProgram(window->present_program);
    window->present_program = 0;
fail:
    if(vertex != 0)
        glDeleteShader(vertex);
    if(fragment != 0)
        glDeleteShader(fragment);
    return 0;
}

static void
ui_window_present(NativeWindow *window)
{
    glViewport(0, 0, window->width, window->height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(window->present_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, window->target.texture.id);
    glUniform1i(window->present_texture, 0);
    glBindBuffer(GL_ARRAY_BUFFER, window->present_vbo);
    glEnableVertexAttribArray((GLuint)window->present_position);
    glEnableVertexAttribArray((GLuint)window->present_texcoord);
    glVertexAttribPointer((GLuint)window->present_position, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glVertexAttribPointer((GLuint)window->present_texcoord, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray((GLuint)window->present_position);
    glDisableVertexAttribArray((GLuint)window->present_texcoord);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}
#endif

/* raylib owns SDLs normal event pump. An event watch sees secondary-window
 * pointer events without consuming the core windows events. The watch only
 * records plain state: it can run inside SDLs event pump, where calling
 * back into SDL (moving windows, pushing events) is not safe. PumpWindows
 * applies the recorded state from the frame loop. */
static int ui_window_core_close_pending;
static int ui_window_core_close_quit_pushed;

static int
ui_window_event_watch(void *userdata, SDL_Event *event)
{
    Uint32 window_id = 0;

    (void)userdata;
    if(event == NULL)
        return 1;

    if(event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)
        window_id = event->button.windowID;
    else if(event->type == SDL_MOUSEMOTION)
        window_id = event->motion.windowID;
    else if(event->type == SDL_WINDOWEVENT &&
            event->window.event == SDL_WINDOWEVENT_CLOSE)
        window_id = event->window.windowID;

    if(window_id != 0) {
        for(int i = 0; i < ui_window_count; i++) {
            NativeWindow *window = ui_windows[i];
            if(window == NULL || window->window_id != window_id)
                continue;
            if(event->type == SDL_WINDOWEVENT) {
                /* Closing an overlay is not the app quitting; swallow it so
                 * the core-window bridge below never fires for our windows. */
            } else if(event->type == SDL_MOUSEBUTTONDOWN &&
                      event->button.button == SDL_BUTTON_LEFT) {
                window->drag_active = 1;
                window->drag_release_seen = 0;
                window->dragged = 0;
                /* Baseline the drag at the true press position; starting
                 * it at the first pump frame would swallow any motion
                 * between press and pump. */
                window->drag_last_gx = window->x + event->button.x;
                window->drag_last_gy = window->y + event->button.y;
            } else if(event->type == SDL_MOUSEBUTTONUP) {
                if(window->drag_active &&
                   event->button.button == SDL_BUTTON_LEFT)
                    window->drag_release_seen = 1;
                window->click_x = event->button.x;
                window->click_y = event->button.y;
                if(event->button.button == SDL_BUTTON_RIGHT)
                    window->right_clicked = 1;
                else if(event->button.button == SDL_BUTTON_LEFT)
                    window->clicked = 1;
            }
            return 1;
        }
    }

    /* SDL2 reports the core windows close request (X button, Alt+F4, WM
     * delete) as SDL_WINDOWEVENT_CLOSE; raylibs SDL backend only latches
     * SDL_QUIT, so without a bridge the close button does nothing. */
    if(event->type == SDL_WINDOWEVENT &&
       event->window.event == SDL_WINDOWEVENT_CLOSE)
        ui_window_core_close_pending = 1;
    return 1;
}

static int
ui_window_register(NativeWindow *win)
{
    if(ui_window_count == ui_window_capacity) {
        int capacity = ui_window_capacity > 0 ? ui_window_capacity * 2 : 8;
        NativeWindow **windows = (NativeWindow **)realloc(
            ui_windows, (size_t)capacity * sizeof(*windows));
        if(windows == NULL)
            return 0;
        ui_windows = windows;
        ui_window_capacity = capacity;
    }
    if(!ui_window_event_watch_installed) {
        SDL_AddEventWatch(ui_window_event_watch, NULL);
        ui_window_event_watch_installed = 1;
    }
    ui_windows[ui_window_count++] = win;
    return 1;
}

static void
ui_window_unregister(NativeWindow *win)
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
    if(ui_window_count == 0 && ui_window_event_watch_installed) {
        SDL_DelEventWatch(ui_window_event_watch, NULL);
        ui_window_event_watch_installed = 0;
        free(ui_windows);
        ui_windows = NULL;
        ui_window_capacity = 0;
    }
}

NativeWindow *
OpenNativeWindow(const char *title, int x, int y, int width, int height,
             int flags, Color background, float ui_scale)
{
    NativeWindow *win;
    Uint32 sdl_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
    SDL_Window *previous_window;
    SDL_GLContext previous_context;

    if(width <= 0 || height <= 0 || !IsWindowReady())
        return NULL;
    if((flags & NATIVE_WINDOW_BORDERLESS) != 0)
        sdl_flags |= SDL_WINDOW_BORDERLESS;
    if((flags & NATIVE_WINDOW_ALWAYS_ON_TOP) != 0)
        sdl_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    if((flags & NATIVE_WINDOW_SKIP_TASKBAR) != 0)
        sdl_flags |= SDL_WINDOW_SKIP_TASKBAR;
    if((flags & (NATIVE_WINDOW_TOP_RIGHT | NATIVE_WINDOW_CENTER)) != 0) {
        SDL_Rect usable;
        if(SDL_GetDisplayUsableBounds(0, &usable) == 0) {
            WindowPoint position = WindowInitialPosition(
                x, y, width, height, flags,
                usable.x, usable.y, usable.w, usable.h);
            x = position.x;
            y = position.y;
        }
    }

    win = (NativeWindow *)calloc(1, sizeof(NativeWindow));
    if(win == NULL)
        return NULL;
    win->target = LoadRenderTexture(width, height);
    if(win->target.id == 0) {
        free(win);
        return NULL;
    }
    /* Share fonts, icons and every other GPU asset with raylibs current
     * context. Each secondary window still owns a real context and swaps
     * directly; no framebuffer readback or software window surface is used. */
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    win->window = SDL_CreateWindow(title, x, y, width, height, sdl_flags);
    if(win->window == NULL) {
        UnloadRenderTexture(win->target);
        free(win);
        return NULL;
    }
    previous_window = SDL_GL_GetCurrentWindow();
    previous_context = SDL_GL_GetCurrentContext();
    win->context = SDL_GL_CreateContext(win->window);
    if(win->context == NULL) {
        SDL_DestroyWindow(win->window);
        UnloadRenderTexture(win->target);
        free(win);
        return NULL;
    }
#if defined(__linux__) || defined(__FreeBSD__)
    if(!ui_window_init_presenter(win)) {
        SDL_GL_MakeCurrent(previous_window, previous_context);
        SDL_GL_DeleteContext(win->context);
        SDL_DestroyWindow(win->window);
        UnloadRenderTexture(win->target);
        free(win);
        return NULL;
    }
#endif
    SDL_GL_MakeCurrent(previous_window, previous_context);
    win->window_id = SDL_GetWindowID(win->window);
    win->width = width;
    win->height = height;
    win->scale = ui_scale > 0.0f ? ui_scale : 1.0f;
    win->background = background;
    if(!ui_window_register(win)) {
        SDL_GL_DeleteContext(win->context);
        SDL_DestroyWindow(win->window);
        UnloadRenderTexture(win->target);
        free(win);
        return NULL;
    }
    SDL_ShowWindow(win->window);
    SDL_GetWindowPosition(win->window, &win->x, &win->y);
    return win;
}

void
CloseNativeWindow(NativeWindow *window)
{
    if(window == NULL)
        return;
    if(ui_window_active == window) EndNativeWindow();
    ui_window_unregister(window);
    ui_paint_layers_destroy(window->paint_layers);
#if defined(__linux__) || defined(__FreeBSD__)
    {
        SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
        if(SDL_GL_MakeCurrent(window->window, window->context) == 0) {
            glDeleteBuffers(1, &window->present_vbo);
            glDeleteProgram(window->present_program);
            SDL_GL_MakeCurrent(previous_window, previous_context);
        }
    }
#endif
    SDL_GL_DeleteContext(window->context);
    SDL_DestroyWindow(window->window);
    UnloadRenderTexture(window->target);
    free(window);
}

void
BeginNativeWindow(NativeWindow *window)
{
    if(window == NULL)
        return;
    window->previous_focus_id = GetFocus();
    SetFocus(window->focus_id);
    ui_window_active = window;
    BeginTextureMode(window->target);
    ClearBackground(window->background);
    BeginInterfaceFrame(window->width, window->height, window->scale);
    ui_window_layers_begin();
}

void
EndNativeWindow(void)
{
    NativeWindow *window = ui_window_active;

    if(window == NULL)
        return;
    EndInterfaceFrame();
    if(window->paint_layers) ui_paint_layers_composite(window->paint_layers);
    window->focus_id = GetFocus();
    SetFocus(window->previous_focus_id);
    ui_window_active = NULL;
    EndTextureMode();
#if defined(__linux__) || defined(__FreeBSD__)
    {
        SDL_Window *previous_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext previous_context = SDL_GL_GetCurrentContext();
        /* The render target is shared with a different GL context. A context
         * switch alone does not establish completion ordering for writes to
         * shared objects; without this barrier Mesa can present partially
         * rendered font quads and corrupted glyphs. GLES2 has no portable
         * fence-sync API, so finish the small auxiliary target explicitly. */
        glFinish();
        if(SDL_GL_MakeCurrent(window->window, window->context) == 0) {
            ui_window_present(window);
            SDL_GL_SwapWindow(window->window);
            SDL_GL_MakeCurrent(previous_window, previous_context);
        }
    }
#else
    SDL_GL_SwapWindow(window->window);
#endif
}

int
IsNativeWindowClicked(NativeWindow *window)
{
    if(window == NULL || !window->clicked)
        return 0;
    window->clicked = 0;
    return 1;
}

int
IsNativeWindowRightClicked(NativeWindow *window)
{
    if(window == NULL || !window->right_clicked)
        return 0;
    window->right_clicked = 0;
    return 1;
}

int
IsNativeWindowDragged(NativeWindow *window)
{
    if(window == NULL || !window->dragged)
        return 0;
    window->dragged = 0;
    /* Swallow the click that started the drag so it is not also reported
     * as a plain click. */
    window->clicked = 0;
    window->right_clicked = 0;
    return 1;
}

void
GetNativeWindowPosition(NativeWindow *window, int *x, int *y)
{
    if(x != NULL)
        *x = window != NULL ? window->x : 0;
    if(y != NULL)
        *y = window != NULL ? window->y : 0;
}

/* Apply state the event watch recorded (drag motion, core-window close
 * requests) from the frame loop, where calling into SDL is safe. Called
 * once per frame by SetFrameCamera. */
int
StealCoreWindowClose(void)
{
    int pending = ui_window_core_close_pending;

    if(pending) {
        ui_window_core_close_pending = 0;
        ui_window_core_close_quit_pushed = 0;
    }
    return pending;
}

/* While a drag is active the mouse is captured, so the release keeps
 * arriving even when the pointer briefly outruns the small overlay
 * window. Motion comes from diffing the global pointer position each
 * frame: derived motion events lose the final step to SDL3-style
 * pointer batching, and the window position is read from SDL exactly
 * once per drag - per-frame round-trips stall the frame loop. */
static int ui_window_drag_captured;

void
PumpWindows(void)
{
    int any_drag = 0;

    if(ui_window_core_close_pending && !ui_window_core_close_quit_pushed) {
        SDL_Event quit;

        SDL_zero(quit);
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
        ui_window_core_close_quit_pushed = 1;
    }
    for(int i = 0; i < ui_window_count; i++) {
        NativeWindow *window = ui_windows[i];

        if(window == NULL || !window->drag_active)
            continue;
        if(!ui_window_drag_captured) {
            ui_window_drag_captured = 1;
            SDL_CaptureMouse(SDL_TRUE);
            SDL_GetWindowPosition(window->window, &window->x, &window->y);
        }
        {
            int gx = 0, gy = 0;
            int dx, dy;

            SDL_GetGlobalMouseState(&gx, &gy);
            dx = gx - window->drag_last_gx;
            dy = gy - window->drag_last_gy;
            window->drag_last_gx = gx;
            window->drag_last_gy = gy;
            if(dx != 0 || dy != 0) {
                SDL_Rect usable;
                if(SDL_GetDisplayUsableBounds(0, &usable) == 0) {
                    WindowPoint position = WindowDragPosition(
                        window->x, window->y, dx, dy,
                        usable.x, usable.y, usable.w, usable.h);
                    window->x = position.x;
                    window->y = position.y;
                } else {
                    window->x += dx;
                    window->y += dy;
                }
                SDL_SetWindowPosition(window->window, window->x, window->y);
                if(WindowDragMoved(dx, dy))
                    window->dragged = 1;
            }
        }
        if(window->drag_release_seen) {
            window->drag_active = 0;
            window->drag_release_seen = 0;
        } else {
            any_drag = 1;
        }
    }
    if(ui_window_drag_captured && !any_drag) {
        ui_window_drag_captured = 0;
        SDL_CaptureMouse(SDL_FALSE);
    }
}

void
GetNativeWindowClickPosition(NativeWindow *window, int *x, int *y)
{
    if(x != NULL)
        *x = window != NULL ? window->click_x : -1;
    if(y != NULL)
        *y = window != NULL ? window->click_y : -1;
}

#else /* web/android: no extra windows */

NativeWindow *
OpenNativeWindow(const char *title, int x, int y, int width, int height,
             int flags, Color background, float ui_scale)
{
    (void)title; (void)x; (void)y; (void)width; (void)height;
    (void)flags; (void)background; (void)ui_scale;
    return NULL;
}

void
CloseNativeWindow(NativeWindow *window)
{
    (void)window;
}

void
BeginNativeWindow(NativeWindow *window)
{
    (void)window;
}

void
EndNativeWindow(void)
{
}

int
IsNativeWindowClicked(NativeWindow *window)
{
    (void)window;
    return 0;
}

int
IsNativeWindowRightClicked(NativeWindow *window)
{
    (void)window;
    return 0;
}

int
IsNativeWindowDragged(NativeWindow *window)
{
    (void)window;
    return 0;
}

void
PumpWindows(void)
{
}

int
StealCoreWindowClose(void)
{
    return 0;
}

void
GetNativeWindowPosition(NativeWindow *window, int *x, int *y)
{
    if(x != NULL)
        *x = 0;
    if(y != NULL)
        *y = 0;
    (void)window;
}

void
GetNativeWindowClickPosition(NativeWindow *window, int *x, int *y)
{
    if(x != NULL)
        *x = -1;
    if(y != NULL)
        *y = -1;
    (void)window;
}

#endif

PaintLayers *ui_window_paint_layers(void)
{
#if defined(NATIVE_WINDOW_OWNS_PAINT_LAYERS)
    NativeWindow *window = ui_window_active;
    if(window == NULL) return NULL;
    if(window->paint_layers == NULL) {
        window->paint_layers = ui_paint_layers_create();
        ui_paint_layers_frame(window->paint_layers,window->width,window->height);
    }
    return window->paint_layers;
#else
    return NULL;
#endif
}

int ui_window_frame_active(void)
{
#if defined(NATIVE_WINDOW_OWNS_PAINT_LAYERS)
    return ui_window_active != NULL;
#else
    return 0;
#endif
}

void ui_window_layers_begin(void)
{
#if defined(NATIVE_WINDOW_OWNS_PAINT_LAYERS)
    NativeWindow *window = ui_window_active;
    if(window == NULL) return;
    if(window->paint_layers == NULL)
        window->paint_layers = ui_paint_layers_create();
    ui_paint_layers_frame(window->paint_layers,window->width,window->height);
#endif
}
