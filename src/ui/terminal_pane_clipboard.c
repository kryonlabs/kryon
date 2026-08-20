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
