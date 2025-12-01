/**
 * Kryon Hot Reload System Implementation
 */

#define _POSIX_C_SOURCE 200809L
#include "ir_core.h"
#include "ir_builder.h"
#include "ir_hot_reload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// Platform-specific includes
#ifdef __linux__
#include <sys/inotify.h>
#include <sys/select.h>
#include <limits.h>
#endif

// ============================================================================
// File Watcher Implementation (Linux inotify)
// ============================================================================

#define MAX_WATCH_PATHS 256
#define EVENT_BUFFER_SIZE 4096

typedef struct {
    char path[PATH_MAX];
    int watch_descriptor;
    bool recursive;
} WatchedPath;

struct IRFileWatcher {
#ifdef __linux__
    int inotify_fd;
#endif
    WatchedPath watched_paths[MAX_WATCH_PATHS];
    int watch_count;

    IRFileWatchCallback callback;
    void* callback_user_data;

    bool active;
};

IRFileWatcher* ir_file_watcher_create(void) {
    IRFileWatcher* watcher = calloc(1, sizeof(IRFileWatcher));
    if (!watcher) return NULL;

#ifdef __linux__
    watcher->inotify_fd = inotify_init1(IN_NONBLOCK);
    if (watcher->inotify_fd < 0) {
        fprintf(stderr, "[HotReload] Failed to initialize inotify: %s\n", strerror(errno));
        free(watcher);
        return NULL;
    }
#endif

    watcher->active = true;
    return watcher;
}

bool ir_file_watcher_add_path(IRFileWatcher* watcher, const char* path, bool recursive) {
    if (!watcher || !path || watcher->watch_count >= MAX_WATCH_PATHS) {
        return false;
    }

#ifdef __linux__
    // Watch for: modify, create, delete, move events
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;

    int wd = inotify_add_watch(watcher->inotify_fd, path, mask);
    if (wd < 0) {
        fprintf(stderr, "[HotReload] Failed to add watch for %s: %s\n", path, strerror(errno));
        return false;
    }

    WatchedPath* wp = &watcher->watched_paths[watcher->watch_count];
    strncpy(wp->path, path, PATH_MAX - 1);
    wp->path[PATH_MAX - 1] = '\0';
    wp->watch_descriptor = wd;
    wp->recursive = recursive;
    watcher->watch_count++;

    return true;
#else
    fprintf(stderr, "[HotReload] File watching not supported on this platform\n");
    return false;
#endif
}

bool ir_file_watcher_remove_path(IRFileWatcher* watcher, const char* path) {
    if (!watcher || !path) return false;

    for (int i = 0; i < watcher->watch_count; i++) {
        if (strcmp(watcher->watched_paths[i].path, path) == 0) {
#ifdef __linux__
            inotify_rm_watch(watcher->inotify_fd, watcher->watched_paths[i].watch_descriptor);
#endif
            // Shift remaining watches down
            memmove(&watcher->watched_paths[i],
                    &watcher->watched_paths[i + 1],
                    (watcher->watch_count - i - 1) * sizeof(WatchedPath));
            watcher->watch_count--;
            return true;
        }
    }

    return false;
}

void ir_file_watcher_set_callback(IRFileWatcher* watcher,
                                   IRFileWatchCallback callback,
                                   void* user_data) {
    if (!watcher) return;
    watcher->callback = callback;
    watcher->callback_user_data = user_data;
}

