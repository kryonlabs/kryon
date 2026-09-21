#if defined(__linux__) && !defined(KRYON_NATIVE_PLAN9)

#define Font X11Font
#define Screen X11Screen
#include <X11/Xlib.h>
#include <X11/keysym.h>
#undef Font
#undef Screen
#include "kryon.h"
#include "libdraw_x11_input.h"
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

extern int kry_libdraw_key_down[512];
extern int kry_libdraw_key_pressed[512];
extern int kry_libdraw_key_released[512];
extern void kry_libdraw_push_key(int key);

static struct {
    void *library;
    Display *display;
    Window window;
    int focused;
    int delivered_press;
    Display *(*open_display)(const char *);
    int (*close_display)(Display *);
    Status (*query_tree)(Display *, Window, Window *, Window *, Window **, unsigned int *);
    Status (*fetch_name)(Display *, Window, char **);
    int (*free_data)(void *);
    int (*select_input)(Display *, Window, long);
    int (*pending)(Display *);
    int (*next_event)(Display *, XEvent *);
    int (*peek_event)(Display *, XEvent *);
    KeySym (*lookup_keysym)(XKeyEvent *, int);
    int (*get_input_focus)(Display *, Window *, int *);
    int (*query_keymap)(Display *, char [32]);
    KeyCode (*keysym_to_keycode)(Display *, KeySym);
    int (*sync)(Display *, Bool);
    XErrorHandler (*set_error_handler)(XErrorHandler);
    XErrorHandler previous_error_handler;
} input;

static int
input_error(Display *display, XErrorEvent *event)
{
    if(display == input.display)
        return 0;
    return input.previous_error_handler != NULL ?
        input.previous_error_handler(display, event) : 0;
}

static Window
find_window(Window window, const char *title, int depth)
{
    if(depth > 12)
        return None;
    char *name = NULL;
    if(input.fetch_name(input.display, window, &name) && name != NULL) {
        int match = strcmp(name, title) == 0;
        input.free_data(name);
        if(match)
            return window;
    }
    Window root, parent, *children = NULL;
    unsigned int count = 0;
    if(!input.query_tree(input.display, window, &root, &parent, &children, &count))
        return None;
    Window found = None;
    for(unsigned int i = 0; i < count && found == None; i++)
        found = find_window(children[i], title, depth + 1);
    if(children != NULL)
        input.free_data(children);
    return found;
}

static int
key_for_symbol(KeySym symbol)
{
    if(symbol >= XK_a && symbol <= XK_z)
        return KEY_A + (int)(symbol - XK_a);
    if(symbol >= XK_A && symbol <= XK_Z)
        return KEY_A + (int)(symbol - XK_A);
    if(symbol >= XK_space && symbol <= XK_asciitilde)
        return (int)symbol;
    if(symbol >= XK_F1 && symbol <= XK_F12)
        return KEY_F1 + (int)(symbol - XK_F1);
    if(symbol >= XK_KP_0 && symbol <= XK_KP_9)
        return KEY_KP_0 + (int)(symbol - XK_KP_0);
    switch(symbol) {
    case XK_Escape: return KEY_ESCAPE;
    case XK_Return: return KEY_ENTER;
    case XK_KP_Enter: return KEY_KP_ENTER;
    case XK_Tab: case XK_ISO_Left_Tab: return KEY_TAB;
    case XK_BackSpace: return KEY_BACKSPACE;
    case XK_Delete: case XK_KP_Delete: return KEY_DELETE;
    case XK_Insert: case XK_KP_Insert: return KEY_INSERT;
    case XK_Home: case XK_KP_Home: return KEY_HOME;
    case XK_End: case XK_KP_End: return KEY_END;
    case XK_Left: case XK_KP_Left: return KEY_LEFT;
    case XK_Right: case XK_KP_Right: return KEY_RIGHT;
    case XK_Up: case XK_KP_Up: return KEY_UP;
    case XK_Down: case XK_KP_Down: return KEY_DOWN;
    case XK_Page_Up: case XK_KP_Page_Up: return KEY_PAGE_UP;
    case XK_Page_Down: case XK_KP_Page_Down: return KEY_PAGE_DOWN;
    case XK_Shift_L: return KEY_LEFT_SHIFT;
    case XK_Shift_R: return KEY_RIGHT_SHIFT;
    case XK_Control_L: return KEY_LEFT_CONTROL;
    case XK_Control_R: return KEY_RIGHT_CONTROL;
    case XK_Alt_L: case XK_Meta_L: return KEY_LEFT_ALT;
    case XK_Alt_R: case XK_Meta_R: case XK_ISO_Level3_Shift: return KEY_RIGHT_ALT;
    case XK_Super_L: return KEY_LEFT_SUPER;
    case XK_Super_R: return KEY_RIGHT_SUPER;
    case XK_Caps_Lock: return KEY_CAPS_LOCK;
    case XK_Num_Lock: return KEY_NUM_LOCK;
    case XK_Scroll_Lock: return KEY_SCROLL_LOCK;
    case XK_Print: return KEY_PRINT_SCREEN;
    case XK_Pause: return KEY_PAUSE;
    case XK_Menu: return KEY_KB_MENU;
    case XK_KP_Add: return KEY_KP_ADD;
    case XK_KP_Subtract: return KEY_KP_SUBTRACT;
    case XK_KP_Multiply: return KEY_KP_MULTIPLY;
    case XK_KP_Divide: return KEY_KP_DIVIDE;
    case XK_KP_Decimal: return KEY_KP_DECIMAL;
    case XK_KP_Equal: return KEY_KP_EQUAL;
    default: return 0;
    }
}

