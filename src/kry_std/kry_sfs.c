/*
 * kry_sfs.c - kryon's live state as a synthetic file system.
 *
 * A tiny Plan-9-flavoured mount over the engine itself: info, theme
 * tokens, the live widget tree from ui_inspect, and input through
 * kry_inject. Reads format plain text; writes go through the same engine
 * paths a real user would hit (injected events, not private state
 * pokes), so anything driven through the SFS behaves exactly like real
 * interaction - which is what makes it the testing surface for agents
 * and the Hierarchy tab.
 */
#include "kryon.h"
#include "kry_inject.h"
#include "kry_sfs.h"
#include "theme.h"
#include "ui_inspect.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    int key;
} KrySfsKey;

static const KrySfsKey kry_sfs_keys[] = {
    {"KEY_NULL", KEY_NULL},
    {"KEY_SPACE", KEY_SPACE},
    {"KEY_ESCAPE", KEY_ESCAPE},
    {"KEY_ENTER", KEY_ENTER},
    {"KEY_TAB", KEY_TAB},
    {"KEY_BACKSPACE", KEY_BACKSPACE},
    {"KEY_INSERT", KEY_INSERT},
    {"KEY_DELETE", KEY_DELETE},
    {"KEY_RIGHT", KEY_RIGHT},
    {"KEY_LEFT", KEY_LEFT},
    {"KEY_DOWN", KEY_DOWN},
    {"KEY_UP", KEY_UP},
    {"KEY_PAGE_UP", KEY_PAGE_UP},
    {"KEY_PAGE_DOWN", KEY_PAGE_DOWN},
    {"KEY_HOME", KEY_HOME},
    {"KEY_END", KEY_END},
    {"KEY_CAPS_LOCK", KEY_CAPS_LOCK},
    {"KEY_SCROLL_LOCK", KEY_SCROLL_LOCK},
    {"KEY_NUM_LOCK", KEY_NUM_LOCK},
    {"KEY_PRINT_SCREEN", KEY_PRINT_SCREEN},
    {"KEY_PAUSE", KEY_PAUSE},
    {"KEY_F1", KEY_F1},
    {"KEY_F2", KEY_F2},
    {"KEY_F3", KEY_F3},
    {"KEY_F4", KEY_F4},
    {"KEY_F5", KEY_F5},
    {"KEY_F6", KEY_F6},
    {"KEY_F7", KEY_F7},
    {"KEY_F8", KEY_F8},
    {"KEY_F9", KEY_F9},
    {"KEY_F10", KEY_F10},
    {"KEY_F11", KEY_F11},
    {"KEY_F12", KEY_F12},
    {"KEY_LEFT_SHIFT", KEY_LEFT_SHIFT},
    {"KEY_LEFT_CONTROL", KEY_LEFT_CONTROL},
    {"KEY_LEFT_ALT", KEY_LEFT_ALT},
    {"KEY_LEFT_SUPER", KEY_LEFT_SUPER},
    {"KEY_RIGHT_SHIFT", KEY_RIGHT_SHIFT},
    {"KEY_RIGHT_CONTROL", KEY_RIGHT_CONTROL},
    {"KEY_RIGHT_ALT", KEY_RIGHT_ALT},
    {"KEY_RIGHT_SUPER", KEY_RIGHT_SUPER},
    {"KEY_A", KEY_A},
    {"KEY_B", KEY_B},
    {"KEY_C", KEY_C},
    {"KEY_D", KEY_D},
    {"KEY_E", KEY_E},
    {"KEY_F", KEY_F},
    {"KEY_G", KEY_G},
    {"KEY_H", KEY_H},
    {"KEY_I", KEY_I},
    {"KEY_J", KEY_J},
    {"KEY_K", KEY_K},
    {"KEY_L", KEY_L},
    {"KEY_M", KEY_M},
    {"KEY_N", KEY_N},
    {"KEY_O", KEY_O},
    {"KEY_P", KEY_P},
    {"KEY_Q", KEY_Q},
    {"KEY_R", KEY_R},
    {"KEY_S", KEY_S},
    {"KEY_T", KEY_T},
    {"KEY_U", KEY_U},
    {"KEY_V", KEY_V},
    {"KEY_W", KEY_W},
    {"KEY_X", KEY_X},
    {"KEY_Y", KEY_Y},
    {"KEY_Z", KEY_Z},
    {"KEY_ZERO", KEY_ZERO},
    {"KEY_ONE", KEY_ONE},
    {"KEY_TWO", KEY_TWO},
    {"KEY_THREE", KEY_THREE},
    {"KEY_FOUR", KEY_FOUR},
    {"KEY_FIVE", KEY_FIVE},
    {"KEY_SIX", KEY_SIX},
    {"KEY_SEVEN", KEY_SEVEN},
    {"KEY_EIGHT", KEY_EIGHT},
    {"KEY_NINE", KEY_NINE},
    {"KEY_MINUS", KEY_MINUS},
    {"KEY_EQUAL", KEY_EQUAL},
    {"KEY_LEFT_BRACKET", KEY_LEFT_BRACKET},
    {"KEY_RIGHT_BRACKET", KEY_RIGHT_BRACKET},
    {"KEY_BACKSLASH", KEY_BACKSLASH},
    {"KEY_SEMICOLON", KEY_SEMICOLON},
    {"KEY_APOSTROPHE", KEY_APOSTROPHE},
    {"KEY_COMMA", KEY_COMMA},
    {"KEY_PERIOD", KEY_PERIOD},
    {"KEY_SLASH", KEY_SLASH},
};

