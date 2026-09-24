#include "kryon_portable_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SvgHost {
    FILE *file;
    int clip_id;
    int shapes;
    int labels;
} SvgHost;

static void
color(FILE *file, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    fprintf(file, "#%02x%02x%02x\" opacity=\"%.3f", r, g, b,
            (double)a / 255.0);
}

static void
xml_text(FILE *file, const char *value, size_t length)
{
    for(size_t i = 0; i < length; i++) {
        switch(value[i]) {
        case '&': fputs("&amp;", file); break;
        case '<': fputs("&lt;", file); break;
        case '>': fputs("&gt;", file); break;
        case '"': fputs("&quot;", file); break;
        default: fputc((unsigned char)value[i], file); break;
        }
    }
}

static int
width(void *context, const char *value, size_t length, int font,
      const char *typeface, size_t typeface_length)
{
    (void)context;
    (void)value;
    (void)typeface;
    (void)typeface_length;
    return (int)((double)length * font * 0.6);
}

static int
line_height(void *context, int font, const char *typeface,
            size_t typeface_length)
{
    (void)context;
    (void)typeface;
    (void)typeface_length;
    return font;
}

static void
fill(void *context, float x, float y, float width, float height,
     float radius, int segments,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SvgHost *host = context;
    (void)segments;
    fprintf(host->file,
            "<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" "
            "rx=\"%g\" fill=\"", x, y, width, height, radius);
    color(host->file, r, g, b, a);
    fputs("\"/>\n", host->file);
    host->shapes++;
}

static void
outline(void *context, float x, float y, float width, float height,
        float radius, int segments, float line_width,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SvgHost *host = context;
    (void)segments;
    fprintf(host->file,
            "<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" "
            "rx=\"%g\" fill=\"none\" stroke-width=\"%g\" stroke=\"",
            x, y, width, height, radius, line_width);
    color(host->file, r, g, b, a);
    fputs("\"/>\n", host->file);
    host->shapes++;
}

static void
line(void *context, float x1, float y1, float x2, float y2,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SvgHost *host = context;
    fprintf(host->file,
            "<line x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" "
            "stroke=\"", x1, y1, x2, y2);
    color(host->file, r, g, b, a);
    fputs("\"/>\n", host->file);
    host->shapes++;
}

static void
draw_text(void *context, const char *value, size_t length,
          int x, int y, int font,
          uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SvgHost *host = context;
    fprintf(host->file,
            "<text x=\"%d\" y=\"%d\" font-size=\"%d\" fill=\"",
            x, y + font, font);
    color(host->file, r, g, b, a);
    fputs("\">", host->file);
    xml_text(host->file, value, length);
    fputs("</text>\n", host->file);
    host->labels++;
}

static void
draw_text_clipped(void *context, const char *value, size_t length,
                  int x, int y, int font, const float clip[4],
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SvgHost *host = context;
    int id = ++host->clip_id;
    fprintf(host->file,
            "<defs><clipPath id=\"clip%d\"><rect x=\"%g\" y=\"%g\" "
            "width=\"%g\" height=\"%g\"/></clipPath></defs>\n",
            id, clip[0], clip[1], clip[2], clip[3]);
    fprintf(host->file, "<g clip-path=\"url(#clip%d)\">\n", id);
    draw_text(context, value, length, x, y, font, r, g, b, a);
    fputs("</g>\n", host->file);
}

static int
image_size(void *context, const char *path, size_t length,
           int *width, int *height)
{
    (void)context;
    (void)path;
    (void)length;
    *width = 0;
    *height = 0;
    return 0;
}

static void
image_draw(void *context, const char *path, size_t length,
           uint32_t texture_id, const float source[4],
           const float destination[4], const float clip[4],
           const float origin[2], float rotation, float radius,
           const uint8_t tint[4])
{
    (void)context;
    (void)path;
    (void)length;
    (void)texture_id;
    (void)source;
    (void)destination;
    (void)clip;
    (void)origin;
    (void)rotation;
    (void)radius;
    (void)tint;
}

int
main(int argc, char **argv)
{
    if(argc != 3) {
        fprintf(stderr, "usage: %s hello.zib hello.svg\n", argv[0]);
        return 2;
    }
    Bundle *bundle = BundleOpen(argv[1]);
    if(bundle == NULL) return 1;
    FILE *file = fopen(argv[2], "w");
    if(file == NULL) {
        BundleClose(bundle);
        return 1;
    }
    SvgHost host = {file, 0, 0, 0};
    FontMeasurer fonts = {width, line_height, &host};
    RoundedRectangleRenderer shape = {fill, outline, &host};
    TextRenderer text = {draw_text, &host, draw_text_clipped};
    LineRenderer strokes = {line, &host};
    ImageRasterizer images = {image_size, image_draw, &host};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterTextBinding(&text),
        RasterTextClippedBinding(&text),
        RasterLineBinding(&strokes),
        RasterImageBinding(&images),
        ImageWidthBinding(&images),
        ImageHeightBinding(&images),
    };
    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" "
          "width=\"320\" height=\"160\" viewBox=\"0 0 320 160\">\n",
          file);
    fputs("<rect width=\"320\" height=\"160\" fill=\"#f8fafc\"/>\n",
          file);
    long long result = 0;
    int has_result = 0;
    int ok = BundleRun(bundle, bindings,
                       sizeof(bindings) / sizeof(bindings[0]),
                       &result, &has_result);
    fputs("</svg>\n", file);
    ok = ok && has_result && result == 2 &&
         host.shapes > 0 && host.labels > 0 && !ferror(file);
    if(fclose(file) != 0) ok = 0;
    BundleClose(bundle);
    return ok ? 0 : 1;
}
