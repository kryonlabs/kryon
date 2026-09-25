#include "kryon_portable_host.h"

#include <string.h>

static int
read_string(const VmHostValue *value, const char **text, size_t *length)
{
    if(value->kind != VM_HOST_STRING ||
       strcmp(value->type, "string") != 0)
        return 0;
    *text = (const char *)value->data;
    *length = value->length;
    return 1;
}

static int
read_font(const VmHostValue *value, int *font)
{
    if(value->kind != VM_HOST_INTEGER || strcmp(value->type, "s32") != 0)
        return 0;
    *font = (int)value->integer;
    return 1;
}

static int
measure_width(void *context, const char *module, const char *function,
              const VmHostValue *args, int arg_count, VmHostValue *result)
{
    FontMeasurer *measurer = context;
    const char *text, *typeface;
    size_t text_length, typeface_length;
    int font;
    if(measurer == NULL || measurer->width == NULL ||
       strcmp(module, "font_metrics") != 0 ||
       strcmp(function, "MeasureGlyphWidth") != 0 || arg_count != 3 ||
       !read_string(&args[0], &text, &text_length) ||
       !read_font(&args[1], &font) ||
       !read_string(&args[2], &typeface, &typeface_length))
        return 0;
    result->kind = VM_HOST_INTEGER;
    result->integer = measurer->width(measurer->context, (uint8_t *)text,
                                     text_length, font, (uint8_t *)typeface,
                                     typeface_length);
    return 1;
}

static int
measure_line_height(void *context, const char *module,
                    const char *function, const VmHostValue *args,
                    int arg_count, VmHostValue *result)
{
    FontMeasurer *measurer = context;
    const char *typeface;
    size_t typeface_length;
    int font;
    if(measurer == NULL || measurer->line_height == NULL ||
       strcmp(module, "font_metrics") != 0 ||
       strcmp(function, "MeasureGlyphLineHeight") != 0 || arg_count != 2 ||
       !read_font(&args[0], &font) ||
       !read_string(&args[1], &typeface, &typeface_length))
        return 0;
    result->kind = VM_HOST_INTEGER;
    result->integer = measurer->line_height(measurer->context, font,
                                            (uint8_t *)typeface, typeface_length);
    return 1;
}

HostBinding
MeasureGlyphWidthBinding(FontMeasurer *measurer)
{
    return (HostBinding){"font_metrics", "MeasureGlyphWidth",
                         measure_width, measurer};
}

HostBinding
MeasureGlyphLineHeightBinding(FontMeasurer *measurer)
{
    return (HostBinding){"font_metrics", "MeasureGlyphLineHeight",
                         measure_line_height, measurer};
}
