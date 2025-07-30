/**
 * @file file_watcher.c
 * @brief Kryon File Watcher Implementation (Linux inotify)
 */

#include "internal/filesystem.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <errno.h>
#include <limits.h>

// =============================================================================
// TYPES AND STRUCTURES
// =============================================================================

typedef struct KryonWatchEntry {
    int watch_descriptor;
    char* path;
    KryonFileWatchFlags flags;
    KryonFileWatchCallback callback;
    void* user_data;
    struct KryonWatchEntry* next;
} KryonWatchEntry;

typedef struct {
    int inotify_fd;
    KryonWatchEntry* watches;
    size_t watch_count;
    
    // Event buffer
    char event_buffer[4096];
    size_t buffer_size;
    
    // Configuration
    bool recursive_watch;
    double poll_interval;
    
} KryonFileWatcher;

static KryonFileWatcher g_file_watcher = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static uint32_t kryon_flags_to_inotify_mask(KryonFileWatchFlags flags) {
    uint32_t mask = 0;
    
    if (flags & KRYON_FILE_WATCH_CREATE) {
        mask |= IN_CREATE;
    }
    if (flags & KRYON_FILE_WATCH_DELETE) {
        mask |= IN_DELETE;
    }
    if (flags & KRYON_FILE_WATCH_MODIFY) {
        mask |= IN_MODIFY | IN_CLOSE_WRITE;
    }
    if (flags & KRYON_FILE_WATCH_MOVE) {
        mask |= IN_MOVE;
    }
    if (flags & KRYON_FILE_WATCH_ATTRIB) {
        mask |= IN_ATTRIB;
    }
    
    return mask;
}

static KryonFileWatchEvent inotify_mask_to_kryon_event(uint32_t mask) {
    if (mask & IN_CREATE) {
        return KRYON_FILE_EVENT_CREATED;
    }
    if (mask & (IN_DELETE | IN_DELETE_SELF)) {
        return KRYON_FILE_EVENT_DELETED;
    }
    if (mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
        return KRYON_FILE_EVENT_MODIFIED;
    }
    if (mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
        return KRYON_FILE_EVENT_MOVED;
    }
    if (mask & IN_ATTRIB) {
        return KRYON_FILE_EVENT_ATTRIB_CHANGED;
    }
    
    return KRYON_FILE_EVENT_MODIFIED; // Default
}

