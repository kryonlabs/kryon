/**
 * @file platform_linux.c
 * @brief Linux Platform Implementation using X11/Wayland
 * 
 * Provides Linux-specific implementations for window management, input handling,
 * and system integration using X11 as the primary backend.
 */

#include "platform.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>

// =============================================================================
// PLATFORM DATA STRUCTURES
// =============================================================================

typedef struct {
    Display *display;
    int screen;
    Visual *visual;
    Colormap colormap;
    int depth;
    
    // Input state
    bool keys[256];
    KryonInputModifiers modifiers;
    float mouse_x, mouse_y;
    bool mouse_buttons[8];
    
    // Time tracking
    struct timeval start_time;
    
    // Clipboard
    char *clipboard_text;
} KryonLinuxPlatformData;

typedef struct {
    Window window;
    KryonWindowConfig config;
    bool should_close;
    
    // Graphics context
    GC gc;
    XImage *back_buffer;
    char *back_buffer_data;
    int back_buffer_size;
} KryonLinuxWindow;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool linux_init(void);
static void linux_shutdown(void);
static KryonWindow* linux_window_create(const KryonWindowConfig *config);
static void linux_window_destroy(KryonWindow *window);
static void linux_window_show(KryonWindow *window);
static void linux_window_hide(KryonWindow *window);
static void linux_window_set_title(KryonWindow *window, const char *title);
static void linux_window_set_size(KryonWindow *window, int width, int height);
static void linux_window_set_position(KryonWindow *window, int x, int y);
static void linux_window_get_size(KryonWindow *window, int *width, int *height);
static void linux_window_get_position(KryonWindow *window, int *x, int *y);
static void linux_window_get_framebuffer_size(KryonWindow *window, int *width, int *height);
static bool linux_window_should_close(KryonWindow *window);
static void linux_window_swap_buffers(KryonWindow *window);
static void linux_poll_events(void);
static void linux_wait_events(void);
static void linux_post_empty_event(void);
static void linux_get_input_state(KryonInputState *state);
static void linux_set_cursor_mode(KryonWindow *window, int mode);
static void linux_set_clipboard_text(const char *text);
static const char* linux_get_clipboard_text(void);
static double linux_get_time(void);
static void linux_sleep(double seconds);
static int linux_get_display_count(void);
static void linux_get_display_info(int display_index, KryonDisplayInfo *info);
static char* linux_show_file_dialog(const KryonFileDialog *config);
static bool linux_file_exists(const char *path);
static char* linux_get_app_data_path(void);
static char* linux_get_user_documents_path(void);
static void* linux_create_graphics_context(KryonWindow *window);
static void linux_destroy_graphics_context(void *context);
static void linux_make_context_current(void *context);

// Key mapping
static KryonKeyCode linux_translate_key(KeySym keysym);
static void linux_process_key_event(XKeyEvent *event, bool pressed);
static void linux_process_mouse_event(XButtonEvent *event, bool pressed);
static void linux_process_motion_event(XMotionEvent *event);

// Global platform data
static KryonLinuxPlatformData *g_linux_platform = NULL;

// =============================================================================
// PLATFORM INTERFACE
// =============================================================================

const KryonPlatformInterface kryon_platform_linux_interface = {
    .init = linux_init,
    .shutdown = linux_shutdown,
    .window_create = linux_window_create,
    .window_destroy = linux_window_destroy,
    .window_show = linux_window_show,
    .window_hide = linux_window_hide,
    .window_set_title = linux_window_set_title,
    .window_set_size = linux_window_set_size,
    .window_set_position = linux_window_set_position,
    .window_get_size = linux_window_get_size,
    .window_get_position = linux_window_get_position,
    .window_get_framebuffer_size = linux_window_get_framebuffer_size,
    .window_should_close = linux_window_should_close,
    .window_swap_buffers = linux_window_swap_buffers,
    .poll_events = linux_poll_events,
    .wait_events = linux_wait_events,
    .post_empty_event = linux_post_empty_event,
    .get_input_state = linux_get_input_state,
    .set_cursor_mode = linux_set_cursor_mode,
    .set_clipboard_text = linux_set_clipboard_text,
    .get_clipboard_text = linux_get_clipboard_text,
    .get_time = linux_get_time,
    .sleep = linux_sleep,
    .get_display_count = linux_get_display_count,
    .get_display_info = linux_get_display_info,
    .show_file_dialog = linux_show_file_dialog,
    .file_exists = linux_file_exists,
    .get_app_data_path = linux_get_app_data_path,
    .get_user_documents_path = linux_get_user_documents_path,
    .create_graphics_context = linux_create_graphics_context,
    .destroy_graphics_context = linux_destroy_graphics_context,
    .make_context_current = linux_make_context_current
};

