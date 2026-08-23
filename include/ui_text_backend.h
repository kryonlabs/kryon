#ifndef UI_TEXT_BACKEND_H
#define UI_TEXT_BACKEND_H

/*
 * Text-backend seam.
 *
 * src/ui/ never reads Font/GlyphInfo struct fields directly. Instead it goes
 * through these accessors. The default implementation (src/ui/
 * ui_text_backend.c) is backend-neutral and forwards to the glyph accessors of
 * the kryon surface (GetGlyphInfo, GetGlyphAtlasRec); a backend that answers
 * those over its own font layout needs nothing extra here.
 *
 * Font stays a real struct (raylib-layout); only the atlas internals are
 * hidden behind this seam. Functions take Font by value to match the
 * surfaces own accessor signatures.
 */
#include "kryon.h"

/* True if `font` is loaded and ready to measure/draw with. */
int UIFontReady(Font font);

/* Base size of the font in pixels (font.baseSize), >= 0. */
int UIFontBaseSize(Font font);

/* Per-glyph padding of the font (font.glyphPadding), >= 0. */
int UIFontGlyphPadding(Font font);

/* Glyph info (offsetX/offsetY/advanceX/value) for a codepoint, falling back to
 * ? like raylibs GetGlyphInfo. */
GlyphInfo UIFontGlyph(Font font, int codepoint);

/* Atlas sub-rectangle for a codepoint, falling back to ? like raylibs
 * GetGlyphAtlasRec. */
Rectangle UIFontAtlasRec(Font font, int codepoint);

/* The atlas texture holding every glyph image (font.texture). Callers blit a
 * glyph with DrawTexturePro(atlas, UIFontAtlasRec(font, cp), dst, ...). */
Texture2D UIFontAtlasTexture(Font font);

/* Horizontal pen advance for a codepoint in font units
 * (glyphs[GetGlyphIndex].advanceX), fallback ? as raylib. */
int UIFontAdvance(Font font, int codepoint);

/* True if the font provides a glyph for `codepoint` (any glyph slot, not just
 * the loaded codepoint range). */
int UIFontHasGlyphValue(Font font, int codepoint);

/* Number of rasterized glyphs in the fonts atlas (font.glyphCount), >= 0.
 * Used by diagnostics; regular UI code should not need it. */
int UIFontGlyphCount(Font font);

/* Native backend text path. Returns false/0 on backends that only support
 * atlas-backed fonts. */
int UIFontHasNativeText(Font font);
int UIFontNativeTextWidth(Font font, const char *text, int byte_len);
int UIFontNativeTextHeight(Font font);
int UIFontDrawNativeText(Font font, const char *text, int byte_len, int x,
                         int y, int font_size, Color color);

#endif /* UI_TEXT_BACKEND_H */
