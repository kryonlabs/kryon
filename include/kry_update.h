/*
 * kry_update.h - Kry standard library: desktop update checks.
 *
 * Phase-1 scope: fetch an appcast JSON describing the latest release,
 * compare it against the running version, and identify which desktop
 * channel the app is running in so the host can present the update the
 * way that channel expects (system-managed channels get info text, self-
 * service channels get a download link). Nothing here downloads or
 * replaces files — later phases may add that on top of the same appcast.
 *
 * The appcast format (published as a release asset by each app's release
 * workflow, fetchable from a version-less URL like
 * .../releases/latest/download/appcast.json):
 *
 *   {
 *     "version": "1.9.5",
 *     "date": "2026-08-19",
 *     "notes": "Short user-facing summary.",
 *     "notes_url": "https://github.com/<owner>/<app>/releases/tag/v1.9.5",
 *     "channels": {
 *       "appimage": { "url": "...", "sha256": "...", "size": 12345678 },
 *       "windows":  { "url": "...", "sha256": "...", "size": 12345678 },
 *       "deb":      { "url": "..." }
 *     }
 *   }
 *
 * Requests run through kry_http on a worker thread, so a frame loop can
 * poll kry_update_poll without blocking.
 */
#ifndef KRYON_KRY_UPDATE_H
#define KRYON_KRY_UPDATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- channel detection ------------------------------------------------- */

typedef enum {
    KRY_UPDATE_CHANNEL_UNKNOWN = 0,
    KRY_UPDATE_CHANNEL_APPIMAGE,         /* AppImage runtime (APPIMAGE env) */
    KRY_UPDATE_CHANNEL_SNAP,             /* SNAP env */
    KRY_UPDATE_CHANNEL_FLATPAK,          /* FLATPAK_ID env */
    KRY_UPDATE_CHANNEL_WINDOWS_PORTABLE, /* _WIN32 build (zip is portable) */
    KRY_UPDATE_CHANNEL_PACKAGE,          /* /usr, /usr/local, /opt (deb/rpm/pkg) */
    KRY_UPDATE_CHANNEL_SOURCE,           /* run from a build/home directory */
} KryUpdateChannel;

/* Where this process is running from, from definitive env vars first
 * (APPIMAGE/SNAP/FLATPAK_ID are set by the respective runtimes), then
 * the executable path. Deterministic and cheap; safe to call once. */
KryUpdateChannel kry_update_detect_channel(void);

/* Stable display name ("AppImage", "Flatpak", ...). Never NULL. */
const char *kry_update_channel_name(KryUpdateChannel channel);

/* Appcast key the running channel should download from ("appimage",
 * "windows"), or NULL when the OS owns updates for this channel
 * (package managers, snap, flatpak, source builds). */
const char *kry_update_channel_key(KryUpdateChannel channel);

/* --- versions ----------------------------------------------------------- */

/* Compare dotted numeric versions ("1.9.4" vs "v1.10"), ignoring any
 * leading "v" and any "-suffix". Missing components count as 0, so
 * "1.9" equals "1.9.0". Returns <0 when a < b, 0 when equal, >0 when
 * a > b. Malformed input compares as 0 against everything. */
int kry_update_version_compare(const char *a, const char *b);

/* --- appcast ------------------------------------------------------------ */

#define KRY_UPDATE_MAX_CHANNELS 8

typedef struct {
    char name[24];      /* appcast key, e.g. "appimage" */
    char url[512];
    char sha256[65];    /* lowercase hex or empty */
    unsigned long size; /* bytes, 0 when unknown */
} KryUpdateChannelInfo;

typedef struct {
    char version[32];
    char date[24];
    char notes[512];
    char notes_url[512];
    KryUpdateChannelInfo channels[KRY_UPDATE_MAX_CHANNELS];
    int channel_count;
} KryUpdateInfo;

/* Channel lookup by appcast key. NULL when absent. */
const KryUpdateChannelInfo *kry_update_find_channel(const KryUpdateInfo *info,
                                                    const char *name);

/* Parse an appcast document into `out` (zeroed first). Requires at
 * least a version string. Returns 1 on success, 0 on malformed input. */
int kry_update_appcast_parse(const char *json, KryUpdateInfo *out);

/* --- async check -------------------------------------------------------- */

typedef struct KryUpdateCheck KryUpdateCheck;

