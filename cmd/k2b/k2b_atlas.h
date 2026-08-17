#ifndef K2B_ATLAS_H
#define K2B_ATLAS_H

#include <stddef.h>

/* Bake a KFA1 glyph atlas (cartridge asset kind 1) from a TTF for the
 * given charset and pixel sizes. Returns malloc'd bytes and length, or
 * NULL when the font can't be loaded. Layout:
 *   u32 'KFA1' | u16 size_count
 *   per size: u16 px, u16 glyph_count, u16 w, u16 h, u32 table_off,
 *             u32 pixels_off (offsets from buffer start)
 *   glyph (16B): u32 cp | u16 x,y,w,h | i16 xoff,yoff | u16 advance
 *   RGBA8 pixels, white with alpha coverage. */
unsigned char *k2b_bake_atlas(const char *ttf_path,
                              const unsigned int *codepoints, int cp_count,
                              const int *sizes, int size_count,
                              unsigned *out_len);

#endif
