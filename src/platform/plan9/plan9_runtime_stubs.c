#ifdef KRYON_NATIVE_PLAN9

#include "kryon_plan9.h"
#include "kry_update_flow.h"
#include "kry_uri.h"
#include "sync/account.h"
#include "sync.h"
#include "notification.h"
#include "notification_schedule.h"
#include "platform.h"

int
KryThreadStart(KryThread *thread, KryThreadMain fn, void *userdata)
{
    (void)thread;
    (void)fn;
    (void)userdata;
    return 0;
}

void KryThreadDetach(KryThread *thread) { (void)thread; }
void KryThreadJoin(KryThread *thread) { (void)thread; }
void KrySleepSeconds(int seconds) { if(seconds > 0) sleep(seconds * 1000); }
void KryMutexInit(KryMutex *mutex) { if(mutex != nil) mutex->lock = 0; }
void KryMutexLock(KryMutex *mutex) { (void)mutex; }
void KryMutexUnlock(KryMutex *mutex) { (void)mutex; }

void KryonRaylibBackend_rlDrawRenderBatch(void) { }

void glReadPixels(int x, int y, int width, int height, unsigned int format,
                  unsigned int type, void *data)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)data;
}

void InjectMousePosition(float x, float y) { (void)x; (void)y; }
void InjectMouseButton(int button, int down) { (void)button; (void)down; }
void InjectKey(int key, int down) { (void)key; (void)down; }
void InjectKeyTap(int key) { (void)key; }
void InjectLayoutKey(int codepoint, int down) { (void)codepoint; (void)down; }
void InjectLayoutKeyTap(int codepoint) { (void)codepoint; }
void InjectText(const char *text) { (void)text; }
void InjectWheel(float move) { (void)move; }
void InjectTap(float x, float y) { (void)x; (void)y; }
void InjectPump(void) { }
int InjectMouseActive(void) { return 0; }
float InjectMouseX(void) { return 0.0f; }
float InjectMouseY(void) { return 0.0f; }
float InjectMouseDeltaX(void) { return 0.0f; }
float InjectMouseDeltaY(void) { return 0.0f; }
float InjectWheelValue(void) { return 0.0f; }
int InjectMousePressed(int button) { (void)button; return 0; }
int InjectMouseReleased(int button) { (void)button; return 0; }
int InjectMouseButtonDown(int button) { (void)button; return 0; }
int InjectMouseButtonUp(int button) { (void)button; return 1; }
int InjectKeyPressed(int key) { (void)key; return 0; }
int InjectKeyReleased(int key) { (void)key; return 0; }
int InjectKeyDown(int key) { (void)key; return 0; }
int InjectLayoutKeyPressed(int codepoint) { (void)codepoint; return 0; }
int InjectLayoutKeyReleased(int codepoint) { (void)codepoint; return 0; }
int InjectLayoutKeyDown(int codepoint) { (void)codepoint; return 0; }
int InjectCharPressed(void) { return 0; }
int InjectKeyPressedCode(void) { return 0; }
void InjectReset(void) { }

int CanOpenURI(const char *uri) { (void)uri; return 0; }
int OpenURI(const char *uri) { (void)uri; return 0; }
void OpenURL(const char *url) { (void)OpenURI(url); }

int
KryNotificationDayKeyNow(void)
{
    return 0;
}

int
KryNotificationReminderDue(const KryNotificationReminder *reminder,
                           int day_key, int hour)
{
    (void)reminder;
    (void)day_key;
    (void)hour;
    return 0;
}

int
KryNotificationSendReminder(KryNotificationReminder *reminder,
                            const char *title, const char *body,
                            int day_key, int hour)
{
    (void)reminder;
    (void)title;
    (void)body;
    (void)day_key;
    (void)hour;
    return 0;
}

int IsSyncAccountAvailable(void) { return 0; }

int
HasSyncAccountValues(const SyncAccount *account)
{
    return account != nil && account->public_id[0] != '\0' &&
           account->public_key_hex[0] != '\0' &&
           account->private_key_hex[0] != '\0';
}

