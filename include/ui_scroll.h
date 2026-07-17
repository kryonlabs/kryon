#ifndef UI_SCROLL_H
#define UI_SCROLL_H

#include "flint_compat.generated.h"

typedef struct {
    Rectangle bounds;
    int content_height;
    int content_x;
    int content_width;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
} UIScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} UIScrollView;

typedef int (*UIScrollPageHeightFn)(int content_width, void *user_data);

typedef struct {
    int y;
    int height;
    int max_content_width;
    int min_content_width;
    int side_padding;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
    int measure_passes;
    UIScrollPageHeightFn content_height;
    void *user_data;
} UIScrollPageSpec;

typedef struct {
    UIScrollArea area;
    UIScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} UIScrollPage;

int GetUIScrollbarReservedWidth(int max_scroll);
int GetUIScrollbarContentWidth(int content_width, int max_scroll);
int GetUIScrollbarSafeContentWidth(int content_x, int content_width,
                                   int scrollbar_x, int max_scroll);
UIScrollView MeasureUIScrollContainer(UIScrollArea area);
UIScrollView BeginUIScrollContainer(UIScrollArea area);
void EndUIScrollContainer(UIScrollArea area, UIScrollView view);
UIScrollPage BeginUIScrollPage(UIScrollPageSpec spec);
void EndUIScrollPage(UIScrollPage page);
int DrawUIScrollbar(int x, int y, int viewport_h, int content_h,
                    int *scroll_offset, int max_scroll);

#endif
