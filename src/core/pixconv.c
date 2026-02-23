/*
 * Kryon Graphics Engine - Pixel Format Conversion
 * C89/C90 compliant
 *
 * Utilities for converting between different pixel formats
 */

#include "pixconv.h"
#include <stdio.h>

/*
 * Convert RGB24 to RGBA32
 * RGB24 format: [R][G][B] (3 bytes per pixel)
 * RGBA32 format: [R][G][B][A] (4 bytes per pixel, SDL2 RGBA8888 format)
 */
void convert_rgb24_to_rgba32(const unsigned char *src, unsigned char *dst, int count)
{
    int i;

    if (src == NULL || dst == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        unsigned char r, g, b;

        /* Read RGB24 */
        r = src[i * 3 + 0];
        g = src[i * 3 + 1];
        b = src[i * 3 + 2];

        /* Write RGBA32 in byte order: R G B A */
        dst[i * 4 + 0] = r;
        dst[i * 4 + 1] = g;
        dst[i * 4 + 2] = b;
        dst[i * 4 + 3] = 255;  /* Opaque alpha */
    }
}

/*
 * Convert RGBA32 to RGB24
 * RGBA32 format: [R][G][B][A] (4 bytes per pixel, SDL2 RGBA8888 format)
 * RGB24 format: [R][G][B] (3 bytes per pixel)
 */
void convert_rgba32_to_rgb24(const unsigned char *src, unsigned char *dst, int count)
{
    int i;

    if (src == NULL || dst == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        /* Read RGBA32 in byte order: R G B A */
        /* Alpha is at src[i * 4 + 3] but we ignore it */
        dst[i * 3 + 0] = src[i * 4 + 0];  /* R */
        dst[i * 3 + 1] = src[i * 4 + 1];  /* G */
        dst[i * 3 + 2] = src[i * 4 + 2];  /* B */
    }
}

/*
 * Convert GREY8 to RGBA32
 * GREY8 format: [Y] (1 byte per pixel, Y = luminance 0-255)
 * RGBA32 format: [R][G][B][A] (4 bytes per pixel, SDL2 RGBA8888 format)
 */
void convert_grey8_to_rgba32(const unsigned char *src, unsigned char *dst, int count)
{
    int i;

    if (src == NULL || dst == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        unsigned char y;

        /* Read GREY8 */
        y = src[i];

        /* Write RGBA32 in byte order: R G B A */
        /* Grayscale: R = G = B = Y */
        dst[i * 4 + 0] = y;  /* R */
        dst[i * 4 + 1] = y;  /* G */
        dst[i * 4 + 2] = y;  /* B */
        dst[i * 4 + 3] = 255;  /* Opaque alpha */
    }
}

/*
 * Convert RGBA32 to GREY8
 * RGBA32 format: [R][G][B][A] (4 bytes per pixel, SDL2 RGBA8888 format)
 * GREY8 format: [Y] (1 byte per pixel, Y = luminance 0-255)
 *
 * Uses ITU-R BT.601 luminance formula:
 * Y = 0.299*R + 0.587*G + 0.114*B
 */
void convert_rgba32_to_grey8(const unsigned char *src, unsigned char *dst, int count)
{
    int i;

    if (src == NULL || dst == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        unsigned char r, g, b;
        int y;

        /* Read RGBA32 in byte order: R G B A */
        r = src[i * 4 + 0];
        g = src[i * 4 + 1];
        b = src[i * 4 + 2];
        /* Alpha is at src[i * 4 + 3] but we ignore it */

        /* Calculate luminance using ITU-R BT.601 formula */
        /* Y = 0.299*R + 0.587*G + 0.114*B */
        /* Scale to avoid floating point: multiply by 1000 then divide */
        y = (299 * (int)r + 587 * (int)g + 114 * (int)b) / 1000;

        /* Clamp to 0-255 */
        if (y < 0) {
            y = 0;
        } else if (y > 255) {
            y = 255;
        }

        /* Write GREY8 */
        dst[i] = (unsigned char)y;
    }
}