#define KRY_SFS_KEY_COUNT \
    ((int)(sizeof(kry_sfs_keys) / sizeof(kry_sfs_keys[0])))

typedef struct {
    const char *name;
    Color (*get)(void);
} KrySfsTheme;

static const KrySfsTheme kry_sfs_theme[] = {
    {"text", GetThemeText},
    {"background", GetThemeBackground},
    {"surface", GetThemeSurface},
    {"circle", GetThemeCircle},
    {"button", GetThemeButton},
    {"button_hover", GetThemeButtonHover},
    {"icon", GetThemeIcon},
    {"link", GetThemeLink},
};

#define KRY_SFS_THEME_COUNT \
    ((int)(sizeof(kry_sfs_theme) / sizeof(kry_sfs_theme[0])))

/* split "/a/b/c" into up to 4 segments; returns the count */
static int
kry_sfs_split(const char *path, char seg[4][96])
{
    const char *p = path;
    int n = 0;

    while(*p != '\0') {
        const char *start;
        size_t len;

        while(*p == '/')
            p++;
        if(*p == '\0')
            break;
        start = p;
        while(*p != '\0' && *p != '/')
            p++;
        len = (size_t)(p - start);
        if(n >= 4)
            return -1;
        if(len >= sizeof(seg[0]))
            len = sizeof(seg[0]) - 1;
        memcpy(seg[n], start, len);
        seg[n][len] = '\0';
        n++;
    }
    return n;
}

static int
kry_sfs_key_from_name(const char *name)
{
    int i;

    for(i = 0; i < KRY_SFS_KEY_COUNT; i++)
        if(strcmp(kry_sfs_keys[i].name, name) == 0)
            return kry_sfs_keys[i].key;
    return -1;
}

static void
kry_sfs_key_name(int key, char *dst, size_t dst_size)
{
    int i;

    for(i = 0; i < KRY_SFS_KEY_COUNT; i++) {
        if(kry_sfs_keys[i].key == key) {
            snprintf(dst, dst_size, "%s", kry_sfs_keys[i].name);
            return;
        }
    }
    snprintf(dst, dst_size, "KEY_%d", key);
}

/* The inspect tree records only when enabled; the SFS is a consumer, so
 * touching /widgets turns recording on (once per process). */
static void
kry_sfs_ensure_widgets(void)
{
    static int ensured;

    if(!ensured) {
        SetUIInspectEnabled(1);
        SetUIInspectVisible(0);
        ensured = 1;
    }
}

