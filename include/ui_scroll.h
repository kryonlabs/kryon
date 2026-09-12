#ifndef KRYON_SCROLL_H
#define KRYON_SCROLL_H

/*
 * Public scroll widgets are lexical .kry `Scroll` blocks. Native scroll
 * container, page, and scaffold helpers are internal host support.
 */

int GetScrollbarReservedWidth(int max_scroll);
int GetScrollbarContentWidth(int content_width, int max_scroll);
int GetScrollbarSafeContentWidth(int content_x, int content_width,
                                   int scrollbar_x, int max_scroll);

#endif