static void
release_keys(void)
{
    for(int i = 0; i < 512; i++) {
        if(kry_libdraw_key_down[i])
            kry_libdraw_key_released[i] = 1;
        kry_libdraw_key_down[i] = 0;
    }
}

static void
read_modifiers(void)
{
    const KeySym symbols[] = {XK_Shift_L, XK_Shift_R, XK_Control_L, XK_Control_R,
                              XK_Alt_L, XK_Alt_R, XK_Super_L, XK_Super_R};
    char keys[32];
    input.query_keymap(input.display, keys);
    for(unsigned int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        KeyCode code = input.keysym_to_keycode(input.display, symbols[i]);
        int key = key_for_symbol(symbols[i]);
        kry_libdraw_key_down[key] = code != 0 && (keys[code / 8] & (1 << (code % 8))) != 0;
    }
}

int
X11InputOpen(const char *title)
{
    X11InputClose();
    input.library = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
    if(input.library == NULL)
        return 0;
#define LOAD(member, symbol) do { \
    *(void **)(&input.member) = dlsym(input.library, symbol); \
    if(input.member == NULL) goto failure; \
} while(0)
    LOAD(open_display, "XOpenDisplay");
    LOAD(close_display, "XCloseDisplay");
    LOAD(query_tree, "XQueryTree");
    LOAD(fetch_name, "XFetchName");
    LOAD(free_data, "XFree");
    LOAD(select_input, "XSelectInput");
    LOAD(pending, "XPending");
    LOAD(next_event, "XNextEvent");
    LOAD(peek_event, "XPeekEvent");
    LOAD(lookup_keysym, "XLookupKeysym");
    LOAD(get_input_focus, "XGetInputFocus");
    LOAD(query_keymap, "XQueryKeymap");
    LOAD(keysym_to_keycode, "XKeysymToKeycode");
    LOAD(sync, "XSync");
    LOAD(set_error_handler, "XSetErrorHandler");
#undef LOAD
    input.display = input.open_display(NULL);
    if(input.display == NULL)
        goto failure;
    input.previous_error_handler = input.set_error_handler(input_error);
    input.window = find_window(DefaultRootWindow(input.display), title, 0);
    if(input.window == None)
        goto failure;
    input.select_input(input.display, input.window,
                        KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask);
    Window focused;
    int revert;
    input.get_input_focus(input.display, &focused, &revert);
    input.focused = focused == input.window;
    if(input.focused)
        read_modifiers();
    input.sync(input.display, False);
    return 1;
failure:
    X11InputClose();
    return 0;
}

void
X11InputPoll(void)
{
    if(input.display == NULL || input.window == None || input.delivered_press)
        return;
    while(input.pending(input.display)) {
        XEvent event;
        input.next_event(input.display, &event);
        if(event.type == FocusIn) {
            if(event.xfocus.mode == NotifyNormal) {
                input.focused = 1;
                read_modifiers();
            }
        } else if(event.type == FocusOut) {
            if(event.xfocus.mode == NotifyNormal) {
                input.focused = 0;
                release_keys();
            }
        } else if(event.type == DestroyNotify) {
            input.window = None;
            input.focused = 0;
            release_keys();
        } else if(event.type == KeyPress || event.type == KeyRelease) {
            if(event.type == KeyRelease && input.pending(input.display)) {
                XEvent next;
                input.peek_event(input.display, &next);
                if(next.type == KeyPress && next.xkey.keycode == event.xkey.keycode &&
                   next.xkey.time == event.xkey.time)
                    continue;
            }
            int key = key_for_symbol(input.lookup_keysym(&event.xkey, 0));
            if(key <= 0 || key >= 512)
                continue;
            if(event.type == KeyPress) {
                kry_libdraw_push_key(key);
                /* Keep a shortcut's modifiers available with its press even
                 * when the host delivers a complete chord between frames. */
                if(key < KEY_LEFT_SHIFT || key > KEY_RIGHT_SUPER) {
                    input.delivered_press = 1;
                    break;
                }
            } else {
                kry_libdraw_key_down[key] = 0;
                kry_libdraw_key_released[key] = 1;
            }
        }
    }
}

void
X11InputNextFrame(void)
{
    input.delivered_press = 0;
}

void
X11InputClose(void)
{
    if(input.display != NULL) {
        input.close_display(input.display);
        XErrorHandler current = input.set_error_handler(input_error);
        input.set_error_handler(current == input_error ? input.previous_error_handler : current);
    }
    if(input.library != NULL)
        dlclose(input.library);
    memset(&input, 0, sizeof(input));
}

int
X11InputReady(void)
{
    return input.display != NULL && input.window != None;
}

int
X11InputFocused(void)
{
    return input.focused;
}

void *
X11InputWindow(void)
{
    return (void *)(uintptr_t)input.window;
}

/* libdraw surfaces have no touch input; the shared pointer frame still
 * references the raylib touch queries, so satisfy them with empty results. */
Vector2
GetTouchPosition(int index)
{
    (void)index;
    return (Vector2){0, 0};
}

int
GetTouchPointCount(void)
{
    return 0;
}

#endif
