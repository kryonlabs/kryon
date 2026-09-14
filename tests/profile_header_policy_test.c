#include <assert.h>
#include <math.h>

#include "runtime/profile_header.h"

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

int
main(void)
{
    ProfileHeaderLayout header;
    ProfilePickerLayout picker;
    ProfilePickerCell cell;

    header = ProfileHeaderLayoutFor(10, 20, 260, 0, 0, 88, 24, 1.0f);
    assert(header.height == 138);
    assert(header.top_pad == 24);
    assert(header.avatar_radius == 28);
    assert(header.avatar_size == 56);
    assert(header.padding_x == 16);
    assert(header.avatar_center_x == 54);
    assert(header.avatar_center_y == 72);
    assert(header.name_x == 96);
    assert(header.name_y == 58);
    assert(header.count_y == 108);
    assert(header.max_name_width == 162);
    check_rect(header.profile_bounds, 22, 40, 64, 64);
    check_rect(header.avatar_tile_bounds, 23, 41, 62, 62);
    check_rect(header.icon_bounds, 29, 47, 50, 50);
    check_rect(header.username_bounds, 96, 54, 88, 24);
    check_rect(header.friends_bounds, 10, 108, 260, 36);
    check_rect(header.header_bounds, 10, 20, 260, 138);

    header = ProfileHeaderLayoutFor(0, 0, 120, 90, 12, 300, 30, 2.0f);
    assert(header.height == 90);
    assert(header.padding_x == 12);
    assert(header.avatar_radius == 56);
    assert(header.name_x == 152);
    assert(header.max_name_width == 96);
    check_rect(header.username_bounds, 152, 68, 96, 30);

    picker = ProfilePickerLayoutFor(640, 480, 0, 25, 1.0f);
    assert(picker.width == 520);
    assert(picker.gap == 8);
    assert(picker.content_width == 484);
    assert(picker.columns == 6);
    assert(picker.cell == 64);
    assert(picker.rows == 5);
    assert(picker.grid_width == 424);
    assert(picker.content_height == 352);
    assert(picker.height == 444);
    assert(picker.max_height == 456);
    assert(picker.icon_inset == 6);

    cell = ProfilePickerCellFor(8, 40, 70, 484, picker);
    assert(cell.row == 1);
    assert(cell.column == 2);
    check_rect(cell.bounds, 214, 142, 64, 64);
    check_rect(cell.icon_bounds, 220, 148, 52, 52);

    picker = ProfilePickerLayoutFor(260, 180, 520, 2, 1.0f);
    assert(picker.width == 240);
    assert(picker.columns == 2);
    assert(picker.cell == 64);
    assert(picker.height == 156);

    return 0;
}
