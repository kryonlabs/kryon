#include "kryon.h"
#include "kry_inject.h"

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

static int
fixture_reflow_char_blank(const void *cell, void *userdata)
{
    (void)userdata;
    return cell == NULL || *(const char *)cell == ' ';
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
        char label[16];

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
        check_int("terminal scroll indicator hidden",
                  FormatTerminalPaneScrollIndicatorLabel(
                      label, (int)sizeof(label), 0),
                  0);
        check_str("terminal scroll indicator hidden text", label, "");
        check_int("terminal scroll indicator label",
                  FormatTerminalPaneScrollIndicatorLabel(
                      label, (int)sizeof(label), 42),
                  8);
        check_str("terminal scroll indicator label text", label, "42 lines");
    }

    {
        const char input[3][8] = {
            {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'},
            {'i', 'j', 'k', 'l', ' ', ' ', ' ', ' '},
            {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}};
        const unsigned char wrapped[3] = {1, 0, 0};
        char output[4][12];
        unsigned char output_wrapped[4];
        char blank = ' ';
        int cursor_col = -1;
        int cursor_row = -1;
        int row_count = -1;
        TerminalPaneReflowSpec spec;

        memset(&spec, 0, sizeof(spec));
        spec.input_cells = input;
        spec.input_wrapped = wrapped;
        spec.input_cols = 8;
        spec.input_rows = 3;
        spec.output_cells = output;
        spec.output_wrapped = output_wrapped;
        spec.output_cols = 12;
        spec.output_rows = 4;
        spec.cell_size = sizeof(char);
        spec.blank_cell = &blank;
        spec.is_blank = fixture_reflow_char_blank;
        spec.trim_blank_rows_after_cursor = 1;
        spec.cursor_input_col = 2;
        spec.cursor_input_row = 1;
        spec.cursor_output_col = &cursor_col;
        spec.cursor_output_row = &cursor_row;
        spec.output_row_count = &row_count;

        check_int("terminal reflow rows unwrap",
                  TerminalPaneReflowRows(&spec), 1);
        check_bytes("terminal reflow unwrapped line", output[0], 12,
                    "abcdefghijkl", 12);
        check_bytes("terminal reflow second blank", output[1], 12,
                    "            ", 12);
        check_int("terminal reflow unwrap row count", row_count, 1);
        check_int("terminal reflow unwrap cursor row", cursor_row, 0);
        check_int("terminal reflow unwrap cursor col", cursor_col, 10);
        check_int("terminal reflow unwrap flag", output_wrapped[0], 0);
    }

    {
        const char input[1][12] = {
            {'a', 'b', 'c', 'd', 'e', 'f',
             'g', 'h', 'i', 'j', 'k', 'l'}};
        char output[3][5];
        unsigned char output_wrapped[3];
        char blank = ' ';
        int cursor_col = -1;
        int cursor_row = -1;
        int row_count = -1;
        TerminalPaneReflowSpec spec;

        memset(&spec, 0, sizeof(spec));
        spec.input_cells = input;
        spec.input_cols = 12;
        spec.input_rows = 1;
        spec.output_cells = output;
        spec.output_wrapped = output_wrapped;
        spec.output_cols = 5;
        spec.output_rows = 3;
        spec.cell_size = sizeof(char);
        spec.blank_cell = &blank;
        spec.is_blank = fixture_reflow_char_blank;
        spec.cursor_input_col = 11;
        spec.cursor_input_row = 0;
        spec.cursor_output_col = &cursor_col;
        spec.cursor_output_row = &cursor_row;
        spec.output_row_count = &row_count;

        check_int("terminal reflow rows wrap",
                  TerminalPaneReflowRows(&spec), 1);
        check_bytes("terminal reflow wrap line 0", output[0], 5, "abcde",
                    5);
        check_bytes("terminal reflow wrap line 1", output[1], 5, "fghij",
                    5);
        check_bytes("terminal reflow wrap line 2", output[2], 5, "kl   ",
                    5);
        check_int("terminal reflow wrap flag 0", output_wrapped[0], 1);
        check_int("terminal reflow wrap flag 1", output_wrapped[1], 1);
        check_int("terminal reflow wrap flag 2", output_wrapped[2], 0);
        check_int("terminal reflow wrap row count", row_count, 3);
        check_int("terminal reflow wrap cursor row", cursor_row, 2);
        check_int("terminal reflow wrap cursor col", cursor_col, 1);
    }

    {
        const char *lines[] = {"open /tmp/ktrem-test.txt now",
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
                  TerminalPaneSelectionContains(&selection, 0, 23), 1);
        check_int("terminal selection excludes after word",
                  TerminalPaneSelectionContains(&selection, 0, 24), 0);
        check_int("terminal word selection text",
                  TerminalPaneSelectionCollectText(
                      &selection, fixture_line_text, fixture_line_wrapped,
                      &fixture, text, (int)sizeof(text)),
                  1);
        check_str("terminal word selection value", text,
                  "/tmp/ktrem-test.txt");

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
                  "open /tmp/ktrem-test.txt");

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
            TerminalPaneClipboard pane_clipboard;

            InitUIClipboardBuffer(&clipboard, "");
            pane_clipboard =
                MakeTerminalPaneClipboard(&clipboard, 0, NULL, NULL);
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
            check_int("terminal pane clipboard selection primary",
                      TerminalPaneClipboardUpdatePrimarySelection(
                          pane_clipboard, &selection, fixture_line_text,
                          fixture_line_wrapped, &fixture),
                      1);
            check_str("terminal pane clipboard selection primary text",
                      GetUIPrimarySelectionTextValue(), "abcdefghijklmnopq");
            check_int("terminal pane clipboard selection copy",
                      TerminalPaneClipboardCopySelection(
                          pane_clipboard, &selection, fixture_line_text,
                          fixture_line_wrapped, &fixture),
                      1);
            check_str("terminal pane clipboard selection copy text",
                      GetUIClipboardBufferText(&clipboard),
                      "abcdefghijklmnopq");
            {
                TerminalPaneClipboardController controller =
                    MakeTerminalPaneClipboardController(
                        pane_clipboard, &selection, fixture_line_text,
                        fixture_line_wrapped, &fixture, fixture.count, 24,
                        NULL);
                char paste_capture[512] = "";
                int scroll_offset = 7;
                TerminalPaneClipboard session_clipboard =
                    MakeTerminalPaneClipboard(&clipboard, 0,
                                              capture_paste_write,
                                              paste_capture);
                TerminalPaneClipboardController session_controller =
                    MakeTerminalPaneClipboardController(
                        session_clipboard, NULL, NULL, NULL, NULL, 0, 0,
                        &scroll_offset);
                TerminalPaneClipboardActions actions =
                    MakeTerminalPaneClipboardActions(controller,
                                                     session_controller);

                SetUIPrimarySelectionTextValue("old primary");
                check_int("terminal pane command primary update",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_UPDATE_PRIMARY_SELECTION,
                              NULL),
                          1);
                check_str("terminal pane command primary text",
                          GetUIPrimarySelectionTextValue(),
                          "abcdefghijklmnopq");
                check_int("terminal pane command copy selection",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_COPY_SELECTION,
                              NULL),
                          1);
                check_str("terminal pane command copy text",
                          GetUIClipboardBufferText(&clipboard),
                          "abcdefghijklmnopq");
                TerminalPaneSelectionClear(&selection);
                check_int("terminal pane command select all",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_SELECT_ALL,
                              NULL),
                          1);
                check_int("terminal pane command select all active",
                          selection.active, 1);
                check_int("terminal pane command select all end row",
                          selection.end_row, fixture.count - 1);
                check_int("terminal pane command select all end col",
                          selection.end_col, 24);

                TerminalPaneSelectionSetRange(
                    &selection, TERMINAL_PANE_SELECTION_CHAR, 0, 2, 0, 3,
                    1);
                check_int("terminal pane actions collect selection",
                          TerminalPaneClipboardCollectSelectionText(
                              actions, text, (int)sizeof(text)),
                          1);
                check_str("terminal pane actions selection text", text,
                          "abcdefghijklmnopq");
                SetUIPrimarySelectionTextValue("old primary");
                check_int("terminal pane actions update primary",
                          TerminalPaneClipboardUpdatePrimary(actions), 1);
                check_str("terminal pane actions primary text",
                          GetUIPrimarySelectionTextValue(),
                          "abcdefghijklmnopq");
                check_int("terminal pane actions copy",
                          TerminalPaneClipboardCopy(actions), 1);
                check_str("terminal pane actions copy text",
                          GetUIClipboardBufferText(&clipboard),
                          "abcdefghijklmnopq");
                TerminalPaneSelectionClear(&selection);
                check_int("terminal pane actions select all",
                          TerminalPaneClipboardSelectAll(actions), 1);
                check_int("terminal pane actions select all active",
                          selection.active, 1);
                check_int("terminal pane actions paste text",
                          TerminalPaneClipboardPasteActionsText(actions,
                                                               "typed"),
                          5);
                check_str("terminal pane actions paste capture",
                          paste_capture, "typed");
                check_int("terminal pane actions paste scroll reset",
                          scroll_offset, 0);
            }

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

        {
            TerminalPaneSelection selection;
            TerminalPaneSearchStart start;
            TerminalPaneSearchController controller;
            int scroll_offset = 99;

            TerminalPaneSelectionClear(&selection);
            start = TerminalPaneSearchStartForDirection(&selection, 12, 4, 1);
            check_int("terminal search start visible row", start.row, 4);
            check_int("terminal search start visible col", start.col, 0);
            start = TerminalPaneSearchStartForDirection(&selection, 12, 4, -1);
            check_int("terminal search reverse start last row", start.row, 11);
            check_int("terminal search reverse start col", start.col, 4096);

            TerminalPaneSelectionSetRange(&selection,
                                          TERMINAL_PANE_SELECTION_CHAR, 0, 3,
                                          2, 5, 7);
            start = TerminalPaneSearchStartForDirection(&selection, 12, 4, 1);
            check_int("terminal search start selection end row", start.row, 5);
            check_int("terminal search start selection end col", start.col, 7);
            start = TerminalPaneSearchStartForDirection(&selection, 12, 4, -1);
            check_int("terminal search start selection begin row", start.row,
                      3);
            check_int("terminal search start selection begin col", start.col,
                      1);

            check_int("terminal search match scroll top",
                      TerminalPaneSearchMatchScrollOffset(20, 5, 0), 15);
            check_int("terminal search match scroll visible bottom",
                      TerminalPaneSearchMatchScrollOffset(20, 5, 19), 0);
            check_int("terminal search match scroll clamped",
                      TerminalPaneSearchMatchScrollOffset(20, 5, 40), 0);
            start = TerminalPaneSearchStartForDirection(NULL, 0, 4, 1);
            check_int("terminal search empty start row", start.row, -1);
            check_int("terminal search empty start col", start.col, -1);

            TerminalPaneSelectionClear(&selection);
            controller = MakeTerminalPaneSearchController(
                fixture_line_text, &fixture, &selection, fixture.count, 2, 1,
                &scroll_offset);
            check_int("terminal search controller initial",
                      TerminalPaneSearchFindInitial(controller, "needle",
                                                    &match),
                      1);
            check_int("terminal search controller initial row", match.row, 2);
            check_int("terminal search controller initial col", match.col, 14);
            check_int("terminal search controller selects", selection.active,
                      1);
            check_int("terminal search controller selection row",
                      selection.start_row, 2);
            check_int("terminal search controller selection col",
                      selection.start_col, 14);
            check_int("terminal search controller selection end",
                      selection.end_col, 20);
            check_int("terminal search controller scroll offset",
                      scroll_offset, 0);

            check_int("terminal search controller next wraps",
                      TerminalPaneSearchFindNext(controller, "alpha", 1,
                                                 &match),
                      1);
            check_int("terminal search controller next row", match.row, 0);
            check_int("terminal search controller next col", match.col, 0);
            check_int("terminal search controller next scroll",
                      scroll_offset, 2);
            check_int("terminal search controller previous",
                      TerminalPaneSearchFindNext(controller, "beta", -1,
                                                 &match),
                      1);
            check_int("terminal search controller previous row", match.row, 3);
            check_int("terminal search controller previous col", match.col, 6);
        }
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
            clipboard = MakeTerminalPaneClipboard(&buffer, 1,
                                                  capture_paste_write, paste);
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

            paste[0] = '\0';
            check_int("terminal pane action text paste",
                      TerminalPaneClipboardPerform(
                          clipboard, TERMINAL_PANE_CLIPBOARD_PASTE_TEXT,
                          "action text"),
                      11);
            check_str("terminal pane action text", paste, "action text");

            SetUIPrimarySelectionTextValue("action primary");
            paste[0] = '\0';
            check_int("terminal pane action preferred paste",
                      TerminalPaneClipboardPerform(
                          clipboard,
                          TERMINAL_PANE_CLIPBOARD_PASTE_PREFERRED, NULL),
                      14);
            check_str("terminal pane action preferred", paste,
                      "action primary");

            SetUIClipboardTextValue("action host");
            check_int("terminal pane action sync",
                      TerminalPaneClipboardPerform(
                          clipboard,
                          TERMINAL_PANE_CLIPBOARD_SYNC_FROM_HOST, NULL),
                      1);
            check_str("terminal pane action synced buffer",
                      GetUIClipboardBufferText(&buffer), "action host");

            RequestUIClipboardBufferWrite(&buffer, "action flush");
            check_int("terminal pane action flush",
                      TerminalPaneClipboardPerform(
                          clipboard,
                          TERMINAL_PANE_CLIPBOARD_FLUSH_TO_HOST, NULL),
                      1);
            check_str("terminal pane action flushed host",
                      GetUIClipboardTextValue(), "action flush");

            {
                int scroll_offset = 0;
                TerminalPaneClipboardController controller =
                    MakeTerminalPaneClipboardController(
                        clipboard, NULL, NULL, NULL, NULL, 0, 0,
                        &scroll_offset);

                paste[0] = '\0';
                check_int("terminal pane command text paste",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT,
                              "command text"),
                          12);
                check_str("terminal pane command text", paste,
                          "command text");
                check_int("terminal pane command paste writes input",
                          TerminalPaneClipboardCommandWritesInput(
                              TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT),
                          1);
                check_int("terminal pane command copy does not write input",
                          TerminalPaneClipboardCommandWritesInput(
                              TERMINAL_PANE_CLIPBOARD_COMMAND_COPY_SELECTION),
                          0);

                SetUIPrimarySelectionTextValue("command primary");
                paste[0] = '\0';
                check_int("terminal pane command primary paste",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PRIMARY,
                              NULL),
                          15);
                check_str("terminal pane command primary", paste,
                          "command primary");
                check_int("terminal pane primary available",
                          TerminalPaneClipboardPrimarySelectionAvailable(), 1);
                {
                    TerminalPaneClipboardCommandResult result;

                    scroll_offset = 9;
                    paste[0] = '\0';
                    result = TerminalPaneClipboardRunCommand(
                        controller,
                        TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT,
                        "result paste");
                    check_int("terminal pane run command performed",
                              result.performed, 12);
                    check_int("terminal pane run command wrote input",
                              result.wrote_input, 1);
                    check_int("terminal pane run command reset scroll",
                              scroll_offset, 0);
                    check_str("terminal pane run command text", paste,
                              "result paste");
                    scroll_offset = 11;
                    result = TerminalPaneClipboardRunCommand(
                        controller,
                        TERMINAL_PANE_CLIPBOARD_COMMAND_SYNC_FROM_HOST, NULL);
                    check_int("terminal pane run command sync performed",
                              result.performed, 1);
                    check_int("terminal pane run command sync input",
                              result.wrote_input, 0);
                    check_int("terminal pane run command sync keeps scroll",
                              scroll_offset, 11);
                    scroll_offset = 7;
                    paste[0] = '\0';
                    result = TerminalPaneClipboardRunSimpleCommand(
                        clipboard, &scroll_offset,
                        TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT,
                        "simple paste");
                    check_int("terminal pane simple command performed",
                              result.performed, 12);
                    check_int("terminal pane simple command wrote input",
                              result.wrote_input, 1);
                    check_int("terminal pane simple command reset scroll",
                              scroll_offset, 0);
                    check_str("terminal pane simple command text", paste,
                              "simple paste");
                }

                SetUIClipboardTextValue("command host");
                check_int("terminal pane command sync",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_SYNC_FROM_HOST,
                              NULL),
                          1);
                check_str("terminal pane command synced buffer",
                          GetUIClipboardBufferText(&buffer), "command host");

                RequestUIClipboardBufferWrite(&buffer, "command flush");
                check_int("terminal pane command flush",
                          TerminalPaneClipboardPerformCommand(
                              controller,
                              TERMINAL_PANE_CLIPBOARD_COMMAND_FLUSH_TO_HOST,
                              NULL),
                          1);
                check_str("terminal pane command flushed host",
                          GetUIClipboardTextValue(), "command flush");
            }
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
            {34, 35, 36, 255},
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
        TerminalPaneViewColors view_colors;
        TerminalPaneColors resolved_theme;
        TerminalPanePalette pane_palette;
        TerminalPaneProfileLimits limits;
        TerminalPaneProfileSettings settings;
        int color_overrides[256];
        char color_text[16];
        char sgr_text[80];
        int parsed_color = 0;
        int decoded_cursor_style = 99;
        int decoded_cursor_blink = 99;
        char escaped_text[64];
        char unescaped_text[64];

        check_int("terminal pane color rgb",
                  TerminalPaneColorToRGB((Color){0x11, 0x22, 0x33, 255}),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x112233);
        check_int("terminal pane invisible color default",
                  TerminalPaneColorToRGB((Color){0, 0, 0, 0}),
                  TERMINAL_PANE_COLOR_DEFAULT);
        resolved_theme = ResolveTerminalPaneThemeColors(
            (TerminalPaneColors){
                {0, 0, 0, 0},
                {40, 41, 42, 255},
                {0, 0, 0, 0},
                {43, 44, 45, 255},
                {0, 0, 0, 0},
                {46, 47, 48, 255},
                {0, 0, 0, 0},
                {49, 50, 51, 255},
                {0, 0, 0, 0},
                {52, 53, 54, 255},
                {0, 0, 0, 0},
                {55, 56, 57, 255}
            });
        check_int("terminal theme resolve background",
                  TerminalPaneColorToRGB(resolved_theme.background),
                  TerminalPaneColorToRGB(GetTerminalPaneThemeColors().background));
        check_int("terminal theme preserve text",
                  TerminalPaneColorToRGB(resolved_theme.text),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x28292a);
        check_int("terminal theme resolve link",
                  TerminalPaneColorToRGB(resolved_theme.link),
                  TerminalPaneColorToRGB(GetTerminalPaneThemeColors().link));
        check_int("terminal theme preserve border",
                  TerminalPaneColorToRGB(resolved_theme.border),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x313233);
        pane_palette = GetTerminalPaneDefaultPalette();
        memset(color_overrides, 0xff, sizeof(color_overrides));
        check_int("terminal pane true color resolve",
                  TerminalPaneColorToRGB(ResolveTerminalPaneColor(
                      &pane_palette, TERMINAL_PANE_COLOR_TRUE_RGB | 0x0a141e,
                      (Color){1, 2, 3, 255})),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x0a141e);
        check_int("terminal pane palette color resolve",
                  TerminalPaneColorToRGB(ResolveTerminalPaneColor(
                      &pane_palette, 1, (Color){1, 2, 3, 255})),
                  TerminalPaneColorToRGB(pane_palette.ansi[1]));
        check_int("terminal pane fallback color resolve",
                  TerminalPaneColorToRGB(ResolveTerminalPaneColor(
                      &pane_palette, TERMINAL_PANE_COLOR_DEFAULT,
                      (Color){1, 2, 3, 255})),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203);
        color_overrides[1] = TERMINAL_PANE_COLOR_TRUE_RGB | 0x223344;
        check_int("terminal pane overridden color resolve",
                  TerminalPaneColorToRGB(ResolveTerminalPaneColorWithOverrides(
                      &pane_palette, color_overrides, 1,
                      (Color){1, 2, 3, 255})),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x223344);
        color_overrides[2] = TERMINAL_PANE_COLOR_TRUE_RGB | 0x778899;
        view_colors = ResolveTerminalPaneViewColors(
            &pane_palette, color_overrides,
            (TerminalPaneProfileColors){
                1,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x445566,
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_DEFAULT,
                2
            },
            theme);
        check_int("terminal view foreground override",
                  TerminalPaneColorToRGB(view_colors.foreground),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x223344);
        check_int("terminal view background true color",
                  TerminalPaneColorToRGB(view_colors.background),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x445566);
        check_int("terminal view cursor follows foreground",
                  TerminalPaneColorToRGB(view_colors.cursor),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x223344);
        check_int("terminal view selection foreground follows background",
                  TerminalPaneColorToRGB(view_colors.selection_foreground),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x445566);
        check_int("terminal view selection background override",
                  TerminalPaneColorToRGB(view_colors.selection_background),
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x778899);
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

        limits = GetDefaultTerminalPaneProfileLimits();
        check_int("profile limits default font", limits.default_font_size, 16);
        check_int("profile limits max scrollback",
                  limits.max_scrollback_limit, 100000);
        InitTerminalPaneProfileSettings(&settings, limits);
        check_int("profile settings default font", settings.font_size, 16);
        check_int("profile settings default scrollback",
                  settings.scrollback_limit, 5000);
        check_int("profile settings default cursor", settings.cursor_style,
                  TERMINAL_PANE_CURSOR_BLOCK);
        check_int("profile settings default fg",
                  settings.terminal_foreground,
                  TERMINAL_PANE_COLOR_DEFAULT);
        check_int("profile settings apply font",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "font-size", "22"),
                  1);
        check_int("profile settings font value", settings.font_size, 22);
        check_int("profile settings reject small font",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "font-size", "4"),
                  0);
        check_int("profile settings font unchanged", settings.font_size, 22);
        check_int("profile settings apply scrollback",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "scrollback_limit", "9000"),
                  1);
        check_int("profile settings scrollback value",
                  settings.scrollback_limit, 9000);
        check_int("profile settings apply cursor style",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "cursor-style", "bar"),
                  1);
        check_int("profile settings cursor value", settings.cursor_style,
                  TERMINAL_PANE_CURSOR_BAR);
        check_int("profile settings apply shell",
                  ApplyTerminalPaneProfileSetting(&settings, limits, "shell",
                                                  "/bin/sh"),
                  1);
        check_str("profile settings shell value", settings.shell, "/bin/sh");
        check_int("profile settings apply cwd",
                  ApplyTerminalPaneProfileSetting(&settings, limits, "cwd",
                                                  "/tmp"),
                  1);
        check_str("profile settings cwd value", settings.working_directory,
                  "/tmp");
        check_int("profile settings apply command",
                  ApplyTerminalPaneProfileSetting(&settings, limits, "command",
                                                  "top"),
                  1);
        check_str("profile settings command value", settings.command, "top");
        check_int("profile settings apply font path",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "terminal-font",
                                                  "/tmp/font.ttf"),
                  1);
        check_str("profile settings font path", settings.terminal_font,
                  "/tmp/font.ttf");
        check_int("profile settings apply foreground",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "foreground", "#010203"),
                  1);
        check_int("profile settings foreground value",
                  settings.terminal_foreground,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203);
        check_int("profile settings apply cursor color",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "cursor-color", "default"),
                  1);
        check_int("profile settings cursor color value",
                  settings.terminal_cursor, TERMINAL_PANE_COLOR_DEFAULT);
        check_int("profile settings apply selection bg",
                  ApplyTerminalPaneProfileSetting(
                      &settings, limits, "selection-background", "040506"),
                  1);
        check_int("profile settings selection bg value",
                  settings.terminal_selection_background,
                  TERMINAL_PANE_COLOR_TRUE_RGB | 0x040506);
        check_int("profile settings reject unknown",
                  ApplyTerminalPaneProfileSetting(&settings, limits,
                                                  "unknown", "1"),
                  0);
        check_str("profile prompt shell title",
                  TerminalPaneProfilePromptTitle(
                      TERMINAL_PANE_PROFILE_PROMPT_SHELL),
                  "Shell");
        check_str("profile prompt foreground setting",
                  TerminalPaneProfilePromptSettingName(
                      TERMINAL_PANE_PROFILE_PROMPT_FOREGROUND),
                  "terminal-foreground");
        check_int("profile prompt colors flag",
                  TerminalPaneProfilePromptAffectsColors(
                      TERMINAL_PANE_PROFILE_PROMPT_CURSOR_COLOR),
                  1);
        check_int("profile prompt font flag",
                  TerminalPaneProfilePromptAffectsFont(
                      TERMINAL_PANE_PROFILE_PROMPT_TERMINAL_FONT),
                  1);
        check_int("profile prompt scrollback flag",
                  TerminalPaneProfilePromptAffectsScrollback(
                      TERMINAL_PANE_PROFILE_PROMPT_SCROLLBACK),
                  1);
        check_int("profile prompt format shell",
                  FormatTerminalPaneProfilePromptValue(
                      escaped_text, (int)sizeof(escaped_text), &settings,
                      TERMINAL_PANE_PROFILE_PROMPT_SHELL),
                  7);
        check_str("profile prompt shell value", escaped_text, "/bin/sh");
        check_int("profile prompt format default color",
                  FormatTerminalPaneProfilePromptValue(
                      escaped_text, (int)sizeof(escaped_text), &settings,
                      TERMINAL_PANE_PROFILE_PROMPT_CURSOR_COLOR),
                  7);
        check_str("profile prompt default color value", escaped_text,
                  "default");
        check_int("profile prompt apply font size",
                  ApplyTerminalPaneProfilePromptValue(
                      &settings, limits,
                      TERMINAL_PANE_PROFILE_PROMPT_FONT_SIZE, "24"),
                  1);
        check_int("profile prompt font size value", settings.font_size, 24);

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
        check_int("profile color index parse",
                  ParseTerminalPaneProfileColor("7", &parsed_color), 1);
        check_int("profile color index value", parsed_color, 7);
        parsed_color = 123;
        check_int("profile color invalid rejected",
                  ParseTerminalPaneProfileColor("zzzzzz", &parsed_color), 0);
        check_int("profile color invalid preserves out", parsed_color, 123);
        check_int("profile color null rejected",
                  ParseTerminalPaneProfileColor(NULL, &parsed_color), 0);
        {
            const char *cursor = "2;?;3;#445566;4;rgb:10/20/30";
            TerminalPaneOSCPaletteEntry entry;
            int reset_index = 99;

            check_int("osc palette query entry",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette query valid", entry.valid, 1);
            check_int("osc palette query index", entry.index, 2);
            check_int("osc palette query flag", entry.query, 1);
            check_int("osc palette color entry",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette color valid", entry.valid, 1);
            check_int("osc palette color index", entry.index, 3);
            check_int("osc palette color value", entry.color,
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x445566);
            check_int("osc palette rgb entry",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette rgb valid", entry.valid, 1);
            check_int("osc palette rgb index", entry.index, 4);
            check_int("osc palette rgb value", entry.color,
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x102030);
            check_int("osc palette entry end",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 0);

            cursor = "x;?;1abc;#000000;256;#000000;1;?";
            check_int("osc palette invalid alpha consumed",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette invalid alpha skipped", entry.valid, 0);
            check_int("osc palette invalid suffix consumed",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette invalid suffix skipped", entry.valid, 0);
            check_int("osc palette invalid range consumed",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette invalid range skipped", entry.valid, 0);
            check_int("osc palette valid after malformed",
                      NextTerminalPaneOSCPaletteEntry(&cursor, &entry), 1);
            check_int("osc palette valid after malformed flag",
                      entry.valid, 1);
            check_int("osc palette valid after malformed index",
                      entry.index, 1);

            cursor = "2;x;3";
            check_int("osc palette reset valid",
                      NextTerminalPaneOSCPaletteResetIndex(&cursor,
                                                           &reset_index),
                      1);
            check_int("osc palette reset index", reset_index, 2);
            check_int("osc palette reset invalid",
                      NextTerminalPaneOSCPaletteResetIndex(&cursor,
                                                           &reset_index),
                      1);
            check_int("osc palette reset invalid value", reset_index, -1);
            check_int("osc palette reset second valid",
                      NextTerminalPaneOSCPaletteResetIndex(&cursor,
                                                           &reset_index),
                      1);
            check_int("osc palette reset second index", reset_index, 3);
            check_int("osc palette reset end",
                      NextTerminalPaneOSCPaletteResetIndex(&cursor,
                                                           &reset_index),
                      0);
        }
        check_int("profile color format",
                  FormatTerminalPaneProfileColor(
                      color_text, (int)sizeof(color_text),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x0a0b0c),
                  7);
        check_str("profile color format text", color_text, "#0a0b0c");
        check_int("profile color format index",
                  FormatTerminalPaneProfileColor(color_text,
                                                 (int)sizeof(color_text), 7),
                  1);
        check_str("profile color format index text", color_text, "7");
        color_text[0] = 'x';
        check_int("profile color format default omitted",
                  FormatTerminalPaneProfileColor(
                      color_text, (int)sizeof(color_text),
                      TERMINAL_PANE_COLOR_DEFAULT),
                  0);
        check_int("profile color format invalid omitted",
                  FormatTerminalPaneProfileColor(color_text,
                                                 (int)sizeof(color_text), 256),
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
        check_int("cursor style default report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_DEFAULT, 1),
                  0);
        check_int("cursor style blinking block report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_BLOCK, 1),
                  1);
        check_int("cursor style steady block report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_BLOCK, 0),
                  2);
        check_int("cursor style blinking underline report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_UNDERLINE, 1),
                  3);
        check_int("cursor style steady underline report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_UNDERLINE, 0),
                  4);
        check_int("cursor style blinking bar report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_BAR, 1),
                  5);
        check_int("cursor style steady bar report",
                  TerminalPaneCursorStyleReportCode(
                      TERMINAL_PANE_CURSOR_BAR, 0),
                  6);
        check_int("cursor style decode default",
                  DecodeTerminalPaneCursorStyleRequest(
                      0, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode default style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_DEFAULT);
        check_int("cursor style decode default blink",
                  decoded_cursor_blink, 1);
        check_int("cursor style decode block",
                  DecodeTerminalPaneCursorStyleRequest(
                      1, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode block style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_BLOCK);
        check_int("cursor style decode block blink",
                  decoded_cursor_blink, 1);
        check_int("cursor style decode steady block",
                  DecodeTerminalPaneCursorStyleRequest(
                      2, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode steady block style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_BLOCK);
        check_int("cursor style decode steady block blink",
                  decoded_cursor_blink, 0);
        check_int("cursor style decode underline",
                  DecodeTerminalPaneCursorStyleRequest(
                      3, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode underline style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_UNDERLINE);
        check_int("cursor style decode underline blink",
                  decoded_cursor_blink, 1);
        check_int("cursor style decode steady underline",
                  DecodeTerminalPaneCursorStyleRequest(
                      4, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode steady underline style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_UNDERLINE);
        check_int("cursor style decode steady underline blink",
                  decoded_cursor_blink, 0);
        check_int("cursor style decode bar",
                  DecodeTerminalPaneCursorStyleRequest(
                      5, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode bar style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_BAR);
        check_int("cursor style decode bar blink",
                  decoded_cursor_blink, 1);
        check_int("cursor style decode steady bar",
                  DecodeTerminalPaneCursorStyleRequest(
                      6, &decoded_cursor_style, &decoded_cursor_blink),
                  1);
        check_int("cursor style decode steady bar style",
                  decoded_cursor_style, TERMINAL_PANE_CURSOR_BAR);
        check_int("cursor style decode steady bar blink",
                  decoded_cursor_blink, 0);
        decoded_cursor_style = 99;
        decoded_cursor_blink = 99;
        check_int("cursor style decode invalid",
                  DecodeTerminalPaneCursorStyleRequest(
                      7, &decoded_cursor_style, &decoded_cursor_blink),
                  0);
        check_int("cursor style decode invalid preserves style",
                  decoded_cursor_style, 99);
        check_int("cursor style decode invalid preserves blink",
                  decoded_cursor_blink, 99);
        check_int("sgr status default",
                  FormatTerminalPaneSGRStatus(
                      sgr_text, (int)sizeof(sgr_text),
                      (TerminalPaneSGRStatus){
                          0,
                          TERMINAL_PANE_COLOR_DEFAULT,
                          TERMINAL_PANE_COLOR_DEFAULT,
                          TERMINAL_PANE_COLOR_DEFAULT,
                      }),
                  2);
        check_str("sgr status default text", sgr_text, "0m");
        check_int("sgr status mixed",
                  FormatTerminalPaneSGRStatus(
                      sgr_text, (int)sizeof(sgr_text),
                      (TerminalPaneSGRStatus){
                          TERMINAL_PANE_SGR_BOLD |
                              TERMINAL_PANE_SGR_UNDERLINE |
                              TERMINAL_PANE_SGR_OVERLINE,
                          1,
                          TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203,
                          14,
                      }),
                  33);
        check_str("sgr status mixed text", sgr_text,
                  "1;4;53;38;5;1;48;2;1;2;3;58;5;14m");
        sgr_text[0] = 'x';
        check_int("sgr status truncates safely",
                  FormatTerminalPaneSGRStatus(
                      sgr_text, 4,
                      (TerminalPaneSGRStatus){
                          TERMINAL_PANE_SGR_BOLD,
                          1,
                          TERMINAL_PANE_COLOR_DEFAULT,
                          TERMINAL_PANE_COLOR_DEFAULT,
                      }),
                  0);
        check_str("sgr status truncated empty", sgr_text, "");
    }

    {
        char response[80];
        char dcs_response[128];
        int len;
        int window;
        int icon;
        char title_report[80];
        char title_stack[2][12] = {{0}};
        int title_count = 0;
        char popped[12];
        char path[80];
        const char *osc_payload;
        int osc_code;

        check_int("osc command parse title",
                  ParseTerminalPaneOSCCommand("2;ktrem", &osc_code,
                                              &osc_payload),
                  1);
        check_int("osc command title code", osc_code, 2);
        check_str("osc command title payload", osc_payload, "ktrem");
        check_int("osc command parse reset",
                  ParseTerminalPaneOSCCommand("110", &osc_code,
                                              &osc_payload),
                  1);
        check_int("osc command reset code", osc_code, 110);
        check_str("osc command reset payload", osc_payload, "");
        check_int("osc command rejects empty",
                  ParseTerminalPaneOSCCommand("", &osc_code, &osc_payload),
                  0);
        check_int("osc command rejects alpha",
                  ParseTerminalPaneOSCCommand("x;Title", &osc_code,
                                              &osc_payload),
                  0);
        check_int("osc command rejects suffix",
                  ParseTerminalPaneOSCCommand("2x;Title", &osc_code,
                                              &osc_payload),
                  0);
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

        check_int("osc color target foreground",
                  TerminalPaneOSCColorTargetForCode(10),
                  TERMINAL_PANE_OSC_COLOR_FOREGROUND);
        check_int("osc color target selection foreground",
                  TerminalPaneOSCColorTargetForCode(19),
                  TERMINAL_PANE_OSC_COLOR_SELECTION_FOREGROUND);
        check_int("osc color target invalid",
                  TerminalPaneOSCColorTargetForCode(99),
                  TERMINAL_PANE_OSC_COLOR_INVALID);
        check_int("osc color reset target cursor",
                  TerminalPaneOSCColorTargetForResetCode(112),
                  TERMINAL_PANE_OSC_COLOR_CURSOR);
        check_int("osc color reset target invalid",
                  TerminalPaneOSCColorTargetForResetCode(99),
                  TERMINAL_PANE_OSC_COLOR_INVALID);
        {
            TerminalPaneOSCColorState state = {
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x112233,
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_DEFAULT,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x010203,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x334455,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0xeeccaa,
                TERMINAL_PANE_COLOR_TRUE_RGB | 0x556677,
            };

            check_int("osc color query foreground fallback",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_FOREGROUND, state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc);
            check_int("osc color query mouse foreground fallback",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_MOUSE_FOREGROUND, state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0xaabbcc);
            state.foreground = TERMINAL_PANE_COLOR_TRUE_RGB | 0x102030;
            check_int("osc color query mouse foreground current",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_MOUSE_FOREGROUND, state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x102030);
            check_int("osc color query background current",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_BACKGROUND, state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x112233);
            check_int("osc color query cursor fallback",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_CURSOR, state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x334455);
            check_int("osc color query selection foreground fallback",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_SELECTION_FOREGROUND,
                          state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0xeeccaa);
            check_int("osc color query selection background fallback",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_SELECTION_BACKGROUND,
                          state),
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x556677);
            check_int("osc color query invalid",
                      TerminalPaneOSCColorQueryValue(
                          TERMINAL_PANE_OSC_COLOR_INVALID, state),
                      TERMINAL_PANE_COLOR_DEFAULT);
        }

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

        len = FormatTerminalPaneXTGETTCAPResponse(
            dcs_response, (int)sizeof(dcs_response), "+q544e");
        check_int("xtgettcap terminal name len", len,
                  (int)strlen("\x1bP1+r544e=787465726d2d323536636f6c6f72\x1b\\"));
        check_str("xtgettcap terminal name", dcs_response,
                  "\x1bP1+r544e=787465726d2d323536636f6c6f72\x1b\\");
        len = FormatTerminalPaneXTGETTCAPResponse(
            dcs_response, (int)sizeof(dcs_response),
            "+q436f;6b63757531;6b6635");
        check_int("xtgettcap color and key len", len,
                  (int)strlen("\x1bP1+r436f=323536;6b63757531=1b5b41;6b6635=1b5b31357e\x1b\\"));
        check_str("xtgettcap color and key", dcs_response,
                  "\x1bP1+r436f=323536;6b63757531=1b5b41;6b6635=1b5b31357e\x1b\\");
        len = FormatTerminalPaneXTGETTCAPResponse(
            dcs_response, (int)sizeof(dcs_response), "+q5858");
        check_int("xtgettcap invalid len", len,
                  (int)strlen("\x1bP0+r\x1b\\"));
        check_str("xtgettcap invalid", dcs_response, "\x1bP0+r\x1b\\");
        dcs_response[0] = 'x';
        check_int("xtgettcap non request",
                  FormatTerminalPaneXTGETTCAPResponse(
                      dcs_response, (int)sizeof(dcs_response), "$qm"),
                  0);
        check_str("xtgettcap non request clears", dcs_response, "");
        dcs_response[0] = 'x';
        check_int("xtgettcap truncates",
                  FormatTerminalPaneXTGETTCAPResponse(dcs_response, 8,
                                                       "+q544e"),
                  0);
        check_str("xtgettcap truncates clears", dcs_response, "");

        {
            TerminalPaneDCSBuffer dcs;

            InitTerminalPaneDCSBuffer(&dcs, 8);
            check_int("dcs append ascii",
                      AppendTerminalPaneDCSCodepoint(&dcs, 'q'), 1);
            check_int("dcs append utf8",
                      AppendTerminalPaneDCSCodepoint(&dcs, 0x00e9), 2);
            check_str("dcs buffer text",
                      GetTerminalPaneDCSBufferText(&dcs), "q\xc3\xa9");
            check_int("dcs buffer not ignored",
                      TerminalPaneDCSBufferIgnored(&dcs), 0);
            ResetTerminalPaneDCSBuffer(&dcs);
            check_str("dcs reset text",
                      GetTerminalPaneDCSBufferText(&dcs), "");
            check_int("dcs reset not ignored",
                      TerminalPaneDCSBufferIgnored(&dcs), 0);
            check_int("dcs append large",
                      AppendTerminalPaneDCSCodepoint(&dcs, 0x1f600), 4);
            check_int("dcs append over limit",
                      AppendTerminalPaneDCSCodepoint(&dcs, 0x1f600), 0);
            check_int("dcs ignored after over limit",
                      TerminalPaneDCSBufferIgnored(&dcs), 1);
            check_int("dcs ignored text",
                      GetTerminalPaneDCSBufferText(&dcs) == NULL, 1);
            FreeTerminalPaneDCSBuffer(&dcs);
        }

        {
            TerminalPaneSixelImage image;

            check_int("sixel decode rows",
                      DecodeTerminalPaneSixel(
                          &image, "q#1;2;100;0;0!3~-$#2;2;0;100;0!2~",
                          TERMINAL_PANE_COLOR_TRUE_RGB, NULL, NULL),
                      2);
            check_int("sixel decode width", image.width, 3);
            check_int("sixel decode height", image.height, 12);
            check_int("sixel decode aspect num", image.pixel_aspect_num, 1);
            check_int("sixel decode aspect den", image.pixel_aspect_den, 1);
            check_int("sixel decode red pixel", image.pixels[0],
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0xff0000);
            check_int("sixel decode red lower pixel",
                      image.pixels[5 * image.width + 2],
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0xff0000);
            check_int("sixel decode green pixel",
                      image.pixels[6 * image.width],
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x00ff00);
            check_int("sixel decode transparent background",
                      image.pixels[6 * image.width + 2],
                      TERMINAL_PANE_COLOR_DEFAULT);
            FreeTerminalPaneSixelImage(&image);

            check_int("sixel raster background",
                      DecodeTerminalPaneSixel(
                          &image, "0;2q\"1;1;4;6#3;1;120;50;100?",
                          TERMINAL_PANE_COLOR_TRUE_RGB, NULL, NULL),
                      1);
            check_int("sixel raster width", image.width, 4);
            check_int("sixel raster height", image.height, 6);
            check_int("sixel raster background pixel", image.pixels[0],
                      TERMINAL_PANE_COLOR_TRUE_RGB);
            FreeTerminalPaneSixelImage(&image);

            check_int("sixel aspect rows",
                      DecodeTerminalPaneSixel(
                          &image, "q\"2;1;2;6#4;2;0;0;100~~",
                          TERMINAL_PANE_COLOR_TRUE_RGB, NULL, NULL),
                      1);
            check_int("sixel aspect width", image.width, 2);
            check_int("sixel aspect height", image.height, 6);
            check_int("sixel aspect num", image.pixel_aspect_num, 2);
            check_int("sixel aspect den", image.pixel_aspect_den, 1);
            check_int("sixel aspect blue pixel", image.pixels[0],
                      TERMINAL_PANE_COLOR_TRUE_RGB | 0x0000ff);
            FreeTerminalPaneSixelImage(&image);

            check_int("sixel missing q",
                      DecodeTerminalPaneSixel(&image, "not sixel",
                                              TERMINAL_PANE_COLOR_TRUE_RGB,
                                              NULL, NULL),
                      0);
            check_int("sixel malformed repeat",
                      DecodeTerminalPaneSixel(&image, "q!12",
                                              TERMINAL_PANE_COLOR_TRUE_RGB,
                                              NULL, NULL),
                      0);
        }

        check_int("osc hyperlink url sanitizer",
                  CopyTerminalPaneOSCHyperlinkURL(
                      title_report, (int)sizeof(title_report),
                      "https://example.test/a\nb\x1b" "c\x7f"),
                  1);
        check_str("osc hyperlink url sanitizer text", title_report,
                  "https://example.test/abc");
        check_int("osc hyperlink url sanitizer empty",
                  CopyTerminalPaneOSCHyperlinkURL(
                      title_report, (int)sizeof(title_report), "\a\x1b\n"),
                  0);
        check_str("osc hyperlink url sanitizer empty text", title_report, "");
        check_int("osc hyperlink id sanitizer",
                  CopyTerminalPaneOSCHyperlinkID(
                      title_report, (int)sizeof(title_report),
                      "foo=bar:id=link\n42\x1b" ":end=x;ignored"),
                  1);
        check_str("osc hyperlink id sanitizer text", title_report, "link42");
        check_int("osc hyperlink id missing",
                  CopyTerminalPaneOSCHyperlinkID(
                      title_report, (int)sizeof(title_report),
                      "foo=bar;id=ignored"),
                  0);
        check_str("osc hyperlink id missing text", title_report, "");

        TerminalPaneOSCTitleTargets("", &window, &icon);
        check_int("osc title empty targets window", window, 1);
        check_int("osc title empty targets icon", icon, 1);
        TerminalPaneOSCTitleTargets("1;2;99", &window, &icon);
        check_int("osc title parsed targets window", window, 1);
        check_int("osc title parsed targets icon", icon, 1);
        TerminalPaneOSCTitleTargets("1", &window, &icon);
        check_int("osc title icon target window", window, 0);
        check_int("osc title icon target icon", icon, 1);
        TerminalPaneOSCTitleTargets("x", &window, &icon);
        check_int("osc title invalid target window", window, 0);
        check_int("osc title invalid target icon", icon, 0);
        TerminalPaneOSCTitleTargets("1x;2", &window, &icon);
        check_int("osc title mixed invalid target window", window, 1);
        check_int("osc title mixed invalid target icon", icon, 0);

        check_int("osc title sanitizer",
                  CopyTerminalPaneTitleText(title_report,
                                            (int)sizeof(title_report),
                                            "Bad\nTitle\tOK\x1bno\a"),
                  1);
        check_str("osc title sanitizer text", title_report,
                  "BadTitle\tOKno");
        check_int("osc title sanitizer empty",
                  CopyTerminalPaneTitleText(title_report,
                                            (int)sizeof(title_report),
                                            "\a\x1b\n"),
                  1);
        check_str("osc title sanitizer empty text", title_report, "");
        title_report[0] = 'x';
        check_int("osc title sanitizer small",
                  CopyTerminalPaneTitleText(title_report, 4,
                                            "Too long"),
                  0);
        check_str("osc title sanitizer small clears", title_report, "");

        len = FormatTerminalPaneOSCTitleReport(
            title_report, (int)sizeof(title_report), 0,
            "Clean\a\x1bName\tOk");
        check_int("osc title report len", len, 17);
        check_str("osc title report", title_report,
                  "\x1b]lCleanName\tOk\x1b\\");
        len = FormatTerminalPaneOSCTitleReport(
            title_report, (int)sizeof(title_report), 0,
            "\a\x1b\n");
        check_int("osc title report empty len", len, 5);
        check_str("osc title report empty", title_report, "\x1b]l\x1b\\");
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
                      "/home/wao/Projects/ktrem", "terminal"),
                  5);
        check_str("session title path text", title, "ktrem");

        check_int("session title path trailing slash",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "/home/wao/Projects/ktrem///", "terminal"),
                  5);
        check_str("session title path trailing text", title, "ktrem");

        check_int("session title host path",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "wao@omega:/mnt/storage/Projects/tools", "terminal"),
                  5);
        check_str("session title host path text", title, "tools");

        check_int("session title host home path",
                  FormatTerminalPaneSessionTitle(
                      title, (int)sizeof(title),
                      "wao@omega:~/Projects/ktrem Test", "terminal"),
                  10);
        check_str("session title host home text", title, "ktrem Test");

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

        {
            char line[8192];
            TerminalPaneSessionRecord record = {0};
            TerminalPaneSessionRecord parsed = {0};
            int active = -1;

            snprintf(record.cwd, sizeof(record.cwd), "%s",
                     "/tmp/with\ttab");
            snprintf(record.shell, sizeof(record.shell), "%s",
                     "/bin/sh");
            snprintf(record.title, sizeof(record.title), "%s",
                     "work\none");
            snprintf(record.command, sizeof(record.command), "%s",
                     "printf '\\t'");
            record.title_override = 1;
            record.scroll_offset = 42;

            check_int("session record format",
                      FormatTerminalPaneSessionRecord(
                          line, (int)sizeof(line), record) > 0,
                      1);
            check_int("session record parse",
                      ParseTerminalPaneSessionRecord(line, &parsed), 1);
            check_str("session record cwd", parsed.cwd, record.cwd);
            check_str("session record shell", parsed.shell, record.shell);
            check_str("session record title", parsed.title, record.title);
            check_str("session record command", parsed.command,
                      record.command);
            check_int("session record title override",
                      parsed.title_override, 1);
            check_int("session record scroll offset", parsed.scroll_offset,
                      42);
            check_int("session active parse",
                      ParseTerminalPaneSessionActive("active=3", &active), 1);
            check_int("session active value", active, 3);
            check_int("session active rejects tab",
                      ParseTerminalPaneSessionActive(line, &active), 0);
            check_int("session old record parse",
                      ParseTerminalPaneSessionRecord(
                          "tab\t/tmp\t/bin/sh\tstale", &parsed),
                      1);
            check_str("session old record title", parsed.title, "stale");
            check_int("session old record title override",
                      parsed.title_override, 0);
        }
    }

    {
        char seq[64];
        TerminalPaneKeyMode mode = {0};
        int used = 0;

        check_int("terminal key plain codepoint",
                  EncodeTerminalPaneCodepoint(seq, (int)sizeof(seq), 'a', 0,
                                              mode),
                  1);
        check_str("terminal key plain text", seq, "a");

        seq[0] = '\0';
        check_int("terminal utf8 append ascii",
                  AppendTerminalPaneUTF8Codepoint(seq, (int)sizeof(seq),
                                                  &used, 'A'),
                  1);
        check_int("terminal utf8 append accent",
                  AppendTerminalPaneUTF8Codepoint(seq, (int)sizeof(seq),
                                                  &used, 0x0301),
                  1);
        check_int("terminal utf8 append emoji",
                  AppendTerminalPaneUTF8Codepoint(seq, (int)sizeof(seq),
                                                  &used, 0x1f600),
                  1);
        check_str("terminal utf8 appended text", seq,
                  "A\xcc\x81\xf0\x9f\x98\x80");
        check_int("terminal utf8 rejects invalid",
                  AppendTerminalPaneUTF8Codepoint(seq, (int)sizeof(seq),
                                                  &used, 0x110000),
                  0);
        check_str("terminal utf8 invalid keeps text", seq,
                  "A\xcc\x81\xf0\x9f\x98\x80");

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

        {
            TerminalPaneMappedKey mapped =
                MapTerminalPaneFunctionKey(1, 0);
            check_int("terminal map f1 key", mapped.key,
                      TERMINAL_PANE_KEY_F1);
            check_int("terminal map f1 mods", mapped.mods, 0);

            mapped = MapTerminalPaneFunctionKey(
                1, TERMINAL_PANE_MOD_SHIFT);
            check_int("terminal map shift f1 key", mapped.key,
                      TERMINAL_PANE_KEY_F13);
            check_int("terminal map shift f1 mods", mapped.mods, 0);

            mapped = MapTerminalPaneFunctionKey(
                12, TERMINAL_PANE_MOD_SHIFT | TERMINAL_PANE_MOD_CTRL);
            check_int("terminal map ctrl shift f12 key", mapped.key,
                      TERMINAL_PANE_KEY_F24);
            check_int("terminal map ctrl shift f12 mods", mapped.mods,
                      TERMINAL_PANE_MOD_CTRL);

            mapped = MapTerminalPaneFunctionKey(
                24, TERMINAL_PANE_MOD_CTRL);
            check_int("terminal map physical f24 key", mapped.key,
                      TERMINAL_PANE_KEY_F24);
            check_int("terminal map physical f24 mods", mapped.mods,
                      TERMINAL_PANE_MOD_CTRL);

            mapped = MapTerminalPaneFunctionKey(
                0, TERMINAL_PANE_MOD_ALT);
            check_int("terminal map invalid key", mapped.key, 0);
            check_int("terminal map invalid mods", mapped.mods,
                      TERMINAL_PANE_MOD_ALT);
        }

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
        char out[512] = {0};
        TerminalPaneKeyMode mode = {0};
        TerminalPaneInputState state = {0};
        TerminalPaneInput input =
            MakeTerminalPaneInput(mode, capture_paste_write, out, &state);

        InjectReset();
        InjectText("ab");
        InjectPump();
        check_int("terminal input text wrote",
                  PumpTerminalPaneKeyboardInput(input), 1);
        check_str("terminal input text", out, "ab");

        InjectReset();
        InjectLayoutKeyTap('T');
        InjectPump();
        check_int("layout key pressed normalizes uppercase",
                  IsLayoutKeyPressed('t'), 1);
        InjectPump();
        check_int("layout key released normalizes uppercase",
                  IsLayoutKeyReleased('t'), 1);

        out[0] = '\0';
        InjectReset();
        InjectKeyTap(KEY_F5);
        InjectPump();
        check_int("terminal input function wrote",
                  PumpTerminalPaneKeyboardInput(input), 1);
        check_str("terminal input function", out, "\x1b[15~");

        out[0] = '\0';
        InjectReset();
        InjectKey(KEY_LEFT_CONTROL, 1);
        InjectKeyTap(KEY_C);
        InjectPump();
        check_int("terminal input ctrl c wrote",
                  PumpTerminalPaneKeyboardInput(input), 1);
        check_bytes("terminal input ctrl c", out, (int)strlen(out), "\x03",
                    1);
        InjectPump();
        check_int("terminal input ctrl c repeats once",
                  PumpTerminalPaneKeyboardInput(input), 0);
        InjectKey(KEY_LEFT_CONTROL, 0);
        InjectPump();
        (void)PumpTerminalPaneKeyboardInput(input);

        out[0] = '\0';
        InjectReset();
        InjectKey(KEY_LEFT_CONTROL, 1);
        InjectLayoutKeyTap('C');
        InjectPump();
        check_int("terminal input layout ctrl c wrote",
                  PumpTerminalPaneKeyboardInput(input), 1);
        check_bytes("terminal input layout ctrl c", out, (int)strlen(out),
                    "\x03", 1);
        InjectPump();
        check_int("terminal input layout ctrl c repeats once",
                  PumpTerminalPaneKeyboardInput(input), 0);
        InjectKey(KEY_LEFT_CONTROL, 0);
        InjectPump();
        (void)PumpTerminalPaneKeyboardInput(input);

        out[0] = '\0';
        mode.application_keypad = 1;
        input = MakeTerminalPaneInput(mode, capture_paste_write, out, &state);
        InjectReset();
        InjectKeyTap(KEY_KP_7);
        InjectPump();
        check_int("terminal input keypad wrote",
                  PumpTerminalPaneKeyboardInput(input), 1);
        check_str("terminal input keypad", out, "\x1bOw");
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

    {
        char report[32];
        TerminalPaneModeState state = {0};

        state.cursor_visible = 1;
        state.autowrap = 1;
        state.bracketed_paste = 1;
        state.mouse_mode = 1002;
        state.mouse_sgr = 1;
        state.insert_mode = 1;

        check_int("terminal mode set insert",
                  TerminalPaneModeStateSetMode(&state, 4, 0),
                  TERMINAL_PANE_MODE_ACTION_NONE);
        check_int("terminal mode set insert value", state.insert_mode, 0);
        check_int("terminal mode set newline",
                  TerminalPaneModeStateSetMode(&state, 20, 1),
                  TERMINAL_PANE_MODE_ACTION_NONE);
        check_int("terminal mode set newline value", state.newline_mode, 1);
        check_int("terminal private mode origin action",
                  TerminalPaneModeStateSetPrivateMode(&state, 6, 1),
                  TERMINAL_PANE_MODE_ACTION_ORIGIN_CURSOR);
        check_int("terminal private mode origin value", state.origin_mode, 1);

        state.mouse_mode = 0;
        check_int("terminal private mode mouse press set",
                  TerminalPaneModeStateSetPrivateMode(&state, 1000, 1),
                  TERMINAL_PANE_MODE_ACTION_NONE);
        check_int("terminal private mode mouse press value", state.mouse_mode,
                  1000);
        check_int("terminal private mode mouse button set",
                  TerminalPaneModeStateSetPrivateMode(&state, 1002, 1),
                  TERMINAL_PANE_MODE_ACTION_NONE);
        check_int("terminal private mode mouse button value", state.mouse_mode,
                  1002);
        check_int("terminal private mode inactive mouse reset",
                  TerminalPaneModeStateSetPrivateMode(&state, 1000, 0),
                  TERMINAL_PANE_MODE_ACTION_NONE);
        check_int("terminal private mode inactive mouse keeps value",
                  state.mouse_mode, 1002);
        check_int("terminal private mode active mouse reset",
                  TerminalPaneModeStateSetPrivateMode(&state, 1002, 0),
                  TERMINAL_PANE_MODE_ACTION_NONE);
        check_int("terminal private mode active mouse clears value",
                  state.mouse_mode, 0);

        state.mouse_utf8 = 0;
        state.mouse_sgr = 0;
        state.mouse_urxvt = 0;
        state.mouse_pixels = 0;
        (void)TerminalPaneModeStateSetPrivateMode(&state, 1005, 1);
        check_int("terminal private mode utf8 encoding", state.mouse_utf8, 1);
        (void)TerminalPaneModeStateSetPrivateMode(&state, 1006, 1);
        check_int("terminal private mode sgr clears utf8", state.mouse_utf8,
                  0);
        check_int("terminal private mode sgr encoding", state.mouse_sgr, 1);
        (void)TerminalPaneModeStateSetPrivateMode(&state, 1015, 1);
        check_int("terminal private mode urxvt clears sgr", state.mouse_sgr,
                  0);
        check_int("terminal private mode urxvt encoding", state.mouse_urxvt,
                  1);
        (void)TerminalPaneModeStateSetPrivateMode(&state, 1016, 1);
        check_int("terminal private mode pixel clears urxvt",
                  state.mouse_urxvt, 0);
        check_int("terminal private mode pixel encoding", state.mouse_pixels,
                  1);

        check_int("terminal private mode save cursor action",
                  TerminalPaneModeStateSetPrivateMode(&state, 1048, 1),
                  TERMINAL_PANE_MODE_ACTION_SAVE_CURSOR);
        check_int("terminal private mode restore cursor action",
                  TerminalPaneModeStateSetPrivateMode(&state, 1048, 0),
                  TERMINAL_PANE_MODE_ACTION_RESTORE_CURSOR);
        check_int("terminal private mode 1047 enter action",
                  TerminalPaneModeStateSetPrivateMode(&state, 1047, 1),
                  TERMINAL_PANE_MODE_ACTION_CLEAR_SCREEN);
        check_int("terminal private mode 1047 enter alternate",
                  state.alternate_screen, 1);
        check_int("terminal private mode 1047 leave action",
                  TerminalPaneModeStateSetPrivateMode(&state, 1047, 0),
                  TERMINAL_PANE_MODE_ACTION_CLEAR_ALTERNATE);
        check_int("terminal private mode 1047 leave alternate",
                  state.alternate_screen, 0);
        check_int("terminal private mode 1049 enter action",
                  TerminalPaneModeStateSetPrivateMode(&state, 1049, 1),
                  TERMINAL_PANE_MODE_ACTION_SAVE_CURSOR |
                      TERMINAL_PANE_MODE_ACTION_CLEAR_SCREEN);
        check_int("terminal private mode 1049 enter alternate",
                  state.alternate_screen, 1);
        check_int("terminal private mode 1049 leave action",
                  TerminalPaneModeStateSetPrivateMode(&state, 1049, 0),
                  TERMINAL_PANE_MODE_ACTION_CLEAR_ALTERNATE |
                      TERMINAL_PANE_MODE_ACTION_RESTORE_CURSOR);
        check_int("terminal private mode 1049 leave alternate",
                  state.alternate_screen, 0);

        state.bracketed_paste = 1;
        state.mouse_mode = 1002;
        state.mouse_sgr = 1;
        state.mouse_pixels = 0;
        state.insert_mode = 1;
        state.newline_mode = 0;

        check_int("terminal mode report insert status",
                  TerminalPaneModeReportStatus(state, 0, 4), 1);
        check_int("terminal mode report newline reset",
                  TerminalPaneModeReportStatus(state, 0, 20), 2);
        check_int("terminal mode report unknown normal",
                  TerminalPaneModeReportStatus(state, 0, 999), 0);
        check_int("terminal private mode cursor visible",
                  TerminalPaneModeReportStatus(state, 1, 25), 1);
        check_int("terminal private mode bracketed paste",
                  TerminalPaneModeReportStatus(state, 1, 2004), 1);
        check_int("terminal private mode mouse active",
                  TerminalPaneModeReportStatus(state, 1, 1002), 1);
        check_int("terminal private mode mouse inactive",
                  TerminalPaneModeReportStatus(state, 1, 1000), 2);
        check_int("terminal private mode alternate cursor",
                  TerminalPaneModeReportStatus(state, 1, 1048), 2);
        check_int("terminal private mode unknown",
                  TerminalPaneModeReportStatus(state, 1, 12345), 0);

        check_int("terminal mode report format",
                  FormatTerminalPaneModeReport(
                      report, (int)sizeof(report), state, 0, 4),
                  7);
        check_str("terminal mode report format text", report,
                  "\x1b[4;1$y");
        check_int("terminal private mode report format",
                  FormatTerminalPaneModeReport(
                      report, (int)sizeof(report), state, 1, 2004),
                  11);
        check_str("terminal private mode report format text", report,
                  "\x1b[?2004;1$y");
        report[0] = 'x';
        check_int("terminal mode report truncates",
                  FormatTerminalPaneModeReport(report, 4, state, 1, 2004),
                  0);
        check_str("terminal mode report truncates clears", report, "");

        check_int("terminal status report ready",
                  FormatTerminalPaneDeviceStatusReport(
                      report, (int)sizeof(report), 0, 5, 2, 4),
                  4);
        check_str("terminal status report ready text", report, "\x1b[0n");
        check_int("terminal status report cursor",
                  FormatTerminalPaneDeviceStatusReport(
                      report, (int)sizeof(report), 0, 6, 2, 4),
                  6);
        check_str("terminal status report cursor text", report, "\x1b[3;5R");
        check_int("terminal private status report cursor",
                  FormatTerminalPaneDeviceStatusReport(
                      report, (int)sizeof(report), 1, 6, 2, 4),
                  7);
        check_str("terminal private status report cursor text", report,
                  "\x1b[?3;5R");
        check_int("terminal private status report printer",
                  FormatTerminalPaneDeviceStatusReport(
                      report, (int)sizeof(report), 1, 15, 0, 0),
                  6);
        check_str("terminal private status report printer text", report,
                  "\x1b[?10n");
        check_int("terminal private status report locator",
                  FormatTerminalPaneDeviceStatusReport(
                      report, (int)sizeof(report), 1, 26, 0, 0),
                  12);
        check_str("terminal private status report locator text", report,
                  "\x1b[?27;1;0;0n");
        report[0] = 'x';
        check_int("terminal status report unsupported",
                  FormatTerminalPaneDeviceStatusReport(
                      report, (int)sizeof(report), 0, 15, 0, 0),
                  0);
        check_str("terminal status report unsupported clears", report, "");
        report[0] = 'x';
        check_int("terminal status report truncates",
                  FormatTerminalPaneDeviceStatusReport(report, 4, 1, 26,
                                                       0, 0),
                  0);
        check_str("terminal status report truncates clears", report, "");
    }

    return 0;
}
