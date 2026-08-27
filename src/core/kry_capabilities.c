#include "kry_capabilities.h"

int
KryCapabilitiesHas(int capabilities, KryCapability capability)
{
    return (capabilities & (int)capability) != 0;
}

const char *
KryCapabilityName(KryCapability capability)
{
    switch(capability) {
    case KRY_CAP_FILE_PICKER: return "file-picker";
    case KRY_CAP_FILE_SAVE: return "file-save";
    case KRY_CAP_SHARE: return "share";
    case KRY_CAP_SECURE_STORE: return "secure-store";
    case KRY_CAP_BIOMETRIC_UNLOCK: return "biometric-unlock";
    case KRY_CAP_NOTIFICATIONS: return "notifications";
    case KRY_CAP_WAKELOCK: return "wakelock";
    case KRY_CAP_CLIPBOARD: return "clipboard";
    default: return "unknown";
    }
}

Rectangle
KrySafeContentRect(KryViewportSpec spec)
{
    int x = spec.safe_area.left + spec.padding;
    int y = spec.safe_area.top + spec.reserved_top + spec.padding;
    int w = spec.width - spec.safe_area.left - spec.safe_area.right -
            spec.padding * 2;
    int h = spec.height - spec.safe_area.top - spec.safe_area.bottom -
            spec.reserved_top - spec.reserved_bottom - spec.padding * 2;

    if(spec.min_content_width > 0 && w < spec.min_content_width)
        w = spec.min_content_width;
    if(h < 0)
        h = 0;
    return (Rectangle){(float)x, (float)y, (float)w, (float)h};
}
