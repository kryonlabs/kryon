#include "kryon_portable_host.h"

#ifdef KRYON_NATIVE_DESKTOP
#ifdef KRYON_NATIVE_IMAGE
#include "image_demo.h"
#else
#include "hello.h"
#endif
#include "font_metrics.h"
#include "image_raster.h"
#include "paint_queue.h"
#include "pointer_input.h"
#include "raster.h"
#include "raster_shape.h"
#include "raster_text.h"
#endif

#include <SDL.h>
#include <cairo.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 160

typedef struct DesktopHost {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Surface *pixels;
    cairo_surface_t *surface;
    cairo_t *paint;
    PointerHost pointer;
    int image_error;
} DesktopHost;

static char *
copy_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1);
    if(copy == NULL) return NULL;
    if(length > 0) memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static void
set_color(cairo_t *paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    cairo_set_source_rgba(paint, r / 255.0, g / 255.0, b / 255.0,
                          a / 255.0);
}

static void
round_box(cairo_t *paint, double x, double y, double width,
          double height, double radius)
{
    const double pi = 3.14159265358979323846;
    cairo_new_path(paint);
    if(width <= 0 || height <= 0) return;
    if(radius < 0) radius = 0;
    if(radius > width / 2) radius = width / 2;
    if(radius > height / 2) radius = height / 2;
    if(radius == 0) {
        cairo_rectangle(paint, x, y, width, height);
        return;
    }
    cairo_arc(paint, x + width - radius, y + radius, radius,
              -pi / 2, 0);
    cairo_arc(paint, x + width - radius, y + height - radius, radius,
              0, pi / 2);
    cairo_arc(paint, x + radius, y + height - radius, radius,
              pi / 2, pi);
    cairo_arc(paint, x + radius, y + radius, radius,
              pi, 3 * pi / 2);
    cairo_close_path(paint);
}

