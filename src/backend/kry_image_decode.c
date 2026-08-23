#include <stdlib.h>

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char *
kry_decode_image_rgba(const unsigned char *data, int len, int *width,
                      int *height)
{
    int channels = 0;

    if(data == NULL || len <= 0 || width == NULL || height == NULL)
        return NULL;
    return stbi_load_from_memory(data, len, width, height, &channels, 4);
}