// =============================================================================
// PLATFORM INITIALIZATION
// =============================================================================

static bool linux_init(void) {
    if (g_linux_platform) {
        return true; // Already initialized
    }
    
    g_linux_platform = kryon_alloc(sizeof(KryonLinuxPlatformData));
    if (!g_linux_platform) {
        return false;
    }
    
    memset(g_linux_platform, 0, sizeof(KryonLinuxPlatformData));
    
    // Open X11 display
    g_linux_platform->display = XOpenDisplay(NULL);
    if (!g_linux_platform->display) {
        kryon_free(g_linux_platform);
        g_linux_platform = NULL;
        return false;
    }
    
    g_linux_platform->screen = DefaultScreen(g_linux_platform->display);
    g_linux_platform->visual = DefaultVisual(g_linux_platform->display, g_linux_platform->screen);
    g_linux_platform->colormap = DefaultColormap(g_linux_platform->display, g_linux_platform->screen);
    g_linux_platform->depth = DefaultDepth(g_linux_platform->display, g_linux_platform->screen);
    
    // Initialize time tracking
    gettimeofday(&g_linux_platform->start_time, NULL);
    
    return true;
}

static void linux_shutdown(void) {
    if (!g_linux_platform) {
        return;
    }
    
    if (g_linux_platform->clipboard_text) {
        kryon_free(g_linux_platform->clipboard_text);
    }
    
    if (g_linux_platform->display) {
        XCloseDisplay(g_linux_platform->display);
    }
    
    kryon_free(g_linux_platform);
    g_linux_platform = NULL;
}

// =============================================================================
// WINDOW MANAGEMENT
// =============================================================================

static KryonWindow* linux_window_create(const KryonWindowConfig *config) {
    if (!g_linux_platform || !config) {
        return NULL;
    }
    
    KryonLinuxWindow *window = kryon_alloc(sizeof(KryonLinuxWindow));
    if (!window) {
        return NULL;
    }
    
    memset(window, 0, sizeof(KryonLinuxWindow));
    window->config = *config;
    
    // Create X11 window
    XSetWindowAttributes attrs;
    attrs.colormap = g_linux_platform->colormap;
    attrs.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | 
                       ButtonReleaseMask | PointerMotionMask | StructureNotifyMask |
                       ExposureMask | FocusChangeMask;
    
    int x = config->x >= 0 ? config->x : 100;
    int y = config->y >= 0 ? config->y : 100;
    
    window->window = XCreateWindow(
        g_linux_platform->display,
        RootWindow(g_linux_platform->display, g_linux_platform->screen),
        x, y, config->width, config->height, 0,
        g_linux_platform->depth, InputOutput, g_linux_platform->visual,
        CWColormap | CWEventMask, &attrs
    );
    
    if (!window->window) {
        kryon_free(window);
        return NULL;
    }
    
    // Set window title
    if (config->title) {
        XStoreName(g_linux_platform->display, window->window, config->title);
    }
    
    // Create graphics context
    window->gc = XCreateGC(g_linux_platform->display, window->window, 0, NULL);
    
    // Create back buffer for software rendering
    window->back_buffer_size = config->width * config->height * 4; // RGBA
    window->back_buffer_data = kryon_alloc(window->back_buffer_size);
    if (window->back_buffer_data) {
        window->back_buffer = XCreateImage(
            g_linux_platform->display, g_linux_platform->visual, g_linux_platform->depth,
            ZPixmap, 0, window->back_buffer_data, config->width, config->height, 32, 0
        );
    }
    
    // Set window properties
    if (!config->resizable) {
        XSizeHints hints;
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = hints.max_width = config->width;
        hints.min_height = hints.max_height = config->height;
        XSetWMNormalHints(g_linux_platform->display, window->window, &hints);
    }
    
    // Handle window close button
    Atom wm_delete = XInternAtom(g_linux_platform->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_linux_platform->display, window->window, &wm_delete, 1);
    
    if (config->visible) {
        XMapWindow(g_linux_platform->display, window->window);
    }
    
    XFlush(g_linux_platform->display);
    
    return (KryonWindow*)window;
}

