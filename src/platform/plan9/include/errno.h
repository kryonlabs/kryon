/* Plan 9 native shim: errno is a per-TU scratch int. Native libc functions
 * signal errors through return values, and the Kryon call sites that check
 * errno only do so after mkdir-style failures that the plan9 helpers already
 * resolve themselves. */
#ifndef KRYON_PLAN9_SHIM_ERRNO_H
#define KRYON_PLAN9_SHIM_ERRNO_H

#define EPERM 1
#define ENOENT 2
#define EINTR 4
#define EIO 5
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENOSPC 28
#define EPIPE 32

static int kryon_plan9_errno_scratch;
#define errno kryon_plan9_errno_scratch

#endif
