/*
 * kry_update_test.c - desktop update-check tests.
 *
 * Hermetic: version math, appcast parsing, channel detection (faked via
 * env vars), and full checks against file:// appcasts — no network. A
 * live appcast round trip runs only when KRYON_UPDATE_TEST_URL is set.
 */
#include "kry_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static const char *NEWER_APPCAST =
    "{\n"
    "  \"version\": \"1.9.5\",\n"
    "  \"date\": \"2026-08-19\",\n"
    "  \"notes\": \"Breath pattern fixes.\",\n"
    "  \"notes_url\": \"https://github.com/waozixyz/inbe/releases/tag/v1.9.5\",\n"
    "  \"channels\": {\n"
    "    \"appimage\": {\"url\": \"https://x/inbe.AppImage\", \"sha256\": \"aa\", \"size\": 123},\n"
    "    \"windows\": {\"url\": \"https://x/inbe.zip\", \"sha256\": \"bb\"},\n"
    "    \"deb\": {\"url\": \"https://x/inbe.deb\"}\n"
    "  }\n"
    "}\n";

static KryUpdateStatus
poll_until_terminal(KryUpdateCheck *c, int spins)
{
    KryUpdateStatus s = kry_update_poll(c);
    int i;

    for(i = 0; i < spins && s == KRY_UPDATE_PENDING; i++) {
        struct timespec ts = {0, 10 * 1000 * 1000};

        nanosleep(&ts, NULL);
        s = kry_update_poll(c);
    }
    return s;
}

static void
write_tmp(char *path, size_t cap, const char *tag, const char *content)
{
    FILE *f;

    snprintf(path, cap, "/tmp/kry_update_test.%s.%d", tag, (int)getpid());
    f = fopen(path, "wb");
    if(f != NULL) {
        fputs(content, f);
        fclose(f);
    }
}

static void
test_version_compare(void)
{
    CHECK(kry_update_version_compare("1.9.4", "1.10.0") < 0);
    CHECK(kry_update_version_compare("1.10.0", "1.9.4") > 0);
    CHECK(kry_update_version_compare("1.9.4", "1.9.4") == 0);
    CHECK(kry_update_version_compare("v1.9.5", "1.9.4") > 0);
    CHECK(kry_update_version_compare("1.9", "1.9.0") == 0);
    CHECK(kry_update_version_compare("2.0", "2.0.0.0") == 0);
    CHECK(kry_update_version_compare("1.9.4-rc1", "1.9.4") == 0);
    CHECK(kry_update_version_compare(NULL, "0.0.1") < 0);
    CHECK(kry_update_version_compare("", "") == 0);
}

static void
test_appcast_parse(void)
{
    KryUpdateInfo info;
    const KryUpdateChannelInfo *ch;

    CHECK(kry_update_appcast_parse(NEWER_APPCAST, &info) == 1);
    CHECK(strcmp(info.version, "1.9.5") == 0);
    CHECK(strcmp(info.date, "2026-08-19") == 0);
    CHECK(strstr(info.notes, "Breath") != NULL);
    CHECK(strncmp(info.notes_url, "https://github.com/waozixyz/inbe", 31) == 0);
    CHECK(info.channel_count == 3);

    ch = kry_update_find_channel(&info, "appimage");
    CHECK(ch != NULL);
    if(ch != NULL) {
        CHECK(strcmp(ch->url, "https://x/inbe.AppImage") == 0);
        CHECK(strcmp(ch->sha256, "aa") == 0);
        CHECK(ch->size == 123);
    }
    ch = kry_update_find_channel(&info, "windows");
    CHECK(ch != NULL && ch->size == 0);
    CHECK(kry_update_find_channel(&info, "nope") == NULL);

    CHECK(kry_update_appcast_parse("{", &info) == 0);
    CHECK(kry_update_appcast_parse("{\"date\":\"x\"}", &info) == 0);
    CHECK(kry_update_appcast_parse("{\"version\":\"\"}", &info) == 0);
    CHECK(kry_update_appcast_parse(NULL, &info) == 0);

    /* non-object and overflow channel entries are skipped, not fatal */
    CHECK(kry_update_appcast_parse(
        "{\"version\":\"1\","
        "\"channels\":{\"ok\":{\"url\":\"u\"},\"bad\":\"str\"}}", &info) == 1);
    CHECK(info.channel_count == 1);
    CHECK(kry_update_find_channel(&info, "ok") != NULL);
}

