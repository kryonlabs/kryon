#ifndef UI_SCROLL_H
#define UI_SCROLL_H

#include "kryon_compat.generated.h"

typedef struct {
    Rectangle bounds;
    int content_height;
    int content_x;
    int content_width;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
} ScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} ScrollView;

typedef int (*ScrollPageHeightFn)(int content_width, void *user_data);

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
    ScrollPageHeightFn content_height;
    void *user_data;
} ScrollPageSpec;

typedef struct {
    ScrollArea area;
    ScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} ScrollPage;

typedef int (*ScreenScaffoldTitleFn)(const char *title, int height,
                                       void *user_data);

typedef struct {
    const char *title;
    int title_height;
    int top_gap;
    int bottom_reserved;
    int max_content_width;
    int min_content_width;
    int side_padding;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
    int measure_passes;
    ScrollPageHeightFn content_height;
    void *user_data;
    ScreenScaffoldTitleFn draw_title;
    void *title_user_data;
} ScreenScaffoldSpec;

typedef struct {
    int closed;
    int title_height;
    int content_y;
    int content_h;
    ScrollPage page;
    int content_x;
    int content_w;
    int y;
} ScreenScaffold;

int GetScrollbarReservedWidth(int max_scroll);
int GetScrollbarContentWidth(int content_width, int max_scroll);
int GetScrollbarSafeContentWidth(int content_x, int content_width,
                                   int scrollbar_x, int max_scroll);
ScrollView MeasureScrollContainer(ScrollArea area);
ScrollView BeginScrollContainer(ScrollArea area);
void EndScrollContainer(ScrollArea area, ScrollView view);
void EnsureScrollRectVisible(ScrollArea area, Rectangle rect, int margin);
ScrollPage BeginScrollPage(ScrollPageSpec spec);
void EndScrollPage(ScrollPage page);
ScreenScaffold BeginScreenScaffold(ScreenScaffoldSpec spec);
void EndScreenScaffold(ScreenScaffold scaffold);

#endif
