#ifndef TERMINAL_PANE_H
#define TERMINAL_PANE_H

#include "kryon_compat.generated.h"
#include "terminal.h"
#include "ui_tk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TerminalPaneColors {
    Color background;
    Color text;
    Color muted_text;
    Color selection;
    Color selection_text;
    Color cursor;
    Color link;
    Color border;
    Color scroll_indicator;
    Color scroll_indicator_text;
    Color bell_overlay;
    Color bell_border;
} TerminalPaneColors;

typedef struct TerminalPanePalette {
    Color ansi[256];
} TerminalPanePalette;

typedef enum TerminalPaneColor {
    TERMINAL_PANE_COLOR_DEFAULT = -1,
    TERMINAL_PANE_COLOR_TRUE_RGB = 0x01000000
} TerminalPaneColor;

typedef enum TerminalPaneCursorStyle {
    TERMINAL_PANE_CURSOR_DEFAULT = 0,
    TERMINAL_PANE_CURSOR_BLOCK = 1,
    TERMINAL_PANE_CURSOR_UNDERLINE = 2,
    TERMINAL_PANE_CURSOR_BAR = 3
} TerminalPaneCursorStyle;

typedef struct TerminalPaneProfileColors {
    int foreground;
    int background;
    int cursor;
    int selection_foreground;
    int selection_background;
} TerminalPaneProfileColors;

typedef struct TerminalPaneViewColors {
    Color foreground;
    Color background;
    Color cursor;
    Color selection_foreground;
    Color selection_background;
} TerminalPaneViewColors;

typedef struct TerminalPaneProfileState {
    int base_foreground;
    int base_background;
    int base_cursor;
    int base_selection_foreground;
    int base_selection_background;
    int foreground;
    int background;
    int cursor;
    int selection_foreground;
    int selection_background;
} TerminalPaneProfileState;

typedef struct TerminalPaneMetrics {
    int cols;
    int rows;
    int cell_width;
    int line_height;
    Rectangle content;
} TerminalPaneMetrics;

typedef enum TerminalPaneSelectionMode {
    TERMINAL_PANE_SELECTION_CHAR = 0,
    TERMINAL_PANE_SELECTION_WORD = 1,
    TERMINAL_PANE_SELECTION_LINE = 2
} TerminalPaneSelectionMode;

typedef struct TerminalPaneSelection {
    int active;
    int dragging;
    int mode;
    int start_row;
    int start_col;
    int end_row;
    int end_col;
    int anchor_row;
    int anchor_start_col;
    int anchor_end_col;
} TerminalPaneSelection;

typedef int (*TerminalPaneSelectionLineFn)(void *userdata, int row,
                                           char *out, int out_size);
typedef int (*TerminalPaneSelectionWrappedFn)(void *userdata, int row);

typedef struct TerminalPaneSearchMatch {
    int row;
    int col;
    int length;
} TerminalPaneSearchMatch;

typedef enum TerminalPaneKey {
    TERMINAL_PANE_KEY_ENTER = 1,
    TERMINAL_PANE_KEY_BACKSPACE,
    TERMINAL_PANE_KEY_TAB,
    TERMINAL_PANE_KEY_ESCAPE,
    TERMINAL_PANE_KEY_UP,
    TERMINAL_PANE_KEY_DOWN,
    TERMINAL_PANE_KEY_RIGHT,
    TERMINAL_PANE_KEY_LEFT,
    TERMINAL_PANE_KEY_HOME,
    TERMINAL_PANE_KEY_END,
    TERMINAL_PANE_KEY_PAGE_UP,
    TERMINAL_PANE_KEY_PAGE_DOWN,
    TERMINAL_PANE_KEY_DELETE,
    TERMINAL_PANE_KEY_INSERT,
    TERMINAL_PANE_KEY_F1,
    TERMINAL_PANE_KEY_F2,
    TERMINAL_PANE_KEY_F3,
    TERMINAL_PANE_KEY_F4,
    TERMINAL_PANE_KEY_F5,
    TERMINAL_PANE_KEY_F6,
    TERMINAL_PANE_KEY_F7,
    TERMINAL_PANE_KEY_F8,
    TERMINAL_PANE_KEY_F9,
    TERMINAL_PANE_KEY_F10,
    TERMINAL_PANE_KEY_F11,
    TERMINAL_PANE_KEY_F12,
    TERMINAL_PANE_KEY_F13,
    TERMINAL_PANE_KEY_F14,
    TERMINAL_PANE_KEY_F15,
    TERMINAL_PANE_KEY_F16,
    TERMINAL_PANE_KEY_F17,
    TERMINAL_PANE_KEY_F18,
    TERMINAL_PANE_KEY_F19,
    TERMINAL_PANE_KEY_F20,
    TERMINAL_PANE_KEY_F21,
    TERMINAL_PANE_KEY_F22,
    TERMINAL_PANE_KEY_F23,
    TERMINAL_PANE_KEY_F24
} TerminalPaneKey;

