#ifdef KRYON_NATIVE_PLAN9

#include "kryon_plan9.h"
#include "kry_update_flow.h"
#include "kry_uri.h"
#include "ksync_account.h"
#include "ksync_sync.h"
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

void KryonInjectMousePosition(float x, float y) { (void)x; (void)y; }
void KryonInjectMouseButton(int button, int down) { (void)button; (void)down; }
void KryonInjectKey(int key, int down) { (void)key; (void)down; }
void KryonInjectKeyTap(int key) { (void)key; }
void KryonInjectText(const char *text) { (void)text; }
void KryonInjectWheel(float move) { (void)move; }
void KryonInjectTap(float x, float y) { (void)x; (void)y; }
void KryonInjectPump(void) { }
int KryonInjectMouseActive(void) { return 0; }
float KryonInjectMouseX(void) { return 0.0f; }
float KryonInjectMouseY(void) { return 0.0f; }
float KryonInjectMouseDeltaX(void) { return 0.0f; }
float KryonInjectMouseDeltaY(void) { return 0.0f; }
float KryonInjectWheelValue(void) { return 0.0f; }
int KryonInjectMousePressed(int button) { (void)button; return 0; }
int KryonInjectMouseReleased(int button) { (void)button; return 0; }
int KryonInjectMouseButtonDown(int button) { (void)button; return 0; }
int KryonInjectMouseButtonUp(int button) { (void)button; return 1; }
int KryonInjectKeyPressed(int key) { (void)key; return 0; }
int KryonInjectKeyReleased(int key) { (void)key; return 0; }
int KryonInjectKeyDown(int key) { (void)key; return 0; }
int KryonInjectCharPressed(void) { return 0; }
int KryonInjectKeyPressedCode(void) { return 0; }
void KryonInjectReset(void) { }

int CanOpenURI(const char *uri) { (void)uri; return 0; }
int OpenURI(const char *uri) { (void)uri; return 0; }

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

int IsKsyncAccountAvailable(void) { return 0; }

int
HasKsyncAccountValues(const KsyncAccount *account)
{
    return account != nil && account->public_id[0] != '\0' &&
           account->public_key_hex[0] != '\0' &&
           account->private_key_hex[0] != '\0';
}

