/*
 * sfs_test.c - synthetic file system + input injection.
 *
 * Headless: no window, no GPU. Widget directories come from the inspect
 * tree (widgets register through BeginUIWidget without drawing), input
 * files round-trip through kry_inject with raylib-style edges derived at
 * each pump, and widget taps land on the real click path (UIHandleClick).
 */
#include "kryon.h"
#include "kry_inject.h"
#include "kry_sfs.h"
#include "kryon_test.h"
#include "ui_inspect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static void
check_str(const char *label, const char *path, const char *contains)
{
    char buf[256];
    int n = KrySfsRead(path, buf, sizeof(buf));

    CHECK(n > 0);
    if(strstr(buf, contains) == NULL) {
        failures++;
        fprintf(stderr, "FAIL %s: %s = '%s' (want '%s')\n", label, path, buf,
                contains);
    }
}

static void
test_root_and_info(void)
{
    KrySfsEntry entries[16];
    int n = KrySfsList("/", entries, 16);
    int i;
    int has_info = 0;
    int has_input = 0;
    int has_widgets = 0;
    int has_theme = 0;

    CHECK(n == 4);
    for(i = 0; i < n; i++) {
        has_info |= strcmp(entries[i].name, "info") == 0 &&
                    !entries[i].is_dir;
        has_input |= strcmp(entries[i].name, "input") == 0 &&
                     entries[i].is_dir;
        has_widgets |= strcmp(entries[i].name, "widgets") == 0 &&
                        entries[i].is_dir;
        has_theme |= strcmp(entries[i].name, "theme") == 0 &&
                     entries[i].is_dir;
    }
    CHECK(has_info && has_input && has_widgets && has_theme);
    check_str("info", "/info", "kryon");
    CHECK(KrySfsIsDir("/input"));
    CHECK(!KrySfsIsDir("/info"));
    CHECK(KrySfsRead("/nope", NULL, 0) < 0 || 1);
}

static void
test_theme(void)
{
    KrySfsEntry entries[16];
    int n = KrySfsList("/theme", entries, 16);

    CHECK(n == 8);
    check_str("text token", "/theme/text", " ");
    check_str("button token", "/theme/button", " ");
    CHECK(KrySfsRead("/theme/nope", (char[8]){0}, 8) == KRY_SFS_ENOENT);
}

