#include "terminal_pane.h"

#include <stdlib.h>
#include <string.h>

#define TERMINAL_PANE_SIXEL_MAX_DIMENSION 4096
#define TERMINAL_PANE_SIXEL_MAX_PIXELS 4194304

typedef struct TerminalPaneSixelBuilder {
    int *pixels;
    int width;
    int height;
    int max_x;
    int max_y;
    int raster_width;
    int raster_height;
    int pixel_aspect_num;
    int pixel_aspect_den;
    int x;
    int y;
    int color_index;
    int palette[256];
    int background;
    int transparent;
    int failed;
} TerminalPaneSixelBuilder;

static int
clamp_sixel_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static void
init_sixel_builder(TerminalPaneSixelBuilder *builder, int transparent,
                   int background, TerminalPaneSixelPaletteFn palette,
                   void *userdata)
{
    int i;

    memset(builder, 0, sizeof(*builder));
    builder->max_x = -1;
    builder->max_y = -1;
    builder->color_index = 0;
    builder->pixel_aspect_num = 1;
    builder->pixel_aspect_den = 1;
    builder->background =
        transparent ? TERMINAL_PANE_COLOR_DEFAULT : background;
    builder->transparent = transparent ? 1 : 0;
    for(i = 0; i < 256; i++) {
        builder->palette[i] = palette != NULL
                                  ? palette(userdata, i)
                                  : TerminalPaneDefaultPaletteColor(i);
    }
}

static int
ensure_sixel_canvas(TerminalPaneSixelBuilder *builder, int width, int height)
{
    int new_width;
    int new_height;
    int *pixels;
    int row;

    if(builder == NULL || width <= 0 || height <= 0 || builder->failed)
        return 0;
    if(width > TERMINAL_PANE_SIXEL_MAX_DIMENSION ||
       height > TERMINAL_PANE_SIXEL_MAX_DIMENSION ||
       (size_t)width * (size_t)height > TERMINAL_PANE_SIXEL_MAX_PIXELS) {
        builder->failed = 1;
        return 0;
    }
    if(width <= builder->width && height <= builder->height)
        return 1;
    new_width = builder->width > 0 ? builder->width : 1;
    new_height = builder->height > 0 ? builder->height : 1;
    while(new_width < width)
        new_width *= 2;
    while(new_height < height)
        new_height *= 2;
    if(new_width > TERMINAL_PANE_SIXEL_MAX_DIMENSION)
        new_width = width;
    if(new_height > TERMINAL_PANE_SIXEL_MAX_DIMENSION)
        new_height = height;
    if((size_t)new_width * (size_t)new_height >
       TERMINAL_PANE_SIXEL_MAX_PIXELS) {
        new_width = width;
        new_height = height;
    }
    if((size_t)new_width * (size_t)new_height >
       TERMINAL_PANE_SIXEL_MAX_PIXELS) {
        builder->failed = 1;
        return 0;
    }
    pixels = malloc((size_t)new_width * (size_t)new_height * sizeof(int));
    if(pixels == NULL) {
        builder->failed = 1;
        return 0;
    }
    for(row = 0; row < new_height; row++) {
        int col;

        for(col = 0; col < new_width; col++)
            pixels[row * new_width + col] = builder->background;
    }
    for(row = 0; row < builder->height; row++) {
        memcpy(pixels + row * new_width,
               builder->pixels + row * builder->width,
               (size_t)builder->width * sizeof(int));
    }
    free(builder->pixels);
    builder->pixels = pixels;
    builder->width = new_width;
    builder->height = new_height;
    return 1;
}

static int
read_sixel_number(const char **cursor)
{
    int value = 0;
    int seen = 0;

    if(cursor == NULL || *cursor == NULL)
        return -1;
    while(**cursor >= '0' && **cursor <= '9') {
        value = value * 10 + (**cursor - '0');
        (*cursor)++;
        seen = 1;
    }
    return seen ? value : -1;
}

static int
read_sixel_param(const char **cursor, int fallback)
{
    int value;

    value = read_sixel_number(cursor);
    if(**cursor == ';')
        (*cursor)++;
    return value >= 0 ? value : fallback;
}

static int
percent_to_byte(int value)
{
    value = clamp_sixel_int(value, 0, 100);
    return value * 255 / 100;
}

static int
hls_to_rgb(int hue, int lightness, int saturation)
{
    int c;
    int x;
    int m;
    int segment;
    int remainder;
    int r = 0;
    int g = 0;
    int b = 0;

    hue %= 360;
    if(hue < 0)
        hue += 360;
    lightness = clamp_sixel_int(lightness, 0, 100);
    saturation = clamp_sixel_int(saturation, 0, 100);
    c = (100 - abs(2 * lightness - 100)) * saturation / 100;
    segment = hue / 60;
    remainder = hue % 120;
    x = c * (60 - abs(remainder - 60)) / 60;
    m = lightness - c / 2;
    if(segment == 0) {
        r = c;
        g = x;
    } else if(segment == 1) {
        r = x;
        g = c;
    } else if(segment == 2) {
        g = c;
        b = x;
    } else if(segment == 3) {
        g = x;
        b = c;
    } else if(segment == 4) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    r = percent_to_byte(r + m);
    g = percent_to_byte(g + m);
    b = percent_to_byte(b + m);
    return TERMINAL_PANE_COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
}

