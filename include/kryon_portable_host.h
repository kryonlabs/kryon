#ifndef KRYON_PORTABLE_HOST_H
#define KRYON_PORTABLE_HOST_H

#include "ziran_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bind Kryon's frame pacing effect to the platform's SetTargetFPS. */
HostBinding FramePacingBinding(void);

#ifdef __cplusplus
}
#endif

#endif