static int
kry_sfs_widget_at(const char *index_text, UIInspectNode *node)
{
    char *end = NULL;
    long index;

    if(index_text == NULL || index_text[0] == '\0')
        return 0;
    index = strtol(index_text, &end, 10);
    if(end == NULL || *end != '\0' || index < 0)
        return 0;
    return UIInspectGetNode((int)index, node) && node->valid;
}

int
KrySfsList(const char *path, KrySfsEntry *entries, int cap)
{
    char seg[4][96];
    int n;
    int count = 0;

    if(path == NULL || entries == NULL || cap <= 0)
        return KRY_SFS_EINVAL;
    kry_sfs_ensure_widgets();
    n = kry_sfs_split(path, seg);
    if(n < 0)
        return KRY_SFS_EINVAL;

#define KRY_SFS_ADD(name_, is_dir_) do { \
        if(count >= cap) \
            return count; \
        snprintf(entries[count].name, sizeof(entries[count].name), "%s", \
                 (name_)); \
        entries[count].is_dir = (is_dir_); \
        count++; \
    } while(0)

    if(n == 0) {
        KRY_SFS_ADD("info", 0);
        KRY_SFS_ADD("input", 1);
        KRY_SFS_ADD("widgets", 1);
        KRY_SFS_ADD("theme", 1);
        return count;
    }
    if(strcmp(seg[0], "input") == 0) {
        if(n == 1) {
            KRY_SFS_ADD("mouse", 1);
            KRY_SFS_ADD("keys", 1);
            KRY_SFS_ADD("text", 0);
            KRY_SFS_ADD("wheel", 0);
            return count;
        }
        if(strcmp(seg[1], "mouse") == 0) {
            if(n == 2) {
                KRY_SFS_ADD("x", 0);
                KRY_SFS_ADD("y", 0);
                KRY_SFS_ADD("button", 1);
                return count;
            }
            if(n == 3 && strcmp(seg[2], "button") == 0) {
                char name[16];
                int i;

                for(i = 0; i < KRY_INJECT_MAX_BUTTONS; i++) {
                    snprintf(name, sizeof(name), "%d", i);
                    KRY_SFS_ADD(name, 0);
                }
                return count;
            }
            return KRY_SFS_ENOENT;
        }
        if(strcmp(seg[1], "keys") == 0) {
            if(n == 2) {
                int i;

                for(i = 0; i < KRY_SFS_KEY_COUNT; i++)
                    KRY_SFS_ADD(kry_sfs_keys[i].name, 0);
                return count;
            }
            return KRY_SFS_ENOENT;
        }
        return KRY_SFS_ENOENT;
    }
    if(strcmp(seg[0], "widgets") == 0) {
        kry_sfs_ensure_widgets();
        if(n == 1) {
            int total = UIInspectNodeCount();
            char name[16];
            int i;

            for(i = 0; i < total; i++) {
                snprintf(name, sizeof(name), "%d", i);
                KRY_SFS_ADD(name, 1);
            }
            return count;
        }
        {
            UIInspectNode node;

            if(!kry_sfs_widget_at(seg[1], &node))
                return KRY_SFS_ENOENT;
            if(n == 2) {
                KRY_SFS_ADD("name", 0);
                KRY_SFS_ADD("kind", 0);
                KRY_SFS_ADD("text", 0);
                KRY_SFS_ADD("value", 0);
                KRY_SFS_ADD("bounds", 0);
                KRY_SFS_ADD("source", 0);
                KRY_SFS_ADD("tap", 0);
                return count;
            }
            return KRY_SFS_ENOENT;
        }
    }
    if(strcmp(seg[0], "theme") == 0) {
        if(n == 1) {
            int i;

            for(i = 0; i < KRY_SFS_THEME_COUNT; i++)
                KRY_SFS_ADD(kry_sfs_theme[i].name, 0);
            return count;
        }
        return KRY_SFS_ENOENT;
    }
    return KRY_SFS_ENOENT;
#undef KRY_SFS_ADD
}

