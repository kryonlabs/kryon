#include "kryon_portable_host.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_FRAMES = 1024 };

typedef struct PointerSample {
    float x, y;
    int down, pressed, released;
} PointerSample;

typedef struct ReplayHost {
    FILE *output;
    char *pointer_module;
    PointerSample input;
    VmHostField pointer_fields[5];
    int pointer_calls;
    int capture_calls;
    int phase;
    int nodes, paints;
    int expected_nodes, expected_paints;
    int effects;
} ReplayHost;

static int
json_string(FILE *out, const unsigned char *data, size_t length)
{
    if(fputc('"', out) == EOF) return 0;
    for(size_t i = 0; i < length; i++) {
        unsigned char c = data[i];
        if(c == '"' || c == '\\') {
            if(fputc('\\', out) == EOF || fputc(c, out) == EOF) return 0;
        } else if(c < 0x20) {
            if(fprintf(out, "\\u%04x", c) < 0) return 0;
        } else if(fputc(c, out) == EOF) {
            return 0;
        }
    }
    return fputc('"', out) != EOF;
}

static int
json_value(FILE *out, const VmHostValue *value, int depth)
{
    if(depth > 32) return 0;
    switch(value->kind) {
    case VM_HOST_VOID:
        return fputs("null", out) != EOF;
    case VM_HOST_INTEGER:
        if(value->type != NULL && strcmp(value->type, "bool") == 0)
            return fputs(value->integer ? "true" : "false", out) != EOF;
        return fprintf(out, "%lld", (long long)value->integer) >= 0;
    case VM_HOST_UNSIGNED:
        return fprintf(out, "%llu", (unsigned long long)value->bits) >= 0;
    case VM_HOST_REAL:
        return isfinite(value->real) &&
            fprintf(out, "%.17g", value->real) >= 0;
    case VM_HOST_STRING:
        return json_string(out, value->data, value->length);
    case VM_HOST_RECORD:
        if(fputc('{', out) == EOF) return 0;
        for(size_t i = 0; i < value->field_count; i++) {
            const VmHostField *field = &value->fields[i];
            if((i && fputc(',', out) == EOF) ||
               !json_string(out, (const unsigned char *)field->name,
                            strlen(field->name)) ||
               fputc(':', out) == EOF ||
               !json_value(out, &field->value, depth + 1)) return 0;
        }
        return fputc('}', out) != EOF;
    case VM_HOST_SLICE:
        if(fputc('[', out) == EOF) return 0;
        for(size_t i = 0; i < value->length; i++) {
            if((i && fputc(',', out) == EOF) ||
               !json_value(out, &value->elements[i], depth + 1)) return 0;
        }
        return fputc(']', out) != EOF;
    }
    return 0;
}

static int
poll_pointer(void *context, const char *module, const char *function,
             const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ReplayHost *host = context;
    (void)args;
    if(strcmp(module, host->pointer_module) != 0 ||
       strcmp(function, "PollPointer") != 0 || arg_count != 0 ||
       host->pointer_calls != 0) return 0;
    host->pointer_calls++;
    host->pointer_fields[0] = (VmHostField){"x",
        {.kind = VM_HOST_REAL, .type = "float32", .real = host->input.x}};
    host->pointer_fields[1] = (VmHostField){"y",
        {.kind = VM_HOST_REAL, .type = "float32", .real = host->input.y}};
    host->pointer_fields[2] = (VmHostField){"down",
        {.kind = VM_HOST_INTEGER, .type = "bool", .integer = host->input.down}};
    host->pointer_fields[3] = (VmHostField){"pressed",
        {.kind = VM_HOST_INTEGER, .type = "bool", .integer = host->input.pressed}};
    host->pointer_fields[4] = (VmHostField){"released",
        {.kind = VM_HOST_INTEGER, .type = "bool", .integer = host->input.released}};
    *result = (VmHostValue){.kind = VM_HOST_RECORD, .type = "PointerFrame",
        .fields = host->pointer_fields, .field_count = 5};
    return 1;
}

static int
record_frame_start(void *context, const char *module, const char *function,
                   const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ReplayHost *host = context;
    (void)module;
    (void)function;
    if(arg_count != 2 || host->phase != 0 || host->capture_calls != 0 ||
       args[0].kind != VM_HOST_INTEGER ||
       args[1].kind != VM_HOST_INTEGER ||
       args[0].integer < 0 || args[0].integer > 1024 ||
       args[1].integer < 0 || args[1].integer > 4096) return 0;
    host->expected_nodes = (int)args[0].integer;
    host->expected_paints = (int)args[1].integer;
    host->capture_calls++;
    host->phase = 1;
    if(fputs("\"capture\":{\"nodes\":[", host->output) == EOF) return 0;
    *result = (VmHostValue){.kind = VM_HOST_VOID, .type = "void"};
    return 1;
}