int CreateSyncAccount(SyncAccount *account) { (void)account; return 0; }
int ValidateSyncAccount(SyncAccount *account) { (void)account; return 0; }
int ParseSyncAccountText(const char *text, SyncAccount *account)
{
    (void)text;
    (void)account;
    return 0;
}
int ExportSyncAccountText(const SyncAccount *account, char *out,
                           size_t out_size)
{
    (void)account;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return 0;
}
int ImportSyncAccountFile(const char *filename, SyncAccount *account)
{
    (void)filename;
    (void)account;
    return 0;
}
int ExportSyncAccountFile(const SyncAccount *account, const char *filename)
{
    (void)account;
    (void)filename;
    return 0;
}
int ExportSyncAccountTextEncrypted(const SyncAccount *account,
                                    const char *passphrase, char *out,
                                    size_t out_size)
{
    (void)account;
    (void)passphrase;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return 0;
}
int ExportSyncAccountFileEncrypted(const SyncAccount *account,
                                    const char *passphrase,
                                    const char *filename)
{
    (void)account;
    (void)passphrase;
    (void)filename;
    return 0;
}
int ParseSyncAccountTextEncrypted(const char *text, const char *passphrase,
                                   SyncAccount *account)
{
    (void)text;
    (void)passphrase;
    (void)account;
    return 0;
}
int ImportSyncAccountFileEncrypted(const char *filename,
                                    const char *passphrase,
                                    SyncAccount *account)
{
    (void)filename;
    (void)passphrase;
    (void)account;
    return 0;
}
void SyncSha256Hex(const uint8_t *data, size_t len,
                    char out_hex[SYNC_PUBLIC_ID_HEX_SIZE])
{
    int i;

    (void)data;
    (void)len;
    if(out_hex == nil)
        return;
    for(i = 0; i < SYNC_PUBLIC_ID_HEX_SIZE - 1; i++)
        out_hex[i] = '0';
    out_hex[SYNC_PUBLIC_ID_HEX_SIZE - 1] = '\0';
}
int SignSyncAccountHex(const SyncAccount *account, const uint8_t *message,
                        size_t message_len, char *out_signature_hex,
                        size_t out_size)
{
    (void)account;
    (void)message;
    (void)message_len;
    if(out_signature_hex != nil && out_size > 0)
        out_signature_hex[0] = '\0';
    return 0;
}

