/*
 * Kryon Graphics Engine - Image Allocation
 * C89/C90 compliant
 *
 * Reference: Plan 9 libmemdraw/memalloc.c
 */

#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Calculate shift and mask values for channels
 */
static void compute_chan(Memimage *img)
{
    int i;
    unsigned long c;
    int nbits;

    img->nchan = 0;

    for (i = 0; i < 4; i++) {
        c = (img->chan >> (8 * (3 - i))) & 0xFF;
        if (c == 0) {
            img->shift[i] = 0;
            img->mask[i] = 0;
            img->nbits[i] = 0;
            continue;
        }

        img->nchan++;
        nbits = c >> 1;
        img->nbits[i] = nbits;
        img->shift[i] = (c & 1) ? 0 : (8 - nbits);
        img->mask[i] = (1 << nbits) - 1;
    }
}

/*
 * Allocate a new memory image
 */
Memimage *memimage_alloc(Rectangle r, unsigned long chan)
{
    Memimage *img;
    Memdata *data;
    int width;
    size_t size;

    /* Validate rectangle */
    if (r.max.x <= r.min.x || r.max.y <= r.min.y) {
        fprintf(stderr, "memimage_alloc: invalid rectangle\n");
        return NULL;
    }

    /* Allocate image structure */
    img = (Memimage *)malloc(sizeof(Memimage));
    if (img == NULL) {
        fprintf(stderr, "memimage_alloc: cannot allocate image\n");
        return NULL;
    }
    memset(img, 0, sizeof(Memimage));

    /* Allocate data structure */
    data = (Memdata *)malloc(sizeof(Memdata));
    if (data == NULL) {
        fprintf(stderr, "memimage_alloc: cannot allocate data\n");
        free(img);
        return NULL;
    }

    /* Calculate dimensions */
    img->r = r;
    img->clipr = r;
    img->chan = chan;

    /* Determine depth from channel */
    switch (chan) {
    case RGBA32:
    case ARGB32:
    case XRGB32:
        img->depth = 32;
        width = Dx(r) * 4;
        break;
    case RGB24:
        img->depth = 24;
        width = Dx(r) * 3;
        break;
    case GREY8:
        img->depth = 8;
        width = Dx(r);
        break;
    default:
        fprintf(stderr, "memimage_alloc: unsupported channel 0x%08lX\n", chan);
        free(data);
        free(img);
        return NULL;
    }

    img->width = width;

    /* Allocate pixel data */
    size = width * Dy(r);
    if (size == 0 || size > (1 << 28)) {  /* 256MB limit */
        fprintf(stderr, "memimage_alloc: invalid image size %lu\n",
                (unsigned long)size);
        free(data);
        free(img);
        return NULL;
    }

    data->bdata = (unsigned char *)malloc(size);
    if (data->bdata == NULL) {
        fprintf(stderr, "memimage_alloc: cannot allocate pixel data (%lu bytes)\n",
                (unsigned long)size);
        free(data);
        free(img);
        return NULL;
    }

    /* Set up data structure */
    data->base = (unsigned long *)data->bdata;
    data->ref = 1;
    data->allocd = 1;

    img->data = data;
    img->zero = 1;  /* Assume data starts as zero */

    /* Compute channel information */
    compute_chan(img);

    return img;
}

/*
 * Free a memory image
 */
void memimage_free(Memimage *img)
{
    if (img == NULL) {
        return;
    }

    if (img->data != NULL) {
        img->data->ref--;
        if (img->data->ref <= 0 && img->data->allocd) {
            if (img->data->bdata != NULL) {
                free(img->data->bdata);
            }
            free(img->data);
        }
    }

    if (img->x != NULL) {
        /* Free extension data if any */
        free(img->x);
    }

    free(img);
}

/*
 * Set clipping rectangle
 */
int memimage_setclipr(Memimage *img, Rectangle clipr)
{
    if (img == NULL) {
        return -1;
    }

    img->clipr = clipr;
    return 0;
}
