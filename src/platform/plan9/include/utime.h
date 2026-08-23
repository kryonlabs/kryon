/* Plan 9 native shim for POSIX utime(). */
#ifndef KRYON_PLAN9_SHIM_UTIME_H
#define KRYON_PLAN9_SHIM_UTIME_H

#include "kryon_plan9_libc.h"

struct utimbuf {
    time_t actime;
    time_t modtime;
};

static int
kryon_plan9_utime(const char *path, const struct utimbuf *times)
{
    Dir d;

    if(path == nil || times == nil)
        return -1;
    nulldir(&d);
    d.mtime = times->modtime;
    return dirwstat(path, &d);
}

#define utime(path, times) kryon_plan9_utime(path, times)

#endif
