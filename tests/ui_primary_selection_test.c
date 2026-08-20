#include "kryon.h"

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

static void
check_str(const char *name, const char *got, const char *want)
{
    if(got != NULL && want != NULL && strcmp(got, want) == 0)
        return;
    fprintf(stderr, "%s: got \"%s\" want \"%s\"\n", name,
            got != NULL ? got : "(null)", want != NULL ? want : "(null)");
    exit(1);
}

int
main(void)
{
    check_int("set primary selection", SetUIPrimarySelectionTextValue("alpha"),
              1);
    check_str("get primary selection", GetUIPrimarySelectionTextValue(),
              "alpha");

    SetUIPrimarySelectionTextValue(NULL);
    check_str("null clears primary selection", GetUIPrimarySelectionTextValue(),
              "");

    SetUIPrimarySelectionTextValue("primary");
    check_str("primary selection resets", GetUIPrimarySelectionTextValue(),
              "primary");

    {
        UIClipboardBuffer buffer;

        InitUIClipboardBuffer(&buffer, "seed");
        check_str("clipboard buffer init", GetUIClipboardBufferText(&buffer),
                  "seed");
        check_int("clipboard buffer init pending",
                  UIClipboardBufferHasPendingWrite(&buffer), 0);

        check_int("clipboard buffer request",
                  RequestUIClipboardBufferWrite(&buffer, "shared"), 1);
        check_str("clipboard buffer request text",
                  GetUIClipboardBufferText(&buffer), "shared");
        check_int("clipboard buffer request pending",
                  UIClipboardBufferHasPendingWrite(&buffer), 1);

        check_int("clipboard buffer flush",
                  FlushUIClipboardBufferToHost(&buffer), 1);
        check_str("clipboard buffer host text", GetUIClipboardTextValue(),
                  "shared");
        check_int("clipboard buffer flush pending",
                  UIClipboardBufferHasPendingWrite(&buffer), 0);

        SetUIClipboardTextValue("host");
        check_int("clipboard buffer host sync",
                  SyncUIClipboardBufferFromHost(&buffer), 1);
        check_str("clipboard buffer synced text",
                  GetUIClipboardBufferText(&buffer), "host");
    }

    {
        TerminalPanePalette palette = GetTerminalPaneDefaultPalette();

        check_int("terminal palette black red", palette.ansi[1].r, 205);
        check_int("terminal palette black red green", palette.ansi[1].g, 49);
        check_int("terminal palette cube red", palette.ansi[196].r, 255);
        check_int("terminal palette cube red green", palette.ansi[196].g, 0);
        check_int("terminal palette gray", palette.ansi[232].r, 8);
        check_int("terminal palette white", palette.ansi[255].b, 238);
    }

    return 0;
}
