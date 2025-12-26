/**
 * Process Utilities
 * Functions for spawning and managing child processes
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

/**
 * Run command and capture output
 * Returns exit code, output stored in *output (caller must free)
 */
int process_run(const char* cmd, char** output) {
    if (!cmd) return -1;

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;

    // Read output if requested
    if (output) {
        size_t buffer_size = 4096;
        size_t total_size = 0;
        char* buffer = (char*)malloc(buffer_size);
        if (!buffer) {
            pclose(pipe);
            return -1;
        }

        char chunk[1024];
        while (fgets(chunk, sizeof(chunk), pipe)) {
            size_t chunk_len = strlen(chunk);
            if (total_size + chunk_len >= buffer_size) {
                buffer_size *= 2;
                char* new_buffer = (char*)realloc(buffer, buffer_size);
                if (!new_buffer) {
                    free(buffer);
                    pclose(pipe);
                    return -1;
                }
                buffer = new_buffer;
            }
            strcpy(buffer + total_size, chunk);
            total_size += chunk_len;
        }

        buffer[total_size] = '\0';
        *output = buffer;
    }

    int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/**
 * Run command asynchronously with callback
 * Callback is called with exit status when process completes
 */
int process_run_async(const char* cmd, void (*callback)(int status)) {
    if (!cmd) return -1;

    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        return -1;
    }
    else if (pid == 0) {
        // Child process
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        exit(127);  // execl failed
    }
    else {
        // Parent process
        if (callback) {
            // Wait for child process and call callback with exit status
            int status;
            waitpid(pid, &status, 0);
            callback(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        }
        return 0;
    }
}
