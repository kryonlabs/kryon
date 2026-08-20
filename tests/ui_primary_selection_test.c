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

static int
capture_osc52_response(void *userdata, const char *text)
{
    char *buffer = userdata;

    if(buffer == NULL || text == NULL)
        return 0;
    snprintf(buffer, 256, "%s", text);
    return 1;
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
        UIClipboardBuffer buffer;

        InitUIClipboardBuffer(&buffer, "clip");
        SetUIPrimarySelectionTextValue("primary");

        check_int("default target includes clipboard",
                  UIClipboardTargetIncludes(NULL, 'c'), 1);
        check_int("default target excludes primary",
                  UIClipboardTargetIncludes(NULL, 'p'), 0);
        check_int("primary target uses primary",
                  UIClipboardTargetUsesPrimary("p"), 1);
        check_int("combined target uses clipboard for reads",
                  UIClipboardTargetUsesPrimary("cp"), 0);
        check_str("default target reads clipboard",
                  GetUIClipboardTargetText(&buffer, NULL), "clip");
        check_str("primary target reads primary",
                  GetUIClipboardTargetText(&buffer, "p"), "primary");
        check_str("combined target reads clipboard",
                  GetUIClipboardTargetText(&buffer, "cp"), "clip");

        check_int("primary target write",
                  RequestUIClipboardTargetWrite(&buffer, "p", "ptext"), 1);
        check_str("primary target write text",
                  GetUIPrimarySelectionTextValue(), "ptext");
        check_str("primary target leaves clipboard",
                  GetUIClipboardBufferText(&buffer), "clip");

        check_int("clipboard target write",
                  RequestUIClipboardTargetWrite(&buffer, "c", "ctext"), 1);
        check_str("clipboard target write text",
                  GetUIClipboardBufferText(&buffer), "ctext");
        check_str("clipboard target leaves primary",
                  GetUIPrimarySelectionTextValue(), "ptext");

        check_int("combined target write",
                  RequestUIClipboardTargetWrite(&buffer, "cp", "both"), 1);
        check_str("combined target clipboard text",
                  GetUIClipboardBufferText(&buffer), "both");
        check_str("combined target primary text",
                  GetUIPrimarySelectionTextValue(), "both");

        check_int("selection target writes clipboard",
                  RequestUIClipboardTargetWrite(&buffer, "s", "select"), 1);
        check_str("selection target clipboard text",
                  GetUIClipboardBufferText(&buffer), "select");

        check_int("unknown target falls back to clipboard",
                  RequestUIClipboardTargetWrite(&buffer, "x", "fallback"), 1);
        check_str("unknown target clipboard text",
                  GetUIClipboardBufferText(&buffer), "fallback");
    }

    {
        UIClipboardBuffer buffer;
        char response[256];

        InitUIClipboardBuffer(&buffer, "");
        SetUIPrimarySelectionTextValue("");
        response[0] = '\0';

        check_int("osc 52 writes clipboard",
                  HandleUIClipboardOSC52(&buffer, "c;aGVsbG8=", NULL, NULL),
                  1);
        check_str("osc 52 clipboard text", GetUIClipboardBufferText(&buffer),
                  "hello");
        check_int("osc 52 clipboard query",
                  HandleUIClipboardOSC52(&buffer, "c;?",
                                         capture_osc52_response, response),
                  1);
        check_str("osc 52 clipboard response", response,
                  "\x1b]52;c;aGVsbG8=\a");

        check_int("osc 52 writes primary",
                  HandleUIClipboardOSC52(&buffer, "p;cHJpbWFyeQ==", NULL,
                                         NULL),
                  1);
        response[0] = '\0';
        check_int("osc 52 primary query",
                  HandleUIClipboardOSC52(&buffer, "p;?",
                                         capture_osc52_response, response),
                  1);
        check_str("osc 52 primary response", response,
                  "\x1b]52;p;cHJpbWFyeQ==\a");

        check_int("osc 52 clears clipboard",
                  HandleUIClipboardOSC52(&buffer, "c;", NULL, NULL), 1);
        check_str("osc 52 cleared text", GetUIClipboardBufferText(&buffer),
                  "");

        check_int("osc 52 invalid payload ignored",
                  HandleUIClipboardOSC52(&buffer, "c;%%%%", NULL, NULL), 0);
        check_str("osc 52 invalid keeps clipboard",
                  GetUIClipboardBufferText(&buffer), "");
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
