#ifndef KRYON_PROFILE_H
#define KRYON_PROFILE_H

#include "kryon_compat.generated.h"
#include "ui_icon_types.h"
#include "ui_profile_icon_props.generated.h"

int GetProfileImageIconCount(void);
IconType GetProfileImageIconType(int index);
const char *GetProfileImageIconName(int index);
IconType GetProfileImageIconTypeForSyncID(int sync_id);
int GetSyncIDForProfileImageIconType(IconType type);

#endif
