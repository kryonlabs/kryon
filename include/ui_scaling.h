#ifndef UI_SCALING_H
#define UI_SCALING_H

#include <stdint.h>

// Set the DPI scale factor (should be called once at startup)void SetUIScale(float scale);

// Get the current DPI scale factorfloat GetUIScale(void);

// Scale a pixel value by the DPI factorint ScaleUIPx(int px);

// Scale and clamp a pixel value between min and maxint ClampUIPx(int px, int min_px, int max_px);

#endif // UI_SCALING_H