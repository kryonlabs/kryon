#include <kryon.h>
#include <ui_window.h>

#include <stdio.h>

static int failures;

static void
check_int(const char *name, long got, long want)
{
    if(got != want) {
        fprintf(stderr, "FAIL: %s got %ld want %ld\n", name, got, want);
        failures++;
    }
}

int
main(void)
{
    int x = 12345;
    int y = 12345;

    /* Secondary windows extend a running app: without the main window there
     * is no GL context to allocate the content texture from, so Open must
     * return NULL instead of touching rlgl. */
    if(!IsWindowReady()) {
        UIWindow *win = OpenUIWindow("kryon-test", 0, 0, 32, 32, 0, BLACK, 1.0f);

        check_int("open without a main window", win == NULL, 1);
    }

    /* Every entry point must stay safe with no window at all. */
    BeginUIWindow(NULL);
    EndUIWindow();
    CloseUIWindow(NULL);
    check_int("clicked on null window", IsUIWindowClicked(NULL), 0);
    check_int("right clicked on null window", IsUIWindowRightClicked(NULL), 0);
    check_int("dragged null window", IsUIWindowDragged(NULL), 0);

    GetUIWindowPosition(NULL, &x, &y);
    check_int("null window position x", x, 0);
    check_int("null window position y", y, 0);
    GetUIWindowClickPosition(NULL, &x, &y);
    check_int("null window click x", x, -1);
    check_int("null window click y", y, -1);

    if(failures != 0)
        return 1;
    printf("ui_window tests passed\n");
    return 0;
}