typedef enum TerminalPaneKeyModifier {
    TERMINAL_PANE_MOD_SHIFT = 1,
    TERMINAL_PANE_MOD_ALT = 2,
    TERMINAL_PANE_MOD_CTRL = 4
} TerminalPaneKeyModifier;

typedef enum TerminalPaneMouseButton {
    TERMINAL_PANE_MOUSE_LEFT = 0,
    TERMINAL_PANE_MOUSE_MIDDLE = 1,
    TERMINAL_PANE_MOUSE_RIGHT = 2,
    TERMINAL_PANE_MOUSE_RELEASE = 3,
    TERMINAL_PANE_MOUSE_WHEEL_UP = 64,
    TERMINAL_PANE_MOUSE_WHEEL_DOWN = 65
} TerminalPaneMouseButton;

typedef struct TerminalPaneKeyMode {
    int application_cursor_keys;
    int application_keypad;
    int modify_other_keys;
} TerminalPaneKeyMode;

typedef struct TerminalPaneMouseMode {
    int mode;
    int utf8;
    int sgr;
    int urxvt;
    int pixels;
} TerminalPaneMouseMode;

typedef struct TerminalPaneClipboard {
    UIClipboardBuffer *clipboard;
    int bracketed_paste;
    UIClipboardPasteWriteFn write_text;
    void *userdata;
} TerminalPaneClipboard;

typedef enum TerminalPaneClipboardAction {
    TERMINAL_PANE_CLIPBOARD_PASTE_TEXT,
    TERMINAL_PANE_CLIPBOARD_PASTE_CLIPBOARD,
    TERMINAL_PANE_CLIPBOARD_PASTE_PRIMARY,
    TERMINAL_PANE_CLIPBOARD_PASTE_PREFERRED,
    TERMINAL_PANE_CLIPBOARD_SYNC_FROM_HOST,
    TERMINAL_PANE_CLIPBOARD_FLUSH_TO_HOST
} TerminalPaneClipboardAction;

typedef enum TerminalPaneClipboardCommand {
    TERMINAL_PANE_CLIPBOARD_COMMAND_COPY_SELECTION,
    TERMINAL_PANE_CLIPBOARD_COMMAND_UPDATE_PRIMARY_SELECTION,
    TERMINAL_PANE_CLIPBOARD_COMMAND_SELECT_ALL,
    TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_TEXT,
    TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_CLIPBOARD,
    TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PRIMARY,
    TERMINAL_PANE_CLIPBOARD_COMMAND_PASTE_PREFERRED,
    TERMINAL_PANE_CLIPBOARD_COMMAND_SYNC_FROM_HOST,
    TERMINAL_PANE_CLIPBOARD_COMMAND_FLUSH_TO_HOST
} TerminalPaneClipboardCommand;

typedef struct TerminalPaneClipboardController {
    TerminalPaneClipboard clipboard;
    TerminalPaneSelection *selection;
    TerminalPaneSelectionLineFn line_text;
    TerminalPaneSelectionWrappedFn line_wrapped;
    void *userdata;
    int total_rows;
    int cols;
} TerminalPaneClipboardController;

typedef struct TerminalPane {
    Rectangle bounds;
    Terminal *terminal;
    int focused;
    int font_size;
    int padding;
    int show_cursor;
    int handle_input;
    TerminalPaneColors colors;
} TerminalPane;

typedef struct TerminalPaneResult {
    int focused;
    int cols;
    int rows;
    int wrote_input;
} TerminalPaneResult;

TerminalPaneColors GetTerminalPaneThemeColors(void);
TerminalPaneColors ResolveTerminalPaneThemeColors(TerminalPaneColors colors);
int TerminalPaneColorToRGB(Color color);
Color ResolveTerminalPaneColor(const TerminalPanePalette *palette, int value,
                               Color fallback);
Color ResolveTerminalPaneColorWithOverrides(
    const TerminalPanePalette *palette, const int *overrides, int value,
    Color fallback);