static void linux_window_destroy(KryonWindow *window) {
    if (!window || !g_linux_platform) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    
    if (linux_window->back_buffer) {
        XDestroyImage(linux_window->back_buffer);
    } else if (linux_window->back_buffer_data) {
        kryon_free(linux_window->back_buffer_data);
    }
    
    if (linux_window->gc) {
        XFreeGC(g_linux_platform->display, linux_window->gc);
    }
    
    if (linux_window->window) {
        XDestroyWindow(g_linux_platform->display, linux_window->window);
    }
    
    kryon_free(linux_window);
}

static void linux_window_show(KryonWindow *window) {
    if (!window || !g_linux_platform) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    XMapWindow(g_linux_platform->display, linux_window->window);
    XFlush(g_linux_platform->display);
}

static void linux_window_hide(KryonWindow *window) {
    if (!window || !g_linux_platform) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    XUnmapWindow(g_linux_platform->display, linux_window->window);
    XFlush(g_linux_platform->display);
}

static void linux_window_set_title(KryonWindow *window, const char *title) {
    if (!window || !g_linux_platform || !title) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    XStoreName(g_linux_platform->display, linux_window->window, title);
    XFlush(g_linux_platform->display);
}

static void linux_window_set_size(KryonWindow *window, int width, int height) {
    if (!window || !g_linux_platform) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    XResizeWindow(g_linux_platform->display, linux_window->window, width, height);
    XFlush(g_linux_platform->display);
}

static void linux_window_set_position(KryonWindow *window, int x, int y) {
    if (!window || !g_linux_platform) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    XMoveWindow(g_linux_platform->display, linux_window->window, x, y);
    XFlush(g_linux_platform->display);
}

static void linux_window_get_size(KryonWindow *window, int *width, int *height) {
    if (!window || !g_linux_platform) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    
    Window root;
    int x, y;
    unsigned int w, h, border, depth;
    XGetGeometry(g_linux_platform->display, linux_window->window, &root, &x, &y, &w, &h, &border, &depth);
    
    if (width) *width = w;
    if (height) *height = h;
}

static void linux_window_get_position(KryonWindow *window, int *x, int *y) {
    if (!window || !g_linux_platform) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    
    Window root, child;
    int root_x, root_y;
    unsigned int width, height, border, depth;
    XGetGeometry(g_linux_platform->display, linux_window->window, &root, &root_x, &root_y, &width, &height, &border, &depth);
    XTranslateCoordinates(g_linux_platform->display, linux_window->window, root, 0, 0, &root_x, &root_y, &child);
    
    if (x) *x = root_x;
    if (y) *y = root_y;
}

static void linux_window_get_framebuffer_size(KryonWindow *window, int *width, int *height) {
    // For X11, framebuffer size equals window size
    linux_window_get_size(window, width, height);
}

