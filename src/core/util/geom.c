/*
 * Kryon Geometry Utilities
 * C89/C90 compliant
 *
 * Geometry and math utilities for widget rendering
 */

#include "graphics.h"
#include "util.h"
#include <stdlib.h>

/*
 * Calculate handle center position for slider
 * track_rect: The track rectangle
 * value: Current value (0-100)
 * handle_radius: Radius of the handle (unused, for API compatibility)
 * Returns: Center point of the handle
 */
Point util_calculate_handle_position(Rectangle track_rect, float value, int handle_radius)
{
    Point center;
    int track_width;

    (void)handle_radius;  /* Unused parameter for API compatibility */

    track_width = Dx(track_rect) - 6;  /* 3px inset on each side */
    center.x = track_rect.min.x + 3 + (int)((value / 100.0f) * track_width);
    center.y = track_rect.min.y + Dy(track_rect) / 2;

    return center;
}

/*
 * Check if point is inside handle (square region for detection)
 * Note: Handle is drawn as square centered at center point
 */
int util_is_point_in_handle(Point p, Point center, int radius)
{
    Rectangle handle_rect;
    handle_rect.min.x = center.x - radius;
    handle_rect.min.y = center.y - radius;
    handle_rect.max.x = center.x + radius;
    handle_rect.max.y = center.y + radius;
    return ptinrect(p, handle_rect);
}

/*
 * Calculate slider value from mouse position
 * Returns value in range [0.0, 100.0]
 */
float util_calculate_value_from_position(int mouse_x, Rectangle track_rect)
{
    int track_start, track_width, offset;

    track_start = track_rect.min.x + 3;  /* 3px inset */
    track_width = Dx(track_rect) - 6;
    offset = mouse_x - track_start;

    if (offset < 0) offset = 0;
    if (offset > track_width) offset = track_width;

    return ((float)offset / (float)track_width) * 100.0f;
}

/*
 * Get the bounding rectangle for a two-square handle (Tk style)
 */
Rectangle util_get_two_square_handle_rect(Point center)
{
    int square_size = 10;
    int square_gap = 0;  /* No gap between squares */
    int total_width = square_size * 2 + square_gap;
    Rectangle rect;
    int left_x = center.x - total_width / 2;

    rect.min.x = left_x;
    rect.min.y = center.y - square_size / 2;
    rect.max.x = left_x + total_width;
    rect.max.y = center.y + square_size / 2;

    return rect;
}
