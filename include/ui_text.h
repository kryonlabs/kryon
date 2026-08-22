#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "kryon.h"

#define UI_TEXT_8 8
#define UI_TEXT_12 12
#define UI_TEXT_16 16
#define UI_TEXT_24 24
#define UI_TEXT_BASE_SIZE 16

typedef struct {
    int font_size;
    Color color;
    int italic;
    int selectable;
} TextStyle;

typedef struct {
    int id;                 /* stable positive identity across frames */
    const char *text;
    Rectangle bounds;
    int font_size;
    int line_gap;
    Color color;
} SelectableTextBlock;

Font GetUIFont(void);
int EnsureUIDefaultFont(void);
int RegisterUIFont(const char *name, Font font);
int RegisterUISmallFont(const char *name, Font font);
int RegisterUIFontSource(const char *name, const char *file_type,
                         const unsigned char *font_data, unsigned int font_size,
                         const int *codepoints, int codepoint_count);
int RegisterUIFixedFontSource(const char *name, const char *file_type,
                              const unsigned char *font_data,
                              unsigned int font_size,
                              const int *codepoints, int codepoint_count);
int RegisterUIFontFileSource(const char *name, const char *path,
                             const int *codepoints, int codepoint_count);
int UseUIFont(const char *name);
int PushUIFont(const char *name);
void PopUIFont(int token);
int UIFontHasGlyph(Font font, int codepoint);
Font LoadUIFontFromMemory(const char *file_type, const unsigned char *font_data, unsigned int font_size, int base_size);
Font LoadUIFontAsset(const char *path, int base_size);
void UnloadUIFont(Font *font);
void ClearUIFonts(void);
/* Print per-font rasterization stats to stderr. No-op without
 * KRYON_MEM_DEBUG (see kryon_mem.h). */
void UIFontMemoryReport(const char *tag);
int TextWidth(const char *text, int font_size);
int TextHeight(const char *text, int font_size);
int TextLineHeight(int font_size);
int ScaledTextWidth(const char *text, int scale);
Font GetUIFontForCodepoint(int codepoint, int font_size);
float GetUIFontScale(Font font, int font_size);
int PushTextSelectable(int selectable);
void PopTextSelectable(int token);
int TextBaselineY(const char *text, int box_y, int box_h, int font_size);
int ScaledTextBaselineY(const char *text, int box_y, int box_h, int scale);

#endif
