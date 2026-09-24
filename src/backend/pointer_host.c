#include "kryon_portable_host.h"

#include <string.h>

static int
poll_pointer(void *context, const char *module, const char *function,
             const VmHostValue *args, int arg_count, VmHostValue *result)
{
    PointerHost *pointer = context;
    (void)args;
    if(pointer == NULL || strcmp(module, "pointer_input") != 0 ||
       strcmp(function, "PollPointer") != 0 || arg_count != 0)
        return 0;
    pointer->fields[0] = (VmHostField){"x",
        {.kind = VM_HOST_REAL, .type = "float", .real = pointer->x}};
    pointer->fields[1] = (VmHostField){"y",
        {.kind = VM_HOST_REAL, .type = "float", .real = pointer->y}};
    pointer->fields[2] = (VmHostField){"down",
        {.kind = VM_HOST_INTEGER, .type = "bool",
         .integer = pointer->down != 0}};
    pointer->fields[3] = (VmHostField){"pressed",
        {.kind = VM_HOST_INTEGER, .type = "bool",
         .integer = pointer->pressed != 0}};
    pointer->fields[4] = (VmHostField){"released",
        {.kind = VM_HOST_INTEGER, .type = "bool",
         .integer = pointer->released != 0}};
    *result = (VmHostValue){.kind = VM_HOST_RECORD,
        .type = "PointerFrame", .fields = pointer->fields, .field_count = 5};
    return 1;
}

HostBinding
PointerBinding(PointerHost *pointer)
{
    return (HostBinding){"pointer_input", "PollPointer",
                         poll_pointer, pointer};
}
