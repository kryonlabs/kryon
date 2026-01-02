#include "file_watcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

#define MAX_WATCHES 1024
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define DEBOUNCE_MS 300

typedef struct {
    int wd;                 // Watch descriptor
    char path[PATH_MAX];    // Directory path
} WatchEntry;

struct FileWatcher {
    int fd;                          // inotify file descriptor
    WatchEntry watches[MAX_WATCHES]; // Watch descriptors
    int watch_count;                 // Number of active watches
    bool running;                    // Is watcher running
    char root_dir[PATH_MAX];         // Root directory being watched

    // Debouncing state
    char last_changed_file[PATH_MAX];
    time_t last_change_time;
    bool pending_change;
};

// Directories to skip
static const char* skip_dirs[] = {
    ".kryon_cache", "build", "dist", "node_modules", ".git", "vendor", NULL
};

static bool should_skip_dir(const char* dirname) {
    for (int i = 0; skip_dirs[i] != NULL; i++) {
        if (strcmp(dirname, skip_dirs[i]) == 0) {
            return true;
        }
    }
    return false;
}

static int add_watch(FileWatcher* watcher, const char* path) {
    if (watcher->watch_count >= MAX_WATCHES) {
        fprintf(stderr, "[file_watcher] Max watches reached\n");
        return -1;
    }

    int wd = inotify_add_watch(watcher->fd, path,
                                 IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (wd < 0) {
        perror("[file_watcher] inotify_add_watch");
        return -1;
    }

    watcher->watches[watcher->watch_count].wd = wd;
    strncpy(watcher->watches[watcher->watch_count].path, path, PATH_MAX - 1);
    watcher->watches[watcher->watch_count].path[PATH_MAX - 1] = '\0';
    watcher->watch_count++;

    return 0;
}

static int watch_directory_recursive(FileWatcher* watcher, const char* path) {
    // Add watch for this directory
    if (add_watch(watcher, path) < 0) {
        return -1;
    }

    // Recursively add subdirectories
    DIR* dir = opendir(path);
    if (!dir) {
        return 0; // Not fatal, just skip this directory
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (should_skip_dir(entry->d_name)) {
            continue;
        }

        char subpath[PATH_MAX];
        snprintf(subpath, PATH_MAX, "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            watch_directory_recursive(watcher, subpath);
        }
    }

    closedir(dir);
    return 0;
}

FileWatcher* file_watcher_create(const char* root_dir) {
    FileWatcher* watcher = calloc(1, sizeof(FileWatcher));
    if (!watcher) {
        return NULL;
    }

    watcher->fd = inotify_init();
    if (watcher->fd < 0) {
        perror("[file_watcher] inotify_init");
        free(watcher);
        return NULL;
    }

    strncpy(watcher->root_dir, root_dir, PATH_MAX - 1);
    watcher->root_dir[PATH_MAX - 1] = '\0';
    watcher->watch_count = 0;
    watcher->running = false;
    watcher->pending_change = false;
    watcher->last_change_time = 0;
    watcher->last_changed_file[0] = '\0';

    // Set up recursive watches
    if (watch_directory_recursive(watcher, root_dir) < 0) {
        close(watcher->fd);
        free(watcher);
        return NULL;
    }

    return watcher;
}

int file_watcher_start(FileWatcher* watcher, FileChangeCallback callback, void* user_data) {
    if (!watcher || !callback) {
        return -1;
    }

    watcher->running = true;
    char buffer[BUF_LEN];

    printf("[file_watcher] Watching for changes in %s...\n", watcher->root_dir);

    while (watcher->running) {
        // Use select with timeout for debouncing
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(watcher->fd, &fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = DEBOUNCE_MS * 1000;

        int ret = select(watcher->fd + 1, &fds, NULL, NULL, &timeout);

        if (ret < 0) {
            perror("[file_watcher] select");
            break;
        }

        // Check if debounce period elapsed
        if (watcher->pending_change) {
            time_t now = time(NULL);
            double diff_ms = difftime(now, watcher->last_change_time) * 1000;

            if (diff_ms >= DEBOUNCE_MS) {
                // Trigger callback
                callback(watcher->last_changed_file, user_data);
                watcher->pending_change = false;
            }
        }

        // Check if new events arrived
        if (ret > 0 && FD_ISSET(watcher->fd, &fds)) {
            ssize_t length = read(watcher->fd, buffer, BUF_LEN);
            if (length < 0) {
                perror("[file_watcher] read");
                break;
            }

            int i = 0;
            while (i < length) {
                struct inotify_event* event = (struct inotify_event*)&buffer[i];

                if (event->len > 0) {
                    // Find the directory path for this watch descriptor
                    const char* dir_path = NULL;
                    for (int j = 0; j < watcher->watch_count; j++) {
                        if (watcher->watches[j].wd == event->wd) {
                            dir_path = watcher->watches[j].path;
                            break;
                        }
                    }

                    if (dir_path) {
                        // Build full file path
                        char filepath[PATH_MAX];
                        if (strcmp(dir_path, watcher->root_dir) == 0) {
                            snprintf(filepath, PATH_MAX, "%s", event->name);
                        } else {
                            // Get relative path
                            const char* rel_path = dir_path + strlen(watcher->root_dir);
                            if (rel_path[0] == '/') rel_path++;
                            snprintf(filepath, PATH_MAX, "%s/%s", rel_path, event->name);
                        }

                        // Skip directories and build artifacts
                        if (!(event->mask & IN_ISDIR)) {
                            // Update debounce state
                            strncpy(watcher->last_changed_file, filepath, PATH_MAX - 1);
                            watcher->last_changed_file[PATH_MAX - 1] = '\0';
                            watcher->last_change_time = time(NULL);
                            watcher->pending_change = true;
                        }
                        // If a new directory was created, add watch for it
                        else if (event->mask & IN_CREATE) {
                            char new_dir_path[PATH_MAX];
                            snprintf(new_dir_path, PATH_MAX, "%s/%s", dir_path, event->name);
                            watch_directory_recursive(watcher, new_dir_path);
                        }
                    }
                }

                i += EVENT_SIZE + event->len;
            }
        }
    }

    return 0;
}

void file_watcher_stop(FileWatcher* watcher) {
    if (watcher) {
        watcher->running = false;
    }
}

void file_watcher_destroy(FileWatcher* watcher) {
    if (!watcher) {
        return;
    }

    // Remove all watches
    for (int i = 0; i < watcher->watch_count; i++) {
        inotify_rm_watch(watcher->fd, watcher->watches[i].wd);
    }

    close(watcher->fd);
    free(watcher);
}
