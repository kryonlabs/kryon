#include "ui_internal.h"
#include "ui_paint_internal.h"

int
ui_backend_is_termi(void)
{
#if defined(KRYON_BACKEND_TERMI)
    return 1;
#else
    return 0;
#endif
}

void
ui_draw_surface_direct(SurfaceDrawing command)
{
    if(!command.visible)
        return;
    SurfaceLayer layer = command.layer;
    Rectangle rect = command.bounds;
    int left = (int)floorf(command.area.x);
    int right = (int)ceilf(command.area.x + command.area.width);
    int top = (int)floorf(command.area.y);
    int bottom = (int)ceilf(command.area.y + command.area.height);
    for(int y = top; y < bottom; y++) {
        unsigned int shade = SampleColor(layer, ((float)y + 0.5f - rect.y) / rect.height);
        int run_start = left;
        unsigned int run_color = 0;
        for(int x = left; x <= right; x++) {
            unsigned int pixel = 0;
            if(x < right) {
                float coverage = SampleCoverage(layer, (float)x - rect.x,
                    (float)y - rect.y, command.scale);
                coverage *= SegmentCoverage((float)x - command.surface.x,
                    command.segment.x - command.surface.x,
                    command.segment.width, command.surface.width);
                pixel = Opacity(shade, coverage);
            }
            if(x == left)
                run_color = pixel;
            if(pixel != run_color || x == right) {
                if((run_color & 255) != 0)
                    DrawRectangle(run_start, y, x - run_start, 1, GetColor(run_color));
                run_start = x;
                run_color = pixel;
            }
        }
    }
}

void
ui_draw_texture(Texture2D texture, Rectangle bounds, Color tint)
{
    Rectangle source = {0, 0, (float)texture.width,
                        (float)texture.height};
    DrawTexturePro(texture, source, bounds,
                   (Vector2){0.0f, 0.0f}, 0.0f, tint);
}

static void
paint_surface(void *context, SurfaceDrawing command)
{
    (void)context;
    ui_draw_surface(command);
}

static void
paint_drawing(void *context, Drawing command)
{
    (void)context;
    ui_draw(command);
}

const SurfacePainter ui_surface_painter = {NULL, paint_surface};
const Painter ui_painter = {NULL, paint_drawing};
