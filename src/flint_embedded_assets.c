#include "flint_embedded_assets.h"

#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
__attribute__((weak))
#endif
const FlintEmbeddedAsset flint_embedded_assets[] = {
    {0, 0, 0, 0}
};

#if defined(__GNUC__)
__attribute__((weak))
#endif
const unsigned int flint_embedded_asset_count = 0;

static const char *
normalize_path(const char *path)
{
    if(path == NULL)
        return "";
    while(path[0] == '.' && path[1] == '/')
        path += 2;
    while(path[0] == '/')
        path++;
    return path;
}

const FlintEmbeddedAsset *
flint_embedded_asset(const char *path)
{
    const char *normalized = normalize_path(path);

    for(unsigned int i = 0; i < flint_embedded_asset_count; i++) {
        const FlintEmbeddedAsset *asset = &flint_embedded_assets[i];
        if(asset->path != NULL && strcmp(asset->path, normalized) == 0)
            return asset;
    }

    return NULL;
}

char *
flint_embedded_asset_text(const char *path)
{
    const FlintEmbeddedAsset *asset = flint_embedded_asset(path);
    char *text;

    if(asset == NULL || asset->data == NULL)
        return NULL;

    text = (char *)malloc((size_t)asset->size + 1);
    if(text == NULL)
        return NULL;

    memcpy(text, asset->data, asset->size);
    text[asset->size] = '\0';
    return text;
}

const char *
flint_embedded_asset_extension(const char *path)
{
    const char *dot;

    if(path == NULL)
        return "";

    dot = strrchr(path, '.');
    return dot != NULL ? dot : "";
}
