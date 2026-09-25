#include "reorder_widget_behavior.h"
#include "raster_shape.h"

void RasterRoundedRectangle(Rectangle bounds, float radius,
                            int32_t segments, Color color)
{
    (void)bounds;
    (void)radius;
    (void)segments;
    (void)color;
}

void RasterRoundedRectangleOutline(Rectangle bounds, float radius,
                                   int32_t segments, float width,
                                   Color color)
{
    (void)bounds;
    (void)radius;
    (void)segments;
    (void)width;
    (void)color;
}

int main(void)
{
    return Answer() == 42 ? 0 : 1;
}
