#include "kryon.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
static int backend_init_calls;
static int backend_close_calls;
static unsigned int config_flags;
static int backend_saw_msaa;
static int backend_saw_vsync;

void
SetConfigFlags(unsigned int flags)
{
    config_flags |= flags;
}

void
KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    (void)width;
    (void)height;
    (void)title;
    backend_saw_msaa = (config_flags & FLAG_MSAA_4X_HINT) != 0;
    backend_saw_vsync = (config_flags & FLAG_VSYNC_HINT) != 0;
    backend_init_calls++;
}

void
KryonRaylibBackend_CloseWindow(void)
{
    backend_close_calls++;
}

bool
KryonRaylibBackend_WindowShouldClose(void)
{
    return false;
}

static void
check(int ok, const char *name)
{
    if(!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int
main(void)
{
    char runtime_dir[] = "/tmp/kryon-runtime-dir.XXXXXX";
    char runtime_file[600];
    int fd;

    check(mkdtemp(runtime_dir) != NULL, "runtime dir");
    snprintf(runtime_file, sizeof(runtime_file), "%s/file", runtime_dir);
    fd = creat(runtime_file, 0600);
    check(fd >= 0, "runtime file");
    if(fd >= 0)
        close(fd);

    setenv("XDG_RUNTIME_DIR", runtime_file, 1);
    SetSingleInstance(1);
    InitWindow(320, 240, "Instance Lock Test");
    check(backend_init_calls == 1, "backend starts when lock path is unusable");
    check(backend_saw_msaa,
          "antialiasing is configured before the backend starts");
    check(backend_saw_vsync,
          "vsync is configured before the backend starts");
    check(WindowShouldClose() == false, "unusable lock does not reject window");
    CloseWindow();
    check(backend_close_calls == 1, "backend close after degraded lock");

    unlink(runtime_file);
    /* from here the runtime dir is real, so locks are actually taken */
    setenv("XDG_RUNTIME_DIR", runtime_dir, 1);

    /* a rejected instance must not reach the drawing backend: a second
     * process holding the same title key rejects InitWindow, and
     * BeginFrame/EndFrame become no-ops instead of crashing in rlgl on
     * the NULL GL state */
    {
        pid_t child;

        child = fork();
        if(child == 0) {
            /* an unkillable holder: the steal path SIGTERMs the lock
             * holder, so ignore it to force genuine rejection */
            signal(SIGTERM, SIG_IGN);
            InitWindow(320, 240, "Contested Title");
            usleep(3000000);   /* outlive the parent 2s steal window */
            _exit(0);
        }
        usleep(200000);
        InitWindow(320, 240, "Contested Title");
        check(InstanceRejected() == 1, "second instance is rejected");
        check(WindowShouldClose() == true, "rejected instance wants to close");
        check(backend_init_calls == 1, "rejected window never opens a backend");
        CloseWindow();
        kill(child, SIGKILL);   /* the holder ignores SIGTERM by design */
        waitpid(child, NULL, 0);
        rmdir(runtime_dir);
    }

    if(failures != 0)
        return 1;
    printf("instance lock tests passed\n");
    return 0;
}
