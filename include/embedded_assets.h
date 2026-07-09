#ifndef EMBEDDED_ASSETS_H
#define EMBEDDED_ASSETS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmbeddedAsset {
    const char *path;
    const char *mime;
    const unsigned char *data;
    unsigned int size;
} EmbeddedAsset;

extern const EmbeddedAsset embedded_assets[];
extern const unsigned int embedded_asset_count;

const EmbeddedAsset *GetEmbeddedAsset(const char *path);
char *LoadEmbeddedAssetText(const char *path);
const char *GetEmbeddedAssetExtension(const char *path);

#ifdef __cplusplus
}
#endif

#endif
