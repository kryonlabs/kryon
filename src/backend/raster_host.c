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
read_rectangle(const VmHostValue *value, float out[4])
{
    const char *fields[] = {"x", "y", "width", "height"};
    if(value->kind != VM_HOST_RECORD || strcmp(value->type, "Rectangle") != 0 ||
       value->field_count != 4)
        return 0;
    for(size_t i = 0; i < 4; i++) {
        if(!record_field(value, i, fields[i], "float", VM_HOST_REAL))
            return 0;
        out[i] = (float)value->fields[i].value.real;
    }
    return 1;
}

static int
read_color(const VmHostValue *value, uint8_t out[4])
{
    const char *fields[] = {"r", "g", "b", "a"};
    if(value->kind != VM_HOST_RECORD || strcmp(value->type, "Color") != 0 ||
       value->field_count != 4)
        return 0;
    for(size_t i = 0; i < 4; i++) {
        if(!record_field(value, i, fields[i], "u8", VM_HOST_UNSIGNED))
            return 0;
        out[i] = (uint8_t)value->fields[i].value.bits;
    }
    return 1;
}

static int
read_real(const VmHostValue *value, float *out)
{
    if(value->kind != VM_HOST_REAL || strcmp(value->type, "float") != 0)
        return 0;
    *out = (float)value->real;
    return 1;
}

static int
read_i32(const VmHostValue *value, int *out)
{
    if(value->kind != VM_HOST_INTEGER || strcmp(value->type, "i32") != 0)
        return 0;
    *out = (int)value->integer;
    return 1;
}

static int
draw_raster_line(void *context, const char *module, const char *function,
                const VmHostValue *args, int arg_count, VmHostValue *result)
{
    LineRenderer *renderer = context;
    if(renderer == NULL || renderer->draw == NULL ||
       strcmp(module, "raster") != 0 ||
       strcmp(function, "RasterLine") != 0 || arg_count != 2)
        return 0;
    float rectangle[4];
    uint8_t color[4];
    if(!read_rectangle(&args[0], rectangle) || !read_color(&args[1], color))
        return 0;
    float x1 = rectangle[0];
    float y1 = rectangle[1];
    float x2 = x1 + rectangle[2];
    float y2 = y1 + rectangle[3];
    renderer->draw(renderer->context, x1, y1, x2, y2,
                   color[0], color[1], color[2], color[3]);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
RasterLineBinding(LineRenderer *renderer)
{
    return (HostBinding){"raster", "RasterLine", draw_raster_line,
                         renderer};
}

static int
draw_rounded_rectangle(void *context, const char *module,
                       const char *function, const VmHostValue *args,
                       int arg_count, VmHostValue *result)
{
    RoundedRectangleRenderer *renderer = context;
    int outline = strcmp(function, "RasterRoundedRectangleOutline") == 0;
    if(renderer == NULL || strcmp(module, "raster_shape") != 0 ||
       (!outline && strcmp(function, "RasterRoundedRectangle") != 0) ||
       arg_count != (outline ? 5 : 4) ||
       (outline ? renderer->outline == NULL : renderer->fill == NULL))
        return 0;
    float rectangle[4], radius, line_width = 0.0f;
    uint8_t color[4];
    int segments;
    if(!read_rectangle(&args[0], rectangle) ||
       !read_real(&args[1], &radius) || !read_i32(&args[2], &segments) ||
       (outline && !read_real(&args[3], &line_width)) ||
       !read_color(&args[outline ? 4 : 3], color))
        return 0;
    if(outline)
        renderer->outline(renderer->context, rectangle[0], rectangle[1],
                          rectangle[2], rectangle[3], radius, segments,
                          line_width, color[0], color[1], color[2], color[3]);
    else
        renderer->fill(renderer->context, rectangle[0], rectangle[1],
                       rectangle[2], rectangle[3], radius, segments,
                       color[0], color[1], color[2], color[3]);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
RasterRoundedRectangleBinding(RoundedRectangleRenderer *renderer)
{
    return (HostBinding){"raster_shape", "RasterRoundedRectangle",
                         draw_rounded_rectangle, renderer};
}

HostBinding
RasterRoundedRectangleOutlineBinding(RoundedRectangleRenderer *renderer)
{
    return (HostBinding){"raster_shape", "RasterRoundedRectangleOutline",
                         draw_rounded_rectangle, renderer};
}

static int
draw_raster_text(void *context, const char *module, const char *function,
                 const VmHostValue *args, int arg_count, VmHostValue *result)
{
    TextRenderer *renderer = context;
    int clipped = strcmp(function, "RasterTextClipped") == 0;
    int x, y, font;
    uint8_t color[4];
    float clip[4];
    if(renderer == NULL ||
       (clipped ? renderer->draw_clipped == NULL : renderer->draw == NULL) ||
       strcmp(module, "raster_text") != 0 ||
       (!clipped && strcmp(function, "RasterText") != 0) ||
       arg_count != (clipped ? 6 : 5) ||
       args[0].kind != VM_HOST_STRING ||
       strcmp(args[0].type, "string") != 0 ||
       !read_i32(&args[1], &x) || !read_i32(&args[2], &y) ||
       !read_i32(&args[3], &font) || !read_color(&args[4], color) ||
       (clipped && !read_rectangle(&args[5], clip)))
        return 0;
    if(clipped)
        renderer->draw_clipped(renderer->context,
                               (const char *)args[0].data,
                               args[0].length, x, y, font, clip,
                               color[0], color[1], color[2], color[3]);
    else
        renderer->draw(renderer->context, (const char *)args[0].data,
                       args[0].length, x, y, font,
                       color[0], color[1], color[2], color[3]);
    result->kind = VM_HOST_VOID;
    return 1;
}

HostBinding
RasterTextBinding(TextRenderer *renderer)
{
    return (HostBinding){"raster_text", "RasterText", draw_raster_text,
                         renderer};
}

HostBinding
RasterTextClippedBinding(TextRenderer *renderer)
{
    return (HostBinding){"raster_text", "RasterTextClipped",
                         draw_raster_text, renderer};
}
