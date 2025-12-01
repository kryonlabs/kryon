/**
 * Test File Watcher
 *
 * Simple test to verify the file watcher system works correctly
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ir/ir_hot_reload.h"

static void file_change_callback(const IRFileEvent* event, void* user_data) {
    const char* type_name = "UNKNOWN";
    switch (event->type) {
        case IR_FILE_EVENT_CREATED: type_name = "CREATED"; break;
        case IR_FILE_EVENT_MODIFIED: type_name = "MODIFIED"; break;
        case IR_FILE_EVENT_DELETED: type_name = "DELETED"; break;
        case IR_FILE_EVENT_MOVED: type_name = "MOVED"; break;
    }

    printf("[FileWatcher] %s: %s\n", type_name, event->path);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        printf("Watch a directory for file changes\n");
        return 1;
    }

    const char* watch_path = argv[1];

    printf("=== Kryon File Watcher Test ===\n");
    printf("Watching: %s\n", watch_path);
    printf("Try creating, modifying, or deleting files in this directory\n");
    printf("Press Ctrl+C to exit\n\n");

    // Create file watcher
    IRFileWatcher* watcher = ir_file_watcher_create();
    if (!watcher) {
        fprintf(stderr, "Failed to create file watcher\n");
        return 1;
    }

    // Add path to watch
    if (!ir_file_watcher_add_path(watcher, watch_path, true)) {
        fprintf(stderr, "Failed to add watch path: %s\n", watch_path);
        ir_file_watcher_destroy(watcher);
        return 1;
    }

    // Set callback
    ir_file_watcher_set_callback(watcher, file_change_callback, NULL);

    printf("✓ File watcher active\n\n");

    // Poll loop
    while (ir_file_watcher_is_active(watcher)) {
        int event_count = ir_file_watcher_poll(watcher, 1000);  // 1 second timeout
        if (event_count > 0) {
            printf("  (%d events processed)\n", event_count);
        }
    }

    // Cleanup
    ir_file_watcher_destroy(watcher);
    return 0;
}
