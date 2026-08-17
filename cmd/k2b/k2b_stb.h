#ifndef K2B_STB_H
#define K2B_STB_H

/* Image decoding for cartridge embedding, on the stb_image vendored with
 * raylib. Returns 0 and fills *rgba (malloc'd, top-down RGBA8 rows) on
 * success. */

int k2b_decode_image(const char *file, unsigned char **rgba, int *w, int *h);

#endif