int
KrySfsRead(const char *path, char *buf, size_t size)
{
    char seg[4][96];
    int n;

    if(path == NULL || buf == NULL || size == 0)
        return KRY_SFS_EINVAL;
    buf[0] = '\0';
    kry_sfs_ensure_widgets();
    n = kry_sfs_split(path, seg);
    if(n < 0)
        return KRY_SFS_EINVAL;

    if(n == 1 && strcmp(seg[0], "info") == 0) {
        snprintf(buf, size, "kryon %s backend=raylib abi=%d\n",
                 KRYON_VERSION_STRING, APP_HOST_ABI_VERSION);
        return (int)strlen(buf);
    }
    if(n >= 2 && strcmp(seg[0], "input") == 0) {
        if(strcmp(seg[1], "mouse") == 0 && n == 3) {
            if(strcmp(seg[2], "x") == 0) {
                snprintf(buf, size, "%d\n", (int)KryonInjectMouseX());
                return (int)strlen(buf);
            }
            if(strcmp(seg[2], "y") == 0) {
                snprintf(buf, size, "%d\n", (int)KryonInjectMouseY());
                return (int)strlen(buf);
            }
        }
        if(strcmp(seg[1], "mouse") == 0 && n == 4 &&
           strcmp(seg[2], "button") == 0) {
            char *end = NULL;
            long button = strtol(seg[3], &end, 10);

            if(end != NULL && *end == '\0' && button >= 0 &&
               button < KRY_INJECT_MAX_BUTTONS) {
                snprintf(buf, size, "%s\n",
                         KryonInjectMouseButtonDown((int)button) ? "down"
                                                                 : "up");
                return (int)strlen(buf);
            }
        }
        if(strcmp(seg[1], "keys") == 0 && n == 3) {
            int key = kry_sfs_key_from_name(seg[2]);

            if(key > 0) {
                snprintf(buf, size, "%s\n",
                         KryonInjectKeyDown(key) ? "down" : "up");
                return (int)strlen(buf);
            }
        }
        if(strcmp(seg[1], "wheel") == 0 && n == 2) {
            snprintf(buf, size, "%f\n", KryonInjectWheelValue());
            return (int)strlen(buf);
        }
        return KRY_SFS_ENOENT;
    }
    if(strcmp(seg[0], "widgets") == 0 && n == 3) {
        UIInspectNode node;

        kry_sfs_ensure_widgets();

        if(!kry_sfs_widget_at(seg[1], &node))
            return KRY_SFS_ENOENT;
        if(strcmp(seg[2], "name") == 0)
            snprintf(buf, size, "%s\n", node.name);
        else if(strcmp(seg[2], "kind") == 0)
            snprintf(buf, size, "%s\n", node.role);
        else if(strcmp(seg[2], "text") == 0)
            snprintf(buf, size, "%s\n", node.text);
        else if(strcmp(seg[2], "value") == 0)
            snprintf(buf, size, "%s\n", node.value);
        else if(strcmp(seg[2], "bounds") == 0)
            snprintf(buf, size, "%d %d %d %d\n", (int)node.bounds.x,
                     (int)node.bounds.y, (int)node.bounds.width,
                     (int)node.bounds.height);
        else if(strcmp(seg[2], "source") == 0)
            snprintf(buf, size, "%s:%d\n", node.source_path, node.source_line);
        else
            return KRY_SFS_ENOENT;
        return (int)strlen(buf);
    }
    if(strcmp(seg[0], "theme") == 0 && n == 2) {
        int i;

        for(i = 0; i < KRY_SFS_THEME_COUNT; i++) {
            if(strcmp(kry_sfs_theme[i].name, seg[1]) == 0) {
                Color c = kry_sfs_theme[i].get();

                snprintf(buf, size, "%d %d %d %d\n", c.r, c.g, c.b, c.a);
                return (int)strlen(buf);
            }
        }
        return KRY_SFS_ENOENT;
    }
    return KRY_SFS_ENOENT;
}

