/* Cursor-intent priority regression test.
 *
 * Widgets mark the mouse cursor as they draw: disabled controls mark
 * NOT_ALLOWED, clickable ones POINTING_HAND, text IBEAM. UI drawn on top of
 * disabled background content (a modal over a dimmed, interaction-disabled
 * board) must win the cursor: a ⃠ from the background used to override the
 * hand/ibeam of the foreground because DISABLED had the highest priority,
 * banning the cursor for the whole frame. Disabled keeps the lowest
 * non-default priority: it shows only when nothing interactive is on top. */

#include "kryon.h"

#include <stdio.h>

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
    BeginUIFrame(640, 480, 1.0f);

    /* Draw order = stacking order: a disabled card first, then a button
     * on top of it. The button's hand must win. */
    MarkUIDisabled();
    check(GetUIMouseCursor() == MOUSE_CURSOR_NOT_ALLOWED,
          "disabled control shows the not-allowed cursor");
    MarkUIClickable();
    check(GetUIMouseCursor() == MOUSE_CURSOR_POINTING_HAND,
          "clickable foreground overrides disabled background");
    MarkUICursor(MOUSE_CURSOR_IBEAM);
    check(GetUIMouseCursor() == MOUSE_CURSOR_IBEAM,
          "text foreground overrides disabled background");
    MarkUICursor(MOUSE_CURSOR_RESIZE_EW);
    check(GetUIMouseCursor() == MOUSE_CURSOR_RESIZE_EW,
          "resize foreground overrides disabled background");

    EndUIFrame();
    /* The reset fires on the second consecutive intent-less frame start
     * (the check reads the previous frame's intent flag), which is one
     * 16ms frame of stale cursor — invisible, asserted as designed. */
    BeginUIFrame(640, 480, 1.0f);
    BeginUIFrame(640, 480, 1.0f);
    check(GetUIMouseCursor() == MOUSE_CURSOR_DEFAULT,
          "no intents resets to the default cursor");

    /* Disabled alone still shows not-allowed. */
    MarkUIDisabled();
    check(GetUIMouseCursor() == MOUSE_CURSOR_NOT_ALLOWED,
          "lone disabled control keeps the not-allowed cursor");

    EndUIFrame();

    if(failures == 0)
        printf("cursor_intent_test: OK\n");
    return failures == 0 ? 0 : 1;
}
