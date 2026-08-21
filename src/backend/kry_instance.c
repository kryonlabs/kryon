/* Process ownership for the clean Kryon window lifecycle. */

#include "kryon.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32) && !defined(__ANDROID__)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

extern void KryonRaylibBackend_InitWindow(int width, int height,
                                          const char *title);
extern void KryonRaylibBackend_CloseWindow(void);
extern bool KryonRaylibBackend_WindowShouldClose(void);

static int g_single_instance = 1;
static int g_instance_rejected = 0;

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32) && !defined(__ANDROID__)
static int g_instance_fd = -1;
static char g_instance_path[512];

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

static int acquire_instance(const char *title)
{
    char key[128];
    const char *runtime = getenv("XDG_RUNTIME_DIR");

    if(runtime == NULL || runtime[0] == '\0')
        runtime = "/tmp";
    instance_key(title, key, sizeof(key));
    snprintf(g_instance_path, sizeof(g_instance_path), "%s/kryon-%s.lock",
             runtime, key);

    for(int attempt = 0; attempt < 100; attempt++) {
        char pid_text[32];
        int fd = open(g_instance_path, O_RDWR | O_CREAT, 0600);
        ssize_t bytes;
        long pid = 0;

        if(fd < 0)
            return 0;
        if(flock(fd, LOCK_EX | LOCK_NB) == 0) {
            int size = snprintf(pid_text, sizeof(pid_text), "%ld\n",
                                (long)getpid());
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);
            (void)write(fd, pid_text, (size_t)size);
            g_instance_fd = fd;
            return 1;
        }
        lseek(fd, 0, SEEK_SET);
        bytes = read(fd, pid_text, sizeof(pid_text) - 1);
        if(bytes > 0) {
            pid_text[bytes] = '\0';
            pid = strtol(pid_text, NULL, 10);
        }
        close(fd);
        if(pid > 1 && pid != (long)getpid())
            (void)kill((pid_t)pid, SIGTERM);
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

void SetSingleInstance(int enabled) { g_single_instance = enabled != 0; }
int SingleInstanceEnabled(void) { return g_single_instance; }

void InitWindow(int width, int height, const char *title)
{
    g_instance_rejected = 0;
    if(g_single_instance && !acquire_instance(title)) {
        g_instance_rejected = 1;
        return;
    }
    KryonRaylibBackend_InitWindow(width, height, title);
}

void CloseWindow(void)
{
    if(!g_instance_rejected)
        KryonRaylibBackend_CloseWindow();
    release_instance();
}

bool WindowShouldClose(void)
{
    return g_instance_rejected || KryonRaylibBackend_WindowShouldClose();
}