const char *
GetSyncResultName(SyncResult result)
{
    switch(result) {
    case SYNC_OK: return "ok";
    case SYNC_INVALID_URL: return "invalid-url";
    case SYNC_NO_ACCOUNT: return "no-account";
    case SYNC_PAYLOAD_FAILED: return "payload-failed";
    case SYNC_CHALLENGE_FAILED: return "challenge-failed";
    case SYNC_SIGN_FAILED: return "sign-failed";
    case SYNC_REQUEST_FAILED: return "request-failed";
    case SYNC_AUTH_FAILED: return "auth-failed";
    }
    return "unknown";
}
int IsSyncURLValid(const char *url) { return url != nil && url[0] != '\0'; }
int NormalizeSyncURL(const char *input, char *out, size_t out_size)
{
    if(out != nil && out_size > 0)
        snprint(out, out_size, "%s", input != nil ? input : "");
    return input != nil && input[0] != '\0';
}
int JoinSyncURL(char *out, size_t out_size, const char *base_url,
                     const char *path)
{
    if(out != nil && out_size > 0)
        snprint(out, out_size, "%s%s", base_url != nil ? base_url : "",
                path != nil ? path : "");
    return out != nil && out_size > 0;
}
int JoinSyncWebSocketURL(char *out, size_t out_size,
                              const char *base_url, const char *path)
{
    return JoinSyncURL(out, out_size, base_url, path);
}
int AppendSyncBuffer(SyncBuffer *buffer, const void *data,
                          size_t bytes)
{
    (void)buffer;
    (void)data;
    (void)bytes;
    return 0;
}
int AppendSyncBufferJSONString(SyncBuffer *buffer, const char *text)
{
    (void)buffer;
    (void)text;
    return 0;
}
void FreeSyncBuffer(SyncBuffer *buffer)
{
    if(buffer != nil) {
        free(buffer->data);
        buffer->data = nil;
        buffer->len = 0;
        buffer->cap = 0;
    }
}
int FindSyncJSONString(const char *json, const char *key,
                            char *out, size_t out_size)
{
    (void)json;
    (void)key;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return 0;
}
long long FindSyncJSONInt64(const char *json, const char *key,
                                 long long fallback)
{
    (void)json;
    (void)key;
    return fallback;
}
void ClearSyncAuthToken(const SyncConfig *cfg) { (void)cfg; }
SyncResult LoginSync(const SyncConfig *cfg)
{
    (void)cfg;
    return SYNC_REQUEST_FAILED;
}
SyncResult RunSync(const SyncConfig *cfg)
{
    (void)cfg;
    return SYNC_REQUEST_FAILED;
}
SyncResult RequestSyncBearer(const SyncConfig *cfg,
                                       const char *method, const char *path,
                                       const char *body, char *out,
                                       size_t out_size)
{
    (void)cfg;
    (void)method;
    (void)path;
    (void)body;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return SYNC_REQUEST_FAILED;
}
SyncResult DeleteSyncAccount(const SyncConfig *cfg)
{
    (void)cfg;
    return SYNC_REQUEST_FAILED;
}
int WrapSyncPayload(const SyncAccount *account, const char *payload,
                         char **out)
{
    (void)account;
    (void)payload;
    if(out != nil)
        *out = nil;
    return 0;
}
int UnwrapSyncPayload(const SyncAccount *account,
                           const char *envelope_json, char **out)
{
    (void)account;
    (void)envelope_json;
    if(out != nil)
        *out = nil;
    return 0;
}
int DefaultSyncHttpRequest(const char *method, const char *url,
                            const char *body,
                            const char *const *headers, int header_count,
                            SyncBuffer *response, long *status,
                            void *user)
{
    (void)method;
    (void)url;
    (void)body;
    (void)headers;
    (void)header_count;
    (void)response;
    (void)user;
    if(status != nil)
        *status = 0;
    return 0;
}
SyncResult WaitForRemoteSyncEvent(const SyncConfig *cfg,
                                     const char *path)
{
    (void)cfg;
    (void)path;
    return SYNC_REQUEST_FAILED;
}

KryUpdateChannel kry_update_detect_channel(void) { return KRY_UPDATE_CHANNEL_SOURCE; }
const char *kry_update_channel_name(KryUpdateChannel channel)
{
    (void)channel;
    return "Source";
}
const char *kry_update_channel_key(KryUpdateChannel channel)
{
    (void)channel;
    return nil;
}
int kry_update_version_compare(const char *a, const char *b)
{
    (void)a;
    (void)b;
    return 0;
}
const KryUpdateChannelInfo *kry_update_find_channel(const KryUpdateInfo *info,
                                                    const char *name)
{
    (void)info;
    (void)name;
    return nil;
}
int kry_update_appcast_parse(const char *json, KryUpdateInfo *out)
{
    (void)json;
    if(out != nil)
        memset(out, 0, sizeof(*out));
    return 0;
}
KryUpdateCheck *kry_update_check(const char *appcast_url,
                                 const char *current_version)
{
    (void)appcast_url;
    (void)current_version;
    return nil;
}
KryUpdateStatus kry_update_poll(KryUpdateCheck *check)
{
    (void)check;
    return KRY_UPDATE_FAILED;
}
const KryUpdateInfo *kry_update_info(KryUpdateCheck *check)
{
    (void)check;
    return nil;
}
const char *kry_update_error(KryUpdateCheck *check)
{
    (void)check;
    return "updates unavailable";
}
void kry_update_free(KryUpdateCheck *check) { (void)check; }
int kry_update_download_dir(const char *app_name, char *out, int cap)
{
    (void)app_name;
    if(out != nil && cap > 0)
        out[0] = '\0';
    return 0;
}
KryUpdateDownload *kry_update_download_begin(const KryUpdateChannelInfo *entry,
                                             const char *dest_dir)
{
    (void)entry;
    (void)dest_dir;
    return nil;
}
KryUpdateDownloadStatus kry_update_download_poll(KryUpdateDownload *dl)
{
    (void)dl;
    return KRY_UPDATE_DL_FAILED;
}
double kry_update_download_progress(const KryUpdateDownload *dl)
{
    (void)dl;
    return -1.0;
}
const char *kry_update_download_error(const KryUpdateDownload *dl)
{
    (void)dl;
    return "downloads unavailable";
}
const char *kry_update_download_path(const KryUpdateDownload *dl)
{
    (void)dl;
    return nil;
}
void kry_update_download_free(KryUpdateDownload *dl) { (void)dl; }
int kry_update_appimage_stage(const char *downloaded_path,
                              const char *appimage_path)
{
    (void)downloaded_path;
    (void)appimage_path;
    return 0;
}
KryUpdateApplyResult kry_update_appimage_apply(const char *downloaded_path)
{
    (void)downloaded_path;
    return KRY_UPDATE_APPLY_NOT_APPLICABLE;
}
KryUpdateApplyResult kry_update_windows_stage_swap(const char *new_dir)
{
    (void)new_dir;
    return KRY_UPDATE_APPLY_NOT_APPLICABLE;
}

