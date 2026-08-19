#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "desktop.h"
#include "notification.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#endif

typedef struct DesktopState {
    char app_id[DESKTOP_ID_MAX];
    char name[DESKTOP_NAME_MAX];
    char display_name[DESKTOP_NAME_MAX];
    char summary[DESKTOP_SUMMARY_MAX];
    char icon_name[DESKTOP_NAME_MAX];
    char wm_class[DESKTOP_NAME_MAX];
    DesktopAppInfo public_info;
} DesktopState;

typedef struct DesktopOpenEvent {
    DesktopOpenEventKind kind;
    char value[DESKTOP_PATH_MAX];
} DesktopOpenEvent;

static DesktopState g_desktop;
static int g_desktop_ready;
static int g_instance_fd = -1;
static DesktopOpenEvent g_open_events[8];
static int g_open_head;
static int g_open_count;

static void
desktop_copy(char *dst, size_t cap, const char *src, const char *fallback)
{
    if(dst == NULL || cap == 0)
        return;
    if(src == NULL || src[0] == '\0')
        src = fallback;
    snprintf(dst, cap, "%s", src != NULL ? src : "");
}

static void
desktop_publish(void)
{
    g_desktop.public_info.app_id = g_desktop.app_id;
    g_desktop.public_info.name = g_desktop.name;
    g_desktop.public_info.display_name = g_desktop.display_name;
    g_desktop.public_info.summary = g_desktop.summary;
    g_desktop.public_info.icon_name = g_desktop.icon_name;
    g_desktop.public_info.wm_class = g_desktop.wm_class;
}

static void
desktop_defaults(void)
{
    if(g_desktop_ready)
        return;
    desktop_copy(g_desktop.app_id, sizeof(g_desktop.app_id), NULL, "kryon");
    desktop_copy(g_desktop.name, sizeof(g_desktop.name), NULL, "kryon");
    desktop_copy(g_desktop.display_name, sizeof(g_desktop.display_name), NULL, "Kryon");
    desktop_copy(g_desktop.summary, sizeof(g_desktop.summary), NULL, "");
    desktop_copy(g_desktop.icon_name, sizeof(g_desktop.icon_name), NULL, "kryon");
    desktop_copy(g_desktop.wm_class, sizeof(g_desktop.wm_class), NULL, "kryon");
    desktop_publish();
    g_desktop_ready = 1;
}

void
InitDesktopApp(const DesktopAppInfo *info)
{
    desktop_defaults();
    if(info != NULL) {
        desktop_copy(g_desktop.app_id, sizeof(g_desktop.app_id), info->app_id,
                     g_desktop.app_id);
        desktop_copy(g_desktop.name, sizeof(g_desktop.name), info->name,
                     g_desktop.name);
        desktop_copy(g_desktop.display_name, sizeof(g_desktop.display_name),
                     info->display_name, g_desktop.name);
        desktop_copy(g_desktop.summary, sizeof(g_desktop.summary), info->summary,
                     "");
        desktop_copy(g_desktop.icon_name, sizeof(g_desktop.icon_name),
                     info->icon_name, g_desktop.app_id);
        desktop_copy(g_desktop.wm_class, sizeof(g_desktop.wm_class), info->wm_class,
                     g_desktop.app_id);
        desktop_publish();
    }
    SetNotificationAppName(g_desktop.app_id);
    if(info != NULL && info->single_instance)
        AcquireDesktopSingleInstance(g_desktop.app_id, NULL, 0);
}

const DesktopAppInfo *
GetDesktopAppInfo(void)
{
    desktop_defaults();
    return &g_desktop.public_info;
}

const char *GetDesktopAppID(void) { return GetDesktopAppInfo()->app_id; }
const char *GetDesktopAppName(void) { return GetDesktopAppInfo()->name; }
const char *GetDesktopDisplayName(void) { return GetDesktopAppInfo()->display_name; }
const char *GetDesktopIconName(void) { return GetDesktopAppInfo()->icon_name; }

