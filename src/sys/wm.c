/*
 * Kryon Window Manager Filesystem (/mnt/wm)
 * C89/C90 compliant
 *
 * Provides file-based interface to window management
 * File structure:
 * /mnt/wm/
 * ├── ctl                    # Write JSON to create window
 * ├── windows/
 * │   ├── [id]/
 * │   │   ├── ctl            # Window properties (read/write JSON)
 * │   │   ├── title          # Window title (read/write)
 * │   │   ├── rect           # "x y width height" (read/write)
 * │   │   ├── visible        # "true" / "false" (read/write)
 * │   │   ├── widgets/       # Widget directory
 * │   │   │   └── [id]/
 * │   │   │       ├── type    # Widget type name
 * │   │   │       ├── rect    # Widget geometry
 * │   │   │       └── properties/
 * │   │   │           ├── text   # Widget text content
 * │   │   │           ├── color  # Widget color
 * │   │   │           └── ...    # Other properties
 * │   │   └── events         # Event queue (read-only)
 * │   └── ...
 * └── active                # Symlink to focused window ID
 */

#include "wm.h"
#include "window.h"
#include "widget.h"
#include "events.h"
#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * Global window manager filesystem root
 */
static P9Node *g_wm_root = NULL;
static P9Node *g_windows_dir = NULL;

/*
 * ========== String Utilities ==========
 */

/*
 * Simple string read handler
 * Returns string content at offset
 */
static ssize_t string_read(char *buf, size_t count, uint64_t offset, const char *str)
{
    size_t len;
    size_t to_copy;

    if (str == NULL) {
        str = "";
    }

    len = strlen(str);

    if (offset >= len) {
        return 0;
    }

    to_copy = len - offset;
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, str + offset, to_copy);
    return to_copy;
}

/*
 * ========== Window Control File Handlers ==========
 */

/*
 * /mnt/wm/ctl - Create new window
 * Plan 9 style command format:
 *   "new title width height" - Create new window
 * Examples:
 *   "new MyWindow 800x600"
 *   "new Terminal 1024x768"
 */
static ssize_t wm_ctl_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    char cmd[32];
    char title[256];
    int width, height;
    struct KryonWindow *win;

    (void)offset;
    (void)data;

    /* Copy to null-terminate */
    if (count >= sizeof(cmd) - 1) {
        return -1;
    }

    /* Parse: "new title widthxheight" */
    if (sscanf(buf, "new %255s %dx%d", title, &width, &height) == 3) {
        win = window_create(title, width, height);
        if (win != NULL) {
            /* Create filesystem entries for the window */
            wm_create_window_entry(win);
            fprintf(stderr, "wm_ctl: created window '%s' %dx%d (id=%u)\n",
                    title, width, height, win->id);
            return count;
        }
    }

    fprintf(stderr, "wm_ctl: failed to parse command. Usage: 'new title widthxheight'\n");
    return -1;
}

/*
 * /mnt/wm/windows/{id}/title - Window title
 */
static ssize_t wm_window_title_read(char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    if (win == NULL || win->title == NULL) {
        return string_read(buf, count, offset, "");
    }
    return string_read(buf, count, offset, win->title);
}

static ssize_t wm_window_title_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    char *new_title;

    (void)offset;

    if (win == NULL || count == 0) {
        return -1;
    }

    /* Allocate new title */
    new_title = (char *)malloc(count + 1);
    if (new_title == NULL) {
        return -1;
    }

    memcpy(new_title, buf, count);
    new_title[count] = '\0';

    /* Remove trailing newline if present */
    if (count > 0 && new_title[count - 1] == '\n') {
        new_title[count - 1] = '\0';
    }

    /* Free old title and set new one */
    if (win->title != NULL) {
        free(win->title);
    }
    win->title = new_title;

    return count;
}

/*
 * /mnt/wm/windows/{id}/rect - Window geometry
 */
static ssize_t wm_window_rect_read(char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    if (win == NULL || win->rect == NULL) {
        return string_read(buf, count, offset, "0 0 640 480");
    }
    return string_read(buf, count, offset, win->rect);
}

