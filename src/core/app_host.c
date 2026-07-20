#include "app_host.h"

#include <stdlib.h>

int
IsUIInspectActive(void)
{
    const char *value = getenv("KRYON_INSPECT");

    return value != NULL && value[0] != '\0' && value[0] != '0';
}