static void
put_sixel_column(TerminalPaneSixelBuilder *builder, int bits)
{
    int bit;

    if(builder == NULL || builder->failed)
        return;
    if(!ensure_sixel_canvas(builder, builder->x + 1, builder->y + 6))
        return;
    for(bit = 0; bit < 6; bit++) {
        if((bits & (1 << bit)) != 0) {
            int y = builder->y + bit;

            builder->pixels[y * builder->width + builder->x] =
                builder->palette[builder->color_index];
            if(builder->x > builder->max_x)
                builder->max_x = builder->x;
            if(y > builder->max_y)
                builder->max_y = y;
        }
    }
    if(builder->x > builder->max_x)
        builder->max_x = builder->x;
    if(builder->y + 5 > builder->max_y)
        builder->max_y = builder->y + 5;
    builder->x++;
}

static int
copy_sixel_result(TerminalPaneSixelImage *out,
                  TerminalPaneSixelBuilder *builder, int width, int height)
{
    int *pixels;
    int row;

    if(out == NULL || builder == NULL || builder->pixels == NULL ||
       width <= 0 || height <= 0 || builder->width < width ||
       builder->height < height)
        return 0;
    pixels = malloc((size_t)width * (size_t)height * sizeof(int));
    if(pixels == NULL)
        return 0;
    for(row = 0; row < height; row++) {
        memcpy(pixels + row * width, builder->pixels + row * builder->width,
               (size_t)width * sizeof(int));
    }
    out->pixels = pixels;
    out->width = width;
    out->height = height;
    out->pixel_aspect_num =
        builder->pixel_aspect_num > 0 ? builder->pixel_aspect_num : 1;
    out->pixel_aspect_den =
        builder->pixel_aspect_den > 0 ? builder->pixel_aspect_den : 1;
    return 1;
}

int
DecodeTerminalPaneSixel(TerminalPaneSixelImage *out, const char *payload,
                        int background, TerminalPaneSixelPaletteFn palette,
                        void *userdata)
{
    TerminalPaneSixelBuilder builder;
    const char *cursor;
    const char *data;
    int transparent = 1;
    int width;
    int height;
    int rows_used = 0;

    if(out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));
    if(payload == NULL)
        return 0;
    data = strchr(payload, 'q');
    if(data == NULL)
        return 0;
    cursor = payload;
    if(cursor < data) {
        int param_index = 0;

        while(cursor < data) {
            int value = read_sixel_number(&cursor);

            if(param_index == 1 && value == 2)
                transparent = 0;
            if(*cursor == ';') {
                cursor++;
                param_index++;
            } else if(cursor < data) {
                cursor++;
            }
        }
    }
    init_sixel_builder(&builder, transparent, background, palette, userdata);
    cursor = data + 1;
    while(*cursor != '\0' && !builder.failed) {
        int ch = (unsigned char)*cursor++;

        if(ch >= '?' && ch <= '~') {
            put_sixel_column(&builder, ch - '?');
        } else if(ch == '!') {
            int repeat = read_sixel_number(&cursor);
            int repeated;

            if(repeat <= 0)
                repeat = 1;
            if(*cursor == '\0')
                break;
            repeated = (unsigned char)*cursor++;
            if(repeated >= '?' && repeated <= '~') {
                int i;

                repeat = clamp_sixel_int(
                    repeat, 1, TERMINAL_PANE_SIXEL_MAX_DIMENSION);
                for(i = 0; i < repeat && !builder.failed; i++)
                    put_sixel_column(&builder, repeated - '?');
            }
        } else if(ch == '$') {
            builder.x = 0;
        } else if(ch == '-') {
            builder.x = 0;
            builder.y += 6;
            if(builder.y > TERMINAL_PANE_SIXEL_MAX_DIMENSION)
                builder.failed = 1;
        } else if(ch == '#') {
            int index = read_sixel_number(&cursor);

            if(index >= 0 && index < 256) {
                builder.color_index = index;
                if(*cursor == ';') {
                    int mode;
                    int a;
                    int b;
                    int c;

                    cursor++;
                    mode = read_sixel_param(&cursor, 0);
                    a = read_sixel_param(&cursor, 0);
                    b = read_sixel_param(&cursor, 0);
                    c = read_sixel_param(&cursor, 0);
                    if(mode == 2) {
                        builder.palette[index] =
                            TERMINAL_PANE_COLOR_TRUE_RGB |
                            (percent_to_byte(a) << 16) |
                            (percent_to_byte(b) << 8) |
                            percent_to_byte(c);
                    } else if(mode == 1) {
                        builder.palette[index] = hls_to_rgb(a, b, c);
                    }
                }
            }
        } else if(ch == '"') {
            int pan = read_sixel_param(&cursor, 0);
            int pad = read_sixel_param(&cursor, 0);
            int raster_width = read_sixel_param(&cursor, 0);
            int raster_height = read_sixel_param(&cursor, 0);

            if(pan > 0 && pad > 0) {
                builder.pixel_aspect_num = pan;
                builder.pixel_aspect_den = pad;
            }
            if(raster_width > 0 && raster_height > 0) {
                builder.raster_width = raster_width;
                builder.raster_height = raster_height;
                ensure_sixel_canvas(&builder, raster_width, raster_height);
            }
        }
    }
    width = builder.max_x + 1;
    height = builder.max_y + 1;
    if(builder.raster_width > width)
        width = builder.raster_width;
    if(builder.raster_height > height)
        height = builder.raster_height;
    if(!builder.failed && width > 0 && height > 0 &&
       copy_sixel_result(out, &builder, width, height)) {
        rows_used = (height + 5) / 6;
        if(rows_used < 1)
            rows_used = 1;
    }
    free(builder.pixels);
    if(rows_used == 0)
        FreeTerminalPaneSixelImage(out);
    return rows_used;
}

void
FreeTerminalPaneSixelImage(TerminalPaneSixelImage *image)
{
    if(image == NULL)
        return;
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}