static ssize_t wm_window_rect_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    char *new_rect;

    (void)offset;

    if (win == NULL || count == 0 || count > 255) {
        return -1;
    }

    /* Allocate new rect string */
    new_rect = (char *)malloc(count + 1);
    if (new_rect == NULL) {
        return -1;
    }

    memcpy(new_rect, buf, count);
    new_rect[count] = '\0';

    /* Remove trailing whitespace */
    while (count > 0 && (new_rect[count - 1] == '\n' || new_rect[count - 1] == ' ')) {
        new_rect[--count] = '\0';
    }

    /* Free old rect and set new one */
    if (win->rect != NULL) {
        free(win->rect);
    }
    win->rect = new_rect;

    return count;
}

/*
 * /mnt/wm/windows/{id}/visible - Window visibility
 */
static ssize_t wm_window_visible_read(char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    const char *vis_str = win ? (win->visible ? "true" : "false") : "false";
    return string_read(buf, count, offset, vis_str);
}

static ssize_t wm_window_visible_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    char value[16];
    size_t len;

    (void)offset;

    if (win == NULL || count == 0 || count >= sizeof(value)) {
        return -1;
    }

    /* Copy value */
    len = count < sizeof(value) - 1 ? count : sizeof(value) - 1;
    memcpy(value, buf, len);
    value[len] = '\0';

    /* Parse boolean */
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
        win->visible = 1;
    } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
        win->visible = 0;
    }

    return count;
}

/*
 * /mnt/wm/windows/{id}/events - Event queue (read-only)
 */
static ssize_t wm_window_events_read(char *buf, size_t count, uint64_t offset, void *data)
{
    (void)buf;
    (void)count;
    (void)offset;
    (void)data;
    /* For now, return empty */
    /* TODO: Aggregate events from all widgets in this window */
    return 0;
}

/*
 * ========== Widget File Handlers ==========
 */

/*
 * Get widget type name
 */
static const char *widget_type_name(int type)
{
    switch (type) {
        case WIDGET_BUTTON:        return "button";
        case WIDGET_SUBMIT_BUTTON: return "submit-button";
        case WIDGET_ICON_BUTTON:   return "icon-button";
        case WIDGET_FAB_BUTTON:    return "fab-button";
        case WIDGET_LABEL:         return "label";
        case WIDGET_HEADING:       return "heading";
        case WIDGET_PARAGRAPH:     return "paragraph";
        case WIDGET_CHECKBOX:      return "checkbox";
        case WIDGET_SWITCH:        return "switch";
        case WIDGET_CONTAINER:     return "container";
        case WIDGET_BOX:           return "box";
        case WIDGET_FRAME:         return "frame";
        case WIDGET_CARD:          return "card";
        case WIDGET_TEXT_INPUT:    return "text-input";
        case WIDGET_PASSWORD_INPUT: return "password-input";
        case WIDGET_SEARCH_INPUT:  return "search-input";
        case WIDGET_NUMBER_INPUT:  return "number-input";
        case WIDGET_EMAIL_INPUT:   return "email-input";
        case WIDGET_URL_INPUT:     return "url-input";
        case WIDGET_TEL_INPUT:     return "tel-input";
        case WIDGET_SLIDER:        return "slider";
        case WIDGET_RANGE_SLIDER:  return "range-slider";
        case WIDGET_PROGRESS_BAR:  return "progress-bar";
        case WIDGET_IMAGE:         return "image";
        case WIDGET_CANVAS:        return "canvas";
        case WIDGET_DIVIDER:       return "divider";
        default:                   return "unknown";
    }
}

/*
 * /mnt/wm/windows/{id}/widgets/{id}/type - Widget type
 */
static ssize_t wm_widget_type_read(char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWidget *widget = (struct KryonWidget *)data;
    const char *type_name;

    if (widget == NULL) {
        return string_read(buf, count, offset, "unknown");
    }

    type_name = widget_type_name(widget->type);
    return string_read(buf, count, offset, type_name);
}

/*
 * /mnt/wm/windows/{id}/widgets/{id}/rect - Widget geometry
 */
static ssize_t wm_widget_rect_read(char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWidget *widget = (struct KryonWidget *)data;
    if (widget == NULL || widget->prop_rect == NULL) {
        return string_read(buf, count, offset, "0 0 100 50");
    }
    return string_read(buf, count, offset, widget->prop_rect);
}

/*
 * /mnt/wm/windows/{id}/widgets/{id}/properties/text - Widget text
 */
static ssize_t wm_widget_text_read(char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWidget *widget = (struct KryonWidget *)data;
    if (widget == NULL || widget->prop_text == NULL) {
        return string_read(buf, count, offset, "");
    }
    return string_read(buf, count, offset, widget->prop_text);
}

