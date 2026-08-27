#ifndef KRYON_CAPABILITIES_H
#define KRYON_CAPABILITIES_H

#include "kryon_compat.generated.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KryCapability {
    KRY_CAP_FILE_PICKER = 1 << 0,
    KRY_CAP_FILE_SAVE = 1 << 1,
    KRY_CAP_SHARE = 1 << 2,
    KRY_CAP_SECURE_STORE = 1 << 3,
    KRY_CAP_BIOMETRIC_UNLOCK = 1 << 4,
    KRY_CAP_NOTIFICATIONS = 1 << 5,
    KRY_CAP_WAKELOCK = 1 << 6,
    KRY_CAP_CLIPBOARD = 1 << 7
} KryCapability;

typedef struct KrySafeArea {
    int left;
    int top;
    int right;
    int bottom;
} KrySafeArea;

typedef struct KryViewportSpec {
    int width;
    int height;
    KrySafeArea safe_area;
    int padding;
    int reserved_top;
    int reserved_bottom;
    int min_content_width;
} KryViewportSpec;

int KryCapabilitiesHas(int capabilities, KryCapability capability);
const char *KryCapabilityName(KryCapability capability);
Rectangle KrySafeContentRect(KryViewportSpec spec);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_CAPABILITIES_H */
