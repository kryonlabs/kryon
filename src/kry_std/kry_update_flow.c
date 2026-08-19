/*
 * kry_update_flow.c - the embeddable self-update lifecycle over kry_update.
 * Apps provide persistence and presentation; this file owns the rest.
 */
#include "kry_update_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

struct KryUpdateFlow {
    /* immutable after start */
    char app_name[64];
    char current_version[32];
    char appcast_url[512];

    KryUpdateChannelInfo artifact;   /* channel entry, valid when found */
    int have_artifact;
    KryUpdateChannel channel;

    KryUpdateExtractFn extract;
    void *extract_user;

    KryUpdateInfo info;              /* cached appcast of the newest release */
    char new_version[32];
    char release_url[512];
    char error[160];
    char verified_path[600];

    KryUpdateCheck *check;
    KryUpdateDownload *download;

    KryUpdateFlowState state;
    int apply_armed;                 /* quit requested; AppImage exec at exit */
};

KryUpdateFlow *
kry_update_flow_start(const KryUpdateFlowConfig *cfg, const char *appcast_url)
{
    KryUpdateFlow *f;

    if(cfg == NULL || cfg->app_name == NULL || cfg->app_name[0] == '\0' ||
       cfg->current_version == NULL || cfg->current_version[0] == '\0' ||
       appcast_url == NULL || appcast_url[0] == '\0')
        return NULL;
    f = calloc(1, sizeof(*f));
    if(f == NULL)
        return NULL;
    snprintf(f->app_name, sizeof(f->app_name), "%s", cfg->app_name);
    snprintf(f->current_version, sizeof(f->current_version), "%s",
             cfg->current_version);
    snprintf(f->appcast_url, sizeof(f->appcast_url), "%s", appcast_url);
    f->channel = kry_update_detect_channel();
    f->state = KRY_UPDATE_FLOW_CHECKING;
    f->check = kry_update_check(appcast_url, cfg->current_version);
    if(f->check == NULL) {
        free(f);
        return NULL;
    }
    return f;
}

void
kry_update_flow_set_extractor(KryUpdateFlow *flow, KryUpdateExtractFn extract,
                              void *user)
{
    if(flow == NULL)
        return;
    flow->extract = extract;
    flow->extract_user = user;
}

static void
flow_check_done(KryUpdateFlow *f)
{
    KryUpdateStatus s = kry_update_poll(f->check);
    const KryUpdateInfo *info;
    const char *key;
    const KryUpdateChannelInfo *entry = NULL;

    if(s == KRY_UPDATE_PENDING)
        return;
    info = kry_update_info(f->check);
    if((s == KRY_UPDATE_AVAILABLE || s == KRY_UPDATE_UP_TO_DATE) && info != NULL)
        f->info = *info;
    if(s == KRY_UPDATE_AVAILABLE && info != NULL) {
        snprintf(f->new_version, sizeof(f->new_version), "%s", info->version);
        key = kry_update_channel_key(f->channel);
        if(key != NULL)
            entry = kry_update_find_channel(info, key);
        if(entry != NULL && entry->url[0] != '\0') {
            f->artifact = *entry;
            f->have_artifact = 1;
        }
        snprintf(f->release_url, sizeof(f->release_url), "%s",
                 info->notes_url[0] != '\0' ? info->notes_url
                   : (f->have_artifact ? f->artifact.url : ""));
        f->state = KRY_UPDATE_FLOW_AVAILABLE;
    } else if(s == KRY_UPDATE_UP_TO_DATE) {
        f->state = KRY_UPDATE_FLOW_UP_TO_DATE;
    } else {
        snprintf(f->error, sizeof(f->error), "%s",
                 kry_update_error(f->check) != NULL ? kry_update_error(f->check)
                                                    : "update check failed");
        f->state = KRY_UPDATE_FLOW_FAILED;
    }
    kry_update_free(f->check);
    f->check = NULL;
}

static void
flow_download_done(KryUpdateFlow *f)
{
    KryUpdateDownloadStatus s = kry_update_download_poll(f->download);
    const char *path;

    if(s == KRY_UPDATE_DL_PENDING || s == KRY_UPDATE_DL_RUNNING)
        return;
    if(s == KRY_UPDATE_DL_DONE) {
        path = kry_update_download_path(f->download);
        snprintf(f->verified_path, sizeof(f->verified_path), "%s",
                 path != NULL ? path : "");
        if(f->verified_path[0] != '\0') {
            f->state = KRY_UPDATE_FLOW_READY;
        } else {
            snprintf(f->error, sizeof(f->error), "%s", "verified download vanished");
            f->state = KRY_UPDATE_FLOW_FAILED;
        }
    } else {
        snprintf(f->error, sizeof(f->error), "%s",
                 kry_update_download_error(f->download) != NULL
                   ? kry_update_download_error(f->download) : "download failed");
        f->state = KRY_UPDATE_FLOW_FAILED;
    }
    kry_update_download_free(f->download);
    f->download = NULL;
}

