#include "kryon_portable_host.h"

#include <string.h>

static int
record_field(const VmHostValue *record, size_t index, const char *name,
             const char *type, VmHostValueKind kind)
{
    return record->kind == VM_HOST_RECORD && record->fields != NULL &&
           index < record->field_count &&
           strcmp(record->fields[index].name, name) == 0 &&
           strcmp(record->fields[index].value.type, type) == 0 &&
           record->fields[index].value.kind == kind;
}

static int
draw_bevel_line(void *context, const char *module, const char *function,
                const VmHostValue *args, int arg_count, VmHostValue *result)
{
    LineRenderer *renderer = context;
    if(renderer == NULL || renderer->draw == NULL ||
       strcmp(module, "bevel") != 0 ||
       strcmp(function, "RasterLine") != 0 || arg_count != 2 ||
       args[0].kind != VM_HOST_RECORD ||
       strcmp(args[0].type, "Rectangle") != 0 ||
       args[0].field_count != 4 ||
       args[1].kind != VM_HOST_RECORD ||
       strcmp(args[1].type, "Color") != 0 ||
       args[1].field_count != 4)
        return 0;
    const char *rectangle[] = {"x", "y", "width", "height"};
    const char *color[] = {"r", "g", "b", "a"};
    for(size_t i = 0; i < 4; i++)
        if(!record_field(&args[0], i, rectangle[i], "float", VM_HOST_REAL) ||
           !record_field(&args[1], i, color[i], "u8", VM_HOST_UNSIGNED))
            return 0;
    float x1 = (float)args[0].fields[0].value.real;
    float y1 = (float)args[0].fields[1].value.real;
    float x2 = x1 + (float)args[0].fields[2].value.real;
    float y2 = y1 + (float)args[0].fields[3].value.real;
    renderer->draw(renderer->context, x1, y1, x2, y2,
                   (uint8_t)args[1].fields[0].value.bits,
                   (uint8_t)args[1].fields[1].value.bits,
                   (uint8_t)args[1].fields[2].value.bits,
                   (uint8_t)args[1].fields[3].value.bits);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
BevelLineBinding(LineRenderer *renderer)
{
    return (HostBinding){"bevel", "RasterLine", draw_bevel_line,
                         renderer};
}