int
KrySfsWrite(const char *path, const char *value)
{
    char seg[4][96];
    int n;
    int is_down;
    int is_up;
    int is_tap;
    int is_press;
    int is_release;

    if(path == NULL || value == NULL)
        return KRY_SFS_EINVAL;
    kry_sfs_ensure_widgets();
    n = kry_sfs_split(path, seg);
    if(n < 0)
        return KRY_SFS_EINVAL;
    is_down = strcmp(value, "down") == 0 || strcmp(value, "1") == 0;
    is_up = strcmp(value, "up") == 0 || strcmp(value, "0") == 0;
    is_tap = strcmp(value, "tap") == 0 || strcmp(value, "click") == 0;
    is_press = strcmp(value, "press") == 0;
    is_release = strcmp(value, "release") == 0;

    if(n >= 3 && strcmp(seg[0], "input") == 0 &&
       strcmp(seg[1], "mouse") == 0 && n == 3 &&
       (strcmp(seg[2], "x") == 0 || strcmp(seg[2], "y") == 0)) {
        char *end = NULL;
        long v = strtol(value, &end, 10);

        if(end == NULL || *end != '\0')
            return KRY_SFS_EINVAL;
        if(strcmp(seg[2], "x") == 0)
            KryonInjectMousePosition((float)v, KryonInjectMouseY());
        else
            KryonInjectMousePosition(KryonInjectMouseX(), (float)v);
        return 1;
    }
    if(n == 4 && strcmp(seg[0], "input") == 0 &&
       strcmp(seg[1], "mouse") == 0 && strcmp(seg[2], "button") == 0) {
        char *end = NULL;
        long button = strtol(seg[3], &end, 10);

        if(end == NULL || *end != '\0' || button < 0 ||
           button >= KRY_INJECT_MAX_BUTTONS)
            return KRY_SFS_ENOENT;
        if(is_down || is_press)
            KryonInjectMouseButton((int)button, 1);
        else if(is_up || is_release)
            KryonInjectMouseButton((int)button, 0);
        else if(is_tap)
            KryonInjectTap(KryonInjectMouseX(), KryonInjectMouseY());
        else
            return KRY_SFS_EINVAL;
        return 1;
    }
    if(n == 3 && strcmp(seg[0], "input") == 0 &&
       strcmp(seg[1], "keys") == 0) {
        int key = kry_sfs_key_from_name(seg[2]);

        if(key <= 0)
            return KRY_SFS_ENOENT;
        if(is_down)
            KryonInjectKey(key, 1);
        else if(is_up)
            KryonInjectKey(key, 0);
        else if(is_tap || is_press)
            KryonInjectKeyTap(key);
        else
            return KRY_SFS_EINVAL;
        return 1;
    }
    if(n == 2 && strcmp(seg[0], "input") == 0 &&
       strcmp(seg[1], "text") == 0) {
        KryonInjectText(value);
        return 1;
    }
    if(n == 2 && strcmp(seg[0], "input") == 0 &&
       strcmp(seg[1], "wheel") == 0) {
        char *end = NULL;
        double v = strtod(value, &end);

        if(end == NULL || *end != '\0')
            return KRY_SFS_EINVAL;
        KryonInjectWheel((float)v);
        return 1;
    }
    if(n == 3 && strcmp(seg[0], "widgets") == 0 &&
       strcmp(seg[2], "tap") == 0) {
        UIInspectNode node;

        kry_sfs_ensure_widgets();

        if(!kry_sfs_widget_at(seg[1], &node))
            return KRY_SFS_ENOENT;
        KryonInjectTap(node.bounds.x + node.bounds.width / 2.0f,
                       node.bounds.y + node.bounds.height / 2.0f);
        return 1;
    }
    return KRY_SFS_ERO;
}

int
KrySfsIsDir(const char *path)
{
    char seg[4][96];
    int n;

    if(path == NULL)
        return 0;
    n = kry_sfs_split(path, seg);
    if(n < 0)
        return 0;
    if(n == 0)
        return 1;
    if(strcmp(seg[0], "info") == 0)
        return 0;
    if(strcmp(seg[0], "input") == 0) {
        if(n == 1)
            return 1;
        if(strcmp(seg[1], "mouse") == 0)
            return n == 2 || (n == 3 && strcmp(seg[2], "button") == 0);
        if(strcmp(seg[1], "keys") == 0)
            return n == 2;
        return 0;
    }
    if(strcmp(seg[0], "widgets") == 0)
        return n <= 2;
    if(strcmp(seg[0], "theme") == 0)
        return n == 1;
    return 0;
}