static int
record_node(void *context, const char *module, const char *function,
            const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ReplayHost *host = context;
    (void)module;
    (void)function;
    if(arg_count != 2 || host->phase != 1 ||
       args[0].kind != VM_HOST_INTEGER ||
       args[0].integer != host->nodes ||
       args[1].kind != VM_HOST_RECORD ||
       host->nodes >= host->expected_nodes) return 0;
    if((host->nodes && fputc(',', host->output) == EOF) ||
       !json_value(host->output, &args[1], 0)) return 0;
    host->nodes++;
    *result = (VmHostValue){.kind = VM_HOST_VOID, .type = "void"};
    return 1;
}

static int
record_paint(void *context, const char *module, const char *function,
             const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ReplayHost *host = context;
    (void)module;
    (void)function;
    if(arg_count != 2 || host->phase == 0 ||
       host->nodes != host->expected_nodes ||
       args[0].kind != VM_HOST_INTEGER ||
       args[0].integer != host->paints ||
       args[1].kind != VM_HOST_RECORD ||
       host->paints >= host->expected_paints) return 0;
    if(host->phase == 1) {
        if(fputs("],\"paint\":[", host->output) == EOF) return 0;
        host->phase = 2;
    }
    if((host->paints && fputc(',', host->output) == EOF) ||
       !json_value(host->output, &args[1], 0)) return 0;
    host->paints++;
    *result = (VmHostValue){.kind = VM_HOST_VOID, .type = "void"};
    return 1;
}

static int
record_frame_end(void *context, const char *module, const char *function,
                 const VmHostValue *args, int arg_count, VmHostValue *result)
{
    ReplayHost *host = context;
    (void)module;
    (void)function;
    (void)args;
    if(arg_count != 0 || host->phase == 0 ||
       host->nodes != host->expected_nodes ||
       host->paints != host->expected_paints) return 0;
    if(host->phase == 1 &&
       fputs("],\"paint\":[", host->output) == EOF) return 0;
    if(fputs("]}", host->output) == EOF) return 0;
    host->phase = 3;
    *result = (VmHostValue){.kind = VM_HOST_VOID, .type = "void"};
    return 1;
}

static int
width(void *context, uint8_t *value, size_t length, int font,
      uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)value; (void)typeface; (void)typeface_length;
    return (int)((double)length * font * 0.6);
}

static int
line_height(void *context, int font, uint8_t *typeface,
            size_t typeface_length)
{
    (void)context; (void)typeface; (void)typeface_length;
    return font;
}

static void
fill(void *context, float x, float y, float w, float h, float radius,
     int segments, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    ReplayHost *host = context;
    (void)x; (void)y; (void)w; (void)h; (void)radius; (void)segments;
    (void)r; (void)g; (void)b; (void)a;
    host->effects++;
}

static void
outline(void *context, float x, float y, float w, float h, float radius,
        int segments, float line_width,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)line_width;
    fill(context, x, y, w, h, radius, segments, r, g, b, a);
}

static void
line(void *context, float x1, float y1, float x2, float y2,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    fill(context, x1, y1, x2, y2, 0, 0, r, g, b, a);
}

static void
draw_text(void *context, uint8_t *value, size_t length,
          int x, int y, int font, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)value; (void)length;
    fill(context, (float)x, (float)y, (float)font, 0, 0, 0, r, g, b, a);
}

static void
draw_text_clipped(void *context, uint8_t *value, size_t length,
                  int x, int y, int font, float clip[4],
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)clip;
    draw_text(context, value, length, x, y, font, r, g, b, a);
}

static int
image_size(void *context, uint8_t *path, size_t length,
           int *width, int *height)
{
    (void)context; (void)path; (void)length;
    *width = 0;
    *height = 0;
    return 0;
}

static void
image_draw(void *context, uint8_t *path, size_t length,
           uint32_t texture_id, float source[4],
           float destination[4], float clip[4],
           float origin[2], float rotation, float radius,
           uint8_t tint[4])
{
    ReplayHost *host = context;
    (void)path; (void)length; (void)texture_id; (void)source;
    (void)destination; (void)clip; (void)origin; (void)rotation;
    (void)radius; (void)tint;
    host->effects++;
}