static KryonWatchEntry* find_watch_by_descriptor(int wd) {
    KryonWatchEntry* entry = g_file_watcher.watches;
    while (entry) {
        if (entry->watch_descriptor == wd) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static KryonWatchEntry* find_watch_by_path(const char* path) {
    KryonWatchEntry* entry = g_file_watcher.watches;
    while (entry) {
        if (strcmp(entry->path, path) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void remove_watch_entry(KryonWatchEntry* to_remove) {
    if (!to_remove) return;
    
    KryonWatchEntry* current = g_file_watcher.watches;
    KryonWatchEntry* prev = NULL;
    
    while (current) {
        if (current == to_remove) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_file_watcher.watches = current->next;
            }
            
            inotify_rm_watch(g_file_watcher.inotify_fd, current->watch_descriptor);
            kryon_free(current->path);
            kryon_free(current);
            g_file_watcher.watch_count--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

static void process_inotify_event(const struct inotify_event* event) {
    KryonWatchEntry* watch = find_watch_by_descriptor(event->wd);
    if (!watch || !watch->callback) return;
    
    // Build full path
    char full_path[PATH_MAX];
    if (event->len > 0) {
        snprintf(full_path, sizeof(full_path), "%s/%s", watch->path, event->name);
    } else {
        strncpy(full_path, watch->path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
    
    // Create file watch event
    KryonFileWatchEventData event_data = {0};
    event_data.event_type = inotify_mask_to_kryon_event(event->mask);
    event_data.path = full_path;
    event_data.is_directory = (event->mask & IN_ISDIR) != 0;
    event_data.timestamp = kryon_platform_get_time();
    
    // Call the callback
    watch->callback(&event_data, watch->user_data);
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_file_watcher_init(void) {
    memset(&g_file_watcher, 0, sizeof(g_file_watcher));
    
    g_file_watcher.inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_file_watcher.inotify_fd == -1) {
        return false;
    }
    
    g_file_watcher.poll_interval = 0.1; // 100ms default
    return true;
}

void kryon_file_watcher_shutdown(void) {
    // Remove all watches
    KryonWatchEntry* current = g_file_watcher.watches;
    while (current) {
        KryonWatchEntry* next = current->next;
        inotify_rm_watch(g_file_watcher.inotify_fd, current->watch_descriptor);
        kryon_free(current->path);
        kryon_free(current);
        current = next;
    }
    
    if (g_file_watcher.inotify_fd != -1) {
        close(g_file_watcher.inotify_fd);
    }
    
    memset(&g_file_watcher, 0, sizeof(g_file_watcher));
}

KryonFileWatchId kryon_file_watcher_add(const char* path, KryonFileWatchFlags flags,
                                        KryonFileWatchCallback callback, void* user_data) {
    if (!path || !callback || g_file_watcher.inotify_fd == -1) {
        return 0;
    }
    
    // Check if already watching this path
    KryonWatchEntry* existing = find_watch_by_path(path);
    if (existing) {
        // Update existing watch
        existing->flags = flags;
        existing->callback = callback;
        existing->user_data = user_data;
        return (KryonFileWatchId)existing;
    }
    
    // Create new watch entry
    KryonWatchEntry* entry = kryon_alloc(sizeof(KryonWatchEntry));
    if (!entry) return 0;
    
    entry->path = kryon_alloc(strlen(path) + 1);
    if (!entry->path) {
        kryon_free(entry);
        return 0;
    }
    strcpy(entry->path, path);
    
    // Add inotify watch
    uint32_t mask = kryon_flags_to_inotify_mask(flags);
    entry->watch_descriptor = inotify_add_watch(g_file_watcher.inotify_fd, path, mask);
    
    if (entry->watch_descriptor == -1) {
        kryon_free(entry->path);
        kryon_free(entry);
        return 0;
    }
    
    entry->flags = flags;
    entry->callback = callback;
    entry->user_data = user_data;
    entry->next = g_file_watcher.watches;
    
    g_file_watcher.watches = entry;
    g_file_watcher.watch_count++;
    
    return (KryonFileWatchId)entry;
}

bool kryon_file_watcher_remove(KryonFileWatchId watch_id) {
    if (watch_id == 0) return false;
    
    KryonWatchEntry* entry = (KryonWatchEntry*)watch_id;
    remove_watch_entry(entry);
    return true;
}

bool kryon_file_watcher_remove_path(const char* path) {
    if (!path) return false;
    
    KryonWatchEntry* entry = find_watch_by_path(path);
    if (entry) {
        remove_watch_entry(entry);
        return true;
    }
    
    return false;
}

void kryon_file_watcher_poll(void) {
    if (g_file_watcher.inotify_fd == -1) return;
    
    // Check if there are events to read
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(g_file_watcher.inotify_fd, &read_fds);
    
    struct timeval timeout = {0, 0}; // Non-blocking
    int ready = select(g_file_watcher.inotify_fd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (ready <= 0) return;
    
    // Read events
    ssize_t bytes_read = read(g_file_watcher.inotify_fd, g_file_watcher.event_buffer, 
                             sizeof(g_file_watcher.event_buffer));
    
    if (bytes_read <= 0) return;
    
    // Process events
    const char* ptr = g_file_watcher.event_buffer;
    while (ptr < g_file_watcher.event_buffer + bytes_read) {
        const struct inotify_event* event = (const struct inotify_event*)ptr;
        
        process_inotify_event(event);
        
        ptr += sizeof(struct inotify_event) + event->len;
    }
}

void kryon_file_watcher_wait(double timeout_seconds) {
    if (g_file_watcher.inotify_fd == -1) return;
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(g_file_watcher.inotify_fd, &read_fds);
    
    struct timeval timeout;
    if (timeout_seconds >= 0) {
        timeout.tv_sec = (time_t)timeout_seconds;
        timeout.tv_usec = (suseconds_t)((timeout_seconds - timeout.tv_sec) * 1000000);
    }
    
    int ready = select(g_file_watcher.inotify_fd + 1, &read_fds, NULL, NULL, 
                      timeout_seconds >= 0 ? &timeout : NULL);
    
    if (ready > 0) {
        kryon_file_watcher_poll();
    }
}

size_t kryon_file_watcher_get_watch_count(void) {
    return g_file_watcher.watch_count;
}

bool kryon_file_watcher_is_watching(const char* path) {
    if (!path) return false;
    
    return find_watch_by_path(path) != NULL;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void kryon_file_watcher_set_recursive(bool recursive) {
    g_file_watcher.recursive_watch = recursive;
}

bool kryon_file_watcher_get_recursive(void) {
    return g_file_watcher.recursive_watch;
}

void kryon_file_watcher_set_poll_interval(double interval) {
    g_file_watcher.poll_interval = interval > 0 ? interval : 0.1;
}

double kryon_file_watcher_get_poll_interval(void) {
    return g_file_watcher.poll_interval;
}