typedef enum {
    KRY_UPDATE_PENDING,    /* request in flight */
    KRY_UPDATE_AVAILABLE,  /* appcast newer than current_version */
    KRY_UPDATE_UP_TO_DATE, /* appcast same or older */
    KRY_UPDATE_FAILED,     /* transport or parse error */
} KryUpdateStatus;

/* Start a check against `appcast_url` (any scheme kry_http supports,
 * including file:// for offline tests). Returns NULL when the HTTP
 * client is unavailable on this platform. */
KryUpdateCheck *kry_update_check(const char *appcast_url,
                                 const char *current_version);

/* Current state; cheap, call every frame. Terminal states never change. */
KryUpdateStatus kry_update_poll(KryUpdateCheck *check);

/* Parsed appcast once AVAILABLE or UP_TO_DATE, NULL otherwise. Owned by
 * the check; valid until kry_update_free. */
const KryUpdateInfo *kry_update_info(KryUpdateCheck *check);

/* Diagnostic when FAILED, NULL otherwise. Owned by the check. */
const char *kry_update_error(KryUpdateCheck *check);

/* Release the check (frees the underlying request). NULL is allowed. */
void kry_update_free(KryUpdateCheck *check);

/* --- self-update: download, verify, apply -------------------------------
 *
 * These target the two channels without a system updater (AppImage,
 * Windows portable). Packaged channels (deb/rpm/pkg, Snap, Flatpak) and
 * source builds must stay on check-and-notify only.
 */

/* A per-app staging dir for downloads: $XDG_DATA_HOME/<app>/updates
 * (falling back to ~/.local/share) on POSIX, %LOCALAPPDATA%\<app>\updates
 * on Windows. Creates it when missing. Returns 1 on success. */
int kry_update_download_dir(const char *app_name, char *out, int cap);

typedef struct KryUpdateDownload KryUpdateDownload;

typedef enum {
    KRY_UPDATE_DL_PENDING,  /* transfer in flight */
    KRY_UPDATE_DL_RUNNING,
    KRY_UPDATE_DL_DONE,     /* downloaded and, when provided, sha256 verified */
    KRY_UPDATE_DL_FAILED,   /* partial file removed; error set */
} KryUpdateDownloadStatus;

/* Download `entry` into `dest_dir` (kry_update_download_dir output or any
 * writable dir). The file name is the last URL segment. The sha256 from
 * the appcast entry is verified on completion; a mismatch fails and
 * removes the file. Returns NULL when downloads are unavailable. */
KryUpdateDownload *kry_update_download_begin(const KryUpdateChannelInfo *entry,
                                             const char *dest_dir);
KryUpdateDownloadStatus kry_update_download_poll(KryUpdateDownload *dl);
/* 0..1, or -1 while the size is unknown. */
double kry_update_download_progress(const KryUpdateDownload *dl);
const char *kry_update_download_error(const KryUpdateDownload *dl);
/* Verified file path once DONE, NULL otherwise. Owned by the download. */
const char *kry_update_download_path(const KryUpdateDownload *dl);
void kry_update_download_free(KryUpdateDownload *dl);

typedef enum {
    KRY_UPDATE_APPLY_NOT_APPLICABLE = 0, /* wrong channel or missing input */
    KRY_UPDATE_APPLY_FAILED,             /* staged but the swap failed */
    KRY_UPDATE_APPLY_RESTARTING,         /* swap done; caller should quit/reexec */
} KryUpdateApplyResult;

/* AppImage channel: stage `downloaded_path` next to the running AppImage
 * ($APPIMAGE) — same filesystem — chmod 0755, then atomically rename it
 * over the old file. Testable on its own; it does not exec anything.
 * Returns 1 on success. */
int kry_update_appimage_stage(const char *downloaded_path, const char *appimage_path);

/* kry_update_appimage_stage() against the real $APPIMAGE, then re-exec
 * the new AppImage in place of this process. The call does not return on
 * success. Call only after saving state and shutting down UI/audio. */
KryUpdateApplyResult kry_update_appimage_apply(const char *downloaded_path);

/* Windows portable channel: `new_dir` holds the extracted new version
 * with the app executable at its root. Writes a swap script to %TEMP%,
 * launches it detached, and returns KRY_UPDATE_APPLY_RESTARTING — the
 * script waits for this process to exit, moves the old install aside,
 * moves `new_dir` into its place, starts the new executable, and removes
 * the old copy. No-op (NOT_APPLICABLE) on other platforms. */
KryUpdateApplyResult kry_update_windows_stage_swap(const char *new_dir);

#ifdef __cplusplus
}
#endif

#endif
