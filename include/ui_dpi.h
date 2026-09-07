#ifndef UI_DPI_H
#define UI_DPI_H

#include "kryon.h"

#define UI_DPI_BASE_WIDTH 320
#define UI_DPI_BASE_HEIGHT 560

typedef struct UIDPIState {
    int physical_width;
    int physical_height;
    int view_width;
    int view_height;
    int layout_width;
    int layout_height;
    float ui_scale;
    float ui_scale_clamped;
    float render_scale;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} UIDPIState;

extern UIDPIState ui_dpi_state;

void InitUIDPI(void);
void FixUIDPIFramebufferColor(void);
void InvalidateUIDPI(void);
void SetUIDeviceDensity(float density);
void UpdateUIDPI(int view_width, int view_height);
int IsUIDPIDirty(void);
int GetLayoutWidth(void);
int GetLayoutHeight(void);
float GetRenderScale(void);
static inline float GetUIDPIScale(void) { return ui_dpi_state.ui_scale_clamped; }
static inline int GetUIDPIViewWidth(void) { return ui_dpi_state.layout_width; }
static inline int GetUIDPIViewHeight(void) { return ui_dpi_state.layout_height; }
static inline float GetUIDPICameraZoom(void) { return ui_dpi_state.camera_zoom; }

#endif
