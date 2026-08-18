#ifndef KRY_SW_PNG_H
#define KRY_SW_PNG_H

/*
 * Decode an in-memory PNG into tightly packed RGBA8 (malloc'd, caller frees)
 * and report its dimensions. Accepts 8-bit non-interlaced grayscale,
 * grayscale+alpha, RGB, palette (with optional tRNS) and RGBA images.
 * Returns NULL for anything malformed or unsupported.
 */

#include <stddef.h>

unsigned char *kry_sw_png_rgba(const unsigned char *data, size_t len,
                               int *w, int *h);

#endif /* KRY_SW_PNG_H */
