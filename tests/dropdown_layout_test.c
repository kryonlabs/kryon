#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"
#include "ui_controls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Dropdown popup layout + selection, driven with injected input.
 *
 * Regression for the "I want to press X and nothing happens" class:
 * - a tall dropdown near the bottom of the view must flip its popup
 *   above the button instead of running off-screen (tail unclickable)
 * - a wobbled or slow human click must still select; only a drag that
 *   actually scrolls the list may suppress selection
 * - every close path (option, button toggle, outside click, Escape)
 *   must close the popup
 *
 * Injected pointer positions lapse after two frames, so holds and
 * drags renew the position before each step. Each scenario starts from
 * a clean slate (injected input reset + Escape dismissal). */

#define OPT_COUNT 20
#define DD_ID 7001
#define VIEW_W 900
#define VIEW_H 720

static const char *g_options[OPT_COUNT];
static char g_labels[OPT_COUNT][16];
static int g_selected = -1;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static void
step(void)
{
    KryonInjectPump();
    BeginUIFrame(VIEW_W, VIEW_H, 1.0f);
    DrawUIDropdown(DD_ID, 100, 560, 400, 44,
                   g_options, OPT_COUNT, &g_selected);
    EndUIFrame();
}

/* UIInputCapturesClick answers open popups; the band above the button
 * (y 20..556, button at 560..604) is popup-only territory. */
static int
popup_open(void)
{
    step();
    return UIInputCapturesClick((Vector2){300.0f, 300.0f});
}

/* Clean slate between scenarios: clear injected input, dismiss popups. */
static void
reset_state(void)
{
    KryonInjectReset();
    KryonInjectKeyTap(KEY_ESCAPE);
    step();
    step();
    KryonInjectReset();
    step();
}

static void
open_dropdown(void)
{
    reset_state();
    KryonInjectTap(300, 582);
    step();   /* press */
    step();   /* release: opens */
    step();   /* just_opened clears */
}

static void
tap(int y)
{
    KryonInjectTap(300, y);
    step();
    step();
    step();
}

/* A human click with a wobble: press, stray a little, come back, release
 * where the pointer still is. */
static void
wobble_tap(int y, int wobble)
{
    KryonInjectMousePosition(300, y);
    KryonInjectMouseButton(0, 1);
    step();
    KryonInjectMousePosition(300 + wobble, y + wobble / 2);
    step();
    KryonInjectMousePosition(300, y);
    step();
    KryonInjectMousePosition(300, y);
    KryonInjectMouseButton(0, 0);
    step();
    step();
}

/* A slow human click: press, hold idle frames, release. */
static void
slow_tap(int y)
{
    KryonInjectMousePosition(300, y);
    KryonInjectMouseButton(0, 1);
    step();
    for(int hold = 0; hold < 6; hold++) {
        KryonInjectMousePosition(300, y);
        step();
    }
    KryonInjectMousePosition(300, y);
    KryonInjectMouseButton(0, 0);
    step();
    step();
}

/* Press at from_y and drag to to_y (content follows the finger). */
static void
drag(int from_y, int to_y)
{
    int dir = to_y >= from_y ? 10 : -10;

    KryonInjectMousePosition(300, from_y);
    KryonInjectMouseButton(0, 1);
    step();
    for(int y = from_y + dir; dir > 0 ? y <= to_y : y >= to_y; y += dir) {
        KryonInjectMousePosition(300, y);
        step();
    }
    KryonInjectMousePosition(300, to_y);
    KryonInjectMouseButton(0, 0);
    step();
    step();
}

int
main(void)
{
    SetUIScale(1.0f);

    for(int i = 0; i < OPT_COUNT; i++) {
        snprintf(g_labels[i], sizeof(g_labels[i]), "Option %d", i);
        g_options[i] = g_labels[i];
    }

    InitUI(VIEW_W, VIEW_H, 1.0f);
    for(int i = 0; i < 3; i++)
        step();

    /* Plain clicks on the flipped-up popup (options 0..11 at
     * y = 24 + 44*i). */
    open_dropdown();
    g_selected = -1;
    tap(46);
    check_int("first popup row selects", g_selected, 0);

    open_dropdown();
    g_selected = -1;
    tap(510);
    check_int("bottom visible row selects", g_selected, 11);

    /* Wobbled and slow human clicks still select. */
    open_dropdown();
    g_selected = -1;
    wobble_tap(46, 12);
    check_int("wobbled click selects (12px stray)", g_selected, 0);

    open_dropdown();
    g_selected = -1;
    slow_tap(46);
    check_int("slow hold-click selects", g_selected, 0);

    open_dropdown();
    g_selected = -1;
    wobble_tap(510, 10);
    check_int("wobbled click selects near bottom", g_selected, 11);

    /* A drag that actually scrolls the list must not select on release. */
    open_dropdown();
    g_selected = -1;
    drag(500, 300);
    check_int("scrolling drag does not select", g_selected, -1);

    /* After drag-scrolling to the tail, the last option must be
     * clickable at the popup's bottom rows. */
    open_dropdown();
    g_selected = -1;
    drag(300, 40);
    drag(200, 40);
    tap(530);
    check_int("last option selectable after scroll", g_selected, OPT_COUNT - 1);

    /* Close paths. */
    open_dropdown();
    check_int("popup reports open", popup_open(), 1);
    tap(660);
    check_int("outside click closes", popup_open(), 0);
    check_int("outside click does not select", g_selected, OPT_COUNT - 1);

    open_dropdown();
    tap(582);
    check_int("button toggles the popup closed", popup_open(), 0);

    open_dropdown();
    g_selected = -1;
    tap(46);
    check_int("selection closes the popup", popup_open(), 0);

    open_dropdown();
    KryonInjectKeyTap(KEY_ESCAPE);
    step();
    check_int("escape closes the popup", popup_open(), 0);

    /* Wheel-scroll then close. The wheel only registers while the
     * pointer hovers the popup. */
    open_dropdown();
    for(int i = 0; i < 8; i++) {
        KryonInjectMousePosition(300, 300);
        KryonInjectWheel(-1.0f);
        step();
    }
    tap(582);
    check_int("wheel: button toggles closed", popup_open(), 0);

    open_dropdown();
    for(int i = 0; i < 8; i++) {
        KryonInjectMousePosition(300, 300);
        KryonInjectWheel(-1.0f);
        step();
    }
    g_selected = -1;
    tap(46);
    check_int("wheel: top row selects after scroll-down", g_selected, 8);

    /* App-style clip constraints (titlebar above, bottom nav below):
     * the flip must respect the clips; 10 options fit, so the bottom
     * visible row is option 9. */
    SetUIDropdownClipTop(60);
    SetUIDropdownClipBottom(660);
    open_dropdown();
    check_int("clipped popup reports open", popup_open(), 1);
    tap(660);
    check_int("clipped: outside click closes", popup_open(), 0);
    open_dropdown();
    g_selected = -1;
    tap(510);
    check_int("clipped: bottom row selects", g_selected, 9);
    check_int("clipped: selection closes", popup_open(), 0);
    open_dropdown();
    tap(582);
    check_int("clipped: button toggles closed", popup_open(), 0);
    SetUIDropdownClipTop(0);
    SetUIDropdownClipBottom(0);

    printf("dropdown layout test ok\n");
    return 0;
}
