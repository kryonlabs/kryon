#include "k2b_stb.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "../../vendor/raylib/src/external/stb_image.h"

#include <stdlib.h>

int
k2b_decode_image(const char *file, unsigned char **rgba, int *w, int *h)
{
    int n;

    if(file == NULL || rgba == NULL || w == NULL || h == NULL)
        return -1;
    *rgba = stbi_load(file, w, h, &n, 4);
    return *rgba != NULL ? 0 : -1;
}
