/**
 * @file inferno_compat.h
 * @brief Compatibility layer for TaijiOS/Inferno integration
 *
 * Supports two build modes:
 * 1. EMU mode (native TaijiOS): Uses kernel syscalls (k*)
 * 2. Standalone mode: Uses standard Unix syscalls with stubs
 */

#ifndef KRYON_INFERNO_COMPAT_H
#define KRYON_INFERNO_COMPAT_H

#ifdef EMU
// ============================================================================
// NATIVE TAIJIOS BUILD - Use kernel syscalls
// ============================================================================

// Forward declarations for types lib9.h needs
typedef struct Proc Proc;

// TaijiOS headers provide full kernel syscall access
// Include lib9.h for types (vlong, uvlong, Dir, etc.) and kernel.h for k* syscalls
// NOTE: We avoid including kern.h because it conflicts with system headers
#include "lib9.h"
#include "kernel.h"

// Define constants we need (from kern.h, but avoiding full inclusion)
// File open modes
#ifndef OREAD
#define OREAD   0       /* open for read */
#define OWRITE  1       /* write */
#define ORDWR   2       /* read and write */
#define OEXEC   3       /* execute */
#define OTRUNC  16      /* truncate */
#define OCEXEC  32      /* close on exec */
#define ORCLOSE 64      /* remove on close */
#define OAPPEND 128     /* append only */
#endif

// Mount flags
#ifndef MREPL
#define MREPL   0x0000  /* mount replaces object */
#define MBEFORE 0x0001  /* mount goes before others in union directory */
#define MAFTER  0x0002  /* mount goes after others in union directory */
#define MCREATE 0x0004  /* permit creation in mounted directory */
#define MCACHE  0x0010  /* cache some data */
#endif

// Dir mode bits
#ifndef DMDIR
#define DMDIR           0x80000000      /* directory */
#define DMAPPEND        0x40000000      /* append only */
#define DMEXCL          0x20000000      /* exclusive */
#define DMMOUNT         0x10000000      /* mount point */
#define DMAUTH          0x08000000      /* authentication */
#define DMTMP           0x04000000      /* temporary */
#endif

// Qid types
#ifndef QTDIR
#define QTDIR           0x80    /* directory */
#define QTAPPEND        0x40    /* append only */
#define QTEXCL          0x20    /* exclusive */
#define QTMOUNT         0x10    /* mount point */
#define QTAUTH          0x08    /* authentication */
#define QTTMP           0x04    /* temporary */
#define QTFILE          0x00    /* regular file */
#endif

// File operations (use k* wrappers from kernel.h)
// Inline wrappers handle const-correctness
static inline int inferno_open(const char *path, int mode) {
    return kopen((char*)path, mode);
}
static inline int inferno_close(int fd) {
    return kclose(fd);
}
static inline int inferno_read(int fd, void *buf, int n) {
    return kread(fd, buf, (s32)n);
}
static inline int inferno_write(int fd, const void *buf, int n) {
    return kwrite(fd, (void*)buf, (s32)n);
}
static inline vlong inferno_seek(int fd, vlong off, int mode) {
    return kseek(fd, off, mode);
}

// Directory operations
static inline long inferno_dirread(int fd, Dir **dp) {
    return kdirread(fd, dp);
}
static inline Dir* inferno_dirstat(const char *path) {
    return kdirstat((char*)path);
}
static inline Dir* inferno_dirfstat(int fd) {
    return kdirfstat(fd);
}

// Namespace operations
static inline int inferno_bind(const char *new_path, const char *old, int flags) {
    return kbind((char*)new_path, (char*)old, flags);
}
static inline int inferno_mount(int fd, int afd, const char *old, int flags, const char *spec) {
    return kmount(fd, afd, (char*)old, flags, (char*)spec);
}
static inline int inferno_unmount(const char *name, const char *old) {
    return kunmount((char*)name, (char*)old);
}

// Use TaijiOS Dir type directly
typedef Dir InfernoDir;

#else
// ============================================================================
// STANDALONE BUILD - Use standard Unix syscalls
// ============================================================================

#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

// Map to standard Unix calls
#define inferno_open(path, mode)    open(path, mode)
#define inferno_close(fd)           close(fd)
#define inferno_read(fd, buf, n)    read(fd, buf, n)
#define inferno_write(fd, buf, n)   write(fd, buf, n)
#define inferno_seek(fd, off, mode) lseek(fd, off, mode)

// Define our own types (since no kernel.h)
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;
typedef uint8_t  u8;
typedef uint16_t u16;

typedef struct {
    u64  path;
    u32  vers;
    u8   type;
} Qid;

typedef struct InfernoDir {
    u16  type;
    u32  dev;
    Qid  qid;
    u32  mode;
    u32  atime;
    u32  mtime;
    s64  length;
    char *name;
    char *uid;
    char *gid;
    char *muid;
} InfernoDir;

// File open modes (match kern.h)
#ifndef OREAD
#define OREAD   0
#define OWRITE  1
#define ORDWR   2
#define OEXEC   3
#define OTRUNC  16
#define OCEXEC  32
#define ORCLOSE 64
#define OAPPEND 128
#endif

// Mount flags (match kern.h)
#define MREPL   0x0000
#define MBEFORE 0x0001
#define MAFTER  0x0002
#define MCREATE 0x0004
#define MCACHE  0x0010

// Device mode bits
#define DMDIR           0x80000000
#define DMAPPEND        0x40000000
#define DMEXCL          0x20000000
#define DMMOUNT         0x10000000
#define DMAUTH          0x08000000
#define DMTMP           0x04000000

// Qid types
#define QTDIR           0x80
#define QTAPPEND        0x40
#define QTEXCL          0x20
#define QTMOUNT         0x10
#define QTAUTH          0x08
#define QTTMP           0x04
#define QTFILE          0x00

// Stub functions for operations not available in standalone mode
static inline long inferno_dirread(int fd, InfernoDir **dp) {
    (void)fd; (void)dp;
    return -1; // Not implemented in standalone
}

static inline InfernoDir* inferno_dirstat(const char *path) {
    (void)path;
    return NULL; // Not implemented in standalone
}

static inline InfernoDir* inferno_dirfstat(int fd) {
    (void)fd;
    return NULL; // Not implemented in standalone
}

static inline int inferno_bind(const char *new_path, const char *old, int flags) {
    (void)new_path; (void)old; (void)flags;
    return -1; // Not implemented in standalone
}

static inline int inferno_mount(int fd, int afd, const char *old, int flags, const char *spec) {
    (void)fd; (void)afd; (void)old; (void)flags; (void)spec;
    return -1; // Not implemented in standalone
}

static inline int inferno_unmount(const char *name, const char *old) {
    (void)name; (void)old;
    return -1; // Not implemented in standalone
}

#endif // EMU

#endif // KRYON_INFERNO_COMPAT_H
