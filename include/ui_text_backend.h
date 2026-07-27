#ifndef UI_TEXT_BACKEND_H
#define UI_TEXT_BACKEND_H

/*
 * Text-backend seam.
 *
 * src/ui/ never reads Font/GlyphInfo struct fields directly. Instead it goes
 * through these accessors, which the active graphics backend implements. On the
 * raylib backend (src/backend/raylib_text_backend.c) they forward to raylib's
 * own glyph accessors; other backends answer them over their own font layout.
 *
 * Font stays a real struct (raylib-layout on the raylib backend); only the
 * atlas internals are hidden behind this seam. Functions take Font by value to
 * match raylib's own accessor signatures (GetGlyphInfo, GetGlyphAtlasRec).
 */
#include "kryon.h"

/* True if `font` is loaded and ready to measure/draw with. */
int UIFontReady(Font font);

/* Base size of the font in pixels (font.baseSize), >= 0. */
int UIFontBaseSize(Font font);

/* Per-glyph padding of the font (font.glyphPadding), >= 0. */
int UIFontGlyphPadding(Font font);

/* Glyph info (offsetX/offsetY/advanceX/value) for a codepoint, falling back to
 * '?' like raylib's GetGlyphInfo. */
GlyphInfo UIFontGlyph(Font font, int codepoint);

/* Atlas sub-rectangle for a codepoint, falling back to '?' like raylib's
 * GetGlyphAtlasRec. */
Rectangle UIFontAtlasRec(Font font, int codepoint);

/* The atlas texture holding every glyph image (font.texture). Callers blit a
 * glyph with DrawTexturePro(atlas, UIFontAtlasRec(font, cp), dst, ...). */
Texture2D UIFontAtlasTexture(Font font);

/* Horizontal pen advance for a codepoint in font units
 * (glyphs[GetGlyphIndex].advanceX), fallback '?' as raylib. */
int UIFontAdvance(Font font, int codepoint);

/* True if the font provides a glyph for `codepoint` (any glyph slot, not just
 * the loaded codepoint range). */
int UIFontHasGlyphValue(Font font, int codepoint);

#endif /* UI_TEXT_BACKEND_H */
