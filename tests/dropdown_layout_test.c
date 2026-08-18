#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"
#include "ui_controls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

/* Dropdown popup layout + selection, driven with injected taps.
 *
 * Regression: a tall dropdown near the bottom of the view used to place
 * its popup below the button while sizing it for the space above, so the
 * popup ran off-screen and its tail options were unclickable. The popup
 * must flip above the button instead, and every row - including the last
 * option after scrolling - must select. */

#define OPT_COUNT 20
#define DD_ID 7001
#define VIEW_W 900
#define VIEW_H 720

static const char *g_options[OPT_COUNT];
static char g_labels[OPT_COUNT][16];
static int g_selected = -1;

static void
step(void)
{
    KryonInjectPump();
    BeginUIFrame(VIEW_W, VIEW_H, 1.0f);
    DrawUIDropdown(DD_ID, 100, 560, 400, 44,
                   g_options, OPT_COUNT, &g_selected);
    EndUIFrame();
}

static void
open_dropdown(void)
{
    KryonInjectTap(300, 582);
    step();   /* press */
    step();   /* release: opens */
    step();   /* just_opened clears */
}

static void
drag_up(int from_y, int to_y)
{
    KryonInjectMousePosition(300, from_y);
    KryonInjectMouseButton(0, 1);
    step();
    for(int y = from_y - 10; y >= to_y; y -= 10) {
        KryonInjectMousePosition(300, y);
        step();
    }
    KryonInjectMouseButton(0, 0);
    step();
}

static void
tap(int y)
{
    KryonInjectTap(300, y);
    step();
    step();
    step();
}

/* UIInputCapturesClick includes open dropdown popups, so a probe inside
 * the popup band, mid-height (away from the button and valid for clipped layouts too) reports
 * whether the popup is currently open. */
static int
popup_open(void)
{
    int saved_frame = 0;

    (void)saved_frame;
    step();
    return UIInputCapturesClick((Vector2){300.0f, 300.0f});
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

    /* The popup must open above the button (button at y=560 of a
     * 720-tall view): its rows sit around y=24+44*i. A click on the
     * first row must select instead of closing the popup. */
    open_dropdown();
    g_selected = -1;
    tap(46);
    check_int("first popup row selects", g_selected, 0);

    /* Bottom-most visible row of the unscrolled popup (option 11). */
    open_dropdown();
    g_selected = -1;
    tap(510);
    check_int("bottom visible row selects", g_selected, 11);

    /* Drag-scroll to the tail of the list, then the last option must be
     * clickable at the popup's bottom rows. */
    open_dropdown();
    g_selected = -1;
    drag_up(300, 40);
    drag_up(200, 40);
    tap(530);
    check_int("last option selectable after scroll", g_selected, OPT_COUNT - 1);

    /* A click clearly outside button (560..604) and popup (20..556)
     * must close the popup without selecting anything. */
    open_dropdown();
    tap(660);
    check_int("outside click closes without selecting", g_selected, OPT_COUNT - 1);
    check_int("outside click closes the popup", popup_open(), 0);

    /* Clicking the button again must toggle the popup closed. */
    open_dropdown();
    check_int("popup reports open", popup_open(), 1);
    tap(582);
    check_int("button toggles the popup closed", popup_open(), 0);

    /* Selecting an option closes the popup. */
    open_dropdown();
    g_selected = -1;
    tap(46);
    check_int("selection closes the popup", popup_open(), 0);

    /* Wheel-scroll the list, then all close paths must still work. The
     * wheel only registers while the pointer hovers the popup, so keep
     * the injected position alive across the wheel frames. */
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
    tap(660);
    check_int("wheel: outside click closes", popup_open(), 0);
    open_dropdown();
    for(int i = 0; i < 8; i++) {
        KryonInjectMousePosition(300, 300);
        KryonInjectWheel(-1.0f);
        step();
    }
    g_selected = -1;
    tap(46);
    check_int("wheel: top row selects after scroll", g_selected, 8);
    check_int("wheel: selection closes", popup_open(), 0);

    /* Escape dismisses an open popup. */
    open_dropdown();
    KryonInjectKeyTap(KEY_ESCAPE);
    step();
    check_int("escape closes the popup", popup_open(), 0);

    /* Same close paths with app-style clip constraints (titlebar above,
     * bottom nav below) - the flip math must respect the clips. */
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
