#include "ui_text_backend.h"

/*
 * Backend-neutral implementation of the text-backend seam (declared in
 * ui_text_backend.h). Every function forwards to a public-surface accessor
 * (GetGlyphInfo, GetGlyphAtlasRec, GetGlyphIndex) that the active graphics
 * backend provides, so this single source file works for all backends.
 *
 * The two validity/base-size helpers read Font struct fields because raylib
 * exposes no accessor for them; that is the one struct coupling that remains,
 * confined to these helpers rather than scattered through ui_text.c.
 */

int
UIFontReady(Font font)
{
    return font.texture.id != 0 && font.glyphs != NULL && font.recs != NULL &&
           font.glyphCount > 0 && font.baseSize > 0;
}

int
UIFontBaseSize(Font font)
{
    return font.baseSize;
}

int
UIFontGlyphPadding(Font font)
{
    return font.glyphPadding;
}

GlyphInfo
UIFontGlyph(Font font, int codepoint)
{
    return GetGlyphInfo(font, codepoint);
}

Rectangle
UIFontAtlasRec(Font font, int codepoint)
{
    return GetGlyphAtlasRec(font, codepoint);
}

Texture2D
UIFontAtlasTexture(Font font)
{
    return font.texture;
}

int
UIFontAdvance(Font font, int codepoint)
{
    return GetGlyphInfo(font, codepoint).advanceX;
}

int
UIFontHasGlyphValue(Font font, int codepoint)
{
    GlyphInfo glyph;

    if(!UIFontReady(font))
        return 0;

    glyph = GetGlyphInfo(font, codepoint);
    return glyph.value == codepoint;
}