static ssize_t wm_widget_text_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    struct KryonWidget *widget = (struct KryonWidget *)data;
    char *new_text;

    (void)offset;

    if (widget == NULL || count == 0) {
        return -1;
    }

    /* Allocate new text */
    new_text = (char *)malloc(count + 1);
    if (new_text == NULL) {
        return -1;
    }

    memcpy(new_text, buf, count);
    new_text[count] = '\0';

    /* Remove trailing newline if present */
    if (count > 0 && new_text[count - 1] == '\n') {
        new_text[count - 1] = '\0';
    }

    /* Free old text and set new one */
    if (widget->prop_text != NULL) {
        free(widget->prop_text);
    }
    widget->prop_text = new_text;

    return count;
}

/*
 * ========== Public API ==========
 */

/*
 * Initialize window manager filesystem
 */
int wm_service_init(P9Node *root)
{
    P9Node *wm_dir, *ctl_file, *windows_dir;

    /* Create /mnt/wm directory */
    wm_dir = tree_create_dir(root, "wm");
    if (wm_dir == NULL) {
        fprintf(stderr, "wm_service_init: failed to create /mnt/wm\n");
        return -1;
    }
    g_wm_root = wm_dir;

    /* Create /mnt/wm/ctl - window manager control */
    ctl_file = tree_create_file(wm_dir, "ctl", NULL, NULL, wm_ctl_write);
    if (ctl_file == NULL) {
        fprintf(stderr, "wm_service_init: failed to create /mnt/wm/ctl\n");
        return -1;
    }

    /* Create /mnt/wm/windows directory */
    windows_dir = tree_create_dir(wm_dir, "windows");
    if (windows_dir == NULL) {
        fprintf(stderr, "wm_service_init: failed to create /mnt/wm/windows\n");
        return -1;
    }
    g_windows_dir = windows_dir;

    fprintf(stderr, "wm_service_init: initialized /mnt/wm filesystem\n");
    return 0;
}

/*
 * Get window manager root node
 */
P9Node *wm_get_root(void)
{
    return g_wm_root;
}

/*
 * Create filesystem entries for a window
 */
