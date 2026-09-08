#include "ui_internal.h"

/* The UI has one text-focus owner, so retained and immediate editors share one
 * in-progress platform composition instead of carrying widget-specific
 * preedit buffers. */
typedef struct TextCompositionSession {
    const void *owner;
    int cursor;
    int selection_length;
    char text[KRY_TEXT_COMPOSITION_MAX];
} TextCompositionSession;

static TextCompositionSession text_composition;

int
ui_text_composition_cancel(const void *owner)
{
    if(text_composition.owner == NULL ||
       (owner != NULL && text_composition.owner != owner))
        return 0;
    memset(&text_composition, 0, sizeof(text_composition));
    return 1;
}

int
ui_text_composition_get(const void *owner, const char **text,
                        int *cursor, int *selection_length)
{
    if(owner == NULL || text_composition.owner != owner ||
       text_composition.text[0] == '\0')
        return 0;
    if(text != NULL)
        *text = text_composition.text;
    if(cursor != NULL)
        *cursor = text_composition.cursor;
    if(selection_length != NULL)
        *selection_length = text_composition.selection_length;
    return 1;
}

TextCompositionResult
ui_text_composition_apply(TextEdit edit, int *anchor, const void *owner,
                          int focused, int read_only, int allow_newlines)
{
    TextCompositionResult result = {0};
    KryTextCompositionEvent event;

    if(owner == NULL || edit.text == NULL || edit.text_size == 0 ||
       edit.cursor_position == NULL || anchor == NULL)
        return result;
    if(!focused || read_only) {
        result.presentation_changed = ui_text_composition_cancel(owner);
        if(focused && read_only)
            while(PollTextComposition(&event)) {}
        return result;
    }

    while(PollTextComposition(&event)) {
        if(event.phase == KRY_TEXT_COMPOSITION_START ||
           event.phase == KRY_TEXT_COMPOSITION_UPDATE) {
            text_composition.owner = owner;
            strncpy(text_composition.text, event.text,
                    sizeof(text_composition.text) - 1);
            text_composition.text[sizeof(text_composition.text) - 1] = '\0';
            text_composition.cursor = event.cursor;
            text_composition.selection_length = event.selection_length;
            result.presentation_changed = 1;
        } else if(event.phase == KRY_TEXT_COMPOSITION_COMMIT) {
            int start = *anchor < *edit.cursor_position
                ? *anchor : *edit.cursor_position;
            int end = *anchor > *edit.cursor_position
                ? *anchor : *edit.cursor_position;

            if(end > start)
                result.text_changed |= ui_text_delete_range(
                    edit.text, edit.text_size, edit.cursor_position,
                    start, end);
            result.text_changed |= ui_text_insert_text(
                edit.text, edit.text_size, edit.cursor_position, event.text,
                allow_newlines, edit.filter, edit.filter_user_data,
                edit.max_codepoints);
            *anchor = *edit.cursor_position;
            ui_text_composition_cancel(NULL);
            result.presentation_changed = 1;
            result.selection_changed = 1;
        } else if(event.phase == KRY_TEXT_COMPOSITION_CANCEL) {
            ui_text_composition_cancel(NULL);
            result.presentation_changed = 1;
        }
    }
    return result;
}
