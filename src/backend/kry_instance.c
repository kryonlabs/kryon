/* Process ownership for the clean Kryon window lifecycle. */

#include "kryon.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32) && !defined(__ANDROID__) && \
    !defined(KRYON_NATIVE_PLAN9) && !defined(KRYON_PLATFORM_PLAN9)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* Weak so headless links (KRYON_BACKEND=null) stay resolvable and dormant:
 * every windowed backend defines these three strongly, and the call sites
 * below check the addresses before calling, matching the weak raylib-only
 * extern pattern used by the screenshot front-end. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
extern void KryonRaylibBackend_InitWindow(int width, int height,
                                          const char *title);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
extern void KryonRaylibBackend_CloseWindow(void);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
extern bool KryonRaylibBackend_WindowShouldClose(void);

static int g_single_instance =
#if defined(KRYON_BACKEND_TERMI)
    0;
#else
    1;
#endif
static int g_instance_rejected = 0;

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32) && !defined(__ANDROID__) && \
    !defined(KRYON_NATIVE_PLAN9) && !defined(KRYON_PLATFORM_PLAN9)
static int g_instance_fd = -1;
static char g_instance_path[512];

/* SDL latches SIGTERM into an event that windowed apps only observe
 * through the frame loop, so a SIGTERM arriving while the loop is busy
 * (or an event pump that never surfaces it) leaves an unkillable
 * process holding the instance lock. Own the signal instead: drop the
 * lock (close+unlink are async-signal-safe) and exit immediately.
 * The kernel releases the flock even if the unlink races. */
static void instance_terminate_signal(int sig)
{
    int fd = g_instance_fd;
    (void)sig;
    if(fd >= 0) {
        g_instance_fd = -1;
        (void)flock(fd, LOCK_UN);
        (void)close(fd);
    }
    if(g_instance_path[0] != '\0')
        (void)unlink(g_instance_path);
    _exit(0);
}

/* SIGUSR2 dumps a backtrace to stderr: a poor man's debugger for
 * machines without gdb (stuck loops are diagnosed with kill -USR2). */
#if defined(__GNUC__)
#include <execinfo.h>
static void instance_backtrace_signal(int sig)
{
    void *frames[32];
    int n = backtrace(frames, 32);
    (void)sig;
    backtrace_symbols_fd(frames, n, 2);
    _exit(0);
}
static void install_backtrace_handler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = instance_backtrace_signal;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGUSR2, &sa, NULL);
}
#else
static void install_backtrace_handler(void) { }
#endif

static void install_terminate_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = instance_terminate_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;   /* second signal takes default path */
    (void)sigaction(SIGTERM, &sa, NULL);
    (void)sigaction(SIGINT, &sa, NULL);
}

static void instance_key(const char *title, char *out, size_t out_size)
{
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(title != NULL ? title : "app");

    while(*p != '\0' && n + 1 < out_size) {
        unsigned char c = *p++;
        out[n++] = (char)(isalnum(c) ? tolower(c) : '-');
    }
    if(n == 0 && out_size > 1) {
        out[0] = 'a';
        out[1] = 'p';
        out[2] = 'p';
        n = 3;
    }
    out[n] = '\0';
}

/* pid-reuse guard: only signal pids that still belong to the same
 * application (same process name) as ourselves */
static int same_application(pid_t pid)
{
    char theirs[128];
    char mine[128];
    char path[64];
    FILE *f;
    size_t n;

    snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid);
    f = fopen(path, "r");
    if(f == NULL)
        return 0;
    n = fread(theirs, 1, sizeof(theirs) - 1, f);
    fclose(f);
    theirs[n] = '\0';
    theirs[strcspn(theirs, "\n")] = '\0';

    f = fopen("/proc/self/comm", "r");
    if(f == NULL)
        return 0;
    n = fread(mine, 1, sizeof(mine) - 1, f);
    fclose(f);
    mine[n] = '\0';
    mine[strcspn(mine, "\n")] = '\0';

    return strcmp(theirs, mine) == 0;
}

static int acquire_instance(const char *title)
{
    char key[128];
    const char *runtime = getenv("XDG_RUNTIME_DIR");

    if(runtime == NULL || runtime[0] == '\0')
        runtime = "/tmp";
    instance_key(title, key, sizeof(key));
    snprintf(g_instance_path, sizeof(g_instance_path), "%s/kryon-%s.lock",
             runtime, key);

    /* Steal budget: SIGTERM nudge for the first 1.5 s (a closing app
     * releases the lock after its cleanup), then a single SIGKILL — but
     * only when the pid still names the same application, so a recycled
     * pid never takes the hit — then a final grace second. Total worst
     * case 3.5 s instead of the old 2 s that raced slow shutdowns. */
    for(int attempt = 0; attempt < 175; attempt++) {
        char pid_text[32];
        int fd = open(g_instance_path, O_RDWR | O_CREAT, 0600);
        ssize_t bytes;
        long pid = 0;

        if(fd < 0)
            return -1;
        if(flock(fd, LOCK_EX | LOCK_NB) == 0) {
            int size = snprintf(pid_text, sizeof(pid_text), "%ld\n",
                                (long)getpid());
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);
            (void)write(fd, pid_text, (size_t)size);
            g_instance_fd = fd;
            install_terminate_handlers();
            install_backtrace_handler();
            return 1;
        }
        if(errno != EWOULDBLOCK && errno != EAGAIN) {
            close(fd);
            return -1;
        }
        lseek(fd, 0, SEEK_SET);
        bytes = read(fd, pid_text, sizeof(pid_text) - 1);
        if(bytes > 0) {
            pid_text[bytes] = '\0';
            pid = strtol(pid_text, NULL, 10);
        }
        close(fd);
        if(pid > 1 && pid != (long)getpid()) {
            if(attempt < 75) {
                (void)kill((pid_t)pid, SIGTERM);
            } else if(attempt == 75) {
                if(!same_application((pid_t)pid))
                    break;   /* pid was recycled: never kill a stranger */
                (void)kill((pid_t)pid, SIGKILL);
            }
        }
        usleep(20000);
    }
    return 0;
}

static void release_instance(void)
{
    if(g_instance_fd >= 0) {
        (void)flock(g_instance_fd, LOCK_UN);
        close(g_instance_fd);
        g_instance_fd = -1;
        (void)unlink(g_instance_path);
        g_instance_path[0] = '\0';
    }
}
#else
static int acquire_instance(const char *title) { (void)title; return 1; }
static void release_instance(void) { }
#endif

void KryonReleaseInstanceLock(void)
{
    release_instance();
}

void SetSingleInstance(int enabled) { g_single_instance = enabled != 0; }
int SingleInstanceEnabled(void) { return g_single_instance; }
int InstanceRejected(void) { return g_instance_rejected; }

void InitWindow(int width, int height, const char *title)
{
    int instance_status = 1;

    g_instance_rejected = 0;
    if(g_single_instance)
        instance_status = acquire_instance(title);
    if(instance_status == 0) {
        g_instance_rejected = 1;
        return;
    }
    if(KryonRaylibBackend_InitWindow != 0)
        KryonRaylibBackend_InitWindow(width, height, title);
}

void CloseWindow(void)
{
    if(!g_instance_rejected && KryonRaylibBackend_CloseWindow != 0)
        KryonRaylibBackend_CloseWindow();
    release_instance();
}

bool WindowShouldClose(void)
{
    if(g_instance_rejected)
        return true;
    if(KryonRaylibBackend_WindowShouldClose == 0)
        return false;
    return KryonRaylibBackend_WindowShouldClose();
}
