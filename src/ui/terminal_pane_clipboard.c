#include "terminal_pane.h"

#include <stddef.h>

TerminalPaneClipboard
MakeTerminalPaneClipboard(UIClipboardBuffer *clipboard, int bracketed_paste,
                          UIClipboardPasteWriteFn write_text, void *userdata)
{
    TerminalPaneClipboard pane_clipboard = {0};

    pane_clipboard.clipboard = clipboard;
    pane_clipboard.bracketed_paste = bracketed_paste ? 1 : 0;
    pane_clipboard.write_text = write_text;
    pane_clipboard.userdata = userdata;
    return pane_clipboard;
}

int
TerminalPaneClipboardPasteText(TerminalPaneClipboard clipboard,
                               const char *text)
{
    if(clipboard.clipboard == NULL || clipboard.write_text == NULL ||
       text == NULL || text[0] == '\0')
        return 0;
    return WriteUIClipboardTextPaste(clipboard.clipboard, text,
                                     clipboard.bracketed_paste,
                                     clipboard.write_text,
                                     clipboard.userdata);
}

int
TerminalPaneClipboardPasteSource(TerminalPaneClipboard clipboard,
                                 UIClipboardSource source)
{
    if(clipboard.clipboard == NULL || clipboard.write_text == NULL)
        return 0;
    return WriteUIClipboardSourcePaste(clipboard.clipboard, source,
                                       clipboard.bracketed_paste,
                                       clipboard.write_text,
                                       clipboard.userdata);
}

int
TerminalPaneClipboardPasteClipboard(TerminalPaneClipboard clipboard)
{
    return TerminalPaneClipboardPasteSource(clipboard,
                                            UI_CLIPBOARD_SOURCE_CLIPBOARD);
}

int
TerminalPaneClipboardPastePrimary(TerminalPaneClipboard clipboard)
{
    return TerminalPaneClipboardPasteSource(clipboard,
                                            UI_CLIPBOARD_SOURCE_PRIMARY);
}

int
TerminalPaneClipboardPastePreferred(TerminalPaneClipboard clipboard)
{
    return TerminalPaneClipboardPasteSource(
        clipboard, UI_CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD);
}

int
TerminalPaneClipboardSourceHasText(UIClipboardSource source)
{
    return UIClipboardSourceHasText(source);
}

int
TerminalPaneClipboardPrimarySelectionAvailable(void)
{
    return TerminalPaneClipboardSourceHasText(UI_CLIPBOARD_SOURCE_PRIMARY);
}

int
TerminalPaneClipboardSyncFromHost(TerminalPaneClipboard clipboard)
{
    if(clipboard.clipboard == NULL)
        return 0;
    return SyncUIClipboardBufferFromHost(clipboard.clipboard);
}

int
TerminalPaneClipboardFlushToHost(TerminalPaneClipboard clipboard)
{
    if(clipboard.clipboard == NULL)
        return 0;
    return FlushUIClipboardBufferToHost(clipboard.clipboard);
}

int
TerminalPaneClipboardPerform(TerminalPaneClipboard clipboard,
                             TerminalPaneClipboardAction action,
                             const char *text)
{
    switch(action) {
    case TERMINAL_PANE_CLIPBOARD_PASTE_TEXT:
        return TerminalPaneClipboardPasteText(clipboard, text);
    case TERMINAL_PANE_CLIPBOARD_PASTE_CLIPBOARD:
        return TerminalPaneClipboardPasteClipboard(clipboard);
    case TERMINAL_PANE_CLIPBOARD_PASTE_PRIMARY:
        return TerminalPaneClipboardPastePrimary(clipboard);
    case TERMINAL_PANE_CLIPBOARD_PASTE_PREFERRED:
        return TerminalPaneClipboardPastePreferred(clipboard);
    case TERMINAL_PANE_CLIPBOARD_SYNC_FROM_HOST:
        (void)text;
        return TerminalPaneClipboardSyncFromHost(clipboard);
    case TERMINAL_PANE_CLIPBOARD_FLUSH_TO_HOST:
        (void)text;
        return TerminalPaneClipboardFlushToHost(clipboard);
    default:
        break;
    }
    return 0;
}

int
TerminalPaneClipboardUpdatePrimarySelection(
    TerminalPaneClipboard clipboard, const TerminalPaneSelection *selection,
    TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata)
{
    (void)clipboard;
    return TerminalPaneSelectionUpdatePrimary(selection, line_text,
                                              line_wrapped, userdata);
}

int
TerminalPaneClipboardCopySelection(
    TerminalPaneClipboard clipboard, const TerminalPaneSelection *selection,
    TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata)
{
    if(clipboard.clipboard == NULL)
        return 0;
    return TerminalPaneSelectionCopyToClipboard(
        selection, line_text, line_wrapped, userdata, clipboard.clipboard);
}

int
TerminalPaneClipboardPerformCommand(
    TerminalPaneClipboardController controller,
    TerminalPaneClipboardCommand command, const char *text)
{
    switch(command) {
    case TERMINAL_PANE_CLIPBOARD_COMMAND_COPY_SELECTION:
        return TerminalPaneClipboardCopySelection(
            controller.clipboard, controller.selection, controller.line_text,
            controller.line_wrapped, controller.userdata);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_UPDATE_PRIMARY_SELECTION:
        return TerminalPaneClipboardUpdatePrimarySelection(
            controller.clipboard, controller.selection, controller.line_text,
            controller.line_wrapped, controller.userdata);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_SELECT_ALL:
        if(controller.selection == NULL)
            return 0;
        TerminalPaneSelectionSelectAll(controller.selection,
                                       controller.total_rows,
                                       controller.cols);
        return controller.selection->active ? 1 : 0;
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT:
        return TerminalPaneClipboardPerform(
            controller.clipboard, TERMINAL_PANE_CLIPBOARD_PASTE_TEXT, text);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_CLIPBOARD:
        return TerminalPaneClipboardPerform(
            controller.clipboard, TERMINAL_PANE_CLIPBOARD_PASTE_CLIPBOARD,
            text);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PRIMARY:
        return TerminalPaneClipboardPerform(
            controller.clipboard, TERMINAL_PANE_CLIPBOARD_PASTE_PRIMARY,
            text);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PREFERRED:
        return TerminalPaneClipboardPerform(
            controller.clipboard, TERMINAL_PANE_CLIPBOARD_PASTE_PREFERRED,
            text);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_SYNC_FROM_HOST:
        return TerminalPaneClipboardPerform(
            controller.clipboard, TERMINAL_PANE_CLIPBOARD_SYNC_FROM_HOST,
            text);
    case TERMINAL_PANE_CLIPBOARD_COMMAND_FLUSH_TO_HOST:
        return TerminalPaneClipboardPerform(
            controller.clipboard, TERMINAL_PANE_CLIPBOARD_FLUSH_TO_HOST,
            text);
    default:
        break;
    }
    return 0;
}

int
TerminalPaneClipboardCommandWritesInput(TerminalPaneClipboardCommand command)
{
    switch(command) {
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT:
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_CLIPBOARD:
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PRIMARY:
    case TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PREFERRED:
        return 1;
    default:
        break;
    }
    return 0;
}

TerminalPaneClipboardCommandResult
TerminalPaneClipboardRunCommand(TerminalPaneClipboardController controller,
                                TerminalPaneClipboardCommand command,
                                const char *text)
{
    TerminalPaneClipboardCommandResult result = {0};

    result.performed =
        TerminalPaneClipboardPerformCommand(controller, command, text);
    result.wrote_input =
        result.performed ? TerminalPaneClipboardCommandWritesInput(command) : 0;
    return result;
}
