/*
 * Kryon Graphics Engine - memdraw-compatible software rasterizer
 * C89/C90 compliant
 *
 * Reference: Plan 9 libmemdraw and 9vx implementation
 */

#ifndef KRYON_GRAPHICS_H
#define KRYON_GRAPHICS_H

#include <stddef.h>

/*
 * C89 compatibility: ssize_t is not defined in C89
 */
#ifdef _WIN32
typedef long ssize_t;
#else
#include <sys/types.h>
#endif

/*
 * Basic 2D geometry
 */
typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point min;
    Point max;
} Rectangle;

/*
 * Rectangle constructors (inline functions for C89 compatibility)
 */
static Point Pt_func(int x, int y)
{
    Point p;
    p.x = x;
    p.y = y;
    return p;
}

static Rectangle Rect_func(int x0, int y0, int x1, int y1)
{
    Rectangle r;
    r.min.x = x0;
    r.min.y = y0;
    r.max.x = x1;
    r.max.y = y1;
    return r;
}

#define Pt(x, y)        Pt_func((x), (y))
#define Rect(x0, y0, x1, y1)  Rect_func((x0), (y0), (x1), (y1))

/*
 * Rectangle utilities
 */
#define Dx(r)   ((r).max.x - (r).min.x)
#define Dy(r)   ((r).max.y - (r).min.y)

/*
 * Channel descriptors (little-endian)
 * Format: [<< 24 | << 16 | << 8 | ]
 * Each field: [nbits << 1 | lsb_first]
 *
 * Common formats:
 */
#define RGB24   0x000001FF    /* R,G,B in 3 bytes */
#define RGBA32  0xFF0000FF    /* R,G,B,A in 4 bytes (our primary format) */
#define ARGB32  0x0000FFFF    /* A,R,G,B in 4 bytes */
#define XRGB32  0x0000FEFF    /* X,R,G,B in 4 bytes */
#define GREY8   0x010101FF    /* 8-bit grayscale */

/*
 * Memory data descriptor
 */
typedef struct Memdata {
    unsigned long *base;      /* Base pointer for 32-bit access */
    unsigned char *bdata;     /* Base pointer for byte access */
    int ref;                  /* Reference count */
    int allocd;               /* Non-zero if we allocated the data */
} Memdata;

/*
 * Memory image descriptor
 */
typedef struct Memimage {
    Rectangle r;              /* Image rectangle */
    Rectangle clipr;          /* Clipping rectangle */
    int depth;                /* Bits per pixel */
    int nchan;                /* Number of channels */
    unsigned long chan;       /* Channel descriptor */
    Memdata *data;            /* Pixel data */
    int zero;                 /* Zero if data contains zeros */
    unsigned long width;      /* Width in words (for stride) */
    int shift[4];             /* Bit shift for each channel */
    int mask[4];              /* Bit mask for each channel */
    int nbits[4];             /* Number of bits per channel */
    void *x;                  /* Extension data (for future use) */
} Memimage;

/*
 * Graphics functions - memimage.c
 */
Memimage *memimage_alloc(Rectangle r, unsigned long chan);
void memimage_free(Memimage *img);
int memimage_setclipr(Memimage *img, Rectangle clipr);

/*
 * Graphics functions - memdraw.c
 */
void memfillcolor(Memimage *dst, unsigned long color);
void memfillcolor_rect(Memimage *dst, Rectangle r, unsigned long color);
void memdraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
             Memimage *mask, Point mp, int op);

/*
 * Draw operations
 */
#define SoverD  12   /* Source over Destination (alpha blend) */
#define SoutD   10   /* Source out Destination */
#define SinD    4    /* Source in Destination */
#define S       0    /* Source */

/*
 * Color parsing
 */
unsigned long strtocolor(const char *str);
int parse_rect(const char *str, Rectangle *r);

/*
 * Utilities
 */
int rect_clip(Rectangle *rp, Rectangle clipr);
int rect_intersect(Rectangle *rp, Rectangle s);
int rectXrect(Rectangle r, Rectangle s);

#endif /* KRYON_GRAPHICS_H */