static void
test_channel_detect(void)
{
    KryUpdateChannel got;

    unsetenv("APPIMAGE");
    unsetenv("SNAP");
    unsetenv("FLATPAK_ID");

    setenv("APPIMAGE", "/tmp/inbe.AppImage", 1);
    got = kry_update_detect_channel();
    CHECK(got == KRY_UPDATE_CHANNEL_APPIMAGE);
    CHECK(strcmp(kry_update_channel_name(got), "AppImage") == 0);
    CHECK(strcmp(kry_update_channel_key(got), "appimage") == 0);

    unsetenv("APPIMAGE");
    setenv("SNAP", "/snap/inbe/123", 1);
    CHECK(kry_update_detect_channel() == KRY_UPDATE_CHANNEL_SNAP);
    CHECK(kry_update_channel_key(KRY_UPDATE_CHANNEL_SNAP) == NULL);

    unsetenv("SNAP");
    setenv("FLATPAK_ID", "xyz.inbe", 1);
    CHECK(kry_update_detect_channel() == KRY_UPDATE_CHANNEL_FLATPAK);

    unsetenv("FLATPAK_ID");
#ifndef _WIN32
    /* test binary runs from the build tree, not /usr or /opt */
    CHECK(kry_update_detect_channel() == KRY_UPDATE_CHANNEL_SOURCE);
#endif
    CHECK(kry_update_channel_name(KRY_UPDATE_CHANNEL_UNKNOWN) != NULL);
}

static void
test_check_file_appcast(void)
{
    char path[256], url[300];
    KryUpdateCheck *c;
    const KryUpdateInfo *info;

    write_tmp(path, sizeof(path), "newer", NEWER_APPCAST);
    snprintf(url, sizeof(url), "file://%s", path);

    c = kry_update_check(url, "1.9.4");
    if(c == NULL) {
        printf("kry_update unavailable (no libcurl); skipping network tests\n");
        remove(path);
        return;
    }
    CHECK(poll_until_terminal(c, 500) == KRY_UPDATE_AVAILABLE);
    info = kry_update_info(c);
    CHECK(info != NULL);
    if(info != NULL) {
        CHECK(strcmp(info->version, "1.9.5") == 0);
        CHECK(kry_update_find_channel(info, "appimage") != NULL);
    }
    CHECK(kry_update_error(c) == NULL);
    kry_update_free(c);

    /* same version: up to date, info still readable */
    c = kry_update_check(url, "1.9.5");
    CHECK(c != NULL);
    CHECK(poll_until_terminal(c, 500) == KRY_UPDATE_UP_TO_DATE);
    CHECK(kry_update_info(c) != NULL);
    kry_update_free(c);

    /* newer current than appcast: also up to date */
    c = kry_update_check(url, "2.0.0");
    CHECK(c != NULL);
    CHECK(poll_until_terminal(c, 500) == KRY_UPDATE_UP_TO_DATE);
    kry_update_free(c);
    remove(path);

    /* garbage and missing appcasts fail with a diagnostic */
    write_tmp(path, sizeof(path), "garbage", "not json at all");
    snprintf(url, sizeof(url), "file://%s", path);
    c = kry_update_check(url, "1.9.4");
    CHECK(c != NULL);
    CHECK(poll_until_terminal(c, 500) == KRY_UPDATE_FAILED);
    CHECK(kry_update_error(c) != NULL);
    CHECK(kry_update_info(c) == NULL);
    kry_update_free(c);
    remove(path);

    snprintf(url, sizeof(url), "file:///tmp/definitely-not-here-kryon-update");
    c = kry_update_check(url, "1.9.4");
    CHECK(c != NULL);
    CHECK(poll_until_terminal(c, 500) == KRY_UPDATE_FAILED);
    CHECK(kry_update_error(c) != NULL);
    kry_update_free(c);

    CHECK(kry_update_check(NULL, "1.0") == NULL);
    CHECK(kry_update_check(url, NULL) == NULL);
    CHECK(kry_update_poll(NULL) == KRY_UPDATE_FAILED);
    kry_update_free(NULL);
}

static void
test_live_gated(void)
{
    const char *url = getenv("KRYON_UPDATE_TEST_URL");
    KryUpdateCheck *c;

    if(url == NULL || url[0] == '\0')
        return;
    c = kry_update_check(url, "0.0.1");
    if(c == NULL) {
        printf("kry_update unavailable; live test skipped\n");
        return;
    }
    if(poll_until_terminal(c, 6000) == KRY_UPDATE_FAILED) {
        printf("live test failed: %s\n",
               kry_update_error(c) != NULL ? kry_update_error(c) : "?");
        failures++;
    }
    kry_update_free(c);
}

int
main(void)
{
    test_version_compare();
    test_appcast_parse();
    test_channel_detect();
    test_check_file_appcast();
    test_live_gated();
    if(failures == 0)
        printf("kry_update tests passed\n");
    return failures == 0 ? 0 : 1;
}
