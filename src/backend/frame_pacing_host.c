#include "kryon_portable_host.h"

#include <string.h>

/* Supplied by the selected Kryon platform backend. */
extern void SetTargetFPS(int fps);

static int
apply_target_fps(void *context, const char *module, const char *function,
                 const VmHostValue *args, int arg_count, VmHostValue *result)
{
    (void)context;
    if(strcmp(module, "frame_pacing") != 0 ||
       strcmp(function, "ApplyTargetFPS") != 0 ||
       arg_count != 1 || args[0].kind != VM_HOST_INTEGER ||
       strcmp(args[0].type, "i32") != 0)
        return 0;
    SetTargetFPS((int)args[0].integer);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
FramePacingBinding(void)
{
    return (HostBinding){"frame_pacing", "ApplyTargetFPS",
                         apply_target_fps, NULL};
}
