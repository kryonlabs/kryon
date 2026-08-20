#include "terminal_pane.h"

#include "ui_text.h"
#include "ui_text_backend.h"

typedef struct TerminalPaneGlyphCacheEntry {
    unsigned int active_texture_id;
    int active_glyph_count;
    int codepoint;
    int font_size;
    int ready;
    Font font;
    float scale;
    GlyphInfo glyph;
    Rectangle source;
} TerminalPaneGlyphCacheEntry;

#define TERMINAL_PANE_GLYPH_CACHE_SIZE 512

static TerminalPaneGlyphCacheEntry
    terminal_pane_glyph_cache[TERMINAL_PANE_GLYPH_CACHE_SIZE];

static const TerminalPaneGlyphCacheEntry *
terminal_pane_cached_glyph(unsigned int codepoint, int font_size)
{
    Font active = GetUIFont();
    unsigned int slot =
        (((unsigned int)codepoint * 2654435761u) ^
         ((unsigned int)font_size * 2246822519u) ^ active.texture.id) %
        TERMINAL_PANE_GLYPH_CACHE_SIZE;
    TerminalPaneGlyphCacheEntry *entry = &terminal_pane_glyph_cache[slot];

    if(entry->active_texture_id == active.texture.id &&
       entry->active_glyph_count == active.glyphCount &&
       entry->codepoint == (int)codepoint && entry->font_size == font_size)
        return entry;
    entry->active_texture_id = active.texture.id;
    entry->active_glyph_count = active.glyphCount;
    entry->codepoint = (int)codepoint;
    entry->font_size = font_size;
    entry->font = GetUIFontForCodepoint((int)codepoint, font_size);
    entry->ready = UIFontReady(entry->font);
    if(entry->ready) {
        entry->scale = GetUIFontScale(entry->font, font_size);
        entry->glyph = UIFontGlyph(entry->font, (int)codepoint);
        entry->source = UIFontAtlasRec(entry->font, (int)codepoint);
    } else {
        entry->scale = 1.0f;
        entry->glyph = (GlyphInfo){0};
        entry->source = (Rectangle){0};
    }
    return entry;
}

static void draw_terminal_pane_grid_codepoint(unsigned int codepoint, int x,
                                              int y, int font_size,
                                              Color color)
{
    const TerminalPaneGlyphCacheEntry *glyph;

    if(codepoint == 0 || codepoint == ' ')
        return;
    glyph = terminal_pane_cached_glyph(codepoint, font_size);
    if(glyph == NULL || !glyph->ready)
        return;
    if(glyph->source.width <= 0.0f || glyph->source.height <= 0.0f)
        return;
    DrawTexturePro(
        UIFontAtlasTexture(glyph->font), glyph->source,
        (Rectangle){
            (float)x + (float)glyph->glyph.offsetX * glyph->scale,
            (float)y + (float)glyph->glyph.offsetY * glyph->scale,
            glyph->source.width * glyph->scale,
            glyph->source.height * glyph->scale
        },
        (Vector2){0.0f, 0.0f}, 0.0f, color);
}

void DrawTerminalPaneGlyphCell(unsigned int codepoint, unsigned int combining,
                               int x, int y, int font_size, Color color)
{
    draw_terminal_pane_grid_codepoint(codepoint, x, y, font_size, color);
    if(combining != 0)
        draw_terminal_pane_grid_codepoint(combining, x, y, font_size, color);
}

void DrawTerminalPaneGlyphGrid(TerminalPaneGridDraw grid)
{
    int row;
    int col;

    if(grid.cells == NULL || grid.cols <= 0 || grid.rows <= 0 ||
       grid.cell_width <= 0 || grid.line_height <= 0 || grid.font_size <= 0)
        return;
    for(row = 0; row < grid.rows; row++) {
        int y = (int)grid.bounds.y + row * grid.line_height;

        for(col = 0; col < grid.cols; col++) {
            const TerminalPaneGridCell *cell =
                grid.cells + row * grid.cols + col;
            int x = (int)grid.bounds.x + col * grid.cell_width;

            if((cell->flags & (TERMINAL_PANE_GRID_CELL_WIDE_CONT |
                               TERMINAL_PANE_GRID_CELL_HIDDEN)) != 0)
                continue;
            DrawTerminalPaneGlyphCell(cell->codepoint, cell->combining, x, y,
                                      grid.font_size, cell->foreground);
        }
    }
}
