/*
 * Kryon Graphics Engine - Pixel Format Conversion
 * C89/C90 compliant
 *
 * Utilities for converting between different pixel formats
 */

#ifndef KRYON_PIXCONV_H
#define KRYON_PIXCONV_H

#include <stddef.h>

/*
 * Convert RGB24 to RGBA32
 * Input:  RGB24 pixels (3 bytes per pixel: R,G,B)
 * Output: RGBA32 pixels (4 bytes per pixel: B,G,R,A little-endian)
 */
void convert_rgb24_to_rgba32(const unsigned char *src, unsigned char *dst, int count);

/*
 * Convert RGBA32 to RGB24
 * Input:  RGBA32 pixels (4 bytes per pixel: B,G,R,A little-endian)
 * Output: RGB24 pixels (3 bytes per pixel: R,G,B)
 */
void convert_rgba32_to_rgb24(const unsigned char *src, unsigned char *dst, int count);

/*
 * Convert GREY8 to RGBA32
 * Input:  GREY8 pixels (1 byte per pixel: luminance 0-255)
 * Output: RGBA32 pixels (4 bytes per pixel: B,G,R,A little-endian)
 */
void convert_grey8_to_rgba32(const unsigned char *src, unsigned char *dst, int count);

/*
 * Convert RGBA32 to GREY8
 * Input:  RGBA32 pixels (4 bytes per pixel: B,G,R,A little-endian)
 * Output: GREY8 pixels (1 byte per pixel: luminance 0-255)
 * Uses ITU-R BT.601 luminance formula: Y = 0.299*R + 0.587*G + 0.114*B
 */
void convert_rgba32_to_grey8(const unsigned char *src, unsigned char *dst, int count);

#endif /* KRYON_PIXCONV_H */
