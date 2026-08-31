#include "web.h"

#include "ui_dpi.h"
#include "ui_layout.h"
#include "kryon.h"

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#define KRYON_WEB_JS 1
#endif

#if defined(KRYON_WEB_JS)
#include <emscripten.h>
#endif

#include <string.h>

static int g_web_orientation_mode = 0;
#if defined(PLATFORM_WEB)
static int g_web_storage_after_frame_pending = 0;
static int g_web_storage_after_frame_delay_ms = 0;
static int g_web_storage_after_frame_log_success = 0;
#endif

#if defined(KRYON_WEB_JS)
EM_ASYNC_JS(int, js_web_storage_flush_blocking,
            (int timeout_ms, int log_success), {
    var M = Module || {};
    var timeout = timeout_ms > 0 ? timeout_ms : 5000;
    var flush = typeof M.__kryonFlushStorageSync === 'function'
        ? M.__kryonFlushStorageSync
        : null;

    if (!flush && typeof M.__kryonScheduleStorageSync === 'function') {
        M.__kryonScheduleStorageSync(0, !!log_success);
        flush = typeof M.__kryonFlushStorageSync === 'function'
            ? M.__kryonFlushStorageSync
            : null;
    }
    if (!flush)
        return 0;

    try {
        var promise = flush(!!log_success);
        if (!promise || typeof promise.then !== 'function')
            return M.__kryonStorageSyncLastOk === false ? 0 : 1;
        var timed_out = false;
        var result = await Promise.race([
            promise,
            new Promise(function(resolve) {
                setTimeout(function() {
                    timed_out = true;
                    resolve(false);
                }, timeout);
            })
        ]);
        if (timed_out) {
            M.__kryonStorageSyncLastOk = false;
            M.__kryonStorageSyncLastError = 'timeout';
            return 0;
        }
        return result ? 1 : 0;
    } catch (e) {
        M.__kryonStorageSyncLastOk = false;
        M.__kryonStorageSyncLastError =
            e && e.message ? e.message : String(e);
        return 0;
    }
});

EM_JS(void, js_web_route_set, (int replace, const char *path), {
    var g = globalThis;
    var value = path ? UTF8ToString(path) : "";
    var ensure = function () {
        var R = g.__kryRoute;
        if (!R) {
            R = g.__kryRoute = { route: "", routeVersion: 0, listening: false };
        }
        if (!R.text) {
            R.text = function () {
                var loc = typeof location !== 'undefined' ? location : null;
                if (!loc) return '/';
                return (loc.pathname || '/') + (loc.hash || "");
            };
        }
        if (!R.note) {
            R.note = function () {
                var next = R.text();
                if (next !== R.route) {
                    R.route = next;
                    R.routeVersion++;
                }
                return R.routeVersion | 0;
            };
        }
        if (!R.listening && g.addEventListener) {
            g.addEventListener('popstate', R.note);
            g.addEventListener('hashchange', R.note);
            R.listening = true;
        }
        if (!R.route) R.route = R.text();
        return R;
    };
    var R = ensure();
    if (!value || typeof history === 'undefined') return;
    try {
        if (replace) history.replaceState({}, "", value);
        else history.pushState({}, "", value);
        if (R && R.note) R.note();
    } catch (_) {}
});

EM_JS(void, js_web_route_get, (int which, char *dst, int cap), {
    if (!dst || cap <= 0) return;
    var g = globalThis;
    var loc = typeof location !== 'undefined' ? location : null;
    var text = "";
    if (loc) text = which === 0 ? (loc.pathname || "/") : (loc.hash || "");
    stringToUTF8(text, dst, cap);
});

EM_JS(int, js_web_route_version, (void), {
    var g = globalThis;
    var R = g.__kryRoute;
    if (!R) {
        R = g.__kryRoute = { route: "", routeVersion: 0, listening: false };
        R.text = function () {
            var loc = typeof location !== 'undefined' ? location : null;
            if (!loc) return '/';
            return (loc.pathname || '/') + (loc.hash || "");
        };
        R.note = function () {
            var next = R.text();
            if (next !== R.route) {
                R.route = next;
                R.routeVersion++;
            }
            return R.routeVersion | 0;
        };
    }
    if (!R.text) {
        R.text = function () {
            var loc = typeof location !== 'undefined' ? location : null;
            if (!loc) return '/';
            return (loc.pathname || '/') + (loc.hash || "");
        };
    }
    if (!R.note) {
        R.note = function () {
            var next = R.text();
            if (next !== R.route) {
                R.route = next;
                R.routeVersion++;
            }
            return R.routeVersion | 0;
        };
    }
    if (!R.listening && g.addEventListener) {
        g.addEventListener('popstate', R.note);
        g.addEventListener('hashchange', R.note);
        R.listening = true;
    }
    if (!R.route) R.route = R.text();
    return R.note();
});
#endif

