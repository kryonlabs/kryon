#ifndef KRYON_DPI_H
#define KRYON_DPI_H

#include "kryon.h"
#include "ui_dpi_props.generated.h"

#define DPI_BASE_WIDTH 320
#define DPI_BASE_HEIGHT 560

extern DPIState dpi_state;

void InitDPI(void);
void FixDPIFramebufferColor(void);
void InvalidateDPI(void);
void SetDeviceDensity(float density);
void UpdateDPI(int view_width, int view_height);
int IsDPIDirty(void);
int GetLayoutWidth(void);
int GetLayoutHeight(void);
float GetRenderScale(void);
static inline float GetDPIScale(void) { return dpi_state.ui_scale_clamped; }
static inline int GetDPIViewWidth(void) { return dpi_state.layout_width; }
static inline int GetDPIViewHeight(void) { return dpi_state.layout_height; }
static inline float GetDPICameraZoom(void) { return dpi_state.camera_zoom; }

#endif
