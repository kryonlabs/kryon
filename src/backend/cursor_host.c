#include "kryon_portable_host.h"

#include <string.h>

static int
apply_cursor_shape(void *context, const char *module, const char *function,
                   const VmHostValue *args, int arg_count,
                   VmHostValue *result)
{
    CursorPlatform *platform = context;
    if(platform == NULL || platform->set_shape == NULL ||
       strcmp(module, "cursor") != 0 ||
       strcmp(function, "ApplyCursorShape") != 0 ||
       arg_count != 1 || args[0].kind != VM_HOST_INTEGER ||
       strcmp(args[0].type, "i32") != 0)
        return 0;
    platform->set_shape(platform->context, (int)args[0].integer);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
CursorBinding(CursorPlatform *platform)
{
    return (HostBinding){"cursor", "ApplyCursorShape",
                         apply_cursor_shape, platform};
}