KryUpdateFlow *kry_update_flow_start(const KryUpdateFlowConfig *cfg,
                                     const char *appcast_url)
{
    (void)cfg;
    (void)appcast_url;
    return nil;
}
void kry_update_flow_set_extractor(KryUpdateFlow *flow,
                                   KryUpdateExtractFn extract, void *user)
{
    (void)flow;
    (void)extract;
    (void)user;
}
void kry_update_flow_poll(KryUpdateFlow *flow) { (void)flow; }
KryUpdateFlowState kry_update_flow_state(const KryUpdateFlow *flow)
{
    (void)flow;
    return KRY_UPDATE_FLOW_FAILED;
}
const KryUpdateInfo *kry_update_flow_appcast(const KryUpdateFlow *flow)
{
    (void)flow;
    return nil;
}
KryUpdateChannel kry_update_flow_channel(const KryUpdateFlow *flow)
{
    (void)flow;
    return KRY_UPDATE_CHANNEL_SOURCE;
}
const KryUpdateChannelInfo *kry_update_flow_artifact(const KryUpdateFlow *flow)
{
    (void)flow;
    return nil;
}
const char *kry_update_flow_new_version(const KryUpdateFlow *flow)
{
    (void)flow;
    return "";
}
const char *kry_update_flow_release_url(const KryUpdateFlow *flow)
{
    (void)flow;
    return "";
}
double kry_update_flow_progress(const KryUpdateFlow *flow)
{
    (void)flow;
    return -1.0;
}
const char *kry_update_flow_error(const KryUpdateFlow *flow)
{
    (void)flow;
    return "updates unavailable";
}
int kry_update_flow_download(KryUpdateFlow *flow)
{
    (void)flow;
    return 0;
}
int kry_update_flow_apply(KryUpdateFlow *flow)
{
    (void)flow;
    return 0;
}
int kry_update_flow_exec_pending(KryUpdateFlow *flow)
{
    (void)flow;
    return 0;
}
void kry_update_flow_free(KryUpdateFlow *flow) { (void)flow; }

/* Plan 9 has no rename(2); renaming within a directory is a name wstat.
 * Cross-directory moves fail, which still covers the atomic-save pattern
 * (write temp file, rename over target) kry_fs_move is used for. */
int
rename(const char *oldpath, const char *newpath)
{
    Dir dir;
    char *oldslash;
    char *newslash;
    char *base;

    if(oldpath == nil || newpath == nil)
        return -1;
    oldslash = strrchr(oldpath, '/');
    newslash = strrchr(newpath, '/');
    base = newslash != nil ? newslash + 1 : (char *)newpath;
    if(base[0] == '\0')
        return -1;
    if((oldslash == nil) != (newslash == nil))
        return -1;
    if(oldslash != nil &&
       (oldslash - oldpath != newslash - newpath ||
        strncmp(oldpath, newpath, oldslash - oldpath) != 0))
        return -1;

    memset(&dir, 0, sizeof(dir));
    dir.name = base;
    if(dirwstat(oldpath, &dir) < 0)
        return -1;
    return 0;
}

#endif
