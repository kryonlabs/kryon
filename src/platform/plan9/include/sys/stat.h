/* Plan 9 native shim: mkdir is provided by the plan9 OS helper
 * (src/platform/plan9/plan9_os.c); stat-style probing goes through Dir. */
#ifndef KRYON_PLAN9_SHIM_SYS_STAT_H
#define KRYON_PLAN9_SHIM_SYS_STAT_H

#include "kryon_plan9_libc.h"

int kryon_plan9_mkdir(const char *path);

#define mkdir(path, mode) kryon_plan9_mkdir(path)

#endif