static int
read_samples(char *path, PointerSample samples[MAX_FRAMES])
{
    FILE *file = fopen(path, "r");
    if(file == NULL) { perror(path); return -1; }
    char line[512];
    int count = 0, line_number = 0, valid = 1;
    while(fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        if(strchr(line, '\n') == NULL && !feof(file)) { valid = 0; break; }
        char *start = line;
        while(*start == ' ' || *start == '\t') start++;
        if(*start == '#' || *start == '\n' || *start == '\0') continue;
        PointerSample sample;
        char extra;
        int parsed = sscanf(start, "%f %f %d %d %d %c", &sample.x,
            &sample.y, &sample.down, &sample.pressed, &sample.released, &extra);
        if(parsed != 5 || !isfinite(sample.x) || !isfinite(sample.y) ||
           (sample.down != 0 && sample.down != 1) ||
           (sample.pressed != 0 && sample.pressed != 1) ||
           (sample.released != 0 && sample.released != 1) ||
           count >= MAX_FRAMES) { valid = 0; break; }
        samples[count++] = sample;
    }
    int ok = valid && feof(file) && !ferror(file) && count > 0;
    if(fclose(file) != 0) ok = 0;
    if(!ok) fprintf(stderr, "invalid trace near line %d: %s\n",
                    line_number, path);
    return ok ? count : -1;
}

int
main(int argc, char **argv)
{
    if(argc != 5) {
        fprintf(stderr,
            "usage: %s app.zib pointer-module input.trace output.json\n",
            argv[0]);
        return 2;
    }
    if(strcmp(argv[3], argv[4]) == 0) {
        fputs("input and output must differ\n", stderr);
        return 2;
    }
    PointerSample samples[MAX_FRAMES];
    int sample_count = read_samples(argv[3], samples);
    if(sample_count < 0) return 1;
    Bundle *bundle = BundleOpen(argv[1]);
    if(bundle == NULL) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    FILE *output = fopen(argv[4], "w");
    if(output == NULL) { perror(argv[4]); BundleClose(bundle); return 1; }
    ReplayHost host = {0};
    host.output = output;
    host.pointer_module = argv[2];
    FontMeasurer fonts = {width, line_height, &host};
    RoundedRectangleRenderer shapes = {fill, outline, &host};
    TextRenderer texts = {draw_text, &host, draw_text_clipped};
    LineRenderer lines = {line, &host};
    ImageRasterizer images = {image_size, image_draw, &host};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterTextBinding(&texts),
        RasterTextClippedBinding(&texts),
        RasterLineBinding(&lines),
        RasterImageBinding(&images),
        ImageWidthBinding(&images),
        ImageHeightBinding(&images),
        {argv[2], "PollPointer", poll_pointer, &host},
        {"frame_capture", "RecordFrameStart", record_frame_start, &host},
        {"frame_capture", "RecordNode", record_node, &host},
        {"frame_capture", "RecordPaint", record_paint, &host},
        {"frame_capture", "RecordFrameEnd", record_frame_end, &host},
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings,
        sizeof(bindings) / sizeof(bindings[0]));
    int ok = instance != NULL && fputs("{\"version\":1,\"frames\":[", output) != EOF;
    for(int i = 0; ok && i < sample_count; i++) {
        host.input = samples[i];
        host.pointer_calls = host.capture_calls = 0;
        host.phase = host.nodes = host.paints = host.effects = 0;
        if(i && fputc(',', output) == EOF) { ok = 0; break; }
        ok = fprintf(output,
            "{\"input\":{\"x\":%.9g,\"y\":%.9g,\"down\":%s,"
            "\"pressed\":%s,\"released\":%s},",
            samples[i].x, samples[i].y,
            samples[i].down ? "true" : "false",
            samples[i].pressed ? "true" : "false",
            samples[i].released ? "true" : "false") >= 0;
        long long result = 0;
        int has_result = 0;
        if(ok) ok = BundleInstanceRun(instance, &result, &has_result);
        ok = ok && host.pointer_calls == 1 && host.capture_calls == 1 &&
             host.phase == 3;
        if(ok) ok = fprintf(output, ",\"result\":%lld,\"effects\":%d}",
                            result, host.effects) >= 0 && has_result;
    }
    if(ok) ok = fputs("]}\n", output) != EOF && !ferror(output);
    if(fclose(output) != 0) ok = 0;
    BundleInstanceClose(instance);
    BundleClose(bundle);
    if(!ok) fprintf(stderr, "frame replay failed\n");
    return ok ? 0 : 1;
}