TerminalPaneViewColors ResolveTerminalPaneViewColors(
    const TerminalPanePalette *palette, const int *overrides,
    TerminalPaneProfileColors colors, TerminalPaneColors fallback);
TerminalPaneProfileColors
TerminalPaneProfileColorsFromTheme(TerminalPaneColors colors);
TerminalPaneProfileColors
ResolveTerminalPaneProfileColors(TerminalPaneProfileColors configured,
                                 TerminalPaneColors fallback);
void TerminalPaneProfileStateApplyNew(TerminalPaneProfileState *state,
                                      TerminalPaneProfileColors colors);
void TerminalPaneProfileStateSeedMissing(TerminalPaneProfileState *state,
                                         TerminalPaneProfileColors colors);
void TerminalPaneProfileStateSyncChanged(TerminalPaneProfileState *state,
                                         TerminalPaneProfileColors old_colors,
                                         TerminalPaneProfileColors new_colors);
int ParseTerminalPaneCursorStyle(const char *text, int fallback);
const char *TerminalPaneCursorStyleName(int style);
int ParseTerminalPaneProfileColor(const char *text, int *out);
int FormatTerminalPaneProfileColor(char *out, int out_size, int color);
int EscapeTerminalPaneText(char *out, int out_size, const char *text);
int UnescapeTerminalPaneText(char *out, int out_size, const char *text);
int ParseTerminalPaneOSCColor(const char *text);
int TerminalPaneDefaultPaletteColor(int index);
int FormatTerminalPaneOSCColorResponse(char *out, int out_size, int code,
                                       int color);
int FormatTerminalPaneOSCPaletteResponse(char *out, int out_size, int index,
                                         int color);
int CopyTerminalPaneOSCHyperlinkURL(char *out, int out_size, const char *url);
int CopyTerminalPaneOSCHyperlinkID(char *out, int out_size,
                                   const char *params);
int TerminalPaneOSCTitleTargets(const char *payload, int *window, int *icon);
int CopyTerminalPaneTitleText(char *out, int out_size, const char *title);
int FormatTerminalPaneOSCTitleReport(char *out, int out_size, int icon,
                                     const char *title);
void TerminalPaneOSCPushTitle(char *stack, int depth, int item_size,
                              int *count, const char *value);
void TerminalPaneOSCPopTitle(char *stack, int depth, int item_size,
                             int *count, char *value, int value_size);
int DecodeTerminalPaneOSCFileURIPath(char *out, int out_size,
                                     const char *uri);
static inline TerminalPanePalette GetTerminalPaneDefaultPalette(void)
{
    static const Color base16[16] = {
        {24, 24, 24, 255},     {205, 49, 49, 255},
        {13, 188, 121, 255},   {229, 229, 16, 255},
        {36, 114, 200, 255},   {188, 63, 188, 255},
        {17, 168, 205, 255},   {229, 229, 229, 255},
        {102, 102, 102, 255},  {241, 76, 76, 255},
        {35, 209, 139, 255},   {245, 245, 67, 255},
        {59, 142, 234, 255},   {214, 112, 214, 255},
        {41, 184, 219, 255},   {255, 255, 255, 255},
    };
    TerminalPanePalette palette = {0};
    int i;
    int r;
    int g;
    int b;

    for(i = 0; i < 16; i++)
        palette.ansi[i] = base16[i];
    i = 16;
    for(r = 0; r < 6; r++) {
        for(g = 0; g < 6; g++) {
            for(b = 0; b < 6; b++) {
                palette.ansi[i++] =
                    (Color){(unsigned char)(r == 0 ? 0 : 55 + r * 40),
                            (unsigned char)(g == 0 ? 0 : 55 + g * 40),
                            (unsigned char)(b == 0 ? 0 : 55 + b * 40),
                            255};
            }
        }
    }
    for(i = 232; i < 256; i++) {
        unsigned char gray = (unsigned char)(8 + (i - 232) * 10);

        palette.ansi[i] = (Color){gray, gray, gray, 255};
    }
    return palette;
}
TerminalPaneMetrics MeasureTerminalPane(Rectangle bounds, int font_size, int padding);
Rectangle TerminalPaneContentBounds(Rectangle bounds, int top_inset,
                                    int padding);
TerminalPaneMetrics MeasureTerminalPaneContent(Rectangle content,
                                               int font_size);
