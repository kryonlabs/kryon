#ifndef KRYON_TEXT_H
#define KRYON_TEXT_H

#include "kryon.h"

#define Text8 8
#define Text12 12
#define Text14 14
#define Text16 16
#define Text18 18
#define Text20 20
#define Text24 24
#define Text32 32
#define Text48 48
#define TextBaseSize 16

Font GetTextFont(void);
int EnsureDefaultFont(void);
int RegisterTextFont(const char *name, Font font);
int RegisterSmallTextFont(const char *name, Font font);
int RegisterTextFontSource(const char *name, const char *file_type,
                         const unsigned char *font_data, unsigned int font_size,
                         const int *codepoints, int codepoint_count);
int RegisterTextFontSourceForText(const char *name, const char *file_type,
                                const unsigned char *font_data,
                                unsigned int font_size,
                                const char *text);
int RegisterFixedTextFontSource(const char *name, const char *file_type,
                              const unsigned char *font_data,
                              unsigned int font_size,
                              const int *codepoints, int codepoint_count);
int RegisterTextFontFileSource(const char *name, const char *path,
                             const int *codepoints, int codepoint_count);
int RegisterTextFontFileSourceForText(const char *name, const char *path,
                                    const char *text);
int UseTextFont(const char *name);
int PushTextFont(const char *name);
void PopTextFont(int token);
int TextFontHasGlyph(Font font, int codepoint);
Font LoadTextFontFromMemory(const char *file_type, const unsigned char *font_data, unsigned int font_size, int base_size);
Font LoadTextFontAsset(const char *path, int base_size);
void UnloadTextFont(Font *font);
void ClearTextFonts(void);
/* Print per-font rasterization stats to stderr. No-op without
 * KRYON_MEM_DEBUG (see kryon_mem.h). */
void TextFontMemoryReport(const char *tag);
int MeasureTextWidth(const char *text, int font_size, const char *typeface);
Font GetTextFontForCodepoint(int codepoint, int font_size);
float GetTextFontScale(Font font, int font_size);

#endif