int ir_file_watcher_poll(IRFileWatcher* watcher, int timeout_ms) {
    if (!watcher || !watcher->active) return 0;

#ifdef __linux__
    // Use select for timeout support
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(watcher->inotify_fd, &rfds);

    struct timeval tv;
    struct timeval* tvp = NULL;

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }

    int ret = select(watcher->inotify_fd + 1, &rfds, NULL, NULL, tvp);
    if (ret <= 0) return 0;  // Timeout or error

    // Read events
    char buffer[EVENT_BUFFER_SIZE];
    ssize_t len = read(watcher->inotify_fd, buffer, sizeof(buffer));
    if (len < 0) return 0;

    int event_count = 0;
    const struct inotify_event* event;

    for (char* ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
        event = (const struct inotify_event*)ptr;

        // Find the watched path for this event
        const char* base_path = NULL;
        for (int i = 0; i < watcher->watch_count; i++) {
            if (watcher->watched_paths[i].watch_descriptor == event->wd) {
                base_path = watcher->watched_paths[i].path;
                break;
            }
        }

        if (!base_path) continue;

        // Build full path
        char full_path[PATH_MAX];
        if (event->len > 0) {
            snprintf(full_path, PATH_MAX, "%s/%s", base_path, event->name);
        } else {
            strncpy(full_path, base_path, PATH_MAX - 1);
            full_path[PATH_MAX - 1] = '\0';
        }

        // Create IRFileEvent
        IRFileEvent file_event = {0};
        file_event.path = strdup(full_path);
        file_event.timestamp = (uint64_t)time(NULL);

        if (event->mask & IN_MODIFY) {
            file_event.type = IR_FILE_EVENT_MODIFIED;
        } else if (event->mask & IN_CREATE) {
            file_event.type = IR_FILE_EVENT_CREATED;
        } else if (event->mask & IN_DELETE) {
            file_event.type = IR_FILE_EVENT_DELETED;
        } else if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
            file_event.type = IR_FILE_EVENT_MOVED;
        }

        // Call callback
        if (watcher->callback) {
            watcher->callback(&file_event, watcher->callback_user_data);
        }

        free(file_event.path);
        event_count++;
    }

    return event_count;
#else
    return 0;
#endif
}

bool ir_file_watcher_is_active(IRFileWatcher* watcher) {
    return watcher && watcher->active;
}

void ir_file_watcher_destroy(IRFileWatcher* watcher) {
    if (!watcher) return;

#ifdef __linux__
    if (watcher->inotify_fd >= 0) {
        close(watcher->inotify_fd);
    }
#endif

    free(watcher);
}

// ============================================================================
// State Preservation
// ============================================================================

#define MAX_STATES 1024

IRStateSnapshot* ir_capture_state(struct IRComponent* root) {
    if (!root) return NULL;

    IRStateSnapshot* snapshot = calloc(1, sizeof(IRStateSnapshot));
    if (!snapshot) return NULL;

    snapshot->capacity = MAX_STATES;
    snapshot->states = calloc(MAX_STATES, sizeof(IRComponentState));
    if (!snapshot->states) {
        free(snapshot);
        return NULL;
    }

    // Recursively capture state from all components
    // For now, we'll focus on input fields, checkboxes, and tab selections
    // TODO: Implement recursive tree traversal and state serialization

    return snapshot;
}

void ir_restore_state(struct IRComponent* root, const IRStateSnapshot* snapshot) {
    if (!root || !snapshot) return;

    // TODO: Implement state restoration by matching component IDs
    // and applying saved state values
}

void ir_state_snapshot_destroy(IRStateSnapshot* snapshot) {
    if (!snapshot) return;

    for (uint32_t i = 0; i < snapshot->state_count; i++) {
        free(snapshot->states[i].state_data);
    }

    free(snapshot->states);
    free(snapshot);
}

// ============================================================================
// Hot Reload Protocol
// ============================================================================

struct IRHotReloadContext {
    IRFileWatcher* file_watcher;
    IRRebuildCallback rebuild_callback;
    void* rebuild_user_data;

    IRStateSnapshot* current_state;
    bool enabled;
    bool rebuild_pending;

    // Debouncing
    uint64_t last_change_time;
    uint64_t debounce_ms;
};

static void hot_reload_file_callback(const IRFileEvent* event, void* user_data) {
    IRHotReloadContext* ctx = (IRHotReloadContext*)user_data;
    if (!ctx || !ctx->enabled) return;

    // Filter for relevant file types (.nim, .h, .c)
    const char* ext = strrchr(event->path, '.');
    if (!ext) return;

    bool is_relevant = (strcmp(ext, ".nim") == 0 ||
                       strcmp(ext, ".h") == 0 ||
                       strcmp(ext, ".c") == 0);

    if (!is_relevant) return;

    // Debounce: only trigger rebuild if enough time has passed
    uint64_t now = (uint64_t)time(NULL) * 1000;  // Convert to ms
    if (now - ctx->last_change_time < ctx->debounce_ms) {
        return;
    }

    ctx->last_change_time = now;
    ctx->rebuild_pending = true;

    const char* event_name = "UNKNOWN";
    switch (event->type) {
        case IR_FILE_EVENT_MODIFIED: event_name = "MODIFIED"; break;
        case IR_FILE_EVENT_CREATED: event_name = "CREATED"; break;
        case IR_FILE_EVENT_DELETED: event_name = "DELETED"; break;
        case IR_FILE_EVENT_MOVED: event_name = "MOVED"; break;
    }

    printf("[HotReload] File %s: %s (rebuild pending)\n", event_name, event->path);
}

