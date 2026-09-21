#ifndef KRYON_CAPABILITIES_H
#define KRYON_CAPABILITIES_H

#include "kryon_compat.generated.h"
#include "ui_capability_props.generated.h"

#ifdef __cplusplus
extern "C" {
#endif

int KryCapabilitiesHas(int capabilities, KryCapability capability);
const char *KryCapabilityName(KryCapability capability);
Rectangle KrySafeContentRect(KryViewportSpec spec);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_CAPABILITIES_H */
