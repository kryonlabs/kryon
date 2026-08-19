#ifndef DESKTOP_APP_H
#define DESKTOP_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#define DESKTOP_ID_MAX 128
#define DESKTOP_NAME_MAX 128
#define DESKTOP_SUMMARY_MAX 256
#define DESKTOP_PATH_MAX 512

typedef struct DesktopAppInfo {
    const char *app_id;       /* reverse-DNS id; also the desktop-entry id */
    const char *name;         /* short CLI/process name */
    const char *display_name; /* user-visible name */
    const char *summary;
    const char *icon_name;    /* icon theme name */
    const char *wm_class;
    int single_instance;
} DesktopAppInfo;

typedef enum DesktopOpenEventKind {
    DESKTOP_OPEN_NONE = 0,
    DESKTOP_OPEN_FILE,
    DESKTOP_OPEN_URL
} DesktopOpenEventKind;

void InitDesktopApp(const DesktopAppInfo *info);
const DesktopAppInfo *GetDesktopAppInfo(void);

const char *GetDesktopAppID(void);
const char *GetDesktopAppName(void);
const char *GetDesktopDisplayName(void);
const char *GetDesktopIconName(void);

int GetDesktopConfigDir(char *out, int cap);
int GetDesktopDataDir(char *out, int cap);
int GetDesktopCacheDir(char *out, int cap);

/* Returns 1 when this process acquired the lock, 0 when another instance owns
 * it, and -1 when the lock could not be created. The lock is held until
 * ReleaseDesktopSingleInstance() or process exit. */
int AcquireDesktopSingleInstance(const char *app_id, char *lock_path, int cap);
void ReleaseDesktopSingleInstance(void);

int QueueDesktopOpenPath(const char *path_or_url);
DesktopOpenEventKind PollDesktopOpenEvent(char *out, int cap);

#ifdef __cplusplus
}
#endif

#endif /* DESKTOP_APP_H */