IRHotReloadContext* ir_hot_reload_create(void) {
    IRHotReloadContext* ctx = calloc(1, sizeof(IRHotReloadContext));
    if (!ctx) return NULL;

    ctx->file_watcher = ir_file_watcher_create();
    if (!ctx->file_watcher) {
        free(ctx);
        return NULL;
    }

    ir_file_watcher_set_callback(ctx->file_watcher, hot_reload_file_callback, ctx);

    ctx->enabled = true;
    ctx->debounce_ms = 500;  // 500ms debounce

    return ctx;
}

void ir_hot_reload_set_rebuild_callback(IRHotReloadContext* ctx,
                                         IRRebuildCallback callback,
                                         void* user_data) {
    if (!ctx) return;
    ctx->rebuild_callback = callback;
    ctx->rebuild_user_data = user_data;
}

bool ir_hot_reload_watch_file(IRHotReloadContext* ctx, const char* path) {
    if (!ctx || !ctx->file_watcher) return false;

    // Watch the parent directory (inotify watches directories, not files)
    char dir_path[PATH_MAX];
    strncpy(dir_path, path, PATH_MAX - 1);
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        return ir_file_watcher_add_path(ctx->file_watcher, dir_path, false);
    }

    return false;
}

bool ir_hot_reload_watch_directory(IRHotReloadContext* ctx, const char* path) {
    if (!ctx || !ctx->file_watcher) return false;
    return ir_file_watcher_add_path(ctx->file_watcher, path, true);
}

IRReloadResult ir_hot_reload_poll(IRHotReloadContext* ctx, struct IRComponent** current_root) {
    if (!ctx || !current_root) return IR_RELOAD_ERROR;

    // Poll file watcher for events
    ir_file_watcher_poll(ctx->file_watcher, 0);  // Non-blocking

    if (!ctx->rebuild_pending) {
        return IR_RELOAD_NO_CHANGES;
    }

    ctx->rebuild_pending = false;

    if (!ctx->rebuild_callback) {
        fprintf(stderr, "[HotReload] No rebuild callback set\n");
        return IR_RELOAD_ERROR;
    }

    printf("[HotReload] 🔄 Reloading...\n");

    // Capture current state
    if (ctx->current_state) {
        ir_state_snapshot_destroy(ctx->current_state);
    }
    ctx->current_state = ir_capture_state(*current_root);

    // Call rebuild callback
    IRComponent* new_root = ctx->rebuild_callback(ctx->rebuild_user_data);
    if (!new_root) {
        fprintf(stderr, "[HotReload] ❌ Rebuild failed\n");
        return IR_RELOAD_BUILD_FAILED;
    }

    // Restore state to new tree
    if (ctx->current_state) {
        ir_restore_state(new_root, ctx->current_state);
    }

    // Destroy old root
    if (*current_root) {
        ir_destroy_component(*current_root);
    }

    // Swap roots
    *current_root = new_root;

    printf("[HotReload] ✅ Reload complete\n");
    return IR_RELOAD_SUCCESS;
}

void ir_hot_reload_set_enabled(IRHotReloadContext* ctx, bool enabled) {
    if (!ctx) return;
    ctx->enabled = enabled;
}

bool ir_hot_reload_is_enabled(IRHotReloadContext* ctx) {
    return ctx && ctx->enabled;
}

IRFileWatcher* ir_hot_reload_get_watcher(IRHotReloadContext* ctx) {
    return ctx ? ctx->file_watcher : NULL;
}

void ir_hot_reload_destroy(IRHotReloadContext* ctx) {
    if (!ctx) return;

    if (ctx->file_watcher) {
        ir_file_watcher_destroy(ctx->file_watcher);
    }

    if (ctx->current_state) {
        ir_state_snapshot_destroy(ctx->current_state);
    }

    free(ctx);
}
