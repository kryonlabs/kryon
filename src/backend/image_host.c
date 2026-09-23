#include "kryon_portable_host.h"

#include <string.h>

static int
field(const VmHostValue *record, size_t index, const char *name,
      const char *type, VmHostValueKind kind)
{
    return record->kind == VM_HOST_RECORD && record->fields != NULL &&
           index < record->field_count &&
           strcmp(record->fields[index].name, name) == 0 &&
           strcmp(record->fields[index].value.type, type) == 0 &&
           record->fields[index].value.kind == kind;
}

static int
rectangle(const VmHostValue *value, float out[4])
{
    const char *names[] = {"x", "y", "width", "height"};
    if(value->kind != VM_HOST_RECORD ||
       strcmp(value->type, "Rectangle") != 0 ||
       value->field_count != 4)
        return 0;
    for(size_t i = 0; i < 4; i++) {
        if(!field(value, i, names[i], "float", VM_HOST_REAL))
            return 0;
        out[i] = (float)value->fields[i].value.real;
    }
    return 1;
}

static int
vector(const VmHostValue *value, float out[2])
{
    if(value->kind != VM_HOST_RECORD ||
       strcmp(value->type, "Vector2") != 0 ||
       value->field_count != 2 ||
       !field(value, 0, "x", "float", VM_HOST_REAL) ||
       !field(value, 1, "y", "float", VM_HOST_REAL))
        return 0;
    out[0] = (float)value->fields[0].value.real;
    out[1] = (float)value->fields[1].value.real;
    return 1;
}

static int
color(const VmHostValue *value, uint8_t out[4])
{
    const char *names[] = {"r", "g", "b", "a"};
    if(value->kind != VM_HOST_RECORD ||
       strcmp(value->type, "Color") != 0 ||
       value->field_count != 4)
        return 0;
    for(size_t i = 0; i < 4; i++) {
        if(!field(value, i, names[i], "u8", VM_HOST_UNSIGNED))
            return 0;
        out[i] = (uint8_t)value->fields[i].value.bits;
    }
    return 1;
}

static int
asset_size(void *context, const char *module, const char *function,
           const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ImageRasterizer *renderer = context;
    if(renderer == NULL || renderer->size == NULL ||
       strcmp(module, "image_raster") != 0 || arg_count != 1 ||
       args[0].kind != VM_HOST_STRING ||
       strcmp(args[0].type, "string") != 0)
        return 0;
    int width = 0, height = 0;
    if(!renderer->size(renderer->context, (const char *)args[0].data,
                       args[0].length, &width, &height))
        width = height = 0;
    result->kind = VM_HOST_INTEGER;
    if(strcmp(function, "ImageWidth") == 0)
        result->integer = width > 0 ? width : 0;
    else if(strcmp(function, "ImageHeight") == 0)
        result->integer = height > 0 ? height : 0;
    else
        return 0;
    return 1;
}

static int
draw_image(void *context, const char *module, const char *function,
           const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ImageRasterizer *renderer = context;
    if(renderer == NULL || renderer->draw == NULL ||
       strcmp(module, "paint_queue") != 0 ||
       strcmp(function, "RasterImage") != 0 || arg_count != 9 ||
       args[0].kind != VM_HOST_STRING ||
       strcmp(args[0].type, "string") != 0 ||
       args[1].kind != VM_HOST_UNSIGNED ||
       strcmp(args[1].type, "u32") != 0 ||
       args[6].kind != VM_HOST_REAL ||
       strcmp(args[6].type, "float") != 0 ||
       args[7].kind != VM_HOST_REAL ||
       strcmp(args[7].type, "float") != 0)
        return 0;
    float source[4], destination[4], clip[4], origin[2];
    uint8_t tint[4];
    if(!rectangle(&args[2], source) ||
       !rectangle(&args[3], destination) ||
       !rectangle(&args[4], clip) ||
       !vector(&args[5], origin) || !color(&args[8], tint))
        return 0;
    renderer->draw(renderer->context,
                   (const char *)args[0].data, args[0].length,
                   (uint32_t)args[1].bits, source, destination, clip,
                   origin, (float)args[6].real, (float)args[7].real,
                   tint);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
ImageWidthBinding(ImageRasterizer *renderer)
{
    return (HostBinding){"image_raster", "ImageWidth", asset_size, renderer};
}

HostBinding
ImageHeightBinding(ImageRasterizer *renderer)
{
    return (HostBinding){"image_raster", "ImageHeight", asset_size, renderer};
}

HostBinding
RasterImageBinding(ImageRasterizer *renderer)
{
    return (HostBinding){"paint_queue", "RasterImage", draw_image, renderer};
}
