#include "app_host.h"

#include <stdlib.h>

int
IsUIEditorActive(void)
{
    const char *value = getenv("FLINT_EDITOR");

    return value != NULL && value[0] != '\0' && value[0] != '0';
}