static void
ApplyWebOrientationSize(int *width, int *height)
{
    int w;
    int h;

    if(width == 0 || height == 0)
        return;

    w = *width;
    h = *height;
    if(w <= 0 || h <= 0)
        return;

    if(g_web_orientation_mode == 1 && w > h) {
        int portrait_w = (h * 9) / 16;
        if(portrait_w < 1)
            portrait_w = h;
        *width = portrait_w;
    } else if(g_web_orientation_mode == 2 && h > w) {
        int landscape_h = (w * 9) / 16;
        if(landscape_h < 1)
            landscape_h = w;
        *height = landscape_h;
    }
}

int
GetWebViewportWidth(int fallback_width)
{
#if defined(PLATFORM_WEB)
    int width = 0;
    GetWebViewportSize(fallback_width, 1, &width, 0);
    return width > 0 ? width : fallback_width;
#else
    return fallback_width;
#endif
}

int
GetWebViewportHeight(int fallback_height)
{
#if defined(PLATFORM_WEB)
    int height = 0;
    GetWebViewportSize(1, fallback_height, 0, &height);
    return height > 0 ? height : fallback_height;
#else
    return fallback_height;
#endif
}

void
GetWebViewportSize(int fallback_width, int fallback_height,
                        int *width, int *height)
{
#if defined(PLATFORM_WEB)
    int measured_width = 0;
    int measured_height = 0;

    EM_ASM({
        var out = $0;
        var fallbackW = $1;
        var fallbackH = $2;
        var doc = typeof document !== 'undefined' ? document : null;
        var winW = fallbackW;
        var winH = fallbackH;
        var viewport = typeof visualViewport !== 'undefined' ? visualViewport : null;
        if (viewport && viewport.width > 0 && viewport.height > 0) {
            winW = viewport.width;
            winH = viewport.height;
        } else if (typeof window !== 'undefined') {
            winW = window.innerWidth ||
                (doc && doc.documentElement && doc.documentElement.clientWidth) ||
                fallbackW;
            winH = window.innerHeight ||
                (doc && doc.documentElement && doc.documentElement.clientHeight) ||
                fallbackH;
        }

        var bestW = winW;
        var bestH = winH;
        var canvas = null;
        if (typeof Module !== 'undefined' && Module && Module.canvas)
            canvas = Module.canvas;
        if (!canvas && doc)
            canvas = doc.getElementById('canvas');
        var candidates = [];
        if (doc) {
            var frame = doc.getElementById('canvas-frame');
            if (frame) candidates.push(frame);
        }
        if (canvas && canvas.parentElement)
            candidates.push(canvas.parentElement);
        if (canvas)
            candidates.push(canvas);

        for (var i = 0; i < candidates.length; i++) {
            var el = candidates[i];
            if (!el || !el.getBoundingClientRect) continue;
            var r = el.getBoundingClientRect();
            if (!(r.width > 0 && r.height > 0)) continue;
            bestW = Math.min(bestW, r.width);
            bestH = Math.min(bestH, r.height);
            break;
        }
        if (!(bestW > 0)) bestW = fallbackW;
        if (!(bestH > 0)) bestH = fallbackH;
        HEAP32[out >> 2] = Math.max(1, Math.round(bestW)) | 0;
        HEAP32[(out + 4) >> 2] = Math.max(1, Math.round(bestH)) | 0;
    }, &measured_width, fallback_width, fallback_height);

    if(width != 0)
        *width = measured_width > 0 ? measured_width : fallback_width;
    if(height != 0)
        *height = measured_height > 0 ? measured_height : fallback_height;
#else
    if(width != 0)
        *width = fallback_width;
    if(height != 0)
        *height = fallback_height;
#endif
    ApplyWebOrientationSize(width, height);
}

void
SetWebOrientationMode(int mode)
{
    if(mode < 0 || mode > 2)
        mode = 0;
    g_web_orientation_mode = mode;
}

unsigned int
GetWebWindowFlags(void)
{
#if defined(PLATFORM_WEB)
    return FLAG_WINDOW_RESIZABLE;
#else
    return 0;
#endif
}

int
IsWebStorageSyncPending(void)
{
#if defined(PLATFORM_WEB)
    return EM_ASM_INT({
        return Module.__kryonStorageSyncing || Module.__kryonStorageSyncPending ? 1 : 0;
    });
#else
    return 0;
#endif
}

int
GetWebStorageSyncState(WebStorageSyncState *out)
{
    WebStorageSyncState state;

    memset(&state, 0, sizeof(state));
#if defined(PLATFORM_WEB)
    {
        int bits = EM_ASM_INT({
            var M = Module || {};
            var bits = 0;

            if (M.__kryonStorageMounted) bits |= 1;
            if (M.__kryonStorageSyncing) bits |= 2;
            if (M.__kryonStorageSyncPending) bits |= 4;
            if (M.__kryonStorageSyncLastOk) bits |= 8;
            if (M.__kryonStorageSyncLastError) bits |= 16;
            return bits;
        });

        state.mounted = (bits & 1) != 0;
        state.syncing = (bits & 2) != 0;
        state.pending = (bits & 4) != 0;
        state.last_ok = (bits & 8) != 0;
        state.has_error = (bits & 16) != 0;
    }
#endif
    if(out != NULL)
        *out = state;
    return state.mounted || state.syncing || state.pending ||
           state.last_ok || state.has_error;
}