static int
text_width(void *context, const char *text, size_t length, int font,
           const char *typeface, size_t typeface_length)
{
    DesktopHost *host = context;
    char *copy = copy_text(text, length);
    cairo_text_extents_t extents;
    (void)typeface;
    (void)typeface_length;
    if(copy == NULL) return 0;
    cairo_select_font_face(host->paint, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(host->paint, font);
    cairo_text_extents(host->paint, copy, &extents);
    free(copy);
    return (int)ceil(extents.x_advance);
}

static int
text_line_height(void *context, int font, const char *typeface,
                 size_t typeface_length)
{
    DesktopHost *host = context;
    cairo_font_extents_t extents;
    (void)typeface;
    (void)typeface_length;
    cairo_select_font_face(host->paint, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(host->paint, font);
    cairo_font_extents(host->paint, &extents);
    return (int)ceil(extents.height);
}

static void
fill(void *context, float x, float y, float width, float height,
     float radius, int segments,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    DesktopHost *host = context;
    (void)segments;
    round_box(host->paint, x, y, width, height, radius);
    set_color(host->paint, r, g, b, a);
    cairo_fill(host->paint);
}

static void
outline(void *context, float x, float y, float width, float height,
        float radius, int segments, float line_width,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    DesktopHost *host = context;
    (void)segments;
    round_box(host->paint, x, y, width, height, radius);
    set_color(host->paint, r, g, b, a);
    cairo_set_line_width(host->paint, line_width);
    cairo_stroke(host->paint);
}

static void
line(void *context, float x1, float y1, float x2, float y2,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    DesktopHost *host = context;
    cairo_move_to(host->paint, x1, y1);
    cairo_line_to(host->paint, x2, y2);
    set_color(host->paint, r, g, b, a);
    cairo_set_line_width(host->paint, 1);
    cairo_stroke(host->paint);
}

static void
draw_text(void *context, const char *text, size_t length,
          int x, int y, int font,
          uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    DesktopHost *host = context;
    char *copy = copy_text(text, length);
    cairo_font_extents_t extents;
    if(copy == NULL) return;
    cairo_select_font_face(host->paint, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(host->paint, font);
    cairo_font_extents(host->paint, &extents);
    cairo_move_to(host->paint, x, y + extents.ascent);
    set_color(host->paint, r, g, b, a);
    cairo_show_text(host->paint, copy);
    free(copy);
}

static void
draw_text_clipped(void *context, const char *text, size_t length,
                  int x, int y, int font, const float clip[4],
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    DesktopHost *host = context;
    cairo_save(host->paint);
    cairo_rectangle(host->paint, clip[0], clip[1], clip[2], clip[3]);
    cairo_clip(host->paint);
    draw_text(context, text, length, x, y, font, r, g, b, a);
    cairo_restore(host->paint);
}

static int
image_size(void *context, const char *path, size_t length,
           int *width, int *height)
{
    cairo_surface_t *image;
    char *name;
    (void)context;
    name = copy_text(path, length);
    if(name == NULL) return 0;
    image = cairo_image_surface_create_from_png(name);
    free(name);
    if(cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(image);
        return 0;
    }
    *width = cairo_image_surface_get_width(image);
    *height = cairo_image_surface_get_height(image);
    cairo_surface_destroy(image);
    return 1;
}

static cairo_surface_t *
tinted_image(cairo_surface_t *image, const uint8_t tint[4])
{
    int width = cairo_image_surface_get_width(image);
    int height = cairo_image_surface_get_height(image);
    int input_stride = cairo_image_surface_get_stride(image);
    cairo_surface_t *copy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                        width, height);
    if(cairo_surface_status(copy) != CAIRO_STATUS_SUCCESS) return copy;
    cairo_surface_flush(image);
    for(int y = 0; y < height; y++) {
        uint32_t *input = (uint32_t *)(cairo_image_surface_get_data(image) +
                                       y * input_stride);
        uint32_t *output = (uint32_t *)(cairo_image_surface_get_data(copy) +
                                        y * cairo_image_surface_get_stride(copy));
        for(int x = 0; x < width; x++) {
            uint32_t pixel = input[x];
            uint32_t alpha = ((pixel >> 24) & 255u) * tint[3] / 255u;
            uint32_t red = ((pixel >> 16) & 255u) * tint[0] * tint[3] /
                           (255u * 255u);
            uint32_t green = ((pixel >> 8) & 255u) * tint[1] * tint[3] /
                             (255u * 255u);
            uint32_t blue = (pixel & 255u) * tint[2] * tint[3] /
                            (255u * 255u);
            output[x] = (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }
    cairo_surface_mark_dirty(copy);
    return copy;
}

static void
image_draw(void *context, const char *path, size_t length,
           uint32_t texture_id, const float source[4],
           const float destination[4], const float clip[4],
           const float origin[2], float rotation, float radius,
           const uint8_t tint[4])
{
    DesktopHost *host = context;
    char *name;
    cairo_surface_t *image;
    cairo_surface_t *colored;
    if(texture_id != 0 || path == NULL || length == 0 ||
       source[2] <= 0 || source[3] <= 0 ||
       destination[2] <= 0 || destination[3] <= 0 ||
       clip[2] <= 0 || clip[3] <= 0) {
        host->image_error = 1;
        return;
    }
    name = copy_text(path, length);
    if(name == NULL) {
        host->image_error = 1;
        return;
    }
    image = cairo_image_surface_create_from_png(name);
    free(name);
    if(cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        host->image_error = 1;
        cairo_surface_destroy(image);
        return;
    }
    colored = image;
    if(tint[0] != 255 || tint[1] != 255 || tint[2] != 255 ||
       tint[3] != 255) {
        colored = tinted_image(image, tint);
        if(cairo_surface_status(colored) != CAIRO_STATUS_SUCCESS) {
            host->image_error = 1;
            cairo_surface_destroy(colored);
            cairo_surface_destroy(image);
            return;
        }
    }
    cairo_save(host->paint);
    round_box(host->paint, clip[0], clip[1], clip[2], clip[3], radius);
    cairo_clip(host->paint);
    cairo_translate(host->paint, destination[0], destination[1]);
    cairo_rotate(host->paint, rotation * 3.14159265358979323846 / 180.0);
    cairo_translate(host->paint, -origin[0], -origin[1]);
    cairo_rectangle(host->paint, 0, 0, destination[2], destination[3]);
    cairo_clip(host->paint);
    cairo_scale(host->paint, destination[2] / source[2],
                destination[3] / source[3]);
    cairo_translate(host->paint, -source[0], -source[1]);
    cairo_set_source_surface(host->paint, colored, 0, 0);
    cairo_pattern_set_extend(cairo_get_source(host->paint),
                             CAIRO_EXTEND_NONE);
    cairo_pattern_set_filter(cairo_get_source(host->paint),
                             CAIRO_FILTER_NEAREST);
    cairo_paint(host->paint);
    cairo_restore(host->paint);
    if(colored != image) cairo_surface_destroy(colored);
    cairo_surface_destroy(image);
}

static int
open_desktop(DesktopHost *host)
{
    if(SDL_Init(SDL_INIT_VIDEO) != 0) return 0;
    host->window = SDL_CreateWindow("Kryon Ziran", SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    if(host->window == NULL) return 0;
    host->renderer = SDL_CreateRenderer(host->window, -1,
                                        SDL_RENDERER_SOFTWARE);
    if(host->renderer == NULL) return 0;
    host->pixels = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 32,
                                                   SDL_PIXELFORMAT_ARGB8888);
    if(host->pixels == NULL) return 0;
    host->texture = SDL_CreateTexture(host->renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      WIDTH, HEIGHT);
    if(host->texture == NULL) return 0;
    host->surface = cairo_image_surface_create_for_data(host->pixels->pixels,
        CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT, host->pixels->pitch);
    if(cairo_surface_status(host->surface) != CAIRO_STATUS_SUCCESS) return 0;
    host->paint = cairo_create(host->surface);
    return cairo_status(host->paint) == CAIRO_STATUS_SUCCESS;
}

static void
close_desktop(DesktopHost *host)
{
    if(host->paint != NULL) cairo_destroy(host->paint);
    if(host->surface != NULL) cairo_surface_destroy(host->surface);
    if(host->texture != NULL) SDL_DestroyTexture(host->texture);
    if(host->pixels != NULL) SDL_FreeSurface(host->pixels);
    if(host->renderer != NULL) SDL_DestroyRenderer(host->renderer);
    if(host->window != NULL) SDL_DestroyWindow(host->window);
    SDL_Quit();
}

#ifdef KRYON_NATIVE_DESKTOP
/* The generated library calls these host effects directly when linked as C. */
static DesktopHost *active_host;

int32_t
MeasureGlyphWidth(String value, int32_t font, String typeface)
{
    return text_width(active_host, value.data, value.length, font,
                      typeface.data, typeface.length);
}

int32_t
MeasureGlyphLineHeight(int32_t font, String typeface)
{
    return text_line_height(active_host, font, typeface.data, typeface.length);
}

PointerFrame
PollPointer(void)
{
    PointerHost *pointer = &active_host->pointer;
    return (PointerFrame){pointer->x, pointer->y, pointer->down,
                          pointer->pressed, pointer->released};
}

void
RasterRoundedRectangle(Rectangle bounds, float radius, int32_t segments,
                       Color color)
{
    fill(active_host, bounds.x, bounds.y, bounds.width, bounds.height,
         radius, segments, color.r, color.g, color.b, color.a);
}

void
RasterRoundedRectangleOutline(Rectangle bounds, float radius,
                              int32_t segments, float width, Color color)
{
    outline(active_host, bounds.x, bounds.y, bounds.width, bounds.height,
            radius, segments, width, color.r, color.g, color.b, color.a);
}

void
RasterLine(Rectangle bounds, Color color)
{
    line(active_host, bounds.x, bounds.y,
         bounds.x + bounds.width, bounds.y + bounds.height,
         color.r, color.g, color.b, color.a);
}

void
RasterText(String value, int32_t x, int32_t y, int32_t font, Color color)
{
    draw_text(active_host, value.data, value.length, x, y, font,
              color.r, color.g, color.b, color.a);
}

void
RasterTextClipped(String value, int32_t x, int32_t y, int32_t font,
                  Color color, Rectangle clip)
{
    float bounds[4] = {clip.x, clip.y, clip.width, clip.height};
    draw_text_clipped(active_host, value.data, value.length, x, y, font,
                      bounds, color.r, color.g, color.b, color.a);
}

int32_t
ImageWidth(String asset_path)
{
    int width = 0, height = 0;
    if(!image_size(active_host, asset_path.data, asset_path.length,
                   &width, &height)) return 0;
    return width;
}

int32_t
ImageHeight(String asset_path)
{
    int width = 0, height = 0;
    if(!image_size(active_host, asset_path.data, asset_path.length,
                   &width, &height)) return 0;
    return height;
}

void
RasterImage(String asset_path, uint32_t texture_id, Rectangle source,
            Rectangle destination, Rectangle clip, Vector2 origin,
            float rotation, float radius, Color tint)
{
    float src[4] = {source.x, source.y, source.width, source.height};
    float dst[4] = {destination.x, destination.y,
                    destination.width, destination.height};
    float cut[4] = {clip.x, clip.y, clip.width, clip.height};
    float pivot[2] = {origin.x, origin.y};
    uint8_t shade[4] = {tint.r, tint.g, tint.b, tint.a};
    image_draw(active_host, asset_path.data, asset_path.length, texture_id,
               src, dst, cut, pivot, rotation, radius, shade);
}
#endif

static int
run_frame(DesktopHost *host, BundleInstance *instance, long long *value)
{
    cairo_save(host->paint);
    set_color(host->paint, 248, 250, 252, 255);
    cairo_paint(host->paint);
    cairo_restore(host->paint);
#ifdef KRYON_NATIVE_DESKTOP
    (void)instance;
    *value = Frame();
    if(*value < 0 || host->image_error ||
       cairo_status(host->paint) != CAIRO_STATUS_SUCCESS)
        return 0;
#else
    int has_result = 0;
    if(!BundleInstanceRun(instance, value, &has_result) || !has_result ||
       host->image_error || cairo_status(host->paint) != CAIRO_STATUS_SUCCESS)
        return 0;
#endif
    cairo_surface_flush(host->surface);
    if(SDL_UpdateTexture(host->texture, NULL, host->pixels->pixels,
                         host->pixels->pitch) != 0) return 0;
    if(SDL_RenderCopy(host->renderer, host->texture, NULL, NULL) != 0)
        return 0;
    SDL_RenderPresent(host->renderer);
    return 1;
}

int
main(int argc, char **argv)
{
    DesktopHost host = {0};
#ifndef KRYON_NATIVE_DESKTOP
    Bundle *bundle = NULL;
#endif
    BundleInstance *instance = NULL;
    int self_test;
    int image_test;
    int ok = 1;
#ifdef KRYON_NATIVE_DESKTOP
    int flag = 1, output = 2;
    if(argc != 1 && argc != 3) {
        fprintf(stderr, "usage: %s [--self-test|--image-self-test output.png]\n",
                argv[0]);
        return 2;
    }
#else
    int flag = 2, output = 3;
    if(argc != 2 && argc != 4) {
        fprintf(stderr, "usage: %s app.zib "
                "[--self-test|--image-self-test output.png]\n",
                argv[0]);
        return 2;
    }
    bundle = BundleOpen(argv[1]);
#endif
    self_test = argc == output + 1 && strcmp(argv[flag], "--self-test") == 0;
    image_test = argc == output + 1 &&
                 strcmp(argv[flag], "--image-self-test") == 0;
    if(argc == output + 1 && !self_test && !image_test) return 2;
#ifdef KRYON_NATIVE_DESKTOP
    active_host = &host;
#else
    if(bundle == NULL) {
        fprintf(stderr, "bundle load failed\n");
        ok = 0;
        goto done;
    }
#endif
    if(!open_desktop(&host)) {
        fprintf(stderr, "desktop initialization failed: %s\n",
                SDL_GetError());
        ok = 0;
        goto done;
    }
#ifndef KRYON_NATIVE_DESKTOP
    FontMeasurer fonts = {text_width, text_line_height, &host};
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
        PointerBinding(&host.pointer),
    };
    instance = BundleInstantiate(bundle, bindings,
        sizeof(bindings) / sizeof(bindings[0]));
    if(instance == NULL) {
        fprintf(stderr, "bundle instantiation failed\n");
        ok = 0;
        goto done;
    }
#endif
    if(self_test) {
        static const long long expected[4] = {2, 2, 102, 102};
        for(int frame = 0; ok && frame < 4; frame++) {
            long long value = 0;
            host.pointer.x = 80;
            host.pointer.y = 70;
            host.pointer.down = frame == 1;
            host.pointer.pressed = frame == 1;
            host.pointer.released = frame == 2;
            ok = run_frame(&host, instance, &value) && value == expected[frame];
            if(!ok) {
                fprintf(stderr, "desktop frame %d returned %lld (expected %lld), "
                        "image error: %d\n", frame, value,
                        expected[frame], host.image_error);
            }
        }
    } else if(image_test) {
        long long value = 0;
        int painted = 0;
        ok = run_frame(&host, instance, &value) && value == 2;
        if(ok) {
            const unsigned char *data = cairo_image_surface_get_data(host.surface);
            int stride = cairo_image_surface_get_stride(host.surface);
            for(int y = 32; y < 128; y++) {
                const uint32_t *row = (const uint32_t *)(data + y * stride);
                for(int x = 112; x < 208; x++) {
                    unsigned red = (row[x] >> 16) & 255u;
                    unsigned green = (row[x] >> 8) & 255u;
                    unsigned blue = row[x] & 255u;
                    if(red > green + 20 && red > blue + 20)
                        painted++;
                }
            }
            ok = painted > 0;
        }
        if(!ok) {
            fprintf(stderr, "desktop image frame returned %lld, "
                    "painted pixels: %d, image error: %d\n", value,
                    painted, host.image_error);
        }
    } else {
        int running = 1;
        while(running && ok) {
            SDL_Event event;
            long long value = 0;
            host.pointer.pressed = 0;
            host.pointer.released = 0;
            while(SDL_PollEvent(&event)) {
                if(event.type == SDL_QUIT) running = 0;
                if(event.type == SDL_MOUSEBUTTONDOWN &&
                   event.button.button == SDL_BUTTON_LEFT)
                    host.pointer.pressed = 1;
                if(event.type == SDL_MOUSEBUTTONUP &&
                   event.button.button == SDL_BUTTON_LEFT)
                    host.pointer.released = 1;
            }
            if(!running) break;
            int x, y;
            Uint32 buttons = SDL_GetMouseState(&x, &y);
            host.pointer.x = (float)x;
            host.pointer.y = (float)y;
            host.pointer.down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            ok = run_frame(&host, instance, &value);
            SDL_Delay(16);
        }
    }
    if((self_test || image_test) && ok) {
        cairo_status_t status = cairo_surface_write_to_png(host.surface,
                                                           argv[output]);
        if(status != CAIRO_STATUS_SUCCESS) {
            fprintf(stderr, "screenshot failed: %s\n",
                    cairo_status_to_string(status));
            ok = 0;
        }
    }
done:
#ifndef KRYON_NATIVE_DESKTOP
    if(instance != NULL) BundleInstanceClose(instance);
    if(bundle != NULL) BundleClose(bundle);
#endif
    close_desktop(&host);
    return ok ? 0 : 1;
}
