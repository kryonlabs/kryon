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

static void
check_bytes(const char *name, const char *got, int got_len,
            const char *want, int want_len)
{
    if(got != NULL && want != NULL && got_len == want_len &&
       memcmp(got, want, (size_t)want_len) == 0)
        return;
    fprintf(stderr, "%s: byte mismatch\n", name);
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

static int
capture_paste_write(void *userdata, const char *text, int size)
{
    char *buffer = userdata;
    size_t used;

    if(buffer == NULL || text == NULL || size <= 0)
        return 0;
    used = strlen(buffer);
    if(used + (size_t)size >= 512)
        size = (int)(511 - used);
    if(size <= 0)
        return 0;
    memcpy(buffer + used, text, (size_t)size);
    buffer[used + (size_t)size] = '\0';
    return size;
}

typedef struct {
    const char **lines;
    const int *wrapped;
    int count;
} SelectionFixture;

static int
fixture_line_text(void *userdata, int row, char *out, int out_size)
{
    SelectionFixture *fixture = userdata;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(fixture == NULL || row < 0 || row >= fixture->count ||
       fixture->lines[row] == NULL)
        return 0;
    snprintf(out, (size_t)out_size, "%s", fixture->lines[row]);
    return 1;
}

static int
fixture_line_wrapped(void *userdata, int row)
{
    SelectionFixture *fixture = userdata;

    if(fixture == NULL || fixture->wrapped == NULL || row < 0 ||
       row >= fixture->count)
        return 0;
    return fixture->wrapped[row];
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
    check_int("primary source has text",
              UIClipboardSourceHasText(UI_CLIPBOARD_SOURCE_PRIMARY), 1);
    check_int("terminal pane primary source has text",
              TerminalPaneClipboardSourceHasText(
                  UI_CLIPBOARD_SOURCE_PRIMARY),
              1);
    check_int("clipboard source has no text",
              UIClipboardSourceHasText(UI_CLIPBOARD_SOURCE_CLIPBOARD), 0);
    check_int("terminal pane clipboard source has no text",
              TerminalPaneClipboardSourceHasText(
                  UI_CLIPBOARD_SOURCE_CLIPBOARD),
              0);

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

        check_int("clipboard source has text",
                  UIClipboardSourceHasText(UI_CLIPBOARD_SOURCE_CLIPBOARD), 1);
        check_str("clipboard source text",
                  GetUIClipboardSourceText(&buffer,
                                           UI_CLIPBOARD_SOURCE_CLIPBOARD),
                  "host");
        check_int("primary source update",
                  SetUIPrimarySelectionFromText("selection"), 1);
        check_str("primary source text",
                  GetUIClipboardSourceText(&buffer,
                                           UI_CLIPBOARD_SOURCE_PRIMARY),
                  "selection");
        check_int("preferred source uses primary",
                  strcmp(GetUIClipboardSourceText(
                             &buffer,
                             UI_CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD),
                         "selection") == 0,
                  1);
        check_int("copy selection text",
                  CopyUISelectionTextToClipboard(&buffer, "copied"), 1);
        check_str("copied selection host", GetUIClipboardTextValue(),
                  "copied");
        check_str("copied selection buffer", GetUIClipboardBufferText(&buffer),
                  "copied");
        check_str("copied selection primary", GetUIPrimarySelectionTextValue(),
                  "copied");
        check_int("empty copy does not replace clipboard",
                  CopyUISelectionTextToClipboard(&buffer, ""), 0);
        check_str("empty copy keeps clipboard", GetUIClipboardTextValue(),
                  "copied");
        check_str("empty copy clears primary", GetUIPrimarySelectionTextValue(),
                  "");
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
        Rectangle content = TerminalPaneContentBounds(
            (Rectangle){10.0f, 20.0f, 300.0f, 200.0f}, 34, 6);

        check_int("terminal palette black red", palette.ansi[1].r, 205);
        check_int("terminal palette black red green", palette.ansi[1].g, 49);
        check_int("terminal palette cube red", palette.ansi[196].r, 255);
        check_int("terminal palette cube red green", palette.ansi[196].g, 0);
        check_int("terminal palette gray", palette.ansi[232].r, 8);
        check_int("terminal palette white", palette.ansi[255].b, 238);
        check_int("terminal content x", (int)content.x, 16);
        check_int("terminal content y", (int)content.y, 60);
        check_int("terminal content width", (int)content.width, 288);
        check_int("terminal content height", (int)content.height, 154);
    }

    {
        const char *lines[] = {"open /tmp/kapsule-test.txt now",
                               "second line",
                               "abcdefghijklmnop",
                               "q"};
        const int wrapped[] = {0, 0, 1, 0};
        SelectionFixture fixture = {lines, wrapped, 4};
        TerminalPaneSelection selection;
        char text[256];

        TerminalPaneSelectionClear(&selection);
        TerminalPaneSelectionSelectWord(&selection, fixture_line_text,
                                        &fixture, 0, 8);
        check_int("terminal selection contains first path char",
                  TerminalPaneSelectionContains(&selection, 0, 5), 1);
        check_int("terminal selection contains last path char",
                  TerminalPaneSelectionContains(&selection, 0, 25), 1);
        check_int("terminal selection excludes after word",
                  TerminalPaneSelectionContains(&selection, 0, 26), 0);
        check_int("terminal word selection text",
                  TerminalPaneSelectionCollectText(
                      &selection, fixture_line_text, fixture_line_wrapped,
                      &fixture, text, (int)sizeof(text)),
                  1);
        check_str("terminal word selection value", text,
                  "/tmp/kapsule-test.txt");

        TerminalPaneSelectionSelectLine(&selection, fixture_line_text,
                                        &fixture, 1);
        check_int("terminal line selection text",
                  TerminalPaneSelectionCollectText(
                      &selection, fixture_line_text, fixture_line_wrapped,
                      &fixture, text, (int)sizeof(text)),
                  1);
        check_str("terminal line selection value", text, "second line");

        TerminalPaneSelectionSelectWord(&selection, fixture_line_text,
                                        &fixture, 0, 7);
        TerminalPaneSelectionUpdateEnd(&selection, fixture_line_text,
                                       &fixture, 0, 1);
        check_int("terminal backward word drag text",
                  TerminalPaneSelectionCollectText(
                      &selection, fixture_line_text, fixture_line_wrapped,
                      &fixture, text, (int)sizeof(text)),
                  1);
        check_str("terminal backward word drag value", text,
                  "open /tmp/kapsule-test.txt");

        TerminalPaneSelectionSetRange(&selection, TERMINAL_PANE_SELECTION_CHAR,
                                      0, 2, 0, 3, 1);
        check_int("terminal wrapped selection text",
                  TerminalPaneSelectionCollectText(
                      &selection, fixture_line_text, fixture_line_wrapped,
                      &fixture, text, (int)sizeof(text)),
                  1);
        check_str("terminal wrapped selection value", text,
                  "abcdefghijklmnopq");

        {
            UIClipboardBuffer clipboard;

            InitUIClipboardBuffer(&clipboard, "");
            SetUIPrimarySelectionTextValue("old primary");
            check_int("terminal selection primary update",
                      TerminalPaneSelectionUpdatePrimary(
                          &selection, fixture_line_text, fixture_line_wrapped,
                          &fixture),
                      1);
            check_str("terminal selection primary text",
                      GetUIPrimarySelectionTextValue(), "abcdefghijklmnopq");
            check_int("terminal selection clipboard copy",
                      TerminalPaneSelectionCopyToClipboard(
                          &selection, fixture_line_text, fixture_line_wrapped,
                          &fixture, &clipboard),
                      1);
            check_str("terminal selection clipboard host",
                      GetUIClipboardTextValue(), "abcdefghijklmnopq");
            check_str("terminal selection clipboard buffer",
                      GetUIClipboardBufferText(&clipboard),
                      "abcdefghijklmnopq");

            TerminalPaneSelectionClear(&selection);
            check_int("terminal selection empty primary update",
                      TerminalPaneSelectionUpdatePrimary(
                          &selection, fixture_line_text, fixture_line_wrapped,
                          &fixture),
                      1);
            check_str("terminal selection empty primary text",
                      GetUIPrimarySelectionTextValue(), "");
            check_int("terminal selection empty clipboard copy",
                      TerminalPaneSelectionCopyToClipboard(
                          &selection, fixture_line_text, fixture_line_wrapped,
                          &fixture, &clipboard),
                      0);
        }

        check_int("terminal selection edge top",
                  TerminalPaneSelectionEdgeScrollDelta(90.0f, 100.0f, 200.0f,
                                                       24.0f),
                  1);
        check_int("terminal selection edge middle",
                  TerminalPaneSelectionEdgeScrollDelta(180.0f, 100.0f, 200.0f,
                                                       24.0f),
                  0);
        check_int("terminal selection edge bottom",
                  TerminalPaneSelectionEdgeScrollDelta(310.0f, 100.0f, 200.0f,
                                                       24.0f),
                  -1);
        check_int("terminal first visible row",
                  TerminalPaneSelectionFirstVisibleRow(40, 8, 5), 27);
        check_int("terminal edge scroll row top",
                  TerminalPaneSelectionEdgeScrollRow(12, 8, 1), 12);
        check_int("terminal edge scroll row bottom",
                  TerminalPaneSelectionEdgeScrollRow(12, 8, -1), 19);
    }

    {
        const char *lines[] = {"Alpha beta",
                               "gamma ALPHA",
                               "needle middle needle",
                               "final Beta"};
        SelectionFixture fixture = {lines, NULL, 4};
        TerminalPaneSearchMatch match;

        check_int("terminal search smartcase lowercase",
                  TerminalPaneSearchLines(fixture_line_text, &fixture, 4,
                                          "alpha", 0, 0, 1, 0, &match),
                  1);
        check_int("terminal search lowercase row", match.row, 0);
        check_int("terminal search lowercase col", match.col, 0);
        check_int("terminal search lowercase len", match.length, 5);

        check_int("terminal search smartcase uppercase",
                  TerminalPaneSearchLines(fixture_line_text, &fixture, 4,
                                          "ALPHA", 0, 0, 1, 0, &match),
                  1);
        check_int("terminal search uppercase row", match.row, 1);
        check_int("terminal search uppercase col", match.col, 6);

        check_int("terminal search forward wrap",
                  TerminalPaneSearchLines(fixture_line_text, &fixture, 4,
                                          "beta", 3, 7, 1, 1, &match),
                  1);
        check_int("terminal search forward wrap row", match.row, 0);
        check_int("terminal search forward wrap col", match.col, 6);

        check_int("terminal search backward wrap",
                  TerminalPaneSearchLines(fixture_line_text, &fixture, 4,
                                          "beta", 0, 0, -1, 1, &match),
                  1);
        check_int("terminal search backward wrap row", match.row, 3);
        check_int("terminal search backward wrap col", match.col, 6);

        check_int("terminal search backward same line",
                  TerminalPaneSearchLines(fixture_line_text, &fixture, 4,
                                          "needle", 2, 20, -1, 0, &match),
                  1);
        check_int("terminal search backward same row", match.row, 2);
        check_int("terminal search backward same col", match.col, 14);

        match.row = 99;
        match.col = 99;
        match.length = 99;
        check_int("terminal search empty query",
                  TerminalPaneSearchLines(fixture_line_text, &fixture, 4, "",
                                          0, 0, 1, 1, &match),
                  0);
        check_int("terminal search empty row reset", match.row, -1);
        check_int("terminal search empty col reset", match.col, -1);
        check_int("terminal search empty length reset", match.length, 0);
    }

    {
        char paste[512];
        const char payload[] = "safe\x1b[201~after\x1b[31mred\xd0\x80\a"
                               "\x9b" "32mgreen"
                               "osc\x1b]2;title\aafterosc"
                               "dcs\x1bPq~~\x1b\\afterdcs"
                               "c1osc\x9d" "2;bad\aafterc1osc"
                               "c1dcs\x90q~\x9c" "afterc1dcs";
        const char sanitized[] = "\x1b[200~safeafterred\xd0\x80green"
                                 "oscafteroscdcsafterdcs"
                                 "c1oscafterc1oscc1dcsafterc1dcs\x1b[201~";

        paste[0] = '\0';
        check_int("plain paste writes bytes",
                  WriteUIClipboardPaste("plain", 0, capture_paste_write,
                                        paste),
                  5);
        check_str("plain paste text", paste, "plain");

        paste[0] = '\0';
        check_int("bracketed paste writes bytes",
                  WriteUIClipboardPaste("paste\ntext", 1,
                                        capture_paste_write, paste),
                  22);
        check_str("bracketed paste text", paste,
                  "\x1b[200~paste\ntext\x1b[201~");

        paste[0] = '\0';
        check_int("sanitized bracketed paste writes bytes",
                  WriteUIClipboardPaste(payload, 1, capture_paste_write,
                                        paste),
                  (int)strlen(sanitized));
        check_str("sanitized bracketed paste text", paste, sanitized);

        {
            UIClipboardBuffer buffer;

            InitUIClipboardBuffer(&buffer, "");
            SetUIClipboardTextValue("host paste");
            SetUIPrimarySelectionTextValue("");
            paste[0] = '\0';
            check_int("clipboard source paste",
                      WriteUIClipboardSourcePaste(
                          &buffer, UI_CLIPBOARD_SOURCE_CLIPBOARD, 0,
                          capture_paste_write, paste),
                      10);
            check_str("clipboard source paste text", paste, "host paste");
            check_str("clipboard source paste buffer",
                      GetUIClipboardBufferText(&buffer), "host paste");

            SetUIPrimarySelectionTextValue("primary paste");
            paste[0] = '\0';
            check_int("preferred source paste",
                      WriteUIClipboardSourcePaste(
                          &buffer, UI_CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD,
                          0, capture_paste_write, paste),
                      13);
            check_str("preferred source paste text", paste, "primary paste");
        }

        {
            UIClipboardBuffer buffer;
            TerminalPaneClipboard clipboard;

            InitUIClipboardBuffer(&buffer, "");
            clipboard = (TerminalPaneClipboard){
                &buffer,
                1,
                capture_paste_write,
                paste,
            };
            paste[0] = '\0';
            check_int("terminal pane clipboard text paste",
                      TerminalPaneClipboardPasteText(clipboard,
                                                     "pane\npaste"),
                      22);
            check_str("terminal pane clipboard text", paste,
                      "\x1b[200~pane\npaste\x1b[201~");
            check_str("terminal pane clipboard buffer",
                      GetUIClipboardBufferText(&buffer), "pane\npaste");

            SetUIPrimarySelectionTextValue("pane primary");
            clipboard.bracketed_paste = 0;
            paste[0] = '\0';
            check_int("terminal pane clipboard source paste",
                      TerminalPaneClipboardPasteSource(
                          clipboard,
                          UI_CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD),
                      12);
            check_str("terminal pane clipboard source", paste,
                      "pane primary");

            SetUIClipboardTextValue("pane clipboard");
            (void)SyncUIClipboardBufferFromHost(&buffer);
            paste[0] = '\0';
            check_int("terminal pane clipboard helper paste",
                      TerminalPaneClipboardPasteClipboard(clipboard), 14);
            check_str("terminal pane clipboard helper", paste,
                      "pane clipboard");

            SetUIPrimarySelectionTextValue("helper primary");
            paste[0] = '\0';
            check_int("terminal pane primary helper paste",
                      TerminalPaneClipboardPastePrimary(clipboard), 14);
            check_str("terminal pane primary helper", paste,
                      "helper primary");

            SetUIPrimarySelectionTextValue("helper preferred");
            paste[0] = '\0';
            check_int("terminal pane preferred helper paste",
                      TerminalPaneClipboardPastePreferred(clipboard), 16);
            check_str("terminal pane preferred helper", paste,
                      "helper preferred");

            SetUIClipboardTextValue("host sync");
            check_int("terminal pane clipboard sync",
                      TerminalPaneClipboardSyncFromHost(clipboard), 1);
            check_str("terminal pane clipboard synced buffer",
                      GetUIClipboardBufferText(&buffer), "host sync");

            RequestUIClipboardBufferWrite(&buffer, "host flush");
            check_int("terminal pane clipboard flush",
                      TerminalPaneClipboardFlushToHost(clipboard), 1);
            check_str("terminal pane clipboard flushed host",
                      GetUIClipboardTextValue(), "host flush");
        }
    }

    {
        TerminalPaneColors theme = {
            {1, 2, 3, 255},
            {4, 5, 6, 255},
            {7, 8, 9, 255},
            {10, 11, 12, 255},
            {13, 14, 15, 255},
            {16, 17, 18, 255},
            {19, 20, 21, 255},
            {22, 23, 24, 255},
            {25, 26, 27, 255},
            {28, 29, 30, 255},
            {31, 32, 33, 255}
        };
        TerminalPaneProfileColors configured = {
            TERMINAL_PANE_COLOR_DEFAULT,
            TERMINAL_PANE_COLOR_TRUE_RGB | 0x445566,
            TERMINAL_PANE_COLOR_DEFAULT,
            TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc,
            TERMINAL_PANE_COLOR_DEFAULT
        };
        TerminalPaneProfileColors colors;
        TerminalPaneProfileState state;
        char color_text[16];
        int parsed_color = 0;
        char escaped_text[64];
        char unescaped_text[64];

        check_int("terminal pane color rgb",
                  TerminalPaneColorToRGB((Color){0x11, 0x22, 0x33, 255}),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x112233);
        colors = ResolveTerminalPaneProfileColors(configured, theme);
        check_int("profile foreground from theme", colors.foreground,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x040506);
        check_int("profile background configured", colors.background,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x445566);
        check_int("profile cursor from theme", colors.cursor,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x101112);
        check_int("profile selection foreground configured",
                  colors.selection_foreground,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc);
        check_int("profile selection background from theme",
                  colors.selection_background,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x0a0b0c);

        memset(&state, 0, sizeof(state));
        state.cursor = TERMINAL_PANE_COLOR_DEFAULT;
        state.selection_foreground = TERMINAL_PANE_COLOR_DEFAULT;
        state.selection_background = TERMINAL_PANE_COLOR_DEFAULT;
        TerminalPaneProfileStateApplyNew(&state, colors);
        check_int("profile apply base foreground", state.base_foreground,
                  colors.foreground);
        check_int("profile apply foreground", state.foreground,
                  colors.foreground);
        check_int("profile apply cursor", state.cursor, colors.cursor);
        check_int("profile apply selection background",
                  state.selection_background, colors.selection_background);

        state.foreground = TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203;
        state.cursor = TERMINAL_PANE_COLOR_TRUE_RGB | 0x040506;
        state.selection_background =
            TERMINAL_PANE_COLOR_TRUE_RGB | 0x070809;
        TerminalPaneProfileStateSyncChanged(
            &state, colors,
            (TerminalPaneProfileColors){
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x111111,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x222222,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x333333,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x444444,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x555555
            });
        check_int("profile sync updates base foreground",
                  state.base_foreground,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x111111);
        check_int("profile sync preserves foreground override",
                  state.foreground, TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203);
        check_int("profile sync preserves cursor override", state.cursor,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x040506);
        check_int("profile sync preserves selection background override",
                  state.selection_background,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x070809);

        memset(&state, 0, sizeof(state));
        state.foreground = TERMINAL_PANE_COLOR_DEFAULT;
        state.background = TERMINAL_PANE_COLOR_DEFAULT;
        state.cursor = TERMINAL_PANE_COLOR_DEFAULT;
        state.selection_foreground = TERMINAL_PANE_COLOR_DEFAULT;
        state.selection_background = TERMINAL_PANE_COLOR_DEFAULT;
        TerminalPaneProfileStateSeedMissing(&state, colors);
        check_int("profile seed foreground", state.foreground,
                  colors.foreground);
        check_int("profile seed background", state.background,
                  colors.background);

        check_int("profile color hash parse",
                  ParseTerminalPaneProfileColor("#010203", &parsed_color),
                  1);
        check_int("profile color hash value", parsed_color,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203);
        check_int("profile color bare parse",
                  ParseTerminalPaneProfileColor("aabbcc", &parsed_color), 1);
        check_int("profile color bare value", parsed_color,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc);
        check_int("profile color default parse",
                  ParseTerminalPaneProfileColor("default", &parsed_color), 1);
        check_int("profile color default value", parsed_color,
                  TERMINAL_PANE_COLOR_DEFAULT);
        parsed_color = 123;
        check_int("profile color system parse",
                  ParseTerminalPaneProfileColor("system", &parsed_color), 1);
        check_int("profile color system value", parsed_color,
                  TERMINAL_PANE_COLOR_DEFAULT);
        parsed_color = 123;
        check_int("profile color theme parse",
                  ParseTerminalPaneProfileColor("theme", &parsed_color), 1);
        check_int("profile color theme value", parsed_color,
                  TERMINAL_PANE_COLOR_DEFAULT);
        parsed_color = 123;
        check_int("profile color invalid rejected",
                  ParseTerminalPaneProfileColor("zzzzzz", &parsed_color), 0);
        check_int("profile color invalid preserves out", parsed_color, 123);
        check_int("profile color null rejected",
                  ParseTerminalPaneProfileColor(NULL, &parsed_color), 0);
        check_int("profile color format",
                  FormatTerminalPaneProfileColor(
                      color_text, (int)sizeof(color_text),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x0a0b0c),
                  7);
        check_str("profile color format text", color_text, "#0a0b0c");
        color_text[0] = 'x';
        check_int("profile color format default omitted",
                  FormatTerminalPaneProfileColor(
                      color_text, (int)sizeof(color_text),
                      TERMINAL_PANE_COLOR_DEFAULT),
                  0);
        check_int("profile color format invalid omitted",
                  FormatTerminalPaneProfileColor(color_text,
                                                 (int)sizeof(color_text), 12),
                  0);
        check_int("profile text escape",
                  EscapeTerminalPaneText(escaped_text,
                                         (int)sizeof(escaped_text),
                                         "a\\b\tc\nd\r"),
                  12);
        check_str("profile text escaped value", escaped_text,
                  "a\\\\b\\tc\\nd\\r");
        check_int("profile text unescape",
                  UnescapeTerminalPaneText(unescaped_text,
                                           (int)sizeof(unescaped_text),
                                           escaped_text),
                  8);
        check_str("profile text unescaped value", unescaped_text,
                  "a\\b\tc\nd\r");
        check_int("profile text escape null",
                  EscapeTerminalPaneText(escaped_text,
                                         (int)sizeof(escaped_text), NULL),
                  0);
        check_str("profile text escape null value", escaped_text, "");
        check_int("profile text unescape unknown",
                  UnescapeTerminalPaneText(unescaped_text,
                                           (int)sizeof(unescaped_text),
                                           "\\x"),
                  1);
        check_str("profile text unescape unknown value", unescaped_text,
                  "x");
        check_int("profile text escape truncates safely",
                  EscapeTerminalPaneText(escaped_text, 4, "abcdef"), 3);
        check_str("profile text escape truncated value", escaped_text,
                  "abc");
        check_int("profile text unescape truncates safely",
                  UnescapeTerminalPaneText(unescaped_text, 4, "abcdef"), 3);
        check_str("profile text unescape truncated value", unescaped_text,
                  "abc");

        check_int("cursor style parse block",
                  ParseTerminalPaneCursorStyle("block",
                                               TERMINAL_PANE_CURSOR_BAR),
                  TERMINAL_PANE_CURSOR_BLOCK);
        check_int("cursor style parse numeric block",
                  ParseTerminalPaneCursorStyle("1",
                                               TERMINAL_PANE_CURSOR_BAR),
                  TERMINAL_PANE_CURSOR_BLOCK);
        check_int("cursor style parse underline",
                  ParseTerminalPaneCursorStyle("underline",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_UNDERLINE);
        check_int("cursor style parse numeric underline",
                  ParseTerminalPaneCursorStyle("2",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_UNDERLINE);
        check_int("cursor style parse bar",
                  ParseTerminalPaneCursorStyle("bar",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_BAR);
        check_int("cursor style parse beam",
                  ParseTerminalPaneCursorStyle("beam",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_BAR);
        check_int("cursor style parse numeric bar",
                  ParseTerminalPaneCursorStyle("3",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_BAR);
        check_int("cursor style parse default",
                  ParseTerminalPaneCursorStyle("default",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_DEFAULT);
        check_int("cursor style parse numeric default",
                  ParseTerminalPaneCursorStyle("0",
                                               TERMINAL_PANE_CURSOR_BLOCK),
                  TERMINAL_PANE_CURSOR_DEFAULT);
        check_int("cursor style parse invalid keeps fallback",
                  ParseTerminalPaneCursorStyle("wide",
                                               TERMINAL_PANE_CURSOR_BAR),
                  TERMINAL_PANE_CURSOR_BAR);
        check_int("cursor style parse null keeps fallback",
                  ParseTerminalPaneCursorStyle(NULL,
                                               TERMINAL_PANE_CURSOR_UNDERLINE),
                  TERMINAL_PANE_CURSOR_UNDERLINE);
        check_str("cursor style name block",
                  TerminalPaneCursorStyleName(TERMINAL_PANE_CURSOR_BLOCK),
                  "block");
        check_str("cursor style name underline",
                  TerminalPaneCursorStyleName(TERMINAL_PANE_CURSOR_UNDERLINE),
                  "underline");
        check_str("cursor style name bar",
                  TerminalPaneCursorStyleName(TERMINAL_PANE_CURSOR_BAR),
                  "bar");
        check_str("cursor style name default saved as block",
                  TerminalPaneCursorStyleName(TERMINAL_PANE_CURSOR_DEFAULT),
                  "block");
    }

    {
        char response[80];
        int len;
        int window;
        int icon;
        char title_report[80];
        char title_stack[2][12] = {{0}};
        int title_count = 0;
        char popped[12];
        char path[80];

        check_int("osc color hash short",
                  ParseTerminalPaneOSCColor("#abc"),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc);
        check_int("osc color hash wide",
                  ParseTerminalPaneOSCColor("#123456789"),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x124578);
        check_int("osc color rgb short components",
                  ParseTerminalPaneOSCColor("rgb:1/23/4567"),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x112345);
        check_int("osc color rgba",
                  ParseTerminalPaneOSCColor("rgba:10/30/50/70"),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x103050);
        check_int("osc color rgbi",
                  ParseTerminalPaneOSCColor("rgbi:1/0.5/0"),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0xff8000);
        check_int("osc color invalid",
                  ParseTerminalPaneOSCColor("rgb:zz/00/00"),
                  TERMINAL_PANE_COLOR_DEFAULT);
        check_int("osc default palette clamps low",
                  TerminalPaneDefaultPaletteColor(-10),
                  TerminalPaneDefaultPaletteColor(0));
        check_int("osc default palette clamps high",
                  TerminalPaneDefaultPaletteColor(999),
                  TerminalPaneDefaultPaletteColor(255));

        len = FormatTerminalPaneOSCColorResponse(
            response, (int)sizeof(response), 10,
            TERMINAL_PANE_COLOR_TRUE_RGB | 0x123456);
        check_int("osc color response len", len, 24);
        check_str("osc color response", response,
                  "\x1b]10;rgb:1212/3434/5656\a");

        len = FormatTerminalPaneOSCPaletteResponse(
            response, (int)sizeof(response), 4,
            TERMINAL_PANE_COLOR_TRUE_RGB | 0xabcdef);
        check_int("osc palette response len", len, 25);
        check_str("osc palette response", response,
                  "\x1b]4;4;rgb:abab/cdcd/efef\a");

        response[0] = 'x';
        check_int("osc invalid response",
                  FormatTerminalPaneOSCPaletteResponse(
                      response, (int)sizeof(response), 300,
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0xabcdef),
                  0);
        check_str("osc invalid response clears", response, "");

        TerminalPaneOSCTitleTargets("", &window, &icon);
        check_int("osc title empty targets window", window, 1);
        check_int("osc title empty targets icon", icon, 1);
        TerminalPaneOSCTitleTargets("1;2;99", &window, &icon);
        check_int("osc title parsed targets window", window, 1);
        check_int("osc title parsed targets icon", icon, 1);
        TerminalPaneOSCTitleTargets("1", &window, &icon);
        check_int("osc title icon target window", window, 0);
        check_int("osc title icon target icon", icon, 1);

        len = FormatTerminalPaneOSCTitleReport(
            title_report, (int)sizeof(title_report), 0,
            "Clean\a\x1bName\tOk");
        check_int("osc title report len", len, 17);
        check_str("osc title report", title_report,
                  "\x1b]lCleanName\tOk\x1b\\");
        title_report[0] = 'x';
        check_int("osc title report small",
                  FormatTerminalPaneOSCTitleReport(title_report, 4, 1,
                                                   "Too long"),
                  0);
        check_str("osc title report small clears", title_report, "");

        TerminalPaneOSCPushTitle((char *)title_stack, 2,
                                 (int)sizeof(title_stack[0]), &title_count,
                                 "first");
        TerminalPaneOSCPushTitle((char *)title_stack, 2,
                                 (int)sizeof(title_stack[0]), &title_count,
                                 "second");
        TerminalPaneOSCPushTitle((char *)title_stack, 2,
                                 (int)sizeof(title_stack[0]), &title_count,
                                 "third");
        check_int("osc title stack clamps", title_count, 2);
        check_str("osc title stack shifted", title_stack[0], "second");
        TerminalPaneOSCPopTitle((char *)title_stack, 2,
                                (int)sizeof(title_stack[0]), &title_count,
                                popped, (int)sizeof(popped));
        check_str("osc title stack pop", popped, "third");
        check_int("osc title stack count after pop", title_count, 1);

        check_int("osc file uri decode",
                  DecodeTerminalPaneOSCFileURIPath(
                      path, (int)sizeof(path),
                      "file://host/tmp/with%20space"),
                  1);
        check_str("osc file uri path", path, "/tmp/with space");
        check_int("osc file uri invalid",
                  DecodeTerminalPaneOSCFileURIPath(path, (int)sizeof(path),
                                                   "https://example.test"),
                  0);
        check_str("osc file uri invalid clears", path, "");
    }

    {
        char title[32];

        check_int("session title path",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "/home/wao/Projects/kapsule", "terminal"),
                  7);
        check_str("session title path text", title, "kapsule");

        check_int("session title path trailing slash",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "/home/wao/Projects/kapsule///", "terminal"),
                  7);
        check_str("session title path trailing text", title, "kapsule");

        check_int("session title host path",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "wao@omega:/mnt/storage/Projects/krait", "terminal"),
                  5);
        check_str("session title host path text", title, "krait");

        check_int("session title host home path",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "wao@omega:~/Projects/Kapsule Test", "terminal"),
                  12);
        check_str("session title host home text", title, "Kapsule Test");

        check_int("session title root",
                  FormatTerminalPaneSessionTitle(title, (int)sizeof(title),
                                                 "/", "terminal"),
                  1);
        check_str("session title root text", title, "/");

        check_int("session title fallback",
                  FormatTerminalPaneSessionTitle(title, (int)sizeof(title),
                                                 "", "terminal"),
                  8);
        check_str("session title fallback text", title, "terminal");

        check_int("session title truncates",
                  FormatTerminalPaneSessionTitle(title, 5, "/tmp/abcdef",
                                                 "terminal"),
                  4);
        check_str("session title truncates text", title, "abcd");
    }

    {
        char seq[64];
        TerminalPaneKeyMode mode = {0};

        check_int("terminal key plain codepoint",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 'a', 0,
                                              mode),
                  1);
        check_str("terminal key plain text", seq, "a");

        check_int("terminal key alt unicode",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 0x00e9,
                                              TERMINAL_PANE_MOD_ALT, mode),
                  3);
        check_str("terminal key alt unicode text", seq, "\x1b\xc3\xa9");

        check_int("terminal key ctrl c",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 'c',
                                              TERMINAL_PANE_MOD_CTRL, mode),
                  1);
        check_bytes("terminal key ctrl c bytes", seq, 1, "\x03", 1);

        check_int("terminal key ctrl space",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), ' ',
                                              TERMINAL_PANE_MOD_CTRL, mode),
                  1);
        check_bytes("terminal key ctrl space bytes", seq, 1, "\0", 1);

        mode.modify_other_keys = 1;
        check_int("terminal key modify ctrl",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 'c',
                                              TERMINAL_PANE_MOD_CTRL, mode),
                  10);
        check_str("terminal key modify ctrl text", seq, "\x1b[27;5;99~");

        check_int("terminal key modify mode one shift",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 'a',
                                              TERMINAL_PANE_MOD_SHIFT, mode),
                  1);
        check_str("terminal key modify mode one shift text", seq, "a");

        mode.modify_other_keys = 2;
        check_int("terminal key modify mode two shift",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 'a',
                                              TERMINAL_PANE_MOD_SHIFT, mode),
                  10);
        check_str("terminal key modify mode two shift text", seq,
                  "\x1b[27;2;97~");

        mode.modify_other_keys = 1;
        check_int("terminal enter modify",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_ENTER,
                                        TERMINAL_PANE_MOD_CTRL, mode),
                  10);
        check_str("terminal enter modify text", seq, "\x1b[27;5;13~");

        mode.modify_other_keys = 0;
        check_int("terminal shift tab",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_TAB,
                                        TERMINAL_PANE_MOD_SHIFT, mode),
                  3);
        check_str("terminal shift tab text", seq, "\x1b[Z");

        check_int("terminal f1",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_F1, 0, mode),
                  3);
        check_str("terminal f1 text", seq, "\x1bOP");

        check_int("terminal f5",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_F5, 0, mode),
                  5);
        check_str("terminal f5 text", seq, "\x1b[15~");

        check_int("terminal f12 ctrl",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_F12,
                                        TERMINAL_PANE_MOD_CTRL, mode),
                  7);
        check_str("terminal f12 ctrl text", seq, "\x1b[24;5~");

        check_int("terminal f13",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_F13, 0, mode),
                  6);
        check_str("terminal f13 text", seq, "\x1b[1;2P");

        check_int("terminal f24 ctrl",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_F24,
                                        TERMINAL_PANE_MOD_CTRL, mode),
                  7);
        check_str("terminal f24 ctrl text", seq, "\x1b[24;6~");

        mode.application_cursor_keys = 1;
        check_int("terminal application cursor",
                  EncodeTerminalPaneKey(seq, (int)sizeof(seq),
                                        TERMINAL_PANE_KEY_UP, 0, mode),
                  3);
        check_str("terminal application cursor text", seq, "\x1bOA");

        mode.application_keypad = 0;
        check_int("terminal keypad normal",
                  EncodeTerminalPaneKeypad(seq, (int)sizeof(seq), '7', mode),
                  1);
        check_str("terminal keypad normal text", seq, "7");

        mode.application_keypad = 1;
        check_int("terminal keypad application",
                  EncodeTerminalPaneKeypad(seq, (int)sizeof(seq), '7', mode),
                  3);
        check_str("terminal keypad application text", seq, "\x1bOw");

        check_int("terminal keypad enter application",
                  EncodeTerminalPaneKeypad(seq, (int)sizeof(seq), '\r',
                                           mode),
                  3);
        check_str("terminal keypad enter application text", seq, "\x1bOM");
    }

    {
        char seq[64];
        TerminalPaneMouseMode mouse = {1000, 0, 1, 0, 0};

        check_int("terminal mouse sgr press",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 4, 2,
                                          -1, -1, 1, 0, 0, mouse),
                  9);
        check_str("terminal mouse sgr press text", seq, "\x1b[<0;5;3M");

        check_int("terminal mouse sgr release",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 4, 2,
                                          -1, -1, 0, 0, 0, mouse),
                  9);
        check_str("terminal mouse sgr release text", seq, "\x1b[<0;5;3m");

        check_int("terminal mouse sgr ctrl wheel",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_WHEEL_DOWN, 4, 2,
                                          -1, -1, 1, 0,
                                          TERMINAL_PANE_MOD_CTRL, mouse),
                  10);
        check_str("terminal mouse sgr ctrl wheel text", seq, "\x1b[<81;5;3M");

        mouse.sgr = 0;
        check_int("terminal mouse x10 normal",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 4, 2,
                                          -1, -1, 1, 0, 0, mouse),
                  6);
        check_str("terminal mouse x10 normal text", seq, "\x1b[M %#");

        mouse.mode = 9;
        check_int("terminal mouse x10 suppress release",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 4, 2,
                                          -1, -1, 0, 0, 0, mouse),
                  0);
        check_int("terminal mouse x10 suppress wheel",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_WHEEL_DOWN, 4, 2,
                                          -1, -1, 1, 0, 0, mouse),
                  0);

        mouse.mode = 1000;
        mouse.urxvt = 1;
        check_int("terminal mouse urxvt",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 4, 2,
                                          -1, -1, 1, 0,
                                          TERMINAL_PANE_MOD_CTRL, mouse),
                  9);
        check_str("terminal mouse urxvt text", seq, "\x1b[48;5;3M");

        mouse.urxvt = 0;
        mouse.utf8 = 1;
        check_int("terminal mouse utf8",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 300, 2,
                                          -1, -1, 1, 0, 0, mouse),
                  7);
        check_str("terminal mouse utf8 text", seq, "\x1b[M \xc5\x8d#");

        mouse.utf8 = 0;
        mouse.pixels = 1;
        check_int("terminal mouse pixel",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_LEFT, 4, 2,
                                          80, 40, 1, 0, 0, mouse),
                  11);
        check_str("terminal mouse pixel text", seq, "\x1b[<0;81;41M");

        mouse.pixels = 0;
        mouse.mode = 1002;
        check_int("terminal mouse suppress hover",
                  EncodeTerminalPaneMouse(seq, (int)sizeof(seq),
                                          TERMINAL_PANE_MOUSE_RELEASE, 4, 2,
                                          -1, -1, 1, 1, 0, mouse),
                  0);
    }

    return 0;
}
