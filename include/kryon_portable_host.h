#ifndef KRYON_PORTABLE_HOST_H
#define KRYON_PORTABLE_HOST_H

#include "ziran_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bind Kryon's frame pacing effect to the platform's SetTargetFPS. */
HostBinding FramePacingBinding(void);

typedef struct LineRenderer {
    void (*draw)(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void *context;
} LineRenderer;

/* The renderer and its context must remain valid through BundleRun. */
HostBinding BevelLineBinding(LineRenderer *renderer);

#ifdef __cplusplus
}
#endif

#endif
