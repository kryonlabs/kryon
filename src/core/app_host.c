#include "app_host.h"

#include <stdlib.h>

int
IsInspectActive(void)
{
    const char *value = getenv("KRYON_INSPECT");

    return value != NULL && value[0] != '\0' && value[0] != '0';
}