void TerminalPaneSelectionClear(TerminalPaneSelection *selection);
void TerminalPaneSelectionSetRange(TerminalPaneSelection *selection, int mode,
                                   int dragging, int start_row, int start_col,
                                   int end_row, int end_col);
void TerminalPaneSelectionBeginChar(TerminalPaneSelection *selection, int row,
                                    int col);
void TerminalPaneSelectionSelectLine(TerminalPaneSelection *selection,
                                     TerminalPaneSelectionLineFn line_text,
                                     void *userdata, int row);
void TerminalPaneSelectionSelectWord(TerminalPaneSelection *selection,
                                     TerminalPaneSelectionLineFn line_text,
                                     void *userdata, int row, int col);
void TerminalPaneSelectionSelectAll(TerminalPaneSelection *selection,
                                    int total_rows, int cols);
void TerminalPaneSelectionUpdateEnd(TerminalPaneSelection *selection,
                                    TerminalPaneSelectionLineFn line_text,
                                    void *userdata, int row, int col);
int TerminalPaneSelectionContains(const TerminalPaneSelection *selection,
                                  int row, int col);
int TerminalPaneSelectionCollectText(
    const TerminalPaneSelection *selection, TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata, char *buffer,
    int buffer_size);
int TerminalPaneSelectionUpdatePrimary(
    const TerminalPaneSelection *selection, TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata);
int TerminalPaneSelectionCopyToClipboard(
    const TerminalPaneSelection *selection, TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata,
    UIClipboardBuffer *clipboard);
int TerminalPaneSelectionEdgeScrollDelta(float mouse_y, float viewport_y,
                                         float viewport_height,
                                         float edge_size);
int TerminalPaneSelectionFirstVisibleRow(int total_rows, int visible_rows,
                                         int scroll_offset);
int TerminalPaneSelectionEdgeScrollRow(int first_visible_row, int visible_rows,
                                       int scroll_delta);
int TerminalPaneSearchLines(TerminalPaneSelectionLineFn line_text,
                            void *userdata, int total_rows,
                            const char *needle, int start_row, int start_col,
                            int direction, int wrap,
                            TerminalPaneSearchMatch *out);
int EncodeTerminalPaneCodepoint(char *out, int out_size,
                                unsigned int codepoint, int mods,
                                TerminalPaneKeyMode mode);
int EncodeTerminalPaneKey(char *out, int out_size, int key, int mods,
                          TerminalPaneKeyMode mode);
int EncodeTerminalPaneKeypad(char *out, int out_size, char key,
                             TerminalPaneKeyMode mode);
int EncodeTerminalPaneMouse(char *out, int out_size, int button, int col,
                            int row, int pixel_x, int pixel_y, int pressed,
                            int motion, int mods,
                            TerminalPaneMouseMode mode);
int TerminalPaneClipboardPasteText(TerminalPaneClipboard clipboard,
                                   const char *text);
int TerminalPaneClipboardPasteSource(TerminalPaneClipboard clipboard,
                                     UIClipboardSource source);
int TerminalPaneClipboardPasteClipboard(TerminalPaneClipboard clipboard);
int TerminalPaneClipboardPastePrimary(TerminalPaneClipboard clipboard);
int TerminalPaneClipboardPastePreferred(TerminalPaneClipboard clipboard);
int TerminalPaneClipboardSourceHasText(UIClipboardSource source);
int TerminalPaneClipboardSyncFromHost(TerminalPaneClipboard clipboard);
int TerminalPaneClipboardFlushToHost(TerminalPaneClipboard clipboard);
int TerminalPaneClipboardPerform(TerminalPaneClipboard clipboard,
                                 TerminalPaneClipboardAction action,
                                 const char *text);
int TerminalPaneClipboardPrimarySelectionAvailable(void);
int TerminalPaneClipboardPerformCommand(
    TerminalPaneClipboardController controller,
    TerminalPaneClipboardCommand command, const char *text);
int TerminalPaneClipboardUpdatePrimarySelection(
    TerminalPaneClipboard clipboard, const TerminalPaneSelection *selection,
    TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata);
int TerminalPaneClipboardCopySelection(
    TerminalPaneClipboard clipboard, const TerminalPaneSelection *selection,
    TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata);
int FormatTerminalPaneSessionTitle(char *out, int out_size, const char *text,
                                   const char *fallback);
int TerminalPaneHandleInput(Terminal *terminal);
TerminalPaneResult DrawTerminalPane(TerminalPane pane);

#ifdef __cplusplus
}
#endif

#endif
