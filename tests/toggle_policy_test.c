#include <assert.h>
#include <math.h>

#include "runtime/toggle.h"

int
main(void)
{
    ToggleValueResult value;

    value = ToggleValueFor(0, 1, 1, 1);
    assert(value.value);
    assert(value.changed);
    value = ToggleValueFor(1, 1, 1, 1);
    assert(!value.value);
    assert(value.changed);
    value = ToggleValueFor(1, 1, 0, 1);
    assert(value.value);
    assert(!value.changed);
    value = ToggleValueFor(1, 0, 1, 1);
    assert(value.value);
    assert(!value.changed);
    value = ToggleValueFor(1, 1, 1, 0);
    assert(value.value);
    assert(!value.changed);

    assert(fabsf(ToggleTrackWidthForStyle((StyleFrame){0}, 1.0f) -
                 54.0f) < 0.001f);
    assert(fabsf(ToggleTrackWidthForStyle((StyleFrame){0}, 2.0f) -
                 108.0f) < 0.001f);
    assert(fabsf(ToggleTrackHeightForStyle((StyleFrame){0}, 1.0f) -
                 32.0f) < 0.001f);
    assert(fabsf(ToggleTrackHeightForStyle((StyleFrame){0}, 1.5f) -
                 48.0f) < 0.001f);

    {
        StyleFrame track = {0};
        track.value.fields = StylePaddingX;
        track.value.padding_x = 40.0f;
        assert(fabsf(ToggleTrackWidthForStyle(track, 1.0f) - 40.0f) < 0.001f);
        track.value.padding_x = 0.0f;
        assert(fabsf(ToggleTrackWidthForStyle(track, 1.0f) - 54.0f) < 0.001f);
    }
    {
        StyleFrame track = {0};
        track.value.fields = StylePaddingY;
        track.value.padding_y = 0.0f;
        assert(fabsf(ToggleTrackHeightForStyle(track, 1.0f) - 32.0f) < 0.001f);
    }

    assert(ToggleLabelRole() == 6);

    return 0;
}
