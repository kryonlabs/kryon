/**

 * @file extended_file_io.c
 * @brief Inferno Extended File I/O Implementation
 *
 * Implements extended file I/O service using lib9 for Inferno.
 * Provides access to device files (#c, #p, #D, etc.) and extended
 * file metadata (Qid).
 *
 * @version 1.0.0
 * @author Kryon Labs
 */
#include "lib9.h"


#ifdef KRYON_PLUGIN_INFERNO

#include "services/extended_file_io.h"
#include "inferno_compat.h"
#include <dirent.h>
#include <fcntl.h>

// =============================================================================
// EXTENDED FILE HANDLE
// =============================================================================

/**
 * Extended file handle (wraps lib9 FD)
 */
struct KryonExtendedFile {
    int fd;                     /**< lib9 file descriptor */
    char *path;                 /**< File path (for diagnostics) */
};

// =============================================================================
// FILE OPERATIONS
// =============================================================================

/**
 * Open a file with extended capabilities
 */
static KryonExtendedFile* inferno_file_open(const char *path, int flags) {
    if (!path) {
        return NULL;
    }

    // Convert KRYON_O_* flags to lib9 flags
    int lib9_flags = 0;

    switch (flags & KRYON_O_ACCMODE) {
        case KRYON_O_RDONLY:
            lib9_flags = OREAD;
            break;
        case KRYON_O_WRONLY:
            lib9_flags = OWRITE;
            break;
        case KRYON_O_RDWR:
            lib9_flags = ORDWR;
            break;
    }

    if (flags & KRYON_O_TRUNC) {
        lib9_flags |= OTRUNC;
    }

    // Open file using inferno syscall wrapper
    int fd = inferno_open(path, lib9_flags);
    if (fd < 0) {
        return NULL;
    }

    // Allocate handle
    KryonExtendedFile *file = malloc(sizeof(KryonExtendedFile));
    if (!file) {
        inferno_close(fd);
        return NULL;
    }

    file->fd = fd;
    file->path = strdup(path);

    return file;
}

/**
 * Close a file
 */
static void inferno_file_close(KryonExtendedFile *file) {
    if (!file) {
        return;
    }

    if (file->fd >= 0) {
        inferno_close(file->fd);
    }

    if (file->path) {
        free(file->path);
    }

    free(file);
}

/**
 * Read from file
 */
static int inferno_file_read(KryonExtendedFile *file, void *buf, int count) {
    if (!file || file->fd < 0) {
        return -1;
    }

    return inferno_read(file->fd, buf, count);
}

/**
 * Write to file
 */
static int inferno_file_write(KryonExtendedFile *file, const void *buf, int count) {
    if (!file || file->fd < 0) {
        return -1;
    }

    return inferno_write(file->fd, (void*)buf, count);
}

/**
 * Seek in file
 */
static int64_t inferno_file_seek(KryonExtendedFile *file, int64_t offset, KryonSeekMode mode) {
    if (!file || file->fd < 0) {
        return -1;
    }

    int whence;
    switch (mode) {
        case KRYON_SEEK_SET:
            whence = SEEK_SET;
            break;
        case KRYON_SEEK_CUR:
            whence = SEEK_CUR;
            break;
        case KRYON_SEEK_END:
            whence = SEEK_END;
            break;
        default:
            return -1;
    }

    return inferno_seek(file->fd, offset, whence);
}

// =============================================================================
// FILE TYPE DETECTION
// =============================================================================

/**
 * Check if path is a device file
 */
static bool inferno_is_device(const char *path) {
    if (!path) {
        return false;
    }

    // Inferno device files start with '#'
    return path[0] == '#';
}

/**
 * Check if path is special filesystem
 */
static bool inferno_is_special(const char *path) {
    if (!path) {
        return false;
    }

    // Common special paths in Inferno
    const char *special_prefixes[] = {
        "/prog/",
        "/net/",
        "/srv/",
        "/mnt/",
        "/dev/",
        NULL
    };

    for (int i = 0; special_prefixes[i]; i++) {
        if (strncmp(path, special_prefixes[i], strlen(special_prefixes[i])) == 0) {
            return true;
        }
    }

    return inferno_is_device(path);
}

/**
 * Get device type string
 */
static const char* inferno_get_type(const char *path) {
    if (!path || !inferno_is_device(path)) {
        return NULL;
    }

    // Extract device type (e.g., "#c" from "#c/cons")
    static char type_buf[8];
    const char *slash = strchr(path, '/');

    if (slash) {
        size_t len = slash - path;
        if (len < sizeof(type_buf)) {
            strncpy(type_buf, path, len);
            type_buf[len] = '\0';
            return type_buf;
        }
    } else {
        strncpy(type_buf, path, sizeof(type_buf) - 1);
        type_buf[sizeof(type_buf) - 1] = '\0';
        return type_buf;
    }

    return NULL;
}

