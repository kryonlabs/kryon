#include "ui_internal.h"
#include "ui_paint_internal.h"

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
ui_draw_surface(SurfaceDrawing command)
{
    if(command.visible && !ui_draw_surface_cached(command))
        ui_draw_surface_direct(command);
}

void
ui_draw(Drawing command)
{
    Rectangle bounds = command.bounds;
    Color color = GetColor(command.color);
    switch(command.kind) {
    case DrawingText: {
        int baseline = TextBaselineY(command.text, (int)bounds.y,
            (int)bounds.height, command.font);
        RenderNonSelectableText(command.text, (int)bounds.x, baseline,
                                command.font, color);
        break;
    }
    case DrawingIcon:
        DrawIcon(command.icon, bounds, color);
        break;
    case DrawingTexture: {
        static const Vector2 origin;
        Rectangle source = {0, 0, (float)command.texture.width,
                            (float)command.texture.height};
        DrawTexturePro(command.texture, source, bounds, origin, 0.0f, color);
        break;
    }
    case DrawingChevron:
        for(int y = (int)floorf(bounds.y); y < (int)ceilf(bounds.y + bounds.height); y++) {
            for(int x = (int)floorf(bounds.x); x < (int)ceilf(bounds.x + bounds.width); x++) {
                float coverage = ChevronCoverage(x - bounds.x, y - bounds.y, bounds.width);
                if(coverage > 0.0f)
                    DrawRectangle(x, y, 1, 1, GetColor(Opacity(command.color, coverage)));
            }
        }
        break;
    case DrawingRing: {
        Ring ring = command.ring;
        if(!FancyEffectsEnabled())
            ring.glow_blur = 0.0f;
        float radius = LoadingPaintRadius(ring);
        for(int y = (int)floorf(ring.y - radius); y < (int)ceilf(ring.y + radius); y++) {
            for(int x = (int)floorf(ring.x - radius); x < (int)ceilf(ring.x + radius); x++) {
                RingSample sample = LoadingSample(ring, x - ring.x, y - ring.y);
                if((sample.glow & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.glow));
                if((sample.track & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.track));
                if((sample.arc & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.arc));
                if((sample.tip & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.tip));
            }
        }
        break;
    }
    }
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
