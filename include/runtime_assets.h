#ifndef RUNTIME_ASSETS_H
#define RUNTIME_ASSETS_H

#include <stddef.h>

typedef enum RuntimeAssetStatus {
    RUNTIME_ASSET_IDLE = 0,
    RUNTIME_ASSET_DOWNLOADING,
    RUNTIME_ASSET_READY,
    RUNTIME_ASSET_ERROR,
    RUNTIME_ASSET_UNSUPPORTED
} RuntimeAssetStatus;

typedef struct RuntimeAssetDownload {
    volatile RuntimeAssetStatus status;
    char url[1024];
    char path[512];
    char error[256];
    long http_status;
    volatile size_t bytes;
    volatile size_t total_bytes;
    void *platform;
} RuntimeAssetDownload;

typedef int (*RuntimeAssetDownloadBackend)(RuntimeAssetDownload *download,
                                                const char *url,
                                                const char *path);

int InitRuntimeAssets(const char *app_id);
int GetRuntimeAssetCacheRoot(const char *app_id, char *out, size_t out_size);
int EnsureRuntimeAssetDir(const char *path);
void SetRuntimeAssetDownloadBackend(RuntimeAssetDownloadBackend backend);
int DownloadRuntimeAsset(RuntimeAssetDownload *download,
                                 const char *url,
                                 const char *path);
const char *GetRuntimeAssetStatusText(RuntimeAssetStatus status);

#endif
