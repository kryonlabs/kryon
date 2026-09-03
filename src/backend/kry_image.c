/* Shared in-memory image front-end.
 *
 * LoadImageFromMemory/UnloadImage/IsImageValid/ImageFlipVertical are pure
 * CPU operations over the Image struct: decode bytes to RGBA, free, check,
 * and flip rows. Defining them once here means the raylib wrappers, the
 * generated null backend, and every software backend answer them
 * identically - tools/generate-kryon-compat.sh skips these names in both
 * generated files. Decoding goes through kry_decode_image_rgba (the stb
 * front-end kry_image_decode.c also serves the dom/termi/libdraw
 * backends), so even headless null-backend builds decode real PNG/JPEG/BMP
 * bytes instead of receiving empty images. */

#ifdef KRYON_NATIVE_PLAN9
#include "kryon_plan9.h"
#else
#include "kryon.h"

#include <stdlib.h>
#include <string.h>
#endif

extern unsigned char *kry_decode_image_rgba(const unsigned char *data, int len,
                                            int *width, int *height);

static Image kry_image_from_rgba(unsigned char *rgba, int w, int h)
{
    Image img;

    memset(&img, 0, sizeof(img));
    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData,
                          int dataSize)
{
    int width = 0;
    int height = 0;
    unsigned char *rgba;

    (void)fileType;    /* the decoder sniffs the format itself */
    if(fileData == NULL || dataSize <= 0)
        return kry_image_from_rgba(NULL, 0, 0);
    rgba = kry_decode_image_rgba(fileData, dataSize, &width, &height);
    return kry_image_from_rgba(rgba, width, height);
}

void UnloadImage(Image image)
{
    free(image.data);
}

bool IsImageValid(Image image)
{
    return image.data != NULL && image.width > 0 && image.height > 0;
}

void ImageFlipVertical(Image *image)
{
    int w, h, y;
    unsigned char *row;

    if(image == NULL || image->data == NULL || image->width <= 0 ||
       image->height <= 1)
        return;
    w = image->width;
    h = image->height;
    row = malloc((size_t)w * 4);
    if(row == NULL)
        return;
    for(y = 0; y < h / 2; y++) {
        unsigned char *top = (unsigned char *)image->data + (size_t)y * w * 4;
        unsigned char *bot = (unsigned char *)image->data +
            (size_t)(h - 1 - y) * w * 4;

        memcpy(row, top, (size_t)w * 4);
        memcpy(top, bot, (size_t)w * 4);
        memcpy(bot, row, (size_t)w * 4);
    }
    free(row);
}
