#include "terminal_pane.h"

#include <stddef.h>

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