static int
desktop_mkdir_p(const char *path)
{
#if defined(_WIN32)
    (void)path;
    return -1;
#else
    char tmp[DESKTOP_PATH_MAX];
    size_t len;

    if(path == NULL || path[0] == '\0')
        return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    while(len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for(size_t i = 1; i < len; i++) {
        if(tmp[i] == '/') {
            tmp[i] = '\0';
            if(mkdir(tmp, 0700) != 0 && errno != EEXIST)
                return -1;
            tmp[i] = '/';
        }
    }
    if(mkdir(tmp, 0700) != 0 && errno != EEXIST)
        return -1;
    return 0;
#endif
}

static int
desktop_xdg_dir(const char *env_name, const char *home_suffix, char *out, int cap)
{
    const char *base;
    const char *home;

    desktop_defaults();
    if(out == NULL || cap <= 0)
        return 0;
    base = getenv(env_name);
    if(base != NULL && base[0] != '\0') {
        snprintf(out, (size_t)cap, "%s/%s", base, g_desktop.app_id);
    } else {
        home = getenv("HOME");
        if(home == NULL || home[0] == '\0')
            return 0;
        snprintf(out, (size_t)cap, "%s/%s/%s", home, home_suffix,
                 g_desktop.app_id);
    }
    return desktop_mkdir_p(out) == 0;
}

int GetDesktopConfigDir(char *out, int cap)
{
    return desktop_xdg_dir("XDG_CONFIG_HOME", ".config", out, cap);
}

int GetDesktopDataDir(char *out, int cap)
{
    return desktop_xdg_dir("XDG_DATA_HOME", ".local/share", out, cap);
}

int GetDesktopCacheDir(char *out, int cap)
{
    return desktop_xdg_dir("XDG_CACHE_HOME", ".cache", out, cap);
}

int
AcquireDesktopSingleInstance(const char *app_id, char *lock_path, int cap)
{
    return AcquireDesktopSingleInstanceMode(app_id,
                                            DESKTOP_SINGLE_INSTANCE_REJECT,
                                            lock_path, cap);
}

static int
desktop_read_lock_pid(int fd)
{
#if defined(_WIN32)
    (void)fd;
    return 0;
#else
    char buf[64];
    ssize_t n;
    long pid = 0;

    if(fd < 0)
        return 0;
    if(lseek(fd, 0, SEEK_SET) < 0)
        return 0;
    n = read(fd, buf, sizeof(buf) - 1);
    if(n <= 0)
        return 0;
    buf[n] = '\0';
    if(sscanf(buf, "%ld", &pid) != 1 || pid <= 1)
        return 0;
    return (int)pid;
#endif
}

static void
desktop_write_lock_pid(int fd)
{
#if defined(_WIN32)
    (void)fd;
#else
    char buf[64];
    int len;

    if(fd < 0)
        return;
    len = snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
    if(len <= 0)
        return;
    (void)ftruncate(fd, 0);
    (void)lseek(fd, 0, SEEK_SET);
    (void)write(fd, buf, (size_t)len);
#endif
}

static int
desktop_try_lock_fd(int fd)
{
#if defined(_WIN32)
    (void)fd;
    return -1;
#else
    struct flock lock;

    if(fd < 0)
        return -1;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if(fcntl(fd, F_SETLK, &lock) != 0)
        return (errno == EACCES || errno == EAGAIN) ? 0 : -1;
    desktop_write_lock_pid(fd);
    return 1;
#endif
}

static void
desktop_replace_lock_owner(int fd)
{
#if defined(_WIN32)
    (void)fd;
#else
    int pid = desktop_read_lock_pid(fd);

    if(pid <= 1 || kill((pid_t)pid, 0) != 0)
        return;
    kill((pid_t)pid, SIGTERM);
    for(int i = 0; i < 50; i++) {
        if(kill((pid_t)pid, 0) != 0)
            return;
        usleep(100 * 1000);
    }
    if(kill((pid_t)pid, 0) == 0)
        kill((pid_t)pid, SIGKILL);
#endif
}

int
AcquireDesktopSingleInstanceMode(const char *app_id,
                                 DesktopSingleInstanceMode mode,
                                 char *lock_path, int cap)
{
#if defined(_WIN32)
    (void)app_id; (void)mode; (void)lock_path; (void)cap;
    return -1;
#else
    const char *runtime;
    const char *id;
    char dir[DESKTOP_PATH_MAX];
    char path[DESKTOP_PATH_MAX];
    int result;

    if(g_instance_fd >= 0)
        return 1;
    id = (app_id != NULL && app_id[0] != '\0') ? app_id : GetDesktopAppID();
    runtime = getenv("XDG_RUNTIME_DIR");
    if(runtime != NULL && runtime[0] != '\0') {
        snprintf(dir, sizeof(dir), "%s", runtime);
    } else {
        snprintf(dir, sizeof(dir), "/tmp/kryon-%ld", (long)getuid());
        if(desktop_mkdir_p(dir) != 0)
            return -1;
    }
    {
        size_t dir_len = strlen(dir);
        size_t id_len = strlen(id);

        if(dir_len + 1 + id_len + 5 >= sizeof(path))
            return -1;
        memcpy(path, dir, dir_len);
        path[dir_len] = '/';
        memcpy(path + dir_len + 1, id, id_len);
        memcpy(path + dir_len + 1 + id_len, ".lock", 6);
    }
    if(path[0] == '\0')
        return -1;
    if(lock_path != NULL && cap > 0)
        snprintf(lock_path, (size_t)cap, "%s", path);
    g_instance_fd = open(path, O_RDWR | O_CREAT, 0600);
    if(g_instance_fd < 0)
        return -1;
    result = desktop_try_lock_fd(g_instance_fd);
    if(result == 0 && mode == DESKTOP_SINGLE_INSTANCE_REPLACE) {
        desktop_replace_lock_owner(g_instance_fd);
        result = desktop_try_lock_fd(g_instance_fd);
    }
    if(result != 1) {
        close(g_instance_fd);
        g_instance_fd = -1;
        return result;
    }
    return 1;
#endif
}

void
ReleaseDesktopSingleInstance(void)
{
#if !defined(_WIN32)
    if(g_instance_fd >= 0) {
        close(g_instance_fd);
        g_instance_fd = -1;
    }
#endif
}

int
QueueDesktopOpenPath(const char *path_or_url)
{
    DesktopOpenEvent *event;

    if(path_or_url == NULL || path_or_url[0] == '\0')
        return 0;
    if(g_open_count >= (int)(sizeof(g_open_events) / sizeof(g_open_events[0])))
        return 0;
    event = &g_open_events[(g_open_head + g_open_count) %
                           (int)(sizeof(g_open_events) / sizeof(g_open_events[0]))];
    event->kind = strstr(path_or_url, "://") != NULL ? DESKTOP_OPEN_URL
                                                     : DESKTOP_OPEN_FILE;
    snprintf(event->value, sizeof(event->value), "%s", path_or_url);
    g_open_count++;
    return 1;
}

DesktopOpenEventKind
PollDesktopOpenEvent(char *out, int cap)
{
    DesktopOpenEvent event;

    if(g_open_count <= 0)
        return DESKTOP_OPEN_NONE;
    event = g_open_events[g_open_head];
    g_open_head = (g_open_head + 1) %
                  (int)(sizeof(g_open_events) / sizeof(g_open_events[0]));
    g_open_count--;
    if(out != NULL && cap > 0)
        snprintf(out, (size_t)cap, "%s", event.value);
    return event.kind;
}
