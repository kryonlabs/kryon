/*
 * plan9_os.c - Plan 9 OS helpers for Kryon.
 *
 * mkdir: create a single directory (the recursive walkers in kry_filesystem,
 * kry_archive and audio_library slice paths on / and call mkdir(path, mode)
 * per component, which this helper implements with create(OREAD, 0700|DMDIR)).
 * File probes go through dirstat directly at the guarded call sites.
 */
#include "plan9_os.h"

#include <u.h>
#include <libc.h>
#include <fcall.h>

int
kryon_plan9_mkdir(const char *path)
{
    int fd;

    if(path == nil || path[0] == '\0')
        return -1;
    fd = create((char *)path, OREAD, DMDIR | 0700);
    if(fd < 0)
        return -1;
    close(fd);
    return 0;
}
