/*
 * kry_update_flow_test.c - the embeddable update lifecycle, exercised
 * against file:// appcasts and artifacts. The apply/exec path is
 * deliberately not driven: exec replaces the test process.
 */
#include "kry_update_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static void
spin(KryUpdateFlow *f, int spins)
{
    int i;

    for(i = 0; i < spins; i++) {
        struct timespec ts = {0, 10 * 1000 * 1000};

        kry_update_flow_poll(f);
        nanosleep(&ts, NULL);
    }
}

static void
write_file(const char *path, const char *content)
{
    FILE *out = fopen(path, "wb");

    if(out != NULL) {
        fputs(content, out);
        fclose(out);
    }
}

static const char *APPCAST_FMT =
    "{\"version\":\"9.9.9\",\"date\":\"2026-08-19\",\"notes\":\"n\","
    "\"notes_url\":\"https://x/release\","
    "\"channels\":{\"appimage-amd64\":{\"url\":\"file://%s/artifact.bin\","
    "\"sha256\":\"%s\",\"size\":%d}}}";

int
main(void)
{
    char root[256];
    char appcast_path[300], artifact_path[300], sha_cmd[512];
    char appcast[700];
    char sha[65];
    FILE *f;
    KryUpdateFlow *flow;
    KryUpdateFlowConfig cfg = {.app_name = "flowtest", .current_version = "1.0.0"};

    snprintf(root, sizeof(root), "/tmp/kry_flow_test.%d", (int)getpid());
    mkdir(root, 0755);
    snprintf(artifact_path, sizeof(artifact_path), "%s/artifact.bin", root);
    snprintf(appcast_path, sizeof(appcast_path), "%s/appcast.json", root);
    write_file(artifact_path, "flow-artifact-content");
    snprintf(sha_cmd, sizeof(sha_cmd), "sha256sum %s", artifact_path);
    f = popen(sha_cmd, "r");
    if(f == NULL || fscanf(f, "%64s", sha) != 1) {
        printf("cannot hash artifact; aborting\n");
        if(f != NULL)
            pclose(f);
        return 1;
    }
    pclose(f);
    snprintf(appcast, sizeof(appcast), APPCAST_FMT, root, sha, 20);
    write_file(appcast_path, appcast);

    /* NULL / bad arguments */
    CHECK(kry_update_flow_start(NULL, "file://x") == NULL);
    {
        KryUpdateFlowConfig bad = {.app_name = "", .current_version = "1"};
        char url[600];

        snprintf(url, sizeof(url), "file://%s", appcast_path);
        CHECK(kry_update_flow_start(&bad, url) == NULL);
        bad.app_name = "app";
        bad.current_version = NULL;
        CHECK(kry_update_flow_start(&bad, url) == NULL);
    }

    /* full lifecycle: the test binary runs as a source build, so force
     * the AppImage channel through the env the same way detection reads it */
    setenv("APPIMAGE", "/tmp/kry_flow_test-not-real.AppImage", 1);
    setenv("XDG_DATA_HOME", root, 1);
    {
        char url[600];

        snprintf(url, sizeof(url), "file://%s", appcast_path);
        flow = kry_update_flow_start(&cfg, url);
    }
    if(flow == NULL) {
        printf("flow unavailable (no libcurl); skipping\n");
        unsetenv("APPIMAGE");
        return failures == 0 ? 0 : 1;
    }
    CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_CHECKING);
    spin(flow, 500);
    CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_AVAILABLE);
    CHECK(strcmp(kry_update_flow_new_version(flow), "9.9.9") == 0);
    CHECK(strcmp(kry_update_flow_release_url(flow), "https://x/release") == 0);
    CHECK(kry_update_flow_channel(flow) == KRY_UPDATE_CHANNEL_APPIMAGE);
    CHECK(kry_update_flow_artifact(flow) != NULL);
    CHECK(kry_update_flow_appcast(flow) != NULL);
    CHECK(kry_update_flow_progress(flow) == -1.0);

    /* download to READY */
    CHECK(kry_update_flow_download(flow) == 1);
    CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_DOWNLOADING);
    CHECK(kry_update_flow_download(flow) == 0);   /* no double start */
    spin(flow, 500);
    CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_READY);
    CHECK(kry_update_flow_error(flow) == NULL);

    /* apply arms the exit hook; exec_pending then stages the verified
     * file over $APPIMAGE (real rename). The exec itself fails — the
     * staged "AppImage" is plain text — so the test process survives. */
    CHECK(kry_update_flow_apply(flow) == 1);
    CHECK(kry_update_flow_exec_pending(flow) == 1);
    {
        FILE *staged = fopen("/tmp/kry_flow_test-not-real.AppImage", "rb");
        char buf[32] = {0};

        CHECK(staged != NULL);
        if(staged != NULL) {
            fread(buf, 1, sizeof(buf) - 1, staged);
            fclose(staged);
            CHECK(strcmp(buf, "flow-artifact-content") == 0);
            remove("/tmp/kry_flow_test-not-real.AppImage");
        }
    }
    kry_update_flow_free(flow);

    /* up-to-date appcast */
    {
        char up_path[300];
        char url[600];

        snprintf(up_path, sizeof(up_path), "%s/up.json", root);
        snprintf(appcast, sizeof(appcast),
                 "{\"version\":\"1.0.0\",\"notes_url\":\"u\"}");
        write_file(up_path, appcast);
        snprintf(url, sizeof(url), "file://%s", up_path);
        flow = kry_update_flow_start(&cfg, url);
        CHECK(flow != NULL);
        spin(flow, 500);
        CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_UP_TO_DATE);
        CHECK(kry_update_flow_artifact(flow) == NULL);
        CHECK(kry_update_flow_download(flow) == 0);
        kry_update_flow_free(flow);
    }

    /* bad sha: download fails, retry allowed */
    {
        char bad_path[300];
        char url[600];

        snprintf(bad_path, sizeof(bad_path), "%s/bad.json", root);
        snprintf(appcast, sizeof(appcast), APPCAST_FMT, root,
                 "0000000000000000000000000000000000000000000000000000000000000000", 20);
        write_file(bad_path, appcast);
        snprintf(url, sizeof(url), "file://%s", bad_path);
        flow = kry_update_flow_start(&cfg, url);
        CHECK(flow != NULL);
        spin(flow, 500);
        CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_AVAILABLE);
        CHECK(kry_update_flow_download(flow) == 1);
        spin(flow, 500);
        CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_FAILED);
        CHECK(kry_update_flow_error(flow) != NULL);
        CHECK(kry_update_flow_download(flow) == 1);   /* retry works */
        kry_update_flow_free(flow);
    }

    /* system-managed channel: stops at AVAILABLE with no artifact */
    unsetenv("APPIMAGE");
    {
        char url[600];

        snprintf(url, sizeof(url), "file://%s", appcast_path);
        flow = kry_update_flow_start(&cfg, url);
        CHECK(flow != NULL);
        spin(flow, 500);
        CHECK(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_AVAILABLE);
        CHECK(kry_update_flow_artifact(flow) == NULL);
        CHECK(kry_update_flow_apply(flow) == 0);   /* nothing to apply */
        kry_update_flow_free(flow);
    }

    kry_update_flow_free(NULL);
    kry_update_flow_poll(NULL);
    CHECK(kry_update_flow_state(NULL) == KRY_UPDATE_FLOW_IDLE);
    CHECK(kry_update_flow_download(NULL) == 0);

    unsetenv("XDG_DATA_HOME");
    if(failures == 0)
        printf("kry_update_flow tests passed\n");
    return failures == 0 ? 0 : 1;
}