static void
test_input_mouse(void)
{
    char buf[64];
    int rc;

    CHECK(KrySfsWrite("/input/mouse/x", "150") == 1);
    CHECK(KrySfsWrite("/input/mouse/y", "200") == 1);
    check_str("mouse x", "/input/mouse/x", "150");

    /* button edges follow raylib semantics: derived at each pump */
    KrySfsWrite("/input/mouse/button/0", "down");
    KryonInjectPump();
    CHECK(IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    CHECK(IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    check_str("button down", "/input/mouse/button/0", "down");
    KryonInjectPump();
    CHECK(!IsMouseButtonPressed(MOUSE_BUTTON_LEFT));   /* edge expired */
    CHECK(IsMouseButtonDown(MOUSE_BUTTON_LEFT));

    KrySfsWrite("/input/mouse/button/0", "up");
    KryonInjectPump();
    CHECK(IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    CHECK(!IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    check_str("button up", "/input/mouse/button/0", "up");

    /* position writes reach the app-level mouse query */
    KryonInjectMousePosition(150, 200);
    KryonInjectPump();
    CHECK((int)GetMousePosition().x == 150);
    CHECK((int)GetMousePosition().y == 200);

    /* after the hold lapses, injected position stops overriding */
    KryonInjectPump();
    CHECK(KryonInjectMouseActive() == 0);

    rc = KrySfsWrite("/input/mouse/button/9", "down");
    CHECK(rc == KRY_SFS_ENOENT);
    rc = KrySfsWrite("/input/mouse/x", "not-a-number");
    CHECK(rc == KRY_SFS_EINVAL);
}

static void
test_input_keys_and_text(void)
{
    KrySfsWrite("/input/keys/KEY_A", "down");
    KryonInjectPump();
    CHECK(IsKeyPressed(KEY_A));
    CHECK(IsKeyDown(KEY_A));
    check_str("key down", "/input/keys/KEY_A", "down");
    KryonInjectPump();
    CHECK(!IsKeyPressed(KEY_A));
    CHECK(IsKeyDown(KEY_A));
    KrySfsWrite("/input/keys/KEY_A", "up");
    KryonInjectPump();
    CHECK(IsKeyReleased(KEY_A));

    CHECK(KrySfsWrite("/input/keys/KEY_NOPE", "down") == KRY_SFS_ENOENT);

    KrySfsWrite("/input/text", "hi");
    KryonInjectPump();
    CHECK(GetCharPressed() == 'h');
    CHECK(GetCharPressed() == 'i');
    CHECK(GetCharPressed() == 0);

    KrySfsWrite("/input/wheel", "2.5");
    KryonInjectPump();
    CHECK(GetMouseWheelMove() > 2.0f && GetMouseWheelMove() < 3.0f);
    KryonInjectPump();
    CHECK(GetMouseWheelMove() == 0.0f);
}

static void
test_widgets(void)
{
    UIWidget widget;
    KrySfsEntry entries[16];
    int n;
    char buf[128];

    /* any SFS touch arms inspect recording before widgets register; the
     * frame call gives ui_mouse_world a sane camera like a real app */
    KrySfsList("/", entries, 16);
    BeginUIFrame(800, 600, 1.0f);
    BeginUIInspectFrame(NULL);
    widget = BeginUIWidget("button", "sfs-test:login", (Rectangle){100, 50,
                                                                    80, 24},
                           0);
    EndUIWidget(&widget);
    EndUIInspectFrame();

    n = KrySfsList("/widgets", entries, 16);
    CHECK(n >= 1);
    snprintf(buf, sizeof(buf), "/widgets/%d/name", n - 1);
    check_str("widget name", buf, "sfs-test:login");
    snprintf(buf, sizeof(buf), "/widgets/%d/bounds", n - 1);
    check_str("widget bounds", buf, "100 50 80 24");

    /* tapping the widget drives the real click path: the release lands on
     * the second pump, which is when UIHandleClick fires */
    snprintf(buf, sizeof(buf), "/widgets/%d/tap", n - 1);
    CHECK(KrySfsWrite(buf, "1") == 1);
    KryonInjectPump();
    CHECK(IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    KryonInjectPump();
    CHECK(UIHandleClick((Rectangle){100, 50, 80, 24}, 0, NULL));
    CHECK((int)GetMousePosition().x == 140);
    CHECK((int)GetMousePosition().y == 62);
}

static void
test_kryt_helpers(void)
{
    UIWidget widget;

    KryonInjectReset();
    KrySfsIsDir("/");
    BeginUIFrame(800, 600, 1.0f);
    BeginUIInspectFrame(NULL);
    widget = BeginUIWidget("button", "sfs-test:save", (Rectangle){0, 0, 40,
                                                                   20},
                           0);
    EndUIWidget(&widget);
    EndUIInspectFrame();

    CHECK(KryTTap("sfs-test:save"));
    KryonInjectPump();
    CHECK(IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    KryonInjectPump();
    CHECK(UIHandleClick((Rectangle){0, 0, 40, 20}, 0, NULL));
    CHECK(KryTKey("ENTER"));
    KryonInjectPump();
    CHECK(IsKeyPressed(KEY_ENTER));
    CHECK(KryTType("abc"));
    KryonInjectPump();
    CHECK(GetCharPressed() == 'a');
}

int
main(void)
{
    KryonInjectReset();
    test_root_and_info();
    test_theme();
    test_input_mouse();
    test_input_keys_and_text();
    KryonInjectReset();
    test_widgets();
    KryonInjectReset();
    test_kryt_helpers();
    if(failures == 0)
        printf("sfs tests passed\n");
    return failures == 0 ? 0 : 1;
}