void
kry_update_flow_poll(KryUpdateFlow *flow)
{
    if(flow == NULL)
        return;
    if(flow->check != NULL)
        flow_check_done(flow);
    if(flow->download != NULL)
        flow_download_done(flow);
}

KryUpdateFlowState
kry_update_flow_state(const KryUpdateFlow *flow)
{
    return flow != NULL ? flow->state : KRY_UPDATE_FLOW_IDLE;
}

const KryUpdateInfo *
kry_update_flow_appcast(const KryUpdateFlow *flow)
{
    if(flow == NULL || flow->check != NULL)
        return NULL;
    if(flow->state == KRY_UPDATE_FLOW_IDLE || flow->state == KRY_UPDATE_FLOW_FAILED)
        return NULL;
    return &flow->info;
}

KryUpdateChannel
kry_update_flow_channel(const KryUpdateFlow *flow)
{
    return flow != NULL ? flow->channel : KRY_UPDATE_CHANNEL_UNKNOWN;
}

const KryUpdateChannelInfo *
kry_update_flow_artifact(const KryUpdateFlow *flow)
{
    if(flow == NULL || !flow->have_artifact)
        return NULL;
    return &flow->artifact;
}

const char *
kry_update_flow_new_version(const KryUpdateFlow *flow)
{
    return flow != NULL ? flow->new_version : "";
}

const char *
kry_update_flow_release_url(const KryUpdateFlow *flow)
{
    return flow != NULL ? flow->release_url : "";
}

double
kry_update_flow_progress(const KryUpdateFlow *flow)
{
    if(flow == NULL || flow->download == NULL)
        return -1.0;
    return kry_update_download_progress(flow->download);
}

const char *
kry_update_flow_error(const KryUpdateFlow *flow)
{
    if(flow == NULL || flow->state != KRY_UPDATE_FLOW_FAILED)
        return NULL;
    return flow->error;
}

int
kry_update_flow_download(KryUpdateFlow *flow)
{
    char dir[512];

    if(flow == NULL || flow->download != NULL ||
       (flow->state != KRY_UPDATE_FLOW_AVAILABLE &&
        flow->state != KRY_UPDATE_FLOW_FAILED))
        return 0;
    if(!flow->have_artifact)
        return 0;
    if(!kry_update_download_dir(flow->app_name, dir, sizeof(dir)))
        return 0;
    flow->download = kry_update_download_begin(&flow->artifact, dir);
    if(flow->download == NULL)
        return 0;
    flow->error[0] = '\0';
    flow->state = KRY_UPDATE_FLOW_DOWNLOADING;
    return 1;
}

#ifdef _WIN32

/* Extract beside the install dir and hand the swap to the detached
 * script; the app should quit (exec_pending) right after. */
static int
flow_windows_apply(KryUpdateFlow *f)
{
    char exe_path[MAX_PATH];
    char new_dir[MAX_PATH + 32];
    const char *slash;
    DWORD len;

    if(f->extract == NULL)
        return 0;
    len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if(len == 0 || len >= sizeof(exe_path))
        return 0;
    slash = strrchr(exe_path, '\\');
    if(slash == NULL || slash == exe_path)
        return 0;
    snprintf(new_dir, sizeof(new_dir), "%.*s-update-new", (int)(slash - exe_path),
             exe_path);
    if(!f->extract(f->verified_path, new_dir, f->extract_user))
        return 0;
    return kry_update_windows_stage_swap(new_dir) == KRY_UPDATE_APPLY_RESTARTING;
}

#endif /* _WIN32 */

int
kry_update_flow_apply(KryUpdateFlow *flow)
{
    if(flow == NULL || flow->state != KRY_UPDATE_FLOW_READY)
        return 0;
#ifdef _WIN32
    if(flow_windows_apply(flow)) {
        flow->apply_armed = 1;
        flow->state = KRY_UPDATE_FLOW_IDLE;
        return 1;
    }
    snprintf(flow->error, sizeof(flow->error), "%s", "could not stage update");
    flow->state = KRY_UPDATE_FLOW_FAILED;
    return 1;
#else
    if(flow->channel == KRY_UPDATE_CHANNEL_APPIMAGE) {
        flow->apply_armed = 1;   /* exec happens once the app is shut down */
        flow->state = KRY_UPDATE_FLOW_IDLE;
        return 1;
    }
    return 0;
#endif
}

int
kry_update_flow_exec_pending(KryUpdateFlow *flow)
{
    if(flow == NULL || !flow->apply_armed || flow->verified_path[0] == '\0')
        return 0;
    flow->apply_armed = 0;
#ifndef _WIN32
    kry_update_appimage_apply(flow->verified_path);
#endif
    return 1;
}

void
kry_update_flow_free(KryUpdateFlow *flow)
{
    if(flow == NULL)
        return;
    kry_update_free(flow->check);
    kry_update_download_free(flow->download);
    free(flow);
}
