#define _POSIX_C_SOURCE 200809L

#include "desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;

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
    char dir[512];
    char event[512];
    char runtime_template[] = "/tmp/desktop-runtime.XXXXXX";
    char cache_template[] = "/tmp/desktop-cache.XXXXXX";
    char *runtime = mkdtemp(runtime_template);
    char *cache = mkdtemp(cache_template);
    DesktopAppInfo info = {
        "xyz.waozi.desktoptest",
        "desktoptest",
        "Desktop Test",
        "Test summary",
        "xyz.waozi.desktoptest",
        "xyz.waozi.desktoptest",
        0
    };

    check(runtime != NULL, "runtime temp dir");
    check(cache != NULL, "cache temp dir");
    if(runtime == NULL || cache == NULL)
        return 1;
    setenv("XDG_RUNTIME_DIR", runtime, 1);
    setenv("XDG_CACHE_HOME", cache, 1);

    InitDesktopApp(&info);
    check(strcmp(GetDesktopAppID(), "xyz.waozi.desktoptest") == 0, "app id");
    check(strcmp(GetDesktopDisplayName(), "Desktop Test") == 0, "display name");
    check(GetDesktopCacheDir(dir, sizeof(dir)) == 1, "cache dir created");
    check(strstr(dir, "xyz.waozi.desktoptest") != NULL, "cache dir app id");
    check(access(dir, F_OK) == 0, "cache dir exists");

    check(AcquireDesktopSingleInstance(NULL, dir, sizeof(dir)) == 1,
          "single instance acquire");
    check(strstr(dir, "xyz.waozi.desktoptest.lock") != NULL, "lock path");
    ReleaseDesktopSingleInstance();
    check(AcquireDesktopSingleInstance(NULL, NULL, 0) == 1,
          "single instance reacquire");
    ReleaseDesktopSingleInstance();

    check(QueueDesktopOpenPath("/tmp/file.txt") == 1, "queue file");
    check(QueueDesktopOpenPath("demo://thing") == 1, "queue url");
    check(PollDesktopOpenEvent(event, sizeof(event)) == DESKTOP_OPEN_FILE,
          "poll file kind");
    check(strcmp(event, "/tmp/file.txt") == 0, "poll file value");
    check(PollDesktopOpenEvent(event, sizeof(event)) == DESKTOP_OPEN_URL,
          "poll url kind");
    check(strcmp(event, "demo://thing") == 0, "poll url value");
    check(PollDesktopOpenEvent(event, sizeof(event)) == DESKTOP_OPEN_NONE,
          "poll empty");

    if(failures != 0)
        return 1;
    printf("desktop tests passed\n");
    return 0;
}
