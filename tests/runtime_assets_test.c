#include "runtime_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static RuntimeAssetStatus
poll_until_terminal(RuntimeAssetDownload *download, int spins)
{
    RuntimeAssetStatus status = PollRuntimeAssetDownload(download);
    int i;

    for(i = 0; i < spins &&
        status == RUNTIME_ASSET_DOWNLOADING; i++) {
        struct timespec ts = {0, 10 * 1000 * 1000};

        nanosleep(&ts, NULL);
        status = PollRuntimeAssetDownload(download);
    }
    return status;
}

static int
read_file(const char *path, char *out, size_t out_size)
{
    FILE *file;
    size_t n;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    n = fread(out, 1, out_size - 1, file);
    out[n] = '\0';
    fclose(file);
    return 1;
}

static void
test_file_download(void)
{
    RuntimeAssetDownload download = {0};
    char src[256];
    char dst[256];
    char url[320];
    char got[64];
    FILE *file;
    RuntimeAssetStatus status;

    snprintf(src, sizeof(src), "/tmp/runtime_assets_src.%d.txt", (int)getpid());
    snprintf(dst, sizeof(dst), "/tmp/runtime_assets_dst.%d.txt", (int)getpid());
    file = fopen(src, "wb");
    if(file == NULL)
        return;
    fprintf(file, "runtime asset payload");
    fclose(file);
    remove(dst);
    snprintf(url, sizeof(url), "file://%s", src);

    CHECK(DownloadRuntimeAsset(&download, url, dst));
    CHECK(download.status == RUNTIME_ASSET_DOWNLOADING);
    status = poll_until_terminal(&download, 500);
    CHECK(status == RUNTIME_ASSET_READY);
    CHECK(download.bytes == strlen("runtime asset payload"));
    CHECK(read_file(dst, got, sizeof(got)));
    CHECK(strcmp(got, "runtime asset payload") == 0);
    FreeRuntimeAssetDownload(&download);
    CHECK(download.platform == NULL);

    remove(src);
    remove(dst);
}

int
main(void)
{
    test_file_download();
    return failures == 0 ? 0 : 1;
}
