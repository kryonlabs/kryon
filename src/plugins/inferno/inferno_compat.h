/**
 * @file inferno_compat.h
 * @brief Compatibility layer for TaijiOS/Inferno when building standalone
 *
 * When building standalone kryon with the Inferno plugin, we can't include
 * kern.h directly because it conflicts with standard C library headers.
 * This file extracts just the constants and types we need.
 */

#ifndef KRYON_INFERNO_COMPAT_H
#define KRYON_INFERNO_COMPAT_H

#include <stdint.h>
#include <sys/types.h>

// File open modes (from kern.h)
#define OREAD   0       /* open for read */
#define OWRITE  1       /* write */
#define ORDWR   2       /* read and write */
#define OEXEC   3       /* execute */
#define OTRUNC  16      /* truncate */
#define OCEXEC  32      /* close on exec */
#define ORCLOSE 64      /* remove on close */
#define OAPPEND 128     /* append only */

// Mount flags (from kern.h)
#define MREPL   0x0000  /* mount replaces object */
#define MBEFORE 0x0001  /* mount goes before others in union directory */
#define MAFTER  0x0002  /* mount goes after others in union directory */
#define MCREATE 0x0004  /* permit creation in mounted directory */
#define MCACHE  0x0010  /* cache some data */

// Dir mode bits (from kern.h)
#define DMDIR           0x80000000      /* directory */
#define DMAPPEND        0x40000000      /* append only */
#define DMEXCL          0x20000000      /* exclusive */
#define DMMOUNT         0x10000000      /* mount point */
#define DMAUTH          0x08000000      /* authentication */
#define DMTMP           0x04000000      /* temporary */

// Qid types (from kern.h)
#define QTDIR           0x80    /* directory */
#define QTAPPEND        0x40    /* append only */
#define QTEXCL          0x20    /* exclusive */
#define QTMOUNT         0x10    /* mount point */
#define QTAUTH          0x08    /* authentication */
#define QTTMP           0x04    /* temporary */
#define QTFILE          0x00    /* regular file */

// Basic types (from kern.h)
typedef uint32_t    u32;
typedef int32_t     s32;
typedef uint64_t    u64;
typedef int64_t     s64;
typedef uint8_t     u8;
typedef int8_t      s8;
typedef uint16_t    u16;
typedef int16_t     s16;

// Qid structure (from kern.h)
typedef struct Qid {
    u64     path;
    u32     vers;
    u8      type;
} Qid;

// Dir structure (from kern.h/lib9.h)
typedef struct Dir {
    u16     type;
    u32     dev;
    Qid     qid;
    u32     mode;
    u32     atime;
    u32     mtime;
    s64     length;
    char    *name;
    char    *uid;
    char    *gid;
    char    *muid;
} Dir;

// Function declarations we need
// Note: We declare only Inferno-specific functions here.
// Standard functions (open, close, read, write) come from unistd.h/fcntl.h
#ifdef __cplusplus
extern "C" {
#endif

// Directory operations (Inferno-specific)
extern long dirread(int fd, Dir **dp);
extern Dir* dirstat(const char *path);
extern Dir* dirfstat(int fd);

// Namespace operations (Inferno-specific)
extern int bind(const char *source, const char *target, int flags);
extern int mount(int fd, int afd, const char *mountpoint, int flags, const char *aname);
extern int unmount(const char *old, const char *new_path);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INFERNO_COMPAT_H
