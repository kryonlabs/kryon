/*
 * kry_update_flow.h - Kry standard library: the app-facing self-update
 * flow, built on kry_update (check/download/verify/apply).
 *
 * One object walks the whole lifecycle so every kryon app embeds the same
 * updater instead of re-orchestrating the pieces:
 *
 *   cfg     = {.app_name = "myapp", .current_version = MY_VERSION}
 *   flow    = kry_update_flow_start(&cfg, APPCAST_URL);   // once
 *   every frame: kry_update_flow_poll(flow);
 *   render/state from kry_update_flow_state();
 *   user action: kry_update_flow_download() / kry_update_flow_apply();
 *   after the UI is down: kry_update_flow_exec_pending(flow);
 *
 * Apps own persistence (throttle the start call however they like) and
 * presentation; this layer owns check, channel choice, download, sha256
 * verification, and the apply mechanics:
 *   - AppImage: stage beside the running image, atomic rename, re-exec.
 *   - Windows portable: extract via the registered extractor into a
 *     sibling dir, hand off to the detached swap script, ask to quit.
 *   - system-managed channels (deb/rpm/pkg, Snap, Flatpak) and source
 *     builds never self-replace; the flow stops at AVAILABLE and the app
 *     presents the release URL.
 */
#ifndef KRYON_KRY_UPDATE_FLOW_H
#define KRYON_KRY_UPDATE_FLOW_H

#include "kry_update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KryUpdateFlow KryUpdateFlow;

typedef enum {
    KRY_UPDATE_FLOW_IDLE = 0,     /* no check completed yet */
    KRY_UPDATE_FLOW_CHECKING,     /* appcast request in flight */
    KRY_UPDATE_FLOW_AVAILABLE,    /* newer release; channel artifact or link */
    KRY_UPDATE_FLOW_DOWNLOADING,
    KRY_UPDATE_FLOW_READY,        /* verified; kry_update_flow_apply() next */
    KRY_UPDATE_FLOW_FAILED,       /* download/verify error; retry allowed */
    KRY_UPDATE_FLOW_UP_TO_DATE,   /* checked; no newer release */
} KryUpdateFlowState;

typedef struct {
    const char *app_name;         /* staging dir name; must outlive the flow */
    const char *current_version;  /* dotted, e.g. "1.9.5"; must outlive it */
} KryUpdateFlowConfig;

/* Optional archive extractor for the Windows portable channel: unpack
 * `archive` under `dest_dir`, return 1 on success. Without one the flow
 * cannot swap on Windows and falls back to the release URL. */
typedef int (*KryUpdateExtractFn)(const char *archive, const char *dest_dir,
                                  void *user);

/* Starts the appcast check. Returns NULL on bad arguments or when the
 * HTTP client is unavailable. */
KryUpdateFlow *kry_update_flow_start(const KryUpdateFlowConfig *cfg,
                                     const char *appcast_url);

void kry_update_flow_set_extractor(KryUpdateFlow *flow,
                                   KryUpdateExtractFn extract, void *user);

/* Drive in-flight work; cheap, call every frame. */
void kry_update_flow_poll(KryUpdateFlow *flow);

KryUpdateFlowState kry_update_flow_state(const KryUpdateFlow *flow);

/* Appcast data once AVAILABLE/UP_TO_DATE (NULL before). */
const KryUpdateInfo *kry_update_flow_appcast(const KryUpdateFlow *flow);

/* Channel the app is running on (cached detection). */
KryUpdateChannel kry_update_flow_channel(const KryUpdateFlow *flow);

/* Artifact entry for our channel; NULL on system-managed/source
 * channels — present those with kry_update_flow_release_url(). */
const KryUpdateChannelInfo *kry_update_flow_artifact(const KryUpdateFlow *flow);

/* New version string ("" before AVAILABLE). */
const char *kry_update_flow_new_version(const KryUpdateFlow *flow);

/* Release page URL ("" before AVAILABLE). */
const char *kry_update_flow_release_url(const KryUpdateFlow *flow);

/* 0..1 while DOWNLOADING; -1 otherwise or while size is unknown. */
double kry_update_flow_progress(const KryUpdateFlow *flow);

/* Diagnostic on FAILED (NULL otherwise). */
const char *kry_update_flow_error(const KryUpdateFlow *flow);

/* AVAILABLE/FAILED -> DOWNLOADING. Returns 1 when a download began. */
int kry_update_flow_download(KryUpdateFlow *flow);

/* READY -> apply: AppImage stages+execs (does not return); Windows
 * extracts, launches the swap script, and flags the app to quit; other
 * channels do nothing. Returns 1 when the update path was taken (also
 * when the state was not READY, callers can just call it on the row
 * action). */
int kry_update_flow_apply(KryUpdateFlow *flow);

/* True when apply armed an exit-time action; the app should quit. On
 * AppImage the re-exec happens here, after the app saved state and shut
 * its UI down. */
int kry_update_flow_exec_pending(KryUpdateFlow *flow);

void kry_update_flow_free(KryUpdateFlow *flow);

#ifdef __cplusplus
}
#endif

#endif