void
ScheduleWebStorageSync(int delay_ms, int log_success)
{
#if defined(PLATFORM_WEB)
    EM_ASM({
        if(typeof Module.__kryonScheduleStorageSync === 'function')
            Module.__kryonScheduleStorageSync($0, !!$1);
    }, delay_ms, log_success);
#else
    (void)delay_ms;
    (void)log_success;
#endif
}

#if defined(PLATFORM_WEB)
static void
WebStorageSyncAfterFrame(void *userdata)
{
    int delay_ms = g_web_storage_after_frame_delay_ms;
    int log_success = g_web_storage_after_frame_log_success;

    (void)userdata;
    g_web_storage_after_frame_pending = 0;
    g_web_storage_after_frame_delay_ms = 0;
    g_web_storage_after_frame_log_success = 0;
    ScheduleWebStorageSync(delay_ms, log_success);
}
#endif

void
ScheduleWebStorageSyncAfterFrame(int delay_ms, int log_success)
{
#if defined(PLATFORM_WEB)
    if(g_web_storage_after_frame_pending) {
        if(delay_ms < g_web_storage_after_frame_delay_ms)
            g_web_storage_after_frame_delay_ms = delay_ms;
        if(log_success)
            g_web_storage_after_frame_log_success = 1;
        return;
    }
    g_web_storage_after_frame_pending = 1;
    g_web_storage_after_frame_delay_ms = delay_ms;
    g_web_storage_after_frame_log_success = log_success ? 1 : 0;
    if(!SchedulePostFrameCallback(WebStorageSyncAfterFrame, NULL))
        WebStorageSyncAfterFrame(NULL);
#else
    (void)delay_ms;
    (void)log_success;
#endif
}

void
FlushWebStorageSync(int log_success)
{
#if defined(PLATFORM_WEB)
    EM_ASM({
        if(typeof Module.__kryonFlushStorageSync === 'function')
            Module.__kryonFlushStorageSync(!!$0);
        else if(typeof Module.__kryonScheduleStorageSync === 'function')
            Module.__kryonScheduleStorageSync(0, !!$0);
    }, log_success);
#else
    (void)log_success;
#endif
}

int
FlushWebStorageSyncBlocking(int timeout_ms, int log_success)
{
#if defined(PLATFORM_WEB)
    return js_web_storage_flush_blocking(timeout_ms, log_success);
#else
    (void)timeout_ms;
    (void)log_success;
    return 1;
#endif
}

const char *
kry_web_get_route_path(void)
{
#if defined(KRYON_WEB_JS)
    static char path[1024];

    js_web_route_get(0, path, (int)sizeof(path));
    return path[0] != '\0' ? path : "/";
#else
    return "/";
#endif
}

const char *
kry_web_get_route_hash(void)
{
#if defined(KRYON_WEB_JS)
    static char hash[1024];

    js_web_route_get(1, hash, (int)sizeof(hash));
    return hash;
#else
    return "";
#endif
}

int
kry_web_get_route_version(void)
{
#if defined(KRYON_WEB_JS)
    return js_web_route_version();
#else
    return 0;
#endif
}

void
kry_web_push_route(const char *path)
{
#if defined(KRYON_WEB_JS)
    js_web_route_set(0, path);
#else
    (void)path;
#endif
}

void
kry_web_replace_route(const char *path)
{
#if defined(KRYON_WEB_JS)
    js_web_route_set(1, path);
#else
    (void)path;
#endif
}

int
SyncWebWindowSize(void)
{
#if defined(PLATFORM_WEB)
    static int pending_width = 0;
    static int pending_height = 0;
    static int stable_frames = 0;
    int width;
    int height;
    int current_width;
    int current_height;
    int delta_w;
    int delta_h;

    if(IsWebStorageSyncPending())
        return 0;

    GetWebViewportSize(GetScreenWidth(), GetScreenHeight(), &width, &height);
    current_width = GetScreenWidth();
    current_height = GetScreenHeight();
    delta_w = width - current_width;
    delta_h = height - current_height;
    if(delta_w < 0)
        delta_w = -delta_w;
    if(delta_h < 0)
        delta_h = -delta_h;

    if(width == current_width && height == current_height) {
        pending_width = 0;
        pending_height = 0;
        stable_frames = 0;
        return 0;
    }

    if(delta_w <= 1 && delta_h <= 1)
        return 0;

    if(width != pending_width || height != pending_height) {
        pending_width = width;
        pending_height = height;
        stable_frames = 1;
        return 0;
    }

    stable_frames++;
    if(stable_frames < 2 || IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        return 0;

    SetWindowSize(width, height);
    SetUIViewSize(width, height);
    UpdateUIDPI(width, height);
    pending_width = 0;
    pending_height = 0;
    stable_frames = 0;
    return 1;
#else
    return 0;
#endif
}