static bool linux_window_should_close(KryonWindow *window) {
    if (!window) {
        return true;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    return linux_window->should_close;
}

static void linux_window_swap_buffers(KryonWindow *window) {
    if (!window || !g_linux_platform) {
        return;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    
    if (linux_window->back_buffer) {
        XPutImage(g_linux_platform->display, linux_window->window, linux_window->gc,
                 linux_window->back_buffer, 0, 0, 0, 0,
                 linux_window->back_buffer->width, linux_window->back_buffer->height);
    }
    
    XFlush(g_linux_platform->display);
}

// =============================================================================
// EVENT HANDLING
// =============================================================================

static void linux_poll_events(void) {
    if (!g_linux_platform) {
        return;
    }
    
    while (XPending(g_linux_platform->display)) {
        XEvent event;
        XNextEvent(g_linux_platform->display, &event);
        
        switch (event.type) {
            case KeyPress:
                linux_process_key_event(&event.xkey, true);
                break;
                
            case KeyRelease:
                linux_process_key_event(&event.xkey, false);
                break;
                
            case ButtonPress:
                linux_process_mouse_event(&event.xbutton, true);
                break;
                
            case ButtonRelease:
                linux_process_mouse_event(&event.xbutton, false);
                break;
                
            case MotionNotify:
                linux_process_motion_event(&event.xmotion);
                break;
                
            case ClientMessage:
                // Handle window close
                if (event.xclient.data.l[0] == (long)XInternAtom(g_linux_platform->display, "WM_DELETE_WINDOW", False)) {
                    // Find window and mark for closure
                    // This would require maintaining a window list
                }
                break;
        }
    }
}

static void linux_wait_events(void) {
    if (!g_linux_platform) {
        return;
    }
    
    XEvent event;
    XNextEvent(g_linux_platform->display, &event);
    XPutBackEvent(g_linux_platform->display, &event);
    linux_poll_events();
}

static void linux_post_empty_event(void) {
    if (!g_linux_platform) {
        return;
    }
    
    // Send a dummy event to wake up event loop
    XClientMessageEvent event;
    event.type = ClientMessage;
    event.display = g_linux_platform->display;
    event.window = DefaultRootWindow(g_linux_platform->display);
    event.message_type = XInternAtom(g_linux_platform->display, "KRYON_DUMMY", False);
    event.format = 32;
    
    XSendEvent(g_linux_platform->display, DefaultRootWindow(g_linux_platform->display), False, 0, (XEvent*)&event);
    XFlush(g_linux_platform->display);
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

static void linux_get_input_state(KryonInputState *state) {
    if (!state || !g_linux_platform) {
        return;
    }
    
    memset(state, 0, sizeof(KryonInputState));
    
    // Copy key states
    memcpy(state->keys, g_linux_platform->keys, sizeof(state->keys));
    state->modifiers = g_linux_platform->modifiers;
    
    // Copy mouse state
    state->mouse_x = g_linux_platform->mouse_x;
    state->mouse_y = g_linux_platform->mouse_y;
    memcpy(state->mouse_buttons, g_linux_platform->mouse_buttons, sizeof(state->mouse_buttons));
}

static void linux_set_cursor_mode(KryonWindow *window, int mode) {
    // TODO: Implement cursor mode changes (hide, lock, etc.)
    (void)window;
    (void)mode;
}

static void linux_set_clipboard_text(const char *text) {
    if (!g_linux_platform || !text) {
        return;
    }
    
    if (g_linux_platform->clipboard_text) {
        kryon_free(g_linux_platform->clipboard_text);
    }
    
    size_t len = strlen(text) + 1;
    g_linux_platform->clipboard_text = kryon_alloc(len);
    if (g_linux_platform->clipboard_text) {
        memcpy(g_linux_platform->clipboard_text, text, len);
    }
}

static const char* linux_get_clipboard_text(void) {
    if (!g_linux_platform) {
        return NULL;
    }
    
    return g_linux_platform->clipboard_text;
}

// =============================================================================
// SYSTEM UTILITIES
// =============================================================================

static double linux_get_time(void) {
    if (!g_linux_platform) {
        return 0.0;
    }
    
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    double elapsed = (current_time.tv_sec - g_linux_platform->start_time.tv_sec) +
                    (current_time.tv_usec - g_linux_platform->start_time.tv_usec) / 1000000.0;
    
    return elapsed;
}

static void linux_sleep(double seconds) {
    if (seconds <= 0) {
        return;
    }
    
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1000000000);
    
    nanosleep(&ts, NULL);
}

static int linux_get_display_count(void) {
    if (!g_linux_platform) {
        return 0;
    }
    
    // Simple implementation - just return 1 for now
    return 1;
}

static void linux_get_display_info(int display_index, KryonDisplayInfo *info) {
    if (!info || !g_linux_platform || display_index != 0) {
        return;
    }
    
    info->width = DisplayWidth(g_linux_platform->display, g_linux_platform->screen);
    info->height = DisplayHeight(g_linux_platform->display, g_linux_platform->screen);
    info->refresh_rate = 60; // Default assumption
    info->dpi_scale = 1.0f;
    info->name = "Primary Display";
    info->is_primary = true;
}

// =============================================================================
// FILE SYSTEM
// =============================================================================

static char* linux_show_file_dialog(const KryonFileDialog *config) {
    // TODO: Implement file dialog using zenity or similar
    (void)config;
    return NULL;
}

static bool linux_file_exists(const char *path) {
    if (!path) {
        return false;
    }
    
    struct stat st;
    return stat(path, &st) == 0;
}

static char* linux_get_app_data_path(void) {
    const char *home = getenv("HOME");
    if (!home) {
        return NULL;
    }
    
    size_t len = strlen(home) + strlen("/.local/share/kryon/") + 1;
    char *path = kryon_alloc(len);
    if (path) {
        snprintf(path, len, "%s/.local/share/kryon/", home);
    }
    
    return path;
}

static char* linux_get_user_documents_path(void) {
    const char *home = getenv("HOME");
    if (!home) {
        return NULL;
    }
    
    size_t len = strlen(home) + strlen("/Documents/") + 1;
    char *path = kryon_alloc(len);
    if (path) {
        snprintf(path, len, "%s/Documents/", home);
    }
    
    return path;
}

// =============================================================================
// GRAPHICS CONTEXT
// =============================================================================

static void* linux_create_graphics_context(KryonWindow *window) {
    // For software rendering, return the window's back buffer
    if (!window) {
        return NULL;
    }
    
    KryonLinuxWindow *linux_window = (KryonLinuxWindow*)window;
    return linux_window->back_buffer_data;
}

static void linux_destroy_graphics_context(void *context) {
    // Context is managed by window, nothing to do
    (void)context;
}

static void linux_make_context_current(void *context) {
    // For software rendering, nothing to do
    (void)context;
}

// =============================================================================
// INPUT TRANSLATION
// =============================================================================

static KryonKeyCode linux_translate_key(KeySym keysym) {
    switch (keysym) {
        case XK_a: case XK_A: return KRYON_KEY_A;
        case XK_b: case XK_B: return KRYON_KEY_B;
        case XK_c: case XK_C: return KRYON_KEY_C;
        case XK_d: case XK_D: return KRYON_KEY_D;
        case XK_e: case XK_E: return KRYON_KEY_E;
        case XK_f: case XK_F: return KRYON_KEY_F;
        case XK_g: case XK_G: return KRYON_KEY_G;
        case XK_h: case XK_H: return KRYON_KEY_H;
        case XK_i: case XK_I: return KRYON_KEY_I;
        case XK_j: case XK_J: return KRYON_KEY_J;
        case XK_k: case XK_K: return KRYON_KEY_K;
        case XK_l: case XK_L: return KRYON_KEY_L;
        case XK_m: case XK_M: return KRYON_KEY_M;
        case XK_n: case XK_N: return KRYON_KEY_N;
        case XK_o: case XK_O: return KRYON_KEY_O;
        case XK_p: case XK_P: return KRYON_KEY_P;
        case XK_q: case XK_Q: return KRYON_KEY_Q;
        case XK_r: case XK_R: return KRYON_KEY_R;
        case XK_s: case XK_S: return KRYON_KEY_S;
        case XK_t: case XK_T: return KRYON_KEY_T;
        case XK_u: case XK_U: return KRYON_KEY_U;
        case XK_v: case XK_V: return KRYON_KEY_V;
        case XK_w: case XK_W: return KRYON_KEY_W;
        case XK_x: case XK_X: return KRYON_KEY_X;
        case XK_y: case XK_Y: return KRYON_KEY_Y;
        case XK_z: case XK_Z: return KRYON_KEY_Z;
        
        case XK_1: return KRYON_KEY_1;
        case XK_2: return KRYON_KEY_2;
        case XK_3: return KRYON_KEY_3;
        case XK_4: return KRYON_KEY_4;
        case XK_5: return KRYON_KEY_5;
        case XK_6: return KRYON_KEY_6;
        case XK_7: return KRYON_KEY_7;
        case XK_8: return KRYON_KEY_8;
        case XK_9: return KRYON_KEY_9;
        case XK_0: return KRYON_KEY_0;
        
        case XK_Return: return KRYON_KEY_RETURN;
        case XK_Escape: return KRYON_KEY_ESCAPE;
        case XK_BackSpace: return KRYON_KEY_BACKSPACE;
        case XK_Tab: return KRYON_KEY_TAB;
        case XK_space: return KRYON_KEY_SPACE;
        
        case XK_Control_L: return KRYON_KEY_LCTRL;
        case XK_Shift_L: return KRYON_KEY_LSHIFT;
        case XK_Alt_L: return KRYON_KEY_LALT;
        case XK_Super_L: return KRYON_KEY_LGUI;
        case XK_Control_R: return KRYON_KEY_RCTRL;
        case XK_Shift_R: return KRYON_KEY_RSHIFT;
        case XK_Alt_R: return KRYON_KEY_RALT;
        case XK_Super_R: return KRYON_KEY_RGUI;
        
        case XK_Right: return KRYON_KEY_RIGHT;
        case XK_Left: return KRYON_KEY_LEFT;
        case XK_Down: return KRYON_KEY_DOWN;
        case XK_Up: return KRYON_KEY_UP;
        
        case XK_F1: return KRYON_KEY_F1;
        case XK_F2: return KRYON_KEY_F2;
        case XK_F3: return KRYON_KEY_F3;
        case XK_F4: return KRYON_KEY_F4;
        case XK_F5: return KRYON_KEY_F5;
        case XK_F6: return KRYON_KEY_F6;
        case XK_F7: return KRYON_KEY_F7;
        case XK_F8: return KRYON_KEY_F8;
        case XK_F9: return KRYON_KEY_F9;
        case XK_F10: return KRYON_KEY_F10;
        case XK_F11: return KRYON_KEY_F11;
        case XK_F12: return KRYON_KEY_F12;
        
        default: return KRYON_KEY_UNKNOWN;
    }
}

static void linux_process_key_event(XKeyEvent *event, bool pressed) {
    if (!g_linux_platform) {
        return;
    }
    
    KeySym keysym = XLookupKeysym(event, 0);
    KryonKeyCode key = linux_translate_key(keysym);
    
    if (key != KRYON_KEY_UNKNOWN && key < 256) {
        g_linux_platform->keys[key] = pressed;
    }
    
    // Update modifiers
    g_linux_platform->modifiers = KRYON_MOD_NONE;
    if (event->state & ShiftMask) g_linux_platform->modifiers |= KRYON_MOD_SHIFT;
    if (event->state & ControlMask) g_linux_platform->modifiers |= KRYON_MOD_CTRL;
    if (event->state & Mod1Mask) g_linux_platform->modifiers |= KRYON_MOD_ALT;
    if (event->state & Mod4Mask) g_linux_platform->modifiers |= KRYON_MOD_GUI;
}

static void linux_process_mouse_event(XButtonEvent *event, bool pressed) {
    if (!g_linux_platform) {
        return;
    }
    
    KryonMouseButton button = KRYON_MOUSE_LEFT;
    
    switch (event->button) {
        case Button1: button = KRYON_MOUSE_LEFT; break;
        case Button2: button = KRYON_MOUSE_MIDDLE; break;
        case Button3: button = KRYON_MOUSE_RIGHT; break;
        case 8: button = KRYON_MOUSE_X1; break;
        case 9: button = KRYON_MOUSE_X2; break;
        default: return;
    }
    
    if (button < 8) {
        g_linux_platform->mouse_buttons[button] = pressed;
    }
    
    g_linux_platform->mouse_x = event->x;
    g_linux_platform->mouse_y = event->y;
}

static void linux_process_motion_event(XMotionEvent *event) {
    if (!g_linux_platform) {
        return;
    }
    
    g_linux_platform->mouse_x = event->x;
    g_linux_platform->mouse_y = event->y;
}