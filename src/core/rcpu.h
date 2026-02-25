/*
 * Kryon CPU Server - rcpu Protocol Handler
 * C89/C90 compliant
 *
 * The rcpu protocol is used by drawterm to establish CPU server connections.
 * Protocol format:
 * 1. Client sends: "NNNNNN\n<script_content>" where NNNNNN is 7-digit script length
 * 2. Server executes the script (typically sets up mounts and starts rc shell)
 * 3. Server then serves 9P file operations for /mnt/term hierarchy
 */

#ifndef RCPU_H
#define RCPU_H

/*
 * Handle rcpu connection
 * Returns 0 on success, -1 on error
 * This function takes ownership of the fd and will close it on completion
 */
int handle_rcpu_connection(int fd);

#endif
