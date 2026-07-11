#ifndef UI_DPI_H
#define UI_DPI_H

#include "flint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_DPI_BASE_WIDTH 320
#define UI_DPI_BASE_HEIGHT 560

typedef struct UIDPIState {
    int view_width;
    int view_height;
    float ui_scale;
    float ui_scale_clamped;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} UIDPIState;

extern UIDPIState ui_dpi_state;

void InitUIDPI(void);
void FixUIDPIFramebufferColor(void);
void InvalidateUIDPI(void);
void UpdateUIDPI(int view_width, int view_height);
int IsUIDPIDirty(void);
static inline float GetUIDPIScale(void) { return ui_dpi_state.ui_scale_clamped; }
static inline int GetUIDPIViewWidth(void) { return ui_dpi_state.view_width; }
static inline int GetUIDPIViewHeight(void) { return ui_dpi_state.view_height; }
static inline float GetUIDPICameraZoom(void) { return ui_dpi_state.camera_zoom; }

#ifdef __cplusplus
}
#endif

#endif
