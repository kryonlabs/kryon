#ifndef WEB_H
#define WEB_H

#ifdef __cplusplus
extern "C" {
#endif

int GetWebViewportWidth(int fallback_width);
int GetWebViewportHeight(int fallback_height);
void GetWebViewportSize(int fallback_width, int fallback_height,
                             int *width, int *height);
void SetWebOrientationMode(int mode);

/*
 * Web builds should create a resizable raylib window in CSS-pixel units and
 * keep the window size synced to the browser viewport. Native builds return 0.
 */
unsigned int GetWebWindowFlags(void);

typedef struct WebStorageSyncState {
    int mounted;
    int syncing;
    int pending;
    int last_ok;
    int has_error;
} WebStorageSyncState;

int IsWebStorageSyncPending(void);
void ScheduleWebStorageSync(int delay_ms, int log_success);
void ScheduleWebStorageSyncAfterFrame(int delay_ms, int log_success);
void FlushWebStorageSync(int log_success);
int FlushWebStorageSyncBlocking(int timeout_ms, int log_success);
int GetWebStorageSyncState(WebStorageSyncState *out);

/*
 * Resize raylibs window to the current browser viewport when it changes.
 * Returns 1 when a resize was applied, 0 otherwise.
 */
int SyncWebWindowSize(void);

#ifdef __cplusplus
}
#endif

#endif
