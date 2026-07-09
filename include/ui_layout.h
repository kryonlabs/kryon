#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set the view dimensions (should be called when window/viewport changes)
void SetUIViewSize(int width, int height);

// Get the current view width
int GetUIViewWidth(void);

// Get the current view height
int GetUIViewHeight(void);

// Calculate centered column dimensions
// Returns x position and width via pointers if not NULL
void GetUICenteredColumn(int max_w, int side_pad, int *x, int *w);

// Calculate page side padding based on current view width
int GetUIPageSidePadding(void);

#ifdef __cplusplus
}
#endif

#endif // UI_LAYOUT_H
