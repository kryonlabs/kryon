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
    check_int("clipboard source has no text",
              UIClipboardSourceHasText(UI_CLIPBOARD_SOURCE_CLIPBOARD), 0);

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

    return 0;
}
