#ifndef UI_SCALING_H
#define UI_SCALING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set the DPI scale factor (should be called once at startup)
void SetUIScale(float scale);

// Get the current DPI scale factor
float GetUIScale(void);

// Scale a pixel value by the DPI factor
int ScaleUIPx(int px);

// Scale and clamp a pixel value between min and max
int ClampUIPx(int px, int min_px, int max_px);

#ifdef __cplusplus
}
#endif

#endif // UI_SCALING_H