// =============================================================================
// FILE STAT
// =============================================================================

/**
 * Get extended file statistics
 */
static bool inferno_stat_extended(const char *path, KryonExtendedFileStat *stat) {
    if (!path || !stat) {
        return false;
    }

    // Use inferno stat wrapper
    InfernoDir *dir = inferno_dirstat(path);
    if (!dir) {
        return false;
    }

    // Fill in standard attributes
    stat->size = dir->length;
    stat->mode = dir->mode;
    stat->mtime = dir->mtime;
    stat->atime = dir->atime;

    // Fill in extended attributes
    stat->is_device = inferno_is_device(path);
    stat->is_special = inferno_is_special(path);
    stat->device_type = inferno_get_type(path);

    // Fill in Qid information
    stat->qid_type = dir->qid.type;
    stat->qid_version = dir->qid.vers;
    stat->qid_path = dir->qid.path;

    free(dir);
    return true;
}

/**
 * Get extended statistics for open file
 */
static bool inferno_fstat_extended(KryonExtendedFile *file, KryonExtendedFileStat *stat) {
    if (!file || file->fd < 0 || !stat) {
        return false;
    }

    // Use inferno fstat wrapper
    InfernoDir *dir = inferno_dirfstat(file->fd);
    if (!dir) {
        return false;
    }

    // Fill in standard attributes
    stat->size = dir->length;
    stat->mode = dir->mode;
    stat->mtime = dir->mtime;
    stat->atime = dir->atime;

    // Fill in extended attributes
    stat->is_device = inferno_is_device(file->path);
    stat->is_special = inferno_is_special(file->path);
    stat->device_type = inferno_get_type(file->path);

    // Fill in Qid information
    stat->qid_type = dir->qid.type;
    stat->qid_version = dir->qid.vers;
    stat->qid_path = dir->qid.path;

    free(dir);
    return true;
}

// =============================================================================
// DIRECTORY OPERATIONS
// =============================================================================

/**
 * Read directory entries
 */
static bool inferno_read_dir(const char *path, char ***entries, int *count) {
    if (!path || !entries || !count) {
        return false;
    }

    // Open directory
    int fd = inferno_open(path, OREAD);
    if (fd < 0) {
        return false;
    }

    // Read all directory entries
    int max_entries = 64;
    char **entry_list = malloc(max_entries * sizeof(char*));
    if (!entry_list) {
        inferno_close(fd);
        return false;
    }

    int num_entries = 0;
    InfernoDir *dir;
    long nr;

    while ((nr = inferno_dirread(fd, &dir)) > 0) {
        // Expand array if needed
        if (num_entries >= max_entries) {
            max_entries *= 2;
            char **new_list = realloc(entry_list, max_entries * sizeof(char*));
            if (!new_list) {
                // Cleanup on failure
                for (int i = 0; i < num_entries; i++) {
                    free(entry_list[i]);
                }
                free(entry_list);
                free(dir);
                inferno_close(fd);
                return false;
            }
            entry_list = new_list;
        }

        // Store entry name
        entry_list[num_entries] = strdup(dir->name);
        num_entries++;

        free(dir);
    }

    inferno_close(fd);

    *entries = entry_list;
    *count = num_entries;
    return true;
}

/**
 * Free directory entries
 */
static void inferno_free_dir_entries(char **entries, int count) {
    if (!entries) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (entries[i]) {
            free(entries[i]);
        }
    }

    free(entries);
}

// =============================================================================
// SERVICE INTERFACE
// =============================================================================

/**
 * Extended File I/O service interface for Inferno
 */
static KryonExtendedFileIO inferno_extended_file_io = {
    .open = inferno_file_open,
    .close = inferno_file_close,
    .read = inferno_file_read,
    .write = inferno_file_write,
    .seek = inferno_file_seek,
    .is_device = inferno_is_device,
    .is_special = inferno_is_special,
    .get_type = inferno_get_type,
    .stat_extended = inferno_stat_extended,
    .fstat_extended = inferno_fstat_extended,
    .read_dir = inferno_read_dir,
    .free_dir_entries = inferno_free_dir_entries,
};

/**
 * Get the Extended File I/O service
 */
KryonExtendedFileIO* inferno_get_extended_file_io(void) {
    return &inferno_extended_file_io;
}

#endif // KRYON_PLUGIN_INFERNO
