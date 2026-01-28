/**
 * @file extended_file_io.h
 * @brief Extended File I/O Service - Access beyond standard C library
 *
 * Provides access to OS-specific file features:
 * - Device files (Inferno: #c/cons, #p/PID/ctl, #D for draw device, etc.)
 * - Special filesystems and mount points
 * - Extended file metadata (Qid, device type, etc.)
 * - Direct syscall-level file operations
 *
 * Apps can detect this service and use it when available, falling back
 * to standard fopen/fread when not available.
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_SERVICE_EXTENDED_FILE_IO_H
#define KRYON_SERVICE_EXTENDED_FILE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// FILE HANDLE
// =============================================================================

/**
 * @brief Opaque extended file handle
 *
 * Represents an open file with extended capabilities beyond FILE*.
 * Implementation is plugin-specific (e.g., Inferno uses lib9 FD).
 */
typedef struct KryonExtendedFile KryonExtendedFile;

// =============================================================================
// FILE FLAGS
// =============================================================================

/**
 * @brief File open flags (compatible with POSIX/Plan 9)
 */
typedef enum {
    KRYON_O_RDONLY = 0x0000,    /**< Read only */
    KRYON_O_WRONLY = 0x0001,    /**< Write only */
    KRYON_O_RDWR   = 0x0002,    /**< Read and write */
    KRYON_O_ACCMODE = 0x0003,   /**< Access mode mask */

    KRYON_O_CREAT  = 0x0100,    /**< Create if doesn't exist */
    KRYON_O_EXCL   = 0x0200,    /**< Exclusive create */
    KRYON_O_TRUNC  = 0x0400,    /**< Truncate to zero length */
    KRYON_O_APPEND = 0x0800,    /**< Append mode */

    KRYON_O_NONBLOCK = 0x1000,  /**< Non-blocking I/O */
    KRYON_O_CLOEXEC  = 0x2000,  /**< Close on exec */
} KryonFileOpenFlags;

// =============================================================================
// EXTENDED FILE STAT
// =============================================================================

/**
 * @brief Extended file statistics
 *
 * Provides OS-specific file metadata beyond standard stat().
 * For Inferno: includes Qid information.
 */
typedef struct {
    /** Standard attributes */
    uint64_t size;              /**< File size in bytes */
    uint32_t mode;              /**< File mode/permissions */
    uint64_t mtime;             /**< Modification time */
    uint64_t atime;             /**< Access time */

    /** Extended attributes (plugin-specific) */
    bool is_device;             /**< True if device file */
    bool is_special;            /**< True if special filesystem */
    const char *device_type;    /**< Device type string (e.g., "#c", "#p") */

    /** Inferno-specific: Qid */
    uint8_t qid_type;           /**< Qid type (QTDIR, QTFILE, etc.) */
    uint32_t qid_version;       /**< Qid version */
    uint64_t qid_path;          /**< Qid path (unique file ID) */
} KryonExtendedFileStat;

// =============================================================================
// SEEK MODES
// =============================================================================

typedef enum {
    KRYON_SEEK_SET = 0,         /**< Seek from beginning */
    KRYON_SEEK_CUR = 1,         /**< Seek from current position */
    KRYON_SEEK_END = 2,         /**< Seek from end */
} KryonSeekMode;

// =============================================================================
// EXTENDED FILE I/O INTERFACE
// =============================================================================

/**
 * @brief Extended File I/O service interface
 *
 * Apps receive this interface via kryon_services_get_interface().
 */
typedef struct {
    /**
     * Open a file with extended capabilities
     * @param path File path (may be device file like "#c/cons")
     * @param flags Open flags (KRYON_O_*)
     * @return File handle, or NULL on failure
     */
    KryonExtendedFile* (*open)(const char *path, int flags);

    /**
     * Close a file
     * @param file File handle to close
     */
    void (*close)(KryonExtendedFile *file);

    /**
     * Read from file
     * @param file File handle
     * @param buf Buffer to read into
     * @param count Maximum bytes to read
     * @return Bytes read, or -1 on error
     */
    int (*read)(KryonExtendedFile *file, void *buf, int count);

    /**
     * Write to file
     * @param file File handle
     * @param buf Buffer to write from
     * @param count Bytes to write
     * @return Bytes written, or -1 on error
     */
    int (*write)(KryonExtendedFile *file, const void *buf, int count);

    /**
     * Seek in file
     * @param file File handle
     * @param offset Offset to seek to
     * @param mode Seek mode (KRYON_SEEK_*)
     * @return New file position, or -1 on error
     */
    int64_t (*seek)(KryonExtendedFile *file, int64_t offset, KryonSeekMode mode);

    /**
     * Check if path refers to a device file
     * @param path File path to check
     * @return true if device file (e.g., "#c/cons")
     */
    bool (*is_device)(const char *path);

    /**
     * Check if path refers to special filesystem
     * @param path File path to check
     * @return true if special (e.g., /proc, /dev)
     */
    bool (*is_special)(const char *path);

    /**
     * Get device type string
     * @param path File path
     * @return Device type (e.g., "#c", "#p") or NULL if not device
     */
    const char* (*get_type)(const char *path);

    /**
     * Get extended file statistics
     * @param path File path
     * @param stat Output statistics structure
     * @return true on success, false on failure
     */
    bool (*stat_extended)(const char *path, KryonExtendedFileStat *stat);

    /**
     * Get extended statistics for open file
     * @param file Open file handle
     * @param stat Output statistics structure
     * @return true on success, false on failure
     */
    bool (*fstat_extended)(KryonExtendedFile *file, KryonExtendedFileStat *stat);

    /**
     * Read directory entries (extended)
     * @param path Directory path
     * @param entries Output array of entry names (caller must free)
     * @param count Output number of entries
     * @return true on success, false on failure
     */
    bool (*read_dir)(const char *path, char ***entries, int *count);

    /**
     * Free directory entries returned by read_dir
     * @param entries Array of entry names
     * @param count Number of entries
     */
    void (*free_dir_entries)(char **entries, int count);

} KryonExtendedFileIO;

#ifdef __cplusplus
}
#endif

#endif // KRYON_SERVICE_EXTENDED_FILE_IO_H
