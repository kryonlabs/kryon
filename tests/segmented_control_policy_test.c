#include <assert.h>

#include "runtime/segmented_control.h"

int
main(void)
{
    SegmentedMetrics metrics;
    SegmentedRowAdvance advance;
    SegmentedSelectionResult selection;

    metrics = SegmentedDefaultMetrics(0, 0, 0, 1.0f, (StyleFrame){0},
                                      (StyleFrame){0});
    assert(metrics.gap == 6);
    assert(metrics.row_height == 30);
    assert(metrics.min_item_width == 72);
    assert(metrics.max_item_width == 180);
    assert(metrics.label_padding == 20);

    metrics = SegmentedDefaultMetrics(0, 0, 0, 2.0f, (StyleFrame){0},
                                      (StyleFrame){0});
    assert(metrics.gap == 12);
    assert(metrics.row_height == 60);
    assert(metrics.min_item_width == 144);
    assert(metrics.max_item_width == 360);
    assert(metrics.label_padding == 40);

    metrics = SegmentedDefaultMetrics(24, 50, 90, 1.0f, (StyleFrame){0},
                                      (StyleFrame){0});
    assert(metrics.row_height == 24);
    assert(metrics.min_item_width == 50);
    assert(metrics.max_item_width == 90);

    metrics = SegmentedDefaultMetrics(0, 100, 60, 1.0f, (StyleFrame){0},
                                      (StyleFrame){0});
    assert(metrics.min_item_width == 100);
    assert(metrics.max_item_width == 100);

    {
        StyleFrame segment = {0};
        segment.value.fields = StyleGap;
        segment.value.gap = 10.0f;
        metrics = SegmentedDefaultMetrics(0, 0, 0, 1.0f, (StyleFrame){0},
                                          segment);
        assert(metrics.gap == 10);
    }

    metrics = SegmentedDefaultMetrics(0, 0, 0, 1.0f, (StyleFrame){0},
                                      (StyleFrame){0});
    assert(SegmentedItemWidth(50, metrics) == 72);
    assert(SegmentedItemWidth(200, metrics) == 180);
    assert(SegmentedItemWidth(100, metrics) == 120);

    assert(SegmentedNextRowWidth(0, 50, 6) == 50);
    assert(SegmentedNextRowWidth(100, 50, 6) == 156);

    assert(!SegmentedShouldWrap(1, 0, 200, 100));
    assert(!SegmentedShouldWrap(0, 100, 200, 100));
    assert(!SegmentedShouldWrap(1, 100, 90, 100));
    assert(SegmentedShouldWrap(1, 100, 200, 100));

    advance = SegmentedRowAdvanceFor(1, 100, 2, 50, 100, metrics);
    assert(advance.wrap_before);
    assert(advance.row_width == 50);
    assert(advance.row_count == 1);
    advance = SegmentedRowAdvanceFor(1, 30, 2, 50, 100, metrics);
    assert(!advance.wrap_before);
    assert(advance.row_width == 86);
    assert(advance.row_count == 3);

    assert(SegmentedHeightForRows(2, 30, 6) == 66);
    assert(SegmentedHeightForRows(1, 30, 6) == 30);
    assert(SegmentedHeightForRows(0, 30, 6) == 0);
    assert(SegmentedHeightForRows(2, 0, 6) == 0);

    assert(SegmentedFocusIdFor(5, 2) == 5003);
    assert(SegmentedFocusIdFor(5, 0) == 5001);
    assert(SegmentedFocusIdFor(0, 2) == 0);
    assert(SegmentedFocusIdFor(5, -1) == 0);

    selection = SegmentedSelectionFor(1, 3, 1);
    assert(selection.selected_index == 3);
    assert(selection.changed);
    selection = SegmentedSelectionFor(3, 3, 1);
    assert(selection.selected_index == 3);
    assert(!selection.changed);
    selection = SegmentedSelectionFor(1, -1, 1);
    assert(selection.selected_index == 1);
    assert(!selection.changed);
    selection = SegmentedSelectionFor(1, 3, 0);
    assert(selection.selected_index == 3);
    assert(!selection.changed);

    return 0;
}