int CreateKsyncAccount(KsyncAccount *account) { (void)account; return 0; }
int ValidateKsyncAccount(KsyncAccount *account) { (void)account; return 0; }
int ParseKsyncAccountText(const char *text, KsyncAccount *account)
{
    (void)text;
    (void)account;
    return 0;
}
int ExportKsyncAccountText(const KsyncAccount *account, char *out,
                           size_t out_size)
{
    (void)account;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return 0;
}
int ImportKsyncAccountFile(const char *filename, KsyncAccount *account)
{
    (void)filename;
    (void)account;
    return 0;
}
int ExportKsyncAccountFile(const KsyncAccount *account, const char *filename)
{
    (void)account;
    (void)filename;
    return 0;
}
int ExportKsyncAccountTextEncrypted(const KsyncAccount *account,
                                    const char *passphrase, char *out,
                                    size_t out_size)
{
    (void)account;
    (void)passphrase;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return 0;
}
int ExportKsyncAccountFileEncrypted(const KsyncAccount *account,
                                    const char *passphrase,
                                    const char *filename)
{
    (void)account;
    (void)passphrase;
    (void)filename;
    return 0;
}
int ParseKsyncAccountTextEncrypted(const char *text, const char *passphrase,
                                   KsyncAccount *account)
{
    (void)text;
    (void)passphrase;
    (void)account;
    return 0;
}
int ImportKsyncAccountFileEncrypted(const char *filename,
                                    const char *passphrase,
                                    KsyncAccount *account)
{
    (void)filename;
    (void)passphrase;
    (void)account;
    return 0;
}
void KsyncSha256Hex(const uint8_t *data, size_t len,
                    char out_hex[KSYNC_PUBLIC_ID_HEX_SIZE])
{
    int i;

    (void)data;
    (void)len;
    if(out_hex == nil)
        return;
    for(i = 0; i < KSYNC_PUBLIC_ID_HEX_SIZE - 1; i++)
        out_hex[i] = '0';
    out_hex[KSYNC_PUBLIC_ID_HEX_SIZE - 1] = '\0';
}
int SignKsyncAccountHex(const KsyncAccount *account, const uint8_t *message,
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
GetKsyncSyncResultName(KsyncSyncResult result)
{
    switch(result) {
    case KSYNC_SYNC_OK: return "ok";
    case KSYNC_SYNC_INVALID_URL: return "invalid-url";
    case KSYNC_SYNC_NO_ACCOUNT: return "no-account";
    case KSYNC_SYNC_PAYLOAD_FAILED: return "payload-failed";
    case KSYNC_SYNC_CHALLENGE_FAILED: return "challenge-failed";
    case KSYNC_SYNC_SIGN_FAILED: return "sign-failed";
    case KSYNC_SYNC_REQUEST_FAILED: return "request-failed";
    case KSYNC_SYNC_AUTH_FAILED: return "auth-failed";
    }
    return "unknown";
}
int IsKsyncSyncURLValid(const char *url) { return url != nil && url[0] != '\0'; }
int NormalizeKsyncSyncURL(const char *input, char *out, size_t out_size)
{
    if(out != nil && out_size > 0)
        snprint(out, out_size, "%s", input != nil ? input : "");
    return input != nil && input[0] != '\0';
}
int JoinKsyncSyncURL(char *out, size_t out_size, const char *base_url,
                     const char *path)
{
    if(out != nil && out_size > 0)
        snprint(out, out_size, "%s%s", base_url != nil ? base_url : "",
                path != nil ? path : "");
    return out != nil && out_size > 0;
}
int JoinKsyncSyncWebSocketURL(char *out, size_t out_size,
                              const char *base_url, const char *path)
{
    return JoinKsyncSyncURL(out, out_size, base_url, path);
}
int AppendKsyncSyncBuffer(KsyncSyncBuffer *buffer, const void *data,
                          size_t bytes)
{
    (void)buffer;
    (void)data;
    (void)bytes;
    return 0;
}
int AppendKsyncSyncBufferJSONString(KsyncSyncBuffer *buffer, const char *text)
{
    (void)buffer;
    (void)text;
    return 0;
}
void FreeKsyncSyncBuffer(KsyncSyncBuffer *buffer)
{
    if(buffer != nil) {
        free(buffer->data);
        buffer->data = nil;
        buffer->len = 0;
        buffer->cap = 0;
    }
}
int FindKsyncSyncJSONString(const char *json, const char *key,
                            char *out, size_t out_size)
{
    (void)json;
    (void)key;
    if(out != nil && out_size > 0)
        out[0] = '\0';
    return 0;
}
long long FindKsyncSyncJSONInt64(const char *json, const char *key,
                                 long long fallback)
{
    (void)json;
    (void)key;
    return fallback;
}
void ClearKsyncSyncAuthToken(const KsyncSyncConfig *cfg) { (void)cfg; }
KsyncSyncResult LoginKsyncSync(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return KSYNC_SYNC_REQUEST_FAILED;
}
KsyncSyncResult RunKsyncSync(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return KSYNC_SYNC_REQUEST_FAILED;
}
KsyncSyncResult RequestKsyncSyncBearer(const KsyncSyncConfig *cfg,
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
    return KSYNC_SYNC_REQUEST_FAILED;
}
KsyncSyncResult DeleteKsyncSyncAccount(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return KSYNC_SYNC_REQUEST_FAILED;
}
int WrapKsyncSyncPayload(const KsyncAccount *account, const char *payload,
                         char **out)
{
    (void)account;
    (void)payload;
    if(out != nil)
        *out = nil;
    return 0;
}
int UnwrapKsyncSyncPayload(const KsyncAccount *account,
                           const char *envelope_json, char **out)
{
    (void)account;
    (void)envelope_json;
    if(out != nil)
        *out = nil;
    return 0;
}
int KsyncDefaultHttpRequest(const char *method, const char *url,
                            const char *body,
                            const char *const *headers, int header_count,
                            KsyncSyncBuffer *response, long *status,
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
KsyncSyncResult KsyncRemoteEventWait(const KsyncSyncConfig *cfg,
                                     const char *path)
{
    (void)cfg;
    (void)path;
    return KSYNC_SYNC_REQUEST_FAILED;
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

#endif
