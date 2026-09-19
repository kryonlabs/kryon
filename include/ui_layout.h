#ifndef KRYON_LAYOUT_H
#define KRYON_LAYOUT_H

#include <stdint.h>
#include "ui_layout_props.generated.h"

// Resolve single-line spacing before drawing widgets or handling their input.
FlexCursor BeginFlexCursor(FlexProps props, int32_t count, float total_item_extent);
FlexCursor FlexStep(FlexCursor cursor, float width, float height);

// Set the view dimensions (should be called when window/viewport changes)
void SetViewSize(int width, int height);

// Get the current view width
int GetViewWidth(void);

// Get the current view height
int GetViewHeight(void);

// Calculate centered column dimensions
// Returns x position and width via pointers if not NULL
void GetCenteredColumn(int max_w, int side_pad, int *x, int *w);

// Calculate page side padding based on current view width
int GetPageSidePadding(void);

#endif // KRYON_LAYOUT_H
