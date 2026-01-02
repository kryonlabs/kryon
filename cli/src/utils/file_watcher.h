#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include <stdbool.h>

typedef struct FileWatcher FileWatcher;

typedef void (*FileChangeCallback)(const char* filepath, void* user_data);

/**
 * Create a file watcher for the given directory
 * Returns NULL on failure
 */
FileWatcher* file_watcher_create(const char* root_dir);

/**
 * Start watching for file changes
 * This function blocks and calls the callback when files change
 * Returns 0 on success, non-zero on error
 */
int file_watcher_start(FileWatcher* watcher, FileChangeCallback callback, void* user_data);

/**
 * Stop watching (call from signal handler or another thread)
 */
void file_watcher_stop(FileWatcher* watcher);

/**
 * Destroy the file watcher and free resources
 */
void file_watcher_destroy(FileWatcher* watcher);

#endif // FILE_WATCHER_H
