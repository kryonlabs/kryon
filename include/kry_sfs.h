#ifndef KRY_SFS_H
#define KRY_SFS_H

#include <stddef.h>

/* Kryon synthetic file system: the live engine exposed as a file tree.
 *
 * Everything kryon owns is addressable here - version info, theme tokens,
 * the live widget tree (ui_inspect), and the input layer - readable AND
 * writable, so agents, tests, and the Hierarchy tab all drive the same
 * surface. Writes take effect through the real engine paths (kry_inject
 * for input, the inspect tree for widgets), never through side channels.
 *
 * Paths look like:
 *   /                          info, input, widgets, theme
 *   /info                      one-line engine identity
 *   /input/mouse/x             read: current x;  write: move mouse
 *   /input/mouse/y             read/write
 *   /input/mouse/button/<n>    read: down|up;  write: down|up|press|release|tap
 *   /input/keys/<KEY_NAME>     read: down|up;  write: down|up|tap
 *   /input/text                write: queue text as typed characters
 *   /input/wheel               read/write: frame wheel delta
 *   /widgets                   list: one directory per live widget
 *   /widgets/<i>               list: kind, text, value, bounds, source, tap
 *   /widgets/<i>/bounds        "x y w h"
 *   /widgets/<i>/tap           write "1": inject a click at the center
 *   /theme                     list: token names
 *   /theme/<name>              "r g b a"
 */

typedef struct {
    char name[96];
    int is_dir;
} KrySfsEntry;

typedef enum {
    KRY_SFS_OK = 0,
    KRY_SFS_ENOENT = -1,
    KRY_SFS_EINVAL = -2,
    KRY_SFS_ERO = -3,   /* write to a read-only file */
    KRY_SFS_ETIME = -4  /* write outside a live frame context */
} KrySfsStatus;

int KrySfsList(const char *path, KrySfsEntry *entries, int cap);
/* Returns the number of bytes written (excluding the terminator), or a
 * negative KrySfsStatus. */
int KrySfsRead(const char *path, char *buf, size_t size);
/* Returns 1 on success, or a negative KrySfsStatus. */
int KrySfsWrite(const char *path, const char *value);
int KrySfsIsDir(const char *path);

#endif
