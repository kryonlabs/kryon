/* Plan 9 native shim: unistd surface from libc.h. usleep() takes
 * microseconds on hosted systems; native sleep() takes milliseconds. */
#ifndef KRYON_PLAN9_SHIM_UNISTD_H
#define KRYON_PLAN9_SHIM_UNISTD_H

#include "kryon_plan9_libc.h"

#define unlink(p) remove(p)
#define usleep(us) sleep((int)((us) / 1000))

#endif
