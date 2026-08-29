#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
static int backend_init_calls;
static int backend_close_calls;

void
KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    (void)width;
    (void)height;
    (void)title;
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
    char runtime_file[] = "/tmp/kryon-runtime-file.XXXXXX";
    int fd = mkstemp(runtime_file);

    check(fd >= 0, "runtime file");
    if(fd >= 0)
        close(fd);

    setenv("XDG_RUNTIME_DIR", runtime_file, 1);
    SetSingleInstance(1);
    InitWindow(320, 240, "Instance Lock Test");
    check(backend_init_calls == 1, "backend starts when lock path is unusable");
    check(WindowShouldClose() == false, "unusable lock does not reject window");
    CloseWindow();
    check(backend_close_calls == 1, "backend close after degraded lock");

    unlink(runtime_file);

    if(failures != 0)
        return 1;
    printf("instance lock tests passed\n");
    return 0;
}
