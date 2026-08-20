#include "ui_tk.h"

#include <stdio.h>
#include <string.h>

/* Large enough that the editor's source buffer (512 KiB) is the real ceiling,
 * not this. Matches the raylib SDL read buffer set via RAY_RAYLIB_CONFIG so
 * copy and paste caps stay symmetric. */
#define UI_TK_CLIPBOARD_MAX (1024 * 1024)

static char g_clipboard_text[UI_TK_CLIPBOARD_MAX];
static char g_primary_selection_text[UI_TK_CLIPBOARD_MAX];

int
SetUIClipboardTextValue(const char *text)
{
    if(text == NULL)
        text = "";
    snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text);
    SetClipboardText(g_clipboard_text);
    return 1;
}

const char *
GetUIClipboardTextValue(void)
{
    const char *text = GetClipboardText();

    if(text != NULL && text[0] != '\0')
        snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text);
    return g_clipboard_text;
}

int
SetUIPrimarySelectionTextValue(const char *text)
{
    if(text == NULL)
        text = "";
    snprintf(g_primary_selection_text, sizeof(g_primary_selection_text), "%s",
             text);
    return 1;
}

const char *
GetUIPrimarySelectionTextValue(void)
{
    return g_primary_selection_text;
}

int
UIClipboardTargetIncludes(const char *target, char wanted)
{
    int i;

    if(target == NULL || target[0] == '\0')
        return wanted == 'c';
    for(i = 0; target[i] != '\0' && target[i] != ';' &&
              target[i] != '\a' && target[i] != 0x1b;
        i++) {
        if(target[i] == wanted)
            return 1;
    }
    return 0;
}

int
UIClipboardTargetUsesPrimary(const char *target)
{
    return UIClipboardTargetIncludes(target, 'p') &&
           !UIClipboardTargetIncludes(target, 'c') &&
           !UIClipboardTargetIncludes(target, 's');
}

const char *
GetUIClipboardTargetText(const UIClipboardBuffer *clipboard,
                         const char *target)
{
    if(UIClipboardTargetUsesPrimary(target))
        return GetUIPrimarySelectionTextValue();
    return GetUIClipboardBufferText(clipboard);
}

int
RequestUIClipboardTargetWrite(UIClipboardBuffer *clipboard, const char *target,
                              const char *text)
{
    int changed = 0;
    int wrote = 0;

    if(UIClipboardTargetIncludes(target, 'p')) {
        changed |= SetUIPrimarySelectionTextValue(text);
        wrote = 1;
    }
    if(UIClipboardTargetIncludes(target, 'c') ||
       UIClipboardTargetIncludes(target, 's') || !wrote)
        changed |= RequestUIClipboardBufferWrite(clipboard, text);
    return changed;
}

static int
ui_clipboard_buffer_copy_text(char *dst, int dst_size, const char *text)
{
    int changed;

    if(dst == NULL || dst_size <= 0)
        return 0;
    if(text == NULL)
        text = "";
    changed = strcmp(dst, text) != 0;
    snprintf(dst, (size_t)dst_size, "%s", text);
    return changed;
}

void
InitUIClipboardBuffer(UIClipboardBuffer *buffer, const char *text)
{
    if(buffer == NULL)
        return;
    buffer->text[0] = '\0';
    buffer->pending = 0;
    (void)ui_clipboard_buffer_copy_text(buffer->text,
                                        (int)sizeof(buffer->text), text);
}

int
SetUIClipboardBufferText(UIClipboardBuffer *buffer, const char *text)
{
    int changed;

    if(buffer == NULL)
        return 0;
    changed = ui_clipboard_buffer_copy_text(buffer->text,
                                            (int)sizeof(buffer->text), text);
    buffer->pending = 0;
    return changed;
}

int
RequestUIClipboardBufferWrite(UIClipboardBuffer *buffer, const char *text)
{
    int changed;
    int was_pending;

    if(buffer == NULL)
        return 0;
    was_pending = buffer->pending;
    changed = ui_clipboard_buffer_copy_text(buffer->text,
                                            (int)sizeof(buffer->text), text);
    buffer->pending = 1;
    return changed || !was_pending;
}

const char *
GetUIClipboardBufferText(const UIClipboardBuffer *buffer)
{
    if(buffer == NULL)
        return "";
    return buffer->text;
}

int
UIClipboardBufferHasPendingWrite(const UIClipboardBuffer *buffer)
{
    return buffer != NULL && buffer->pending;
}

int
SyncUIClipboardBufferFromHost(UIClipboardBuffer *buffer)
{
    const char *text;

    if(buffer == NULL || buffer->pending)
        return 0;
    text = GetUIClipboardTextValue();
    return ui_clipboard_buffer_copy_text(buffer->text,
                                         (int)sizeof(buffer->text), text);
}

int
FlushUIClipboardBufferToHost(UIClipboardBuffer *buffer)
{
    if(buffer == NULL || !buffer->pending)
        return 0;
    SetUIClipboardTextValue(buffer->text);
    buffer->pending = 0;
    return 1;
}