int wm_create_window_entry(struct KryonWindow *win)
{
    P9Node *win_dir, *title_file, *rect_file, *visible_file, *events_file, *widgets_dir;
    char win_name[32];

    if (win == NULL || g_windows_dir == NULL) {
        return -1;
    }

    /* Create window directory name */
    sprintf(win_name, "%u", win->id);

    /* Create /mnt/wm/windows/{id} directory */
    win_dir = tree_create_dir(g_windows_dir, win_name);
    if (win_dir == NULL) {
        fprintf(stderr, "wm_create_window_entry: failed to create window dir\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{id}/title file */
    title_file = tree_create_file(win_dir, "title", win,
                                   wm_window_title_read,
                                   wm_window_title_write);
    if (title_file == NULL) {
        fprintf(stderr, "wm_create_window_entry: failed to create title file\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{id}/rect file */
    rect_file = tree_create_file(win_dir, "rect", win,
                                  wm_window_rect_read,
                                  wm_window_rect_write);
    if (rect_file == NULL) {
        fprintf(stderr, "wm_create_window_entry: failed to create rect file\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{id}/visible file */
    visible_file = tree_create_file(win_dir, "visible", win,
                                     wm_window_visible_read,
                                     wm_window_visible_write);
    if (visible_file == NULL) {
        fprintf(stderr, "wm_create_window_entry: failed to create visible file\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{id}/events file */
    events_file = tree_create_file(win_dir, "events", win,
                                    wm_window_events_read,
                                    NULL);
    if (events_file == NULL) {
        fprintf(stderr, "wm_create_window_entry: failed to create events file\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{id}/widgets directory */
    widgets_dir = tree_create_dir(win_dir, "widgets");
    if (widgets_dir == NULL) {
        fprintf(stderr, "wm_create_window_entry: failed to create widgets dir\n");
        return -1;
    }

    fprintf(stderr, "wm_create_window_entry: created entries for window %u\n", win->id);
    return 0;
}

/*
 * Remove filesystem entries for a window
 */
int wm_remove_window_entry(struct KryonWindow *win)
{
    char win_name[32];
    P9Node *win_dir;

    if (win == NULL || g_windows_dir == NULL) {
        return -1;
    }

    /* Create window directory name */
    sprintf(win_name, "%u", win->id);

    /* Find and remove window directory */
    win_dir = tree_walk(g_windows_dir, win_name);
    if (win_dir != NULL) {
        tree_remove_node(win_dir);
        fprintf(stderr, "wm_remove_window_entry: removed entries for window %u\n", win->id);
    }

    return 0;
}

/*
 * Create filesystem entries for a widget
 */
int wm_create_widget_entry(struct KryonWidget *widget)
{
    P9Node *win_dir, *widgets_dir, *widget_dir, *type_file, *rect_file, *props_dir, *text_file;
    char win_name[32];
    char widget_name[32];
    struct KryonWindow *parent_win;

    if (widget == NULL || g_windows_dir == NULL) {
        return -1;
    }

    /* Get parent window from widget */
    parent_win = widget->parent_window;
    if (parent_win == NULL) {
        fprintf(stderr, "wm_create_widget_entry: widget has no parent window\n");
        return -1;
    }

    /* Find parent window directory */
    sprintf(win_name, "%u", parent_win->id);
    win_dir = tree_walk(g_windows_dir, win_name);
    if (win_dir == NULL) {
        fprintf(stderr, "wm_create_widget_entry: window %u not found\n", parent_win->id);
        return -1;
    }

    /* Find widgets directory */
    widgets_dir = tree_walk(win_dir, "widgets");
    if (widgets_dir == NULL) {
        fprintf(stderr, "wm_create_widget_entry: widgets dir not found\n");
        return -1;
    }

    /* Create widget directory name */
    sprintf(widget_name, "%u", widget->id);

    /* Create /mnt/wm/windows/{win}/widgets/{id} directory */
    widget_dir = tree_create_dir(widgets_dir, widget_name);
    if (widget_dir == NULL) {
        fprintf(stderr, "wm_create_widget_entry: failed to create widget dir\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{win}/widgets/{id}/type file */
    type_file = tree_create_file(widget_dir, "type", widget,
                                  wm_widget_type_read,
                                  NULL);
    if (type_file == NULL) {
        fprintf(stderr, "wm_create_widget_entry: failed to create type file\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{win}/widgets/{id}/rect file */
    rect_file = tree_create_file(widget_dir, "rect", widget,
                                  wm_widget_rect_read,
                                  NULL);
    if (rect_file == NULL) {
        fprintf(stderr, "wm_create_widget_entry: failed to create rect file\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{win}/widgets/{id}/properties directory */
    props_dir = tree_create_dir(widget_dir, "properties");
    if (props_dir == NULL) {
        fprintf(stderr, "wm_create_widget_entry: failed to create properties dir\n");
        return -1;
    }

    /* Create /mnt/wm/windows/{win}/widgets/{id}/properties/text file */
    text_file = tree_create_file(props_dir, "text", widget,
                                  wm_widget_text_read,
                                  wm_widget_text_write);
    if (text_file == NULL) {
        fprintf(stderr, "wm_create_widget_entry: failed to create text file\n");
        return -1;
    }

    fprintf(stderr, "wm_create_widget_entry: created entries for widget %u\n", widget->id);
    return 0;
}

/*
 * Remove filesystem entries for a widget
 */
int wm_remove_widget_entry(struct KryonWidget *widget)
{
    char win_name[32];
    char widget_name[32];
    P9Node *win_dir, *widgets_dir, *widget_dir;
    struct KryonWindow *parent_win;

    if (widget == NULL || g_windows_dir == NULL) {
        return -1;
    }

    /* Get parent window from widget */
    parent_win = widget->parent_window;
    if (parent_win == NULL) {
        return -1;
    }

    /* Find parent window directory */
    sprintf(win_name, "%u", parent_win->id);
    win_dir = tree_walk(g_windows_dir, win_name);
    if (win_dir == NULL) {
        return -1;
    }

    /* Find widgets directory */
    widgets_dir = tree_walk(win_dir, "widgets");
    if (widgets_dir == NULL) {
        return -1;
    }

    /* Create widget directory name */
    sprintf(widget_name, "%u", widget->id);

    /* Find and remove widget directory */
    widget_dir = tree_walk(widgets_dir, widget_name);
    if (widget_dir != NULL) {
        tree_remove_node(widget_dir);
        fprintf(stderr, "wm_remove_widget_entry: removed entries for widget %u\n", widget->id);
    }

    return 0;
}